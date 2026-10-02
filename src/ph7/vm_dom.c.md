# src/ph7/vm_dom.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5849/6311 lines (92.68%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits |  Line | Source |
| -----: | ----: | :--- |
|      - |     1 | `/**` |
|      - |     2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |     3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |     4 | ` */` |
|      - |     5 | `#ifdef PH7_ENABLE_LIBXML` |
|      - |     6 | `#include "ph7int.h"` |
|      - |     7 | `#include <libxml/parser.h>` |
|      - |     8 | `#include <libxml/tree.h>` |
|      - |     9 | `#include <libxml/c14n.h>` |
|      - |    10 | `#include <libxml/xmlsave.h>` |
|      - |    11 | `#include <libxml/xpath.h>` |
|      - |    12 | `#include <libxml/xpathInternals.h>` |
|      - |    13 | `#include <libxml/xmlschemas.h>` |
|      - |    14 | `#include <libxml/relaxng.h>` |
|      - |    15 | `#include <libxml/valid.h>` |
|      - |    16 | `#include <libxml/xinclude.h>` |
|      - |    17 | `#include <libxml/encoding.h>` |
|      - |    18 | `#include <libxml/HTMLparser.h>` |
|      - |    19 | `#include <libxml/HTMLtree.h>` |
|      - |    20 |  |
|      - |    21 | `/*` |
|      - |    22 | ` * ext/dom on libxml2: the DOM classes, declared and bodied in C.` |
|      - |    23 | ` *` |
|      - |    24 | ` * Architecture (see also vm_libxml.c): DOMNode and its subclasses are native` |
|      - |    25 | ` * classes (oo_native.c) whose methods ARE the C below.  Every instance holds` |
|      - |    26 | ` * two slots -- $__res, a phl_domnode resource {phl_xmldoc*, xmlNodePtr}, and` |
|      - |    27 | ` * $__doc, the owning DOMDocument wrapper.  There is no PHP layer left in the` |
|      - |    28 | ` * node tree: what used to be a prelude class over ~30 global __dom_* thunks is` |
|      - |    29 | ` * one C body per method, so the thunks stopped being globally visible names.` |
|      - |    30 | ` *` |
|      - |    31 | ` * Node identity: php guarantees $doc->documentElement === $doc->` |
|      - |    32 | ` * documentElement.  Every wrap goes through DomWrap(), which keys a` |
|      - |    33 | ` * per-document cache ($doc->__nodes) by the node POINTER, so the same` |
|      - |    34 | ` * underlying node always yields the same object.  The entry is a BORROWED` |
|      - |    35 | ` * pointer -- php's own shape, and the reason a wrapper can die at all: a` |
|      - |    36 | ` * cache that took a reference kept every node object a program ever touched` |
|      - |    37 | ` * until the document went, and with it the libxml node behind it.  So` |
|      - |    38 | ` * DomWrap hands back an instance the CALLER OWNS, and DomNodeRelease drops` |
|      - |    39 | ` * the entry.  (The old shape allocated a fresh phl_domnode on every` |
|      - |    40 | ` * navigation step even when the cache then threw the result away; only a` |
|      - |    41 | ` * genuine cache MISS allocates one now.)` |
|      - |    42 | ` *` |
|      - |    43 | ` * Tree surgery (append/insert/replace/remove) is done with manual pointer` |
|      - |    44 | ` * splicing instead of xmlAddChild: xmlAddChild MERGES adjacent text nodes` |
|      - |    45 | ` * and frees the merged-away node, which would dangle any PHP wrapper (and` |
|      - |    46 | ` * violates DOM semantics, which php follows -- appendChild never merges).` |
|      - |    47 | ` * Unlinked nodes are parked on the owning phl_xmldoc's orphan set so they` |
|      - |    48 | ` * are freed with the document at VM reset/release.` |
|      - |    49 | ` */` |
|      - |    50 |  |
|      - |    51 | `/* One native method body. Its receiver's node is DomThisNode(pCtx); apArg is` |
|      - |    52 | ` * php's own argument list, already screened against the declared signature. */` |
|      - |    53 | `#define DOM_METHOD(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |    54 |  |
|      - |    55 | `/* The two slots every wrapper carries, and the document's identity cache. */` |
|      - |    56 | `#define DOM_RES   "__res"` |
|      - |    57 | `#define DOM_DOC   "__doc"` |
|      - |    58 | `#define DOM_NODES "__nodes"` |
|      - |    59 | `/* The base-class => user-class table registerNodeClass writes (defined here` |
|      - |    60 | ` * because the document CLONE, far above it, carries the table across). */` |
|      - |    61 | `#define DOM_NCLS  "__ncls"` |
|      - |    62 |  |
|      - |    63 | `/*` |
|      - |    64 | ` * The DOCUMENT's own directives: php's seven boolean properties.  Their value is` |
|      - |    65 | ` * the extension's own state and not a question about the tree, and php keeps no` |
|      - |    66 | ` * property SLOT for any of them -- each is a read_property/write_property handler,` |
|      - |    67 | `` * which is why php's `(array)` cast and `get_object_vars()` show a DOMDocument as`` |
|      - |    68 | ` * empty.  So the object carries ONE hidden integer here and the class declares the` |
|      - |    69 | ` * seven as VIRTUAL names (PH7_MOD_VIRTUAL) that DomDocProp/DomSetDocProp answer.` |
|      - |    70 | ` * Four of them are read by every parse and one by every refusal, and a clone of a` |
|      - |    71 | ` * document carries the whole word across with the slot itself.` |
|      - |    72 | ` */` |
|      - |    73 | `#define DOM_DFLAGS "__dflags"` |
|      - |    74 | `#define DOM_F_PRESERVE_WS    0x01` |
|      - |    75 | `#define DOM_F_FORMAT_OUTPUT  0x02` |
|      - |    76 | `#define DOM_F_VALIDATE       0x04` |
|      - |    77 | `#define DOM_F_RESOLVE_EXT    0x08` |
|      - |    78 | `#define DOM_F_SUBST_ENT      0x10` |
|      - |    79 | `#define DOM_F_RECOVER        0x20` |
|      - |    80 | `#define DOM_F_STRICT_ERR     0x40` |
|      - |    81 | `/* php's defaults: nothing is validated, expanded, defaulted or recovered unless` |
|      - |    82 | ` * the program asks, whitespace is kept, and a refusal is an exception. */` |
|      - |    83 | `#define DOM_F_DEFAULT (DOM_F_PRESERVE_WS\|DOM_F_STRICT_ERR)` |
|      - |    84 | `static const struct { const char *zName; int iBit; } aDomDocFlag[] = {` |
|      - |    85 | `	{ "preserveWhiteSpace",  DOM_F_PRESERVE_WS   },` |
|      - |    86 | `	{ "formatOutput",        DOM_F_FORMAT_OUTPUT },` |
|      - |    87 | `	{ "validateOnParse",     DOM_F_VALIDATE      },` |
|      - |    88 | `	{ "resolveExternals",    DOM_F_RESOLVE_EXT   },` |
|      - |    89 | `	{ "substituteEntities",  DOM_F_SUBST_ENT     },` |
|      - |    90 | `	{ "recover",             DOM_F_RECOVER       },` |
|      - |    91 | `	{ "strictErrorChecking", DOM_F_STRICT_ERR    }` |
|      - |    92 | `};` |
|      - |    93 | `/* Is this directive on for this document object? Answers false for anything that` |
|      - |    94 | ` * is not one (a node's $__doc points at its own holder when it has no document). */` |
|   9550 |    95 | `static int DomDocFlag(ph7_class_instance *pDoc,int iBit)` |
|      5 |    96 | `{` |
|   9555 |    97 | `	return pDoc != 0 && (PH7_NativeAttrInt(pDoc,DOM_DFLAGS) & iBit) != 0;` |
|      5 |    98 | `}` |
|      - |    99 |  |
|      - |   100 | `/*` |
|      - |   101 | ` * php's DOMException carries the DOM level-2 error CODE beside its sentence --` |
|      - |   102 | `` * `catch (DOMException $e) { if ($e->getCode() === DOM_NOT_FOUND_ERR) ... }` is`` |
|      - |   103 | ` * how a caller tells one refusal from another, and the sentence is only a` |
|      - |   104 | ` * sentence.  Every throw below states its code; DOM_PHP_ERR (0) is php's own` |
|      - |   105 | ` * "not a DOM error" and no refusal here uses it.` |
|      - |   106 | ` */` |
|      - |   107 | `#define DOM_ERR_INDEX_SIZE     1` |
|      - |   108 | `#define DOM_ERR_HIERARCHY      3` |
|      - |   109 | `#define DOM_ERR_WRONG_DOC      4` |
|      - |   110 | `#define DOM_ERR_INVALID_CHAR   5` |
|      - |   111 | `#define DOM_ERR_NO_MOD         7` |
|      - |   112 | `#define DOM_ERR_NOT_FOUND      8` |
|      - |   113 | `#define DOM_ERR_NOT_SUPPORTED  9` |
|      - |   114 | `#define DOM_ERR_INVALID_STATE 11` |
|      - |   115 | `#define DOM_ERR_SYNTAX        12` |
|      - |   116 | `#define DOM_ERR_NAMESPACE     14` |
|      - |   117 | `/* The sentence php prints for each -- so a refusal that travels as a code can` |
|      - |   118 | ` * be raised from one place. */` |
|    906 |   119 | `static const char * DomErrText(int iCode)` |
|      1 |   120 | `{` |
|    907 |   121 | `	switch( iCode ){` |
|     61 |   122 | `	case DOM_ERR_INDEX_SIZE:   return "Index Size Error";` |
|    119 |   123 | `	case DOM_ERR_HIERARCHY:    return "Hierarchy Request Error";` |
|     91 |   124 | `	case DOM_ERR_WRONG_DOC:    return "Wrong Document Error";` |
|    169 |   125 | `	case DOM_ERR_INVALID_CHAR: return "Invalid Character Error";` |
|     37 |   126 | `	case DOM_ERR_NO_MOD:       return "No Modification Allowed Error";` |
|     17 |   127 | `	case DOM_ERR_NOT_SUPPORTED: return "Not Supported Error";` |
|     43 |   128 | `	case DOM_ERR_INVALID_STATE: return "Invalid State Error";` |
|      9 |   129 | `	case DOM_ERR_SYNTAX:       return "Syntax Error";` |
|    261 |   130 | `	case DOM_ERR_NAMESPACE:    return "Namespace Error";` |
|    109 |   131 | `	default:                   return "Not Found Error";` |
|      - |   132 | `	}` |
|    454 |   133 | `}` |
|      - |   134 | `/* Forward: the refusal has to ask the receiver's document for its mode --` |
|      - |   135 | ` * and check that what the slot holds IS a document. */` |
|      - |   136 | `static ph7_class_instance * DomThisDoc(ph7_context *pCtx);` |
|      - |   137 | `static phl_domnode * DomResOf(ph7_class_instance *pObj);` |
|      - |   138 | `/*` |
|      - |   139 | ` * A DOM refusal, in whichever of php's TWO modes the document is in.` |
|      - |   140 | ` *` |
|      - |   141 | `` * `$doc->strictErrorChecking` (true by default) decides whether a refusal is an`` |
|      - |   142 | ` * exception or a warning: with it off, php raises the SAME sentence as an` |
|      - |   143 | ` * E_WARNING under the method's own name and the method answers instead of` |
|      - |   144 | ``  * unwinding. The two answers it gives are the two this file needs -- `false` `` |
|      - |   145 | ` * from a method that returns something, and NOTHING from one php declares` |
|      - |   146 | `` * `void` -- so the mode is one call with the answer as its argument.`` |
|      - |   147 | ` *` |
|      - |   148 | ``  * The flag is document state rather than tree state: it survives a `loadXML()` `` |
|      - |   149 | ` * onto the same object, and a clone carries it. It is read off the RECEIVER's` |
|      - |   150 | ` * document -- the argument's own is not consulted even when the refusal is` |
|      - |   151 | `` * about that argument -- with exactly one exception, `adoptNode`, which reads`` |
|      - |   152 | ` * the argument's and is passed it explicitly.` |
|      - |   153 | ` *` |
|      - |   154 | ` * And not every refusal consults it at all: php passes a hardcoded "strict" at` |
|      - |   155 | `` * `setAttribute` and `toggleAttribute`, which throw whatever the flag says.`` |
|      - |   156 | ` * Those call DomThrowAlways.` |
|      - |   157 | ` */` |
|      - |   158 | `#define DOM_REFUSE_FALSE 0   /* the method answers false */` |
|      - |   159 | `#define DOM_REFUSE_VOID  1   /* the method answers nothing (php declares it void) */` |
|      - |   160 | `/*` |
|      - |   161 | ` * A refusal made while the PROPERTY hook is running.` |
|      - |   162 | ` *` |
|      - |   163 | ``  * The readers and writers below are shared: the same body answers `$el->tagName` `` |
|      - |   164 | ` * and the debug walk, and under the hook it runs on a scratch context inside the` |
|      - |   165 | ` * member opcode. A throw raised there would run the enclosing catch mid-access,` |
|      - |   166 | ` * before the opcode has settled its stack -- which is why PH7_NativePropCtx has` |
|      - |   167 | ` * a refusal channel of its own. Answers 1 when the refusal was RECORDED (the` |
|      - |   168 | ` * opcode raises it where the access lands) and 0 when the caller must throw the` |
|      - |   169 | ` * ordinary way, which is every call made from a method body.` |
|      - |   170 | ` */` |
|    599 |   171 | `static int DomPropRefuse(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zMsg)` |
|      1 |   172 | `{` |
|    600 |   173 | `	PH7_NativePropCtx *pProp = pCtx ? pCtx->pPropCtx : 0;` |
|    600 |   174 | `	if( pProp == 0 ){` |
|    403 |   175 | `		return 0;` |
|      - |   176 | `	}` |
|    198 |   177 | `	pProp->bAnswered = 1;` |
|    198 |   178 | `	pProp->zThrowClass = zClass;` |
|    198 |   179 | `	pProp->iThrowCode = iCode;` |
|    198 |   180 | `	SyBufferFormat(pProp->zThrowMsg,sizeof(pProp->zThrowMsg),"%s",zMsg);` |
|    198 |   181 | `	return 1;` |
|    300 |   182 | `}` |
|      - |   183 | `/* The same, for a refusal whose message is formatted. */` |
|    153 |   184 | `static sxi32 DomPropThrow(ph7_context *pCtx,const char *zClass,sxi32 iCode,` |
|      - |   185 | `	const char *zFormat,...)` |
|      1 |   186 | `{` |
|      - |   187 | `	SyBlob sMsg;` |
|      - |   188 | `	va_list ap;` |
|      - |   189 | `	sxi32 rc;` |
|    154 |   190 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    154 |   191 | `	va_start(ap,zFormat);` |
|    154 |   192 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|    154 |   193 | `	va_end(ap);` |
|    154 |   194 | `	SyBlobNullAppend(&sMsg);` |
|    154 |   195 | `	if( DomPropRefuse(pCtx,zClass,iCode,(const char *)SyBlobData(&sMsg)) ){` |
|    154 |   196 | `		rc = PH7_OK;` |
|     77 |   197 | `	}else{` |
|    ! 0 |   198 | `		rc = PH7_VmThrowExceptionCode(pCtx,zClass,iCode,"%s",(const char *)SyBlobData(&sMsg));` |
|      - |   199 | `	}` |
|    154 |   200 | `	SyBlobRelease(&sMsg);` |
|    154 |   201 | `	return rc;` |
|      1 |   202 | `}` |
|    408 |   203 | `static int DomThrowAlways(ph7_context *pCtx,int iCode)` |
|      1 |   204 | `{` |
|    409 |   205 | `	if( DomPropRefuse(pCtx,"DOMException",(sxi32)iCode,DomErrText(iCode)) ){` |
|      7 |   206 | `		return PH7_OK;` |
|      - |   207 | `	}` |
|    403 |   208 | `	return PH7_VmThrowExceptionCode(pCtx,"DOMException",(sxi32)iCode,"%s",DomErrText(iCode));` |
|    205 |   209 | `}` |
|    432 |   210 | `static int DomThrowFor(ph7_context *pCtx,ph7_class_instance *pDoc,int iCode,int iAnswer)` |
|      1 |   211 | `{` |
|      - |   212 | `	/* Only a DOCUMENT carries the flag: a constructed ownerless node's $__doc` |
|      - |   213 | `	 * slot points at its own holder object, and php is always strict there --` |
|      - |   214 | `	 * there is no document to have said otherwise. */` |
|    433 |   215 | `	phl_domnode *pDocNd = pDoc ? DomResOf(pDoc) : 0;` |
|    433 |   216 | `	xmlNodePtr pDocNode = pDocNd ? (xmlNodePtr)pDocNd->pNode : 0;` |
|    432 |   217 | `	if( pDocNode` |
|    406 |   218 | `	 && (pDocNode->type != XML_DOCUMENT_NODE && pDocNode->type != XML_HTML_DOCUMENT_NODE) ){` |
|     15 |   219 | `		pDoc = 0;` |
|      7 |   220 | `	}` |
|    433 |   221 | `	if( pDoc && !DomDocFlag(pDoc,DOM_F_STRICT_ERR) ){` |
|      - |   222 | ``		/* The context prints php's own `Class::method(): ` in front of it. */`` |
|     55 |   223 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,DomErrText(iCode));` |
|     55 |   224 | `		if( iAnswer == DOM_REFUSE_FALSE ){` |
|     37 |   225 | `			ph7_result_bool(pCtx,0);` |
|     18 |   226 | `		}` |
|     55 |   227 | `		return PH7_OK;` |
|      - |   228 | `	}` |
|    379 |   229 | `	return DomThrowAlways(pCtx,iCode);` |
|    217 |   230 | `}` |
|    324 |   231 | `static int DomThrow(ph7_context *pCtx,int iCode)` |
|      1 |   232 | `{` |
|    325 |   233 | `	return DomThrowFor(pCtx,DomThisDoc(pCtx),iCode,DOM_REFUSE_FALSE);` |
|      1 |   234 | `}` |
|     98 |   235 | `static int DomThrowVoid(ph7_context *pCtx,int iCode)` |
|      1 |   236 | `{` |
|     99 |   237 | `	return DomThrowFor(pCtx,DomThisDoc(pCtx),iCode,DOM_REFUSE_VOID);` |
|      1 |   238 | `}` |
|      - |   239 | `/* Property names are byte-exact in php, and every name that reaches here is` |
|      - |   240 | ` * NUL-terminated (ph7_value_to_string null-appends). */` |
|  66596 |   241 | `static int DomNameIs(const char *zName,const char *zWant)` |
|      5 |   242 | `{` |
|  66601 |   243 | `	sxu32 n = (sxu32)SyStrlen(zWant);` |
|  66601 |   244 | `	return SyStrlen(zName) == n && SyStrncmp(zName,zWant,n) == 0;` |
|      5 |   245 | `}` |
|      - |   246 | `/* ...and the ONE name the DOM matches case-insensitively: insertAdjacent*'s` |
|      - |   247 | `` * `$where` word ("BeforeBegin" works), php's zend_string_equals_literal_ci. */`` |
|    130 |   248 | `static int DomNameIsCi(const char *zName,const char *zWant)` |
|      1 |   249 | `{` |
|    131 |   250 | `	sxu32 n = (sxu32)SyStrlen(zWant);` |
|    131 |   251 | `	return SyStrlen(zName) == n && SyStrnicmp(zName,zWant,n) == 0;` |
|      1 |   252 | `}` |
|      - |   253 | `/* The handle behind an instance's $__res, or NULL for anything else. */` |
|  22849 |   254 | `static phl_domnode * DomResOf(ph7_class_instance *pObj)` |
|      5 |   255 | `{` |
|  22854 |   256 | `	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,DOM_RES) : 0;` |
|  22854 |   257 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|    235 |   258 | `		return 0;` |
|      - |   259 | `	}` |
|  22620 |   260 | `	return (phl_domnode *)pVal->x.pOther;` |
|  11431 |   261 | `}` |
|      - |   262 | `/* The receiver of a native method, and the two things every body wants from it. */` |
|  12429 |   263 | `static phl_domnode * DomThisNode(ph7_context *pCtx)` |
|      5 |   264 | `{` |
|  12434 |   265 | `	return DomResOf(PH7_ContextThis(pCtx));` |
|      5 |   266 | `}` |
|   7892 |   267 | `static ph7_class_instance * DomThisDoc(ph7_context *pCtx)` |
|      5 |   268 | `{` |
|   7897 |   269 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   7897 |   270 | `	return pThis ? PH7_NativeAttrObj(pThis,DOM_DOC) : 0;` |
|      5 |   271 | `}` |
|      - |   272 | `/* A fresh handle onto one node of pShell's tree. Freed with the VM allocator. */` |
|   7606 |   273 | `static phl_domnode * DomNewRes(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode)` |
|      5 |   274 | `{` |
|   7611 |   275 | `	phl_domnode *pWrap = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|   7611 |   276 | `	if( pWrap ){` |
|   7611 |   277 | `		pWrap->pShell = pShell;` |
|   7611 |   278 | `		pWrap->pNode = pNode;` |
|   3803 |   279 | `	}` |
|   7611 |   280 | `	return pWrap;` |
|      5 |   281 | `}` |
|      - |   282 | `/* Store a handle in an instance's $__res slot. */` |
|   7542 |   283 | `static void DomSetRes(ph7_vm *pVm,ph7_class_instance *pObj,phl_domnode *pRes)` |
|      5 |   284 | `{` |
|      - |   285 | `	ph7_value sVal;` |
|   7547 |   286 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|   7547 |   287 | `	sVal.x.pOther = pRes;` |
|   7547 |   288 | `	sVal.iFlags = MEMOBJ_RES;` |
|   7547 |   289 | `	PH7_NativeSetProp(&(*pVm),pObj,DOM_RES,sizeof(DOM_RES)-1,&sVal);` |
|      - |   290 | `	/* A handle that IS the document names this object as the tree's document` |
|      - |   291 | `	 * wrapper, so anything holding only the SHELL -- ext/simplexml's` |
|      - |   292 | `	 * dom_import_simplexml() -- can reach the cache the identity rule lives in.` |
|      - |   293 | `	 * Borrowed: DomDocRelease clears it when the object goes. */` |
|   7542 |   294 | `	if( pRes && pRes->pShell && pRes->pNode` |
|   7547 |   295 | `	 && (((xmlNodePtr)pRes->pNode)->type == XML_DOCUMENT_NODE` |
|   5905 |   296 | `	  \|\| ((xmlNodePtr)pRes->pNode)->type == XML_HTML_DOCUMENT_NODE) ){` |
|   3299 |   297 | `		pRes->pShell->pDocObj = (void *)pObj;` |
|   1647 |   298 | `	}` |
|   7547 |   299 | `}` |
|      - |   300 | `/* ph7_class::xRelease for DOMDocument: forget a document object its tree still` |
|      - |   301 | ` * points at. Not a __destruct -- php declares none. */` |
|   1486 |   302 | `static void DomDocRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 |   303 | `{` |
|   1487 |   304 | `	ph7_value *pVal = PH7_NativeAttr(pThis,DOM_RES);` |
|   1487 |   305 | `	phl_domnode *pNd = pVal && (pVal->iFlags & MEMOBJ_RES)` |
|   2228 |   306 | `		? (phl_domnode *)pVal->x.pOther : 0;` |
|    743 |   307 | `	(void)pVm;` |
|   1487 |   308 | `	if( pNd && pNd->pShell && pNd->pShell->pDocObj == (void *)pThis ){` |
|   1485 |   309 | `		pNd->pShell->pDocObj = 0;` |
|    742 |   310 | `	}` |
|   1487 |   311 | `}` |
|      - |   312 | `/*` |
|      - |   313 | ` * The document's identity cache, materialized and separated from any copy that` |
|      - |   314 | ` * shares it. Same three moves a native class always needs to own an array slot` |
|      - |   315 | ` * (WeakMap's WmStore is the other one).` |
|      - |   316 | ` */` |
|   9186 |   317 | `static ph7_hashmap * DomCache(ph7_vm *pVm,ph7_class_instance *pDoc)` |
|      5 |   318 | `{` |
|   9191 |   319 | `	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NODES) : 0;` |
|   9191 |   320 | `	if( pSlot == 0 ){` |
|    175 |   321 | `		return 0;` |
|      - |   322 | `	}` |
|   9017 |   323 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    270 |   324 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|    ! 0 |   325 | `			return 0;` |
|      - |   326 | `		}` |
|    134 |   327 | `	}` |
|   9017 |   328 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|   4598 |   329 | `}` |
|      - |   330 | `/* php's class for a node type. Anything else is a plain DOMNode, as before. */` |
|   3954 |   331 | `static const char * DomClassOfKind(int iKind)` |
|      5 |   332 | `{` |
|   3959 |   333 | `	switch( iKind ){` |
|   2904 |   334 | `	case XML_ELEMENT_NODE:       return "DOMElement";` |
|    412 |   335 | `	case XML_ATTRIBUTE_NODE:     return "DOMAttr";` |
|    270 |   336 | `	case XML_TEXT_NODE:          return "DOMText";` |
|     41 |   337 | `	case XML_CDATA_SECTION_NODE: return "DOMCdataSection";` |
|     53 |   338 | `	case XML_COMMENT_NODE:       return "DOMComment";` |
|     49 |   339 | `	case XML_PI_NODE:            return "DOMProcessingInstruction";` |
|     93 |   340 | `	case XML_DOCUMENT_FRAG_NODE: return "DOMDocumentFragment";` |
|     35 |   341 | `	case XML_ENTITY_REF_NODE:    return "DOMEntityReference";` |
|     27 |   342 | `	case XML_DTD_NODE:` |
|     56 |   343 | `	case XML_DOCUMENT_TYPE_NODE: return "DOMDocumentType";` |
|      - |   344 | `	/* php has one class for the whole declaration half of a DTD and hands an` |
|      - |   345 | ``	 * ELEMENT declaration the entity's, which is the class a `$doctype->`` |
|      - |   346 | ``	 * childNodes` walk meets. DOMEntity's own readers ask the node's real`` |
|      - |   347 | `	 * type before touching a field, so an element declaration answers null` |
|      - |   348 | `	 * from each of them rather than reading an xmlElement as an xmlEntity. */` |
|     17 |   349 | `	case XML_ENTITY_DECL:` |
|     35 |   350 | `	case XML_ELEMENT_DECL:       return "DOMEntity";` |
|     26 |   351 | `	case XML_NOTATION_NODE:      return "DOMNotation";` |
|    ! 0 |   352 | `	default:                     return "DOMNode";` |
|      - |   353 | `	}` |
|   1982 |   354 | `}` |
|      - |   355 | `/*` |
|      - |   356 | ` * The nodeType php reports, which is not always libxml's own.` |
|      - |   357 | ` *` |
|      - |   358 | ` * The two numberings were built to agree -- a text node is 3 in both -- but` |
|      - |   359 | ` * libxml parses a DOCTYPE into an XML_DTD_NODE (14) where the DOM's number for` |
|      - |   360 | ` * one is DOCUMENT_TYPE_NODE (10), and php reports the DOM's.  So` |
|      - |   361 | `` * `$n->nodeType === XML_DOCUMENT_TYPE_NODE` -- the way a walk tells the doctype`` |
|      - |   362 | `` * from an element without a `get_class` -- was FALSE here for every document`` |
|      - |   363 | ` * carrying one.` |
|      - |   364 | ` */` |
|    106 |   365 | `static int DomNodeTypeOf(xmlNodePtr pNode)` |
|      1 |   366 | `{` |
|    107 |   367 | `	if( pNode == 0 ){` |
|    ! 0 |   368 | `		return 0;` |
|      - |   369 | `	}` |
|    107 |   370 | `	return pNode->type == XML_DTD_NODE ? (int)XML_DOCUMENT_TYPE_NODE : (int)pNode->type;` |
|     54 |   371 | `}` |
|      - |   372 | `/* Defined with registerNodeClass below, which is the only thing that makes the` |
|      - |   373 | ` * answer anything other than DomClassOfKind's. */` |
|      - |   374 | `static const char * DomWrapClassName(ph7_vm *pVm,ph7_class_instance *pDoc,int iKind,` |
|      - |   375 | `	SyBlob *pOut);` |
|      - |   376 | `/* Defined with the teardown machinery below: every wrap marks its node HELD. */` |
|      - |   377 | `static void DomNodeMarkHeld(xmlNodePtr pNode,ph7_class_instance *pObj);` |
|      - |   378 | `/*` |
|      - |   379 | ` * The wrapper object for one node of pDoc's tree -- the same one every time,` |
|      - |   380 | `` * which is what makes `$doc->documentElement === $doc->documentElement` true.`` |
|      - |   381 | ` *` |
|      - |   382 | ` * OWNED: the caller holds a reference and gives it back. A caller that hands` |
|      - |   383 | ` * the node to PHP goes through DomResultOwned; one that stores it takes its own` |
|      - |   384 | ` * reference (PH7_NativeSetAttrObj, ph7_array_add_elem) and then unrefs.` |
|      - |   385 | ` */` |
|   4550 |   386 | `static ph7_class_instance * DomWrap(ph7_vm *pVm,ph7_class_instance *pDoc,` |
|      - |   387 | `	phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      5 |   388 | `{` |
|      - |   389 | `	ph7_hashmap *pCache;` |
|   4555 |   390 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |   391 | `	ph7_class_instance *pObj;` |
|      - |   392 | `	ph7_class *pClass;` |
|      - |   393 | `	phl_domnode *pRes;` |
|      - |   394 | `	const char *zClass;` |
|      - |   395 | `	ph7_value sKey,sVal;` |
|      - |   396 | `	SyBlob sName;` |
|   4555 |   397 | `	if( pNode == 0 \|\| pDoc == 0 ){` |
|    141 |   398 | `		return 0;` |
|      - |   399 | `	}` |
|   4415 |   400 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|      - |   401 | `		/* The document is its own wrapper: php answers the SAME DOMDocument. */` |
|     19 |   402 | `		pDoc->iRef++;` |
|     19 |   403 | `		return pDoc;` |
|      - |   404 | `	}` |
|   4397 |   405 | `	pCache = DomCache(&(*pVm),pDoc);` |
|   4397 |   406 | `	if( pCache == 0 ){` |
|    ! 0 |   407 | `		return 0;` |
|      - |   408 | `	}` |
|   4397 |   409 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|   4397 |   410 | `	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|    463 |   411 | `		ph7_value *pHit = HashmapExtractNodeValue(pEntry);` |
|    463 |   412 | `		if( pHit && (pHit->iFlags & MEMOBJ_INT) ){` |
|    463 |   413 | `			ph7_class_instance *pLive = (ph7_class_instance *)(sxuptr)pHit->x.iVal;` |
|    463 |   414 | `			PH7_MemObjRelease(&sKey);` |
|    463 |   415 | `			pLive->iRef++;` |
|    463 |   416 | `			return pLive;` |
|      - |   417 | `		}` |
|    ! 0 |   418 | `	}` |
|      - |   419 | `	/* php's class for the kind, unless this document has REGISTERED another` |
|      - |   420 | `	 * one for it (registerNodeClass). The name may live in sName's buffer, so` |
|      - |   421 | `	 * the blob outlives the lookup. */` |
|   3939 |   422 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|   3939 |   423 | `	zClass = DomWrapClassName(&(*pVm),pDoc,(int)pNode->type,&sName);` |
|   3939 |   424 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|   3939 |   425 | `	SyBlobRelease(&sName);` |
|   3939 |   426 | `	pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|   3939 |   427 | `	pRes = pObj ? DomNewRes(&(*pVm),pShell,pNode) : 0;` |
|   3939 |   428 | `	if( pRes == 0 ){` |
|    ! 0 |   429 | `		if( pObj ){` |
|    ! 0 |   430 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |   431 | `		}` |
|    ! 0 |   432 | `		PH7_MemObjRelease(&sKey);` |
|    ! 0 |   433 | `		return 0;` |
|      - |   434 | `	}` |
|   3939 |   435 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|   3939 |   436 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|   3939 |   437 | `	DomNodeMarkHeld(pNode,pObj);` |
|      - |   438 | `	/* The entry is the POINTER and no reference (an xmlNs handle, which shares` |
|      - |   439 | `	 * this table, is a MEMOBJ_RES: the two never key the same address). The` |
|      - |   440 | `	 * object's own release takes the entry back out. */` |
|   3939 |   441 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,(sxi64)(sxuptr)pObj);` |
|   3939 |   442 | `	PH7_HashmapInsert(pCache,&sKey,&sVal);` |
|   3939 |   443 | `	PH7_MemObjRelease(&sKey);` |
|   3939 |   444 | `	return pObj;                             /* the constructor's reference: the caller's */` |
|   2280 |   445 | `}` |
|      - |   446 | `/*` |
|      - |   447 | ` * The wrapper for one node of a tree whose DOCUMENT OBJECT the caller does not` |
|      - |   448 | `` * have -- ext/simplexml's `dom_import_simplexml()`, which holds a shell and a`` |
|      - |   449 | ` * node and nothing else.` |
|      - |   450 | ` *` |
|      - |   451 | `` * The identity rule (`$doc->documentElement === $doc->documentElement`) lives`` |
|      - |   452 | ` * in a cache keyed on the document object, so a tree that has none yet gets one` |
|      - |   453 | ` * built here and remembered on the shell; a tree that came from a DOMDocument` |
|      - |   454 | ` * already names it, which is what makes an import back out of a SimpleXML made` |
|      - |   455 | ` * from that document answer the document's own nodes. OWNED, like DomWrap's.` |
|      - |   456 | ` */` |
|      8 |   457 | `PH7_PRIVATE ph7_class_instance * PH7_DomWrapForeign(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode)` |
|      1 |   458 | `{` |
|      - |   459 | `	ph7_class_instance *pDoc;` |
|      9 |   460 | `	if( pShell == 0 \|\| pShell->pDoc == 0 ){` |
|    ! 0 |   461 | `		return 0;` |
|      - |   462 | `	}` |
|      9 |   463 | `	pDoc = (ph7_class_instance *)pShell->pDocObj;` |
|      9 |   464 | `	if( pDoc == 0 ){` |
|      3 |   465 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DOMDocument",` |
|      - |   466 | `			sizeof("DOMDocument")-1,FALSE,0);` |
|      - |   467 | `		phl_domnode *pRes;` |
|      3 |   468 | `		pDoc = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|      3 |   469 | `		pRes = pDoc ? DomNewRes(&(*pVm),pShell,pShell->pDoc) : 0;` |
|      3 |   470 | `		if( pRes == 0 ){` |
|    ! 0 |   471 | `			if( pDoc ){` |
|    ! 0 |   472 | `				PH7_ClassInstanceUnref(pDoc);` |
|    ! 0 |   473 | `			}` |
|    ! 0 |   474 | `			return 0;` |
|      - |   475 | `		}` |
|      3 |   476 | `		PH7_NativeSetAttrInt(&(*pVm),pDoc,DOM_DFLAGS,DOM_F_DEFAULT);` |
|      3 |   477 | `		DomSetRes(&(*pVm),pDoc,pRes);          /* ...which records pShell->pDocObj */` |
|      3 |   478 | `		PH7_NativeSetAttrObj(&(*pVm),pDoc,DOM_DOC,pDoc);` |
|      1 |   479 | `	}` |
|      9 |   480 | `	return DomWrap(&(*pVm),pDoc,pShell,(xmlNodePtr)pNode);` |
|      5 |   481 | `}` |
|      - |   482 | `/* Answer a borrowed instance (or NULL) from a native method. */` |
|   4116 |   483 | `static int DomResultWrap(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      5 |   484 | `{` |
|      - |   485 | `	ph7_value sRes;` |
|   4121 |   486 | `	if( pObj == 0 ){` |
|     77 |   487 | `		ph7_result_null(pCtx);` |
|     77 |   488 | `		return PH7_OK;` |
|      - |   489 | `	}` |
|   4045 |   490 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|   4045 |   491 | `	sRes.x.pOther = pObj;` |
|   4045 |   492 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|   4045 |   493 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|   4045 |   494 | `	return PH7_OK;` |
|   2063 |   495 | `}` |
|      - |   496 | `/* The same, for an instance the caller OWNS: the result takes its own` |
|      - |   497 | ` * reference and this one goes back. */` |
|   3936 |   498 | `static int DomResultOwned(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      5 |   499 | `{` |
|   3941 |   500 | `	int rc = DomResultWrap(pCtx,pObj);` |
|   3941 |   501 | `	if( pObj ){` |
|   3909 |   502 | `		PH7_ClassInstanceUnref(pObj);` |
|   1952 |   503 | `	}` |
|   3941 |   504 | `	return rc;` |
|      5 |   505 | `}` |
|      - |   506 | `/* The common tail: wrap a node of the RECEIVER's document and answer it. */` |
|   3782 |   507 | `static int DomResultNodeOf(ph7_context *pCtx,phl_domnode *pNd,xmlNodePtr pNode)` |
|      5 |   508 | `{` |
|   3787 |   509 | `	if( pNd == 0 \|\| pNode == 0 ){` |
|    275 |   510 | `		ph7_result_null(pCtx);` |
|    275 |   511 | `		return PH7_OK;` |
|      - |   512 | `	}` |
|   3515 |   513 | `	return DomResultOwned(pCtx,DomWrap(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNode));` |
|   1896 |   514 | `}` |
|      - |   515 | `/* The phl_domnode behind a DOMNode-typed ARGUMENT (already screened by ZPP). */` |
|   1854 |   516 | `static phl_domnode * DomObjArg(ph7_value *pVal)` |
|      3 |   517 | `{` |
|   1857 |   518 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      5 |   519 | `		return 0;` |
|      - |   520 | `	}` |
|   1853 |   521 | `	return DomResOf((ph7_class_instance *)pVal->x.pOther);` |
|    930 |   522 | `}` |
|      - |   523 | `/* ...and the DOCUMENT object it belongs to, which is where its wrapper is` |
|      - |   524 | `` * cached and what its `ownerDocument` answers. */`` |
|    600 |   525 | `static ph7_class_instance * DomObjArgDoc(ph7_value *pVal)` |
|      3 |   526 | `{` |
|    603 |   527 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |   528 | `		return 0;` |
|      - |   529 | `	}` |
|    603 |   530 | `	return PH7_NativeAttrObj((ph7_class_instance *)pVal->x.pOther,DOM_DOC);` |
|    303 |   531 | `}` |
|      - |   532 | `/* The slot a DOMNameSpaceNode carries beside its own two: the element that` |
|      - |   533 | ` * MAKES the declaration, which is php's parentNode for one. */` |
|      - |   534 | `#define DOM_NS_OWNER "__owner"` |
|      - |   535 |  |
|      - |   536 | `/* The element a DOMNameSpaceNode argument was found on, or NULL for anything` |
|      - |   537 | ` * else -- the slot exists on that class alone. */` |
|     16 |   538 | `static phl_domnode * DomNsNodeOwner(ph7_value *pVal)` |
|      1 |   539 | `{` |
|      - |   540 | `	ph7_class_instance *pObj;` |
|     17 |   541 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |   542 | `		return 0;` |
|      - |   543 | `	}` |
|     17 |   544 | `	pObj = (ph7_class_instance *)pVal->x.pOther;` |
|     17 |   545 | `	return DomResOf(PH7_NativeAttrObj(pObj,DOM_NS_OWNER));` |
|      9 |   546 | `}` |
|      - |   547 | `/* Orphan bookkeeping: nodes not linked into their tree but still owned */` |
|   1372 |   548 | `static void DomOrphanAdd(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      3 |   549 | `{` |
|   1375 |   550 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|      - |   551 | `	sxu32 n;` |
|  14487 |   552 | `	for( n = 0 ; n < SySetUsed(&pShell->aOrphans) ; ++n ){` |
|  13115 |   553 | `		if( apOrphan[n] == pNode ){` |
|    ! 0 |   554 | `			return;` |
|      - |   555 | `		}` |
|   6559 |   556 | `	}` |
|   1375 |   557 | `	SySetPut(&pShell->aOrphans,(const void *)&pNode);` |
|    689 |   558 | `}` |
|   1208 |   559 | `static void DomOrphanRemove(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      3 |   560 | `{` |
|   1211 |   561 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|   1211 |   562 | `	sxu32 n,nUsed = SySetUsed(&pShell->aOrphans);` |
|   5387 |   563 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|   5167 |   564 | `		if( apOrphan[n] == pNode ){` |
|    991 |   565 | `			apOrphan[n] = apOrphan[nUsed-1];` |
|    991 |   566 | `			SySetTruncate(&pShell->aOrphans,nUsed-1);` |
|    991 |   567 | `			return;` |
|      - |   568 | `		}` |
|   2091 |   569 | `	}` |
|    607 |   570 | `}` |
|      - |   571 | `/* Detach a node from wherever it is (tree or orphan set) prior to linking */` |
|    600 |   572 | `static void DomDetach(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      3 |   573 | `{` |
|    603 |   574 | `	if( pNode->parent ){` |
|    171 |   575 | `		xmlUnlinkNode(pNode);` |
|     86 |   576 | `	}else{` |
|    433 |   577 | `		DomOrphanRemove(pShell,pNode);` |
|      - |   578 | `	}` |
|    603 |   579 | `}` |
|      - |   580 | `/*` |
|      - |   581 | ` * Mark a node as HELD by one wrapper, in libxml's own per-node binding slot --` |
|      - |   582 | ` * where php keeps its object too.` |
|      - |   583 | ` *` |
|      - |   584 | ` * That slot, and not the identity cache, is what a free walk asks: the cycle` |
|      - |   585 | ` * collector may reach a document before its nodes, and the cache is gone by` |
|      - |   586 | ``  * then. A synthesized NOTATION stand-in is the exception -- its `_private` `` |
|      - |   587 | ` * already carries the declaration it stands for, which is how a second lookup` |
|      - |   588 | ` * finds the same stand-in (DomNotationNode) -- and nothing frees one of those` |
|      - |   589 | ` * anyway.` |
|      - |   590 | ` */` |
|   4188 |   591 | `static void DomNodeMarkHeld(xmlNodePtr pNode,ph7_class_instance *pObj)` |
|      5 |   592 | `{` |
|   4193 |   593 | `	if( pNode->type != XML_NOTATION_NODE ){` |
|   4169 |   594 | `		pNode->_private = (void *)pObj;` |
|   2082 |   595 | `	}` |
|   4193 |   596 | `}` |
|      - |   597 | `/* ...and is this node still that wrapper's? A notation stand-in always is. */` |
|   4150 |   598 | `static int DomNodeHeldBy(xmlNodePtr pNode,ph7_class_instance *pThis)` |
|      5 |   599 | `{` |
|   4155 |   600 | `	return pNode->type == XML_NOTATION_NODE \|\| pNode->_private == (void *)pThis;` |
|      5 |   601 | `}` |
|      - |   602 | `/* The cache entry under one pointer -- a node's wrapper (MEMOBJ_INT) or an` |
|      - |   603 | ` * xmlNs handle (MEMOBJ_RES) -- or NULL when nothing holds it. */` |
|     22 |   604 | `static ph7_value * DomCacheHit(ph7_vm *pVm,ph7_hashmap *pCache,const void *pKey)` |
|      1 |   605 | `{` |
|     23 |   606 | `	ph7_hashmap_node *pEntry = 0;` |
|     23 |   607 | `	ph7_value sKey,*pHit = 0;` |
|     23 |   608 | `	if( pCache == 0 \|\| pKey == 0 ){` |
|    ! 0 |   609 | `		return 0;` |
|      - |   610 | `	}` |
|     23 |   611 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pKey);` |
|     23 |   612 | `	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|    ! 0 |   613 | `		pHit = HashmapExtractNodeValue(pEntry);` |
|    ! 0 |   614 | `	}` |
|     23 |   615 | `	PH7_MemObjRelease(&sKey);` |
|     23 |   616 | `	return pHit;` |
|     12 |   617 | `}` |
|      - |   618 | `/*` |
|      - |   619 | ` * The node kinds a detached-node free may take. Everything else -- a DTD, the` |
|      - |   620 | ` * declaration nodes inside one, the synthesized notation stand-ins vm_libxml.c` |
|      - |   621 | ` * frees its own way -- stays with the document: xmlFreeNode would read an` |
|      - |   622 | ` * xmlEntity's fields as a node's, and none of them is ever what a program` |
|      - |   623 | ` * drops in a loop.` |
|      - |   624 | ` */` |
|    952 |   625 | `static int DomNodeIsFreeable(xmlNodePtr pNode)` |
|      4 |   626 | `{` |
|    956 |   627 | `	switch( pNode->type ){` |
|    458 |   628 | `	case XML_ELEMENT_NODE:` |
|      - |   629 | `	case XML_ATTRIBUTE_NODE:` |
|      - |   630 | `	case XML_TEXT_NODE:` |
|      - |   631 | `	case XML_CDATA_SECTION_NODE:` |
|      - |   632 | `	case XML_COMMENT_NODE:` |
|      - |   633 | `	case XML_PI_NODE:` |
|      - |   634 | `	case XML_ENTITY_REF_NODE:` |
|      - |   635 | `	case XML_DOCUMENT_FRAG_NODE:` |
|    919 |   636 | `		return 1;` |
|     18 |   637 | `	default:` |
|     38 |   638 | `		return 0;` |
|      - |   639 | `	}` |
|    480 |   640 | `}` |
|      - |   641 | `/* A namespace DECLARATION on this element that PHP still holds a handle onto.` |
|      - |   642 | ` * xmlFreeNode frees the xmlNs under it, and the handle is shared by every` |
|      - |   643 | ` * DOMNameSpaceNode built from it, so such a node is kept instead. */` |
|    690 |   644 | `static int DomNodeNsHeld(ph7_vm *pVm,ph7_hashmap *pCache,xmlNodePtr pNode)` |
|      3 |   645 | `{` |
|      - |   646 | `	xmlNsPtr pNs;` |
|    693 |   647 | `	if( pNode->type != XML_ELEMENT_NODE ){` |
|    431 |   648 | `		return 0;` |
|      - |   649 | `	}` |
|    286 |   650 | `	for( pNs = pNode->nsDef ; pNs ; pNs = pNs->next ){` |
|     23 |   651 | `		if( DomCacheHit(&(*pVm),pCache,pNs) ){` |
|    ! 0 |   652 | `			return 1;` |
|      - |   653 | `		}` |
|     12 |   654 | `	}` |
|    264 |   655 | `	return 0;` |
|    348 |   656 | `}` |
|      - |   657 | `static void DomFreeDetached(ph7_vm *pVm,ph7_hashmap *pCache,phl_xmldoc *pShell,xmlNodePtr pNode);` |
|      - |   658 | `/*` |
|      - |   659 | ` * Is this handle's document still REGISTERED?` |
|      - |   660 | ` *` |
|      - |   661 | ` * At VM reset and release the whole registry goes first (vm_libxml.c frees every` |
|      - |   662 | ` * tree, its orphans and the shell itself) and the objects are torn down after.` |
|      - |   663 | ` * A wrapper released then must not free a node the shell already freed -- and` |
|      - |   664 | ` * must not read the shell to find out, which is why this compares pointers.` |
|      - |   665 | ` */` |
|   4150 |   666 | `static int DomShellLive(ph7_vm *pVm,const phl_xmldoc *pShell)` |
|      5 |   667 | `{` |
|      - |   668 | `	const phl_xmldoc *pCur;` |
| 393109 |   669 | `	for( pCur = (const phl_xmldoc *)pVm->pXmlDocs ; pCur ; pCur = pCur->pNext ){` |
| 393109 |   670 | `		if( pCur == pShell ){` |
|   4155 |   671 | `			return 1;` |
|      - |   672 | `		}` |
| 194479 |   673 | `	}` |
|    ! 0 |   674 | `	return 0;` |
|   2080 |   675 | `}` |
|      - |   676 | `/*` |
|      - |   677 | ` * One child or attribute list of a node that is going. php's own rule` |
|      - |   678 | ` * (php_libxml_node_free_list): a descendant PHP still holds a wrapper for is` |
|      - |   679 | `` * UNLINKED and kept -- which is what makes `$child->parentNode` null once the`` |
|      - |   680 | ` * parent object goes -- and freed later by its own release.` |
|      - |   681 | ` */` |
|    938 |   682 | `static void DomFreeChildList(ph7_vm *pVm,ph7_hashmap *pCache,phl_xmldoc *pShell,` |
|      - |   683 | `	xmlNodePtr pList)` |
|      3 |   684 | `{` |
|    941 |   685 | `	xmlNodePtr pCur = pList,pNext;` |
|   1167 |   686 | `	while( pCur ){` |
|    229 |   687 | `		pNext = pCur->next;` |
|    229 |   688 | `		if( !DomNodeIsFreeable(pCur) ){` |
|      - |   689 | `			/* A declaration or a DTD: libxml's own free owns it, so it stays` |
|      - |   690 | `			 * linked and goes with the parent. */` |
|    ! 0 |   691 | `			pCur = pNext;` |
|    ! 0 |   692 | `			continue;` |
|      - |   693 | `		}` |
|    229 |   694 | `		xmlUnlinkNode(pCur);` |
|    229 |   695 | `		if( pCur->_private ){` |
|      8 |   696 | `			DomOrphanAdd(pShell,pCur);   /* held: the document owns it until then */` |
|      5 |   697 | `		}else{` |
|    223 |   698 | `			DomFreeDetached(&(*pVm),pCache,pShell,pCur);` |
|      - |   699 | `		}` |
|    229 |   700 | `		pCur = pNext;` |
|      3 |   701 | `	}` |
|    941 |   702 | `}` |
|      - |   703 | `/* Free a node with no parent, and everything under it nothing else holds. */` |
|    726 |   704 | `static void DomFreeDetached(ph7_vm *pVm,ph7_hashmap *pCache,phl_xmldoc *pShell,` |
|      - |   705 | `	xmlNodePtr pNode)` |
|      4 |   706 | `{` |
|    730 |   707 | `	if( pNode == 0 ){` |
|    ! 0 |   708 | `		return;` |
|      - |   709 | `	}` |
|    730 |   710 | `	if( !DomNodeIsFreeable(pNode) ){` |
|      - |   711 | `		/* A DTD, a declaration inside one, or one of the synthesized notation` |
|      - |   712 | `		 * stand-ins: the document owns each of those by another route, and` |
|      - |   713 | `		 * parking one on the orphan set would free it twice. */` |
|     38 |   714 | `		return;` |
|      - |   715 | `	}` |
|    693 |   716 | `	if( DomNodeNsHeld(&(*pVm),pCache,pNode) ){` |
|    ! 0 |   717 | `		DomOrphanAdd(pShell,pNode);` |
|    ! 0 |   718 | `		return;` |
|      - |   719 | `	}` |
|      - |   720 | `	/* An entity reference's children are the ENTITY's content and not its own;` |
|      - |   721 | `	 * libxml's own free walks past them for the same reason. */` |
|    693 |   722 | `	if( pNode->type != XML_ENTITY_REF_NODE ){` |
|    679 |   723 | `		DomFreeChildList(&(*pVm),pCache,pShell,pNode->children);` |
|    338 |   724 | `	}` |
|      - |   725 | ``	/* `properties` and `nsDef` live past an xmlAttr's end: elements only. */`` |
|    693 |   726 | `	if( pNode->type == XML_ELEMENT_NODE ){` |
|    264 |   727 | `		DomFreeChildList(&(*pVm),pCache,pShell,(xmlNodePtr)pNode->properties);` |
|    131 |   728 | `	}` |
|    693 |   729 | `	DomOrphanRemove(pShell,pNode);` |
|    693 |   730 | `	xmlFreeNode(pNode);` |
|    367 |   731 | `}` |
|      - |   732 | `/*` |
|      - |   733 | ` * ph7_class::xRelease for every node class -- php's dom_object free.` |
|      - |   734 | ` *` |
|      - |   735 | ` * Two things happen when the last PHP reference to a wrapper goes. The` |
|      - |   736 | ` * document's identity cache holds a BORROWED pointer to it, so this is the only` |
|      - |   737 | ` * place that entry can be dropped, and it must be: the next node libxml puts at` |
|      - |   738 | ` * the same address would otherwise answer a dead object. And a node with no` |
|      - |   739 | ` * PARENT is this object's to free, as it is php's -- an attached one belongs to` |
|      - |   740 | ` * the tree and is never touched.` |
|      - |   741 | ` */` |
|   4238 |   742 | `static void DomNodeRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      5 |   743 | `{` |
|   4243 |   744 | `	ph7_value *pVal = PH7_NativeAttr(pThis,DOM_RES);` |
|   4243 |   745 | `	phl_domnode *pNd = pVal && (pVal->iFlags & MEMOBJ_RES)` |
|   6313 |   746 | `		? (phl_domnode *)pVal->x.pOther : 0;` |
|   4243 |   747 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |   748 | `	ph7_hashmap *pCache;` |
|   4243 |   749 | `	if( pNd == 0 ){` |
|     89 |   750 | `		return;` |
|      - |   751 | `	}` |
|   4155 |   752 | `	pCache = DomCache(&(*pVm),PH7_NativeAttrObj(pThis,DOM_DOC));` |
|   4155 |   753 | `	if( pNode && DomShellLive(&(*pVm),pNd->pShell) && DomNodeHeldBy(pNode,pThis) ){` |
|   4155 |   754 | `		ph7_hashmap_node *pEntry = 0;` |
|      - |   755 | `		ph7_value sKey;` |
|   4155 |   756 | `		if( pNode->type != XML_NOTATION_NODE ){` |
|   4131 |   757 | `			pNode->_private = 0;` |
|   2063 |   758 | `		}` |
|   4155 |   759 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|   4155 |   760 | `		if( pCache && PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|   3981 |   761 | `			ph7_value *pHit = HashmapExtractNodeValue(pEntry);` |
|      - |   762 | `			/* ...only when the entry is still THIS object. A document that has` |
|      - |   763 | `			 * been repointed (loadXML) drops its cache, and a node of the new` |
|      - |   764 | `			 * tree can land on the address a surviving old wrapper names. */` |
|   3976 |   765 | `			if( pHit && (pHit->iFlags & MEMOBJ_INT)` |
|   3981 |   766 | `			 && (ph7_class_instance *)(sxuptr)pHit->x.iVal == pThis ){` |
|   3981 |   767 | `				PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|   1988 |   768 | `			}` |
|   1988 |   769 | `		}` |
|   4155 |   770 | `		PH7_MemObjRelease(&sKey);` |
|      - |   771 | `		/* A namespace handle is the document's and shared, so the nsDef screen` |
|      - |   772 | `		 * inside the walk needs a cache to read: with none, nothing is freed. */` |
|   4155 |   773 | `		if( pCache && pNode != (xmlNodePtr)pNd->pShell->pDoc && pNode->parent == 0 ){` |
|    510 |   774 | `			DomFreeDetached(&(*pVm),pCache,pNd->pShell,pNode);` |
|    253 |   775 | `		}` |
|   2075 |   776 | `	}` |
|   4155 |   777 | `	SyMemBackendFree(&pVm->sAllocator,pNd);` |
|   4155 |   778 | `	pVal->x.pOther = 0;` |
|   4155 |   779 | `	MemObjSetType(pVal,MEMOBJ_NULL);` |
|   2124 |   780 | `}` |
|      - |   781 | `/* Raw child-list splicing (no text-node merging -- DOM/php semantics) */` |
|    496 |   782 | `static void DomLinkLast(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      2 |   783 | `{` |
|    498 |   784 | `	pChild->parent = pParent;` |
|    498 |   785 | `	pChild->next = 0;` |
|    498 |   786 | `	if( pParent->last ){` |
|    153 |   787 | `		pParent->last->next = pChild;` |
|    153 |   788 | `		pChild->prev = pParent->last;` |
|     77 |   789 | `	}else{` |
|    346 |   790 | `		pParent->children = pChild;` |
|    346 |   791 | `		pChild->prev = 0;` |
|      - |   792 | `	}` |
|    498 |   793 | `	pParent->last = pChild;` |
|    498 |   794 | `}` |
|     78 |   795 | `static void DomLinkBefore(xmlNodePtr pParent,xmlNodePtr pChild,xmlNodePtr pRef)` |
|      1 |   796 | `{` |
|     79 |   797 | `	pChild->parent = pParent;` |
|     79 |   798 | `	pChild->next = pRef;` |
|     79 |   799 | `	pChild->prev = pRef->prev;` |
|     79 |   800 | `	if( pRef->prev ){` |
|     41 |   801 | `		pRef->prev->next = pChild;` |
|     21 |   802 | `	}else{` |
|     39 |   803 | `		pParent->children = pChild;` |
|      - |   804 | `	}` |
|     79 |   805 | `	pRef->prev = pChild;` |
|     79 |   806 | `}` |
|      - |   807 |  |
|      - |   808 | `/* ===== Node introspection: the readers behind __get ===== */` |
|      - |   809 |  |
|      - |   810 | `/* php's nodeName rules */` |
|    678 |   811 | `static void DomNodeName(ph7_context *pCtx,xmlNodePtr pNode)` |
|      4 |   812 | `{` |
|    682 |   813 | `	if( pNode == 0 ){` |
|      3 |   814 | `		ph7_result_string(pCtx,"",0);` |
|      3 |   815 | `		return;` |
|      - |   816 | `	}` |
|    680 |   817 | `	switch( pNode->type ){` |
|     13 |   818 | `	case XML_TEXT_NODE:          ph7_result_string(pCtx,"#text",(int)sizeof("#text")-1); break;` |
|      7 |   819 | `	case XML_CDATA_SECTION_NODE: ph7_result_string(pCtx,"#cdata-section",(int)sizeof("#cdata-section")-1); break;` |
|      5 |   820 | `	case XML_COMMENT_NODE:       ph7_result_string(pCtx,"#comment",(int)sizeof("#comment")-1); break;` |
|      8 |   821 | `	case XML_HTML_DOCUMENT_NODE:` |
|     17 |   822 | `	case XML_DOCUMENT_NODE:      ph7_result_string(pCtx,"#document",(int)sizeof("#document")-1); break;` |
|      7 |   823 | `	case XML_DOCUMENT_FRAG_NODE: ph7_result_string(pCtx,"#document-fragment",(int)sizeof("#document-fragment")-1); break;` |
|    316 |   824 | `	default:` |
|    632 |   825 | `		if( (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE)` |
|    619 |   826 | `			&& pNode->ns && pNode->ns->prefix ){` |
|    229 |   827 | `			ph7_result_string_format(pCtx,"%s:%s",(const char *)pNode->ns->prefix,(const char *)pNode->name);` |
|    115 |   828 | `		}else{` |
|    408 |   829 | `			ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|      - |   830 | `		}` |
|    632 |   831 | `		break;` |
|      - |   832 | `	}` |
|    343 |   833 | `}` |
|      - |   834 | `/*` |
|      - |   835 | ` * The three names a namespaced document reads on every node.` |
|      - |   836 | ` *` |
|      - |   837 | `` * `namespaceURI` and `localName` are php's `?string` -- null for a node that`` |
|      - |   838 | ` * cannot carry a name in a namespace at all (a text node, a comment, a PI, the` |
|      - |   839 | `` * document itself) -- while `prefix` is a plain `string` that answers "" there,`` |
|      - |   840 | ` * which is why one of the three cannot be derived from the other two.` |
|      - |   841 | ` *` |
|      - |   842 | ` * All three are gated on the node KIND before anything is read, which is not` |
|      - |   843 | ` * only php's rule but a memory-safety one: only element and attribute nodes are` |
|      - |   844 | ``  * xmlNode/xmlAttr-shaped, and `->ns` on an xmlDoc aliases its `compression` `` |
|      - |   845 | ` * int, on an xmlDtd its notation table.  Reading it there and dereferencing the` |
|      - |   846 | `` * result is a SIGSEGV out of `$doc->namespaceURI` -- an ordinary property read.`` |
|      - |   847 | ` */` |
|    602 |   848 | `static int DomHasNsSlot(xmlNodePtr pNode)` |
|      1 |   849 | `{` |
|    603 |   850 | `	return pNode != 0` |
|    903 |   851 | `		&& (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE);` |
|      1 |   852 | `}` |
|    328 |   853 | `static void DomNamespaceUri(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |   854 | `{` |
|    329 |   855 | `	if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->href ){` |
|    227 |   856 | `		ph7_result_string(pCtx,(const char *)pNode->ns->href,-1);` |
|    114 |   857 | `	}else{` |
|    103 |   858 | `		ph7_result_null(pCtx);` |
|      - |   859 | `	}` |
|    329 |   860 | `}` |
|    126 |   861 | `static void DomPrefix(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |   862 | `{` |
|    127 |   863 | `	if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->prefix ){` |
|     67 |   864 | `		ph7_result_string(pCtx,(const char *)pNode->ns->prefix,-1);` |
|     34 |   865 | `	}else{` |
|     61 |   866 | `		ph7_result_string(pCtx,"",0);` |
|      - |   867 | `	}` |
|    127 |   868 | `}` |
|    124 |   869 | `static void DomLocalName(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |   870 | `{` |
|    125 |   871 | `	if( DomHasNsSlot(pNode) ){` |
|    107 |   872 | `		ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|     54 |   873 | `	}else{` |
|     19 |   874 | `		ph7_result_null(pCtx);` |
|      - |   875 | `	}` |
|    125 |   876 | `}` |
|      - |   877 | ``/* php's `isConnected`: is the node's root the DOCUMENT? A node built by a`` |
|      - |   878 | `` * create* factory carries the document as its `ownerDocument` from birth, so`` |
|      - |   879 | ` * that property cannot answer this and a program testing it reads true for a` |
|      - |   880 | ` * node it has not appended yet. */` |
|    102 |   881 | `static int DomIsConnected(xmlNodePtr pNode)` |
|      1 |   882 | `{` |
|    103 |   883 | `	xmlNodePtr pRoot = pNode;` |
|    103 |   884 | `	if( pRoot == 0 ){` |
|    ! 0 |   885 | `		return 0;` |
|      - |   886 | `	}` |
|    191 |   887 | `	while( pRoot->parent ){` |
|     89 |   888 | `		pRoot = pRoot->parent;` |
|      1 |   889 | `	}` |
|    103 |   890 | `	return pRoot->type == XML_DOCUMENT_NODE \|\| pRoot->type == XML_HTML_DOCUMENT_NODE;` |
|     52 |   891 | `}` |
|      - |   892 | `/*` |
|      - |   893 | ` * php's nodeValue, which is null for every node kind that has no value of its` |
|      - |   894 | ` * own -- the document, a doctype, a fragment, an entity DECLARATION and an` |
|      - |   895 | `` * entity REFERENCE all answer null, where `textContent` on the same node walks`` |
|      - |   896 | ` * its children and answers a string. (The element case is php's own` |
|      - |   897 | ` * convenience: DOM says an element has no node value.)` |
|      - |   898 | ` */` |
|    276 |   899 | `static void DomNodeValue(ph7_context *pCtx,xmlNodePtr pNode)` |
|      3 |   900 | `{` |
|      - |   901 | `	xmlChar *zContent;` |
|    279 |   902 | `	if( pNode == 0 ){` |
|    ! 0 |   903 | `		ph7_result_null(pCtx);` |
|    ! 0 |   904 | `		return;` |
|      - |   905 | `	}` |
|    279 |   906 | `	switch( pNode->type ){` |
|     83 |   907 | `	case XML_TEXT_NODE:` |
|      - |   908 | `	case XML_COMMENT_NODE:` |
|      - |   909 | `	case XML_CDATA_SECTION_NODE:` |
|      - |   910 | `	case XML_PI_NODE:` |
|      - |   911 | `		/* The CONTENT POINTER itself, not xmlNodeGetContent's copy: a null` |
|      - |   912 | `		 * pointer -- the omitted-argument constructor's state -- reads NULL` |
|      - |   913 | `		 * where an empty string reads "", and newer libxml's` |
|      - |   914 | `		 * xmlNodeGetContent papers over exactly that difference (2.13 answers` |
|      - |   915 | `		 * "" for both, 2.9 answers NULL for the pointer). */` |
|    168 |   916 | `		if( pNode->content == 0 ){` |
|     15 |   917 | `			ph7_result_null(pCtx);` |
|      8 |   918 | `		}else{` |
|    154 |   919 | `			ph7_result_string(pCtx,(const char *)pNode->content,-1);` |
|      - |   920 | `		}` |
|    168 |   921 | `		return;` |
|     39 |   922 | `	case XML_ATTRIBUTE_NODE:` |
|      - |   923 | `	case XML_ELEMENT_NODE:` |
|     81 |   924 | `		break;` |
|     16 |   925 | `	default:` |
|     33 |   926 | `		ph7_result_null(pCtx);` |
|     33 |   927 | `		return;` |
|      - |   928 | `	}` |
|     81 |   929 | `	zContent = xmlNodeGetContent(pNode);` |
|     81 |   930 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|     81 |   931 | `	if( zContent ){` |
|     81 |   932 | `		xmlFree(zContent);` |
|     39 |   933 | `	}` |
|    141 |   934 | `}` |
|      - |   935 | ``/* The `data` property's reading of the same content: php COERCES there, so a`` |
|      - |   936 | ` * NULL content pointer -- the omitted-argument constructors' state -- reads ""` |
|      - |   937 | `` * from `$node->data` and null from `$node->nodeValue`, one node, two answers. */`` |
|    126 |   938 | `static void DomDataValue(ph7_context *pCtx,xmlNodePtr pNode)` |
|      2 |   939 | `{` |
|    128 |   940 | `	DomNodeValue(pCtx,pNode);` |
|    128 |   941 | `	if( pCtx->pRet->iFlags & MEMOBJ_NULL ){` |
|      7 |   942 | `		ph7_result_string(pCtx,"",0);` |
|      3 |   943 | `	}` |
|    128 |   944 | `}` |
|      - |   945 | `/* php's textContent: the same walk, but a document answers its text too */` |
|     56 |   946 | `static void DomTextContent(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |   947 | `{` |
|     57 |   948 | `	xmlChar *zContent = pNode ? xmlNodeGetContent(pNode) : 0;` |
|     57 |   949 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|     57 |   950 | `	if( zContent ){` |
|     49 |   951 | `		xmlFree(zContent);` |
|     24 |   952 | `	}` |
|     57 |   953 | `}` |
|      - |   954 | `/* The two child counts childNodes->length and childElementCount read. */` |
|      - |   955 | `static xmlNodePtr DomRefChildren(xmlNodePtr pNode);` |
|    158 |   956 | `static int DomChildCount(xmlNodePtr pNode,int bElementsOnly)` |
|      2 |   957 | `{` |
|    160 |   958 | `	xmlNodePtr pChild = DomRefChildren(pNode);` |
|    160 |   959 | `	int iCount = 0;` |
|    396 |   960 | `	for( ; pChild ; pChild = pChild->next ){` |
|    238 |   961 | `		if( !bElementsOnly \|\| pChild->type == XML_ELEMENT_NODE ){` |
|    216 |   962 | `			iCount++;` |
|    107 |   963 | `		}` |
|    120 |   964 | `	}` |
|    160 |   965 | `	return iCount;` |
|      2 |   966 | `}` |
|    330 |   967 | `static xmlNodePtr DomChildAt(xmlNodePtr pNode,int iWant)` |
|      2 |   968 | `{` |
|    332 |   969 | `	xmlNodePtr pChild = DomRefChildren(pNode);` |
|    886 |   970 | `	for( ; pChild && iWant > 0 ; pChild = pChild->next ){` |
|    556 |   971 | `		iWant--;` |
|    279 |   972 | `	}` |
|    332 |   973 | `	return pChild;` |
|      2 |   974 | `}` |
|      - |   975 |  |
|      - |   976 | `/* ===== Tree surgery: DOMNode's four mutators ===== */` |
|      - |   977 |  |
|      - |   978 | `/*` |
|      - |   979 | ` * Defined with the namespace machinery below, and declared here because the` |
|      - |   980 | ` * surgery runs it: a node LINKED into a tree loses the declarations its new` |
|      - |   981 | ` * scope already makes, and gains the ones its new scope no longer makes.` |
|      - |   982 | ` * (The namespace section cannot move up because it reads the attribute` |
|      - |   983 | ` * walker; DomDropChildren below is with the property-write machinery it was` |
|      - |   984 | ` * built for, and replaceChildren() runs the same wrapper-preserving drop.)` |
|      - |   985 | ` */` |
|      - |   986 | `static void DomNsOnInsertEx(xmlNodePtr pNode,int bDeep);` |
|      - |   987 | `static void DomDropChildren(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode);` |
|      - |   988 | `/* The attribute machinery, defined with the attribute surface below: the four` |
|      - |   989 | ` * mutators reach it because php's appendChild/insertBefore ATTACH an attribute` |
|      - |   990 | ` * argument as a property rather than splicing it among the children. */` |
|      - |   991 | `static xmlAttrPtr DomAttrByLocal(xmlNodePtr pElem,const char *zName);` |
|      - |   992 | `static xmlAttrPtr DomAttrByNs(xmlNodePtr pElem,const xmlChar *zUri,const char *zLocal);` |
|      - |   993 | `static void DomAttrLinkLast(xmlNodePtr pElem,xmlAttrPtr pAttr);` |
|      - |   994 | `static void DomAttrLinkBefore(xmlNodePtr pElem,xmlAttrPtr pAttr,xmlAttrPtr pRef);` |
|      - |   995 | `static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr);` |
|      - |   996 | `static void DomNsAttrArrive(xmlNodePtr pElem,xmlAttrPtr pAttr);` |
|      - |   997 | `/* The adoptNode wrapper machinery, defined with it below: the insertion doors` |
|      - |   998 | ` * run it too, because php ADOPTS a constructed, ownerless argument -- doc,` |
|      - |   999 | ` * identity-cache home and handle shell all move on the first insertion. */` |
|      - |  1000 | `static void DomAdoptWrappers(ph7_vm *pVm,ph7_class_instance *pSrcDoc,` |
|      - |  1001 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode);` |
|      - |  1002 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot);` |
|      - |  1003 | `/*` |
|      - |  1004 | ` * An entity REFERENCE's children as php answers them. libxml's re-homing` |
|      - |  1005 | ` * CLEARS the raw link when a constructed reference is adopted -- and php's` |
|      - |  1006 | ` * raw state stays cleared, which replaceChild's childless-false cell measures` |
|      - |  1007 | `` * -- but php's READERS still resolve: `$ref->firstChild` answers the NEW`` |
|      - |  1008 | ` * document's declaration for the name, or the predefined five, or nothing.` |
|      - |  1009 | ` */` |
|   1082 |  1010 | `static xmlNodePtr DomRefChildren(xmlNodePtr pNode)` |
|      4 |  1011 | `{` |
|   1082 |  1012 | `	if( pNode && pNode->type == XML_ENTITY_REF_NODE` |
|    558 |  1013 | `	 && pNode->children == 0 && pNode->doc ){` |
|      6 |  1014 | `		return (xmlNodePtr)xmlGetDocEntity(pNode->doc,pNode->name);` |
|      - |  1015 | `	}` |
|   1081 |  1016 | `	return pNode ? pNode->children : 0;` |
|    545 |  1017 | `}` |
|      - |  1018 | `/*` |
|      - |  1019 | ` * Take an OWNERLESS subtree into the receiver's world, php's constructed-node` |
|      - |  1020 | ` * adoption: the libxml nodes get the receiver's document (none of their` |
|      - |  1021 | ` * strings are dict-interned -- a constructed node's are plain allocations, so` |
|      - |  1022 | ` * xmlSetTreeDoc is the whole move), the orphan entry crosses from the limbo` |
|      - |  1023 | ` * shell to the receiver's, and every wrapper PHP holds re-homes into the` |
|      - |  1024 | ` * receiver's identity cache.  Also the OWNERLESS-to-OWNERLESS merge, where no` |
|      - |  1025 | ` * document changes hands but the wrappers still need ONE holder for` |
|      - |  1026 | `` * `$a->firstChild === $b` to hold.  The caller has already screened documents:`` |
|      - |  1027 | ` * a mismatch here means the argument's is NULL.` |
|      - |  1028 | ` */` |
|    464 |  1029 | `static void DomAdoptIntoRecv(ph7_context *pCtx,ph7_value *pArgVal,phl_domnode *pArgNd)` |
|      3 |  1030 | `{` |
|    467 |  1031 | `	ph7_vm *pVm = pCtx->pVm;` |
|    467 |  1032 | `	ph7_class_instance *pSrcHolder = DomObjArgDoc(pArgVal);` |
|    467 |  1033 | `	ph7_class_instance *pDstHolder = DomThisDoc(pCtx);` |
|    467 |  1034 | `	phl_domnode *pRecv = DomThisNode(pCtx);` |
|    467 |  1035 | `	xmlNodePtr pNode = (xmlNodePtr)pArgNd->pNode;` |
|    467 |  1036 | `	xmlNodePtr pRecvNode = pRecv ? (xmlNodePtr)pRecv->pNode : 0;` |
|    467 |  1037 | `	if( pSrcHolder == pDstHolder \|\| pRecvNode == 0 ){` |
|    435 |  1038 | `		return;` |
|      - |  1039 | `	}` |
|     33 |  1040 | `	if( pNode->doc == 0 && pRecvNode->doc ){` |
|     29 |  1041 | `		xmlSetTreeDoc(pNode,pRecvNode->doc);` |
|     14 |  1042 | `	}` |
|     33 |  1043 | `	if( pArgNd->pShell != pRecv->pShell ){` |
|     29 |  1044 | `		DomOrphanRemove(pArgNd->pShell,pNode);` |
|     29 |  1045 | `		DomOrphanAdd(pRecv->pShell,pNode);` |
|     29 |  1046 | `		pArgNd->pShell = pRecv->pShell;` |
|     14 |  1047 | `	}` |
|     33 |  1048 | `	DomAdoptWrappers(pVm,pSrcHolder,pDstHolder,pRecv->pShell,pNode);` |
|    235 |  1049 | `}` |
|      - |  1050 |  |
|      - |  1051 | `/*` |
|      - |  1052 | ` * php's dom_node_children_valid: the node kinds that can never have children.` |
|      - |  1053 | ` * A level-2 mutator on such a receiver answers FALSE with nothing said at all` |
|      - |  1054 | ` * -- no warning, no exception -- and answers it BEFORE any other screen, so` |
|      - |  1055 | `` * `$text->appendChild($nodeFromAnotherDocument)` is false, not Wrong Document.`` |
|      - |  1056 | ` */` |
|   1152 |  1057 | `static int DomChildrenValid(xmlNodePtr pNode)` |
|      4 |  1058 | `{` |
|   1156 |  1059 | `	switch( pNode->type ){` |
|     14 |  1060 | `	case XML_TEXT_NODE:` |
|      - |  1061 | `	case XML_CDATA_SECTION_NODE:` |
|      - |  1062 | `	case XML_PI_NODE:` |
|      - |  1063 | `	case XML_COMMENT_NODE:` |
|      - |  1064 | `	case XML_DOCUMENT_TYPE_NODE:` |
|      - |  1065 | `	case XML_DTD_NODE:` |
|      - |  1066 | `	case XML_NOTATION_NODE:` |
|     29 |  1067 | `		return 0;` |
|    562 |  1068 | `	default:` |
|   1128 |  1069 | `		return 1;` |
|      - |  1070 | `	}` |
|    580 |  1071 | `}` |
|      - |  1072 | `/*` |
|      - |  1073 | `` * The two ends of the child list php's `firstChild`/`lastChild` answer, and`` |
|      - |  1074 | `` * what `hasChildNodes()` asks -- all three through the same screen, so the`` |
|      - |  1075 | ` * DOCTYPE (whose declarations libxml really does link as children) answers` |
|      - |  1076 | ` * null, null and false the way php's do.` |
|      - |  1077 | ` */` |
|    594 |  1078 | `static xmlNodePtr DomNodeChildFirst(xmlNodePtr pNode)` |
|      4 |  1079 | `{` |
|    598 |  1080 | `	if( pNode == 0 \|\| !DomChildrenValid(pNode) ){` |
|      7 |  1081 | `		return 0;` |
|      - |  1082 | `	}` |
|    592 |  1083 | `	return DomRefChildren(pNode);` |
|    301 |  1084 | `}` |
|     86 |  1085 | `static xmlNodePtr DomNodeChildLast(xmlNodePtr pNode)` |
|      1 |  1086 | `{` |
|     87 |  1087 | `	if( pNode == 0 \|\| !DomChildrenValid(pNode) ){` |
|      3 |  1088 | `		return 0;` |
|      - |  1089 | `	}` |
|     85 |  1090 | `	return pNode->last ? pNode->last : DomRefChildren(pNode);` |
|     44 |  1091 | `}` |
|      - |  1092 | `/*` |
|      - |  1093 | ` * php's dom_node_is_read_only: the DTD-owned kinds -- an entity reference's` |
|      - |  1094 | ` * subtree is the entity's, shared by every reference to it -- and, one clause` |
|      - |  1095 | `` * later, a node with NO document: a constructed `new DOMText('t')` that was`` |
|      - |  1096 | ` * never adopted refuses the level-2 child-list doors with No Modification` |
|      - |  1097 | ` * Allowed where the modern variadic family compares documents instead.` |
|      - |  1098 | ` */` |
|    612 |  1099 | `static int DomNodeReadOnly(xmlNodePtr pNode)` |
|      3 |  1100 | `{` |
|    615 |  1101 | `	switch( pNode->type ){` |
|      5 |  1102 | `	case XML_ENTITY_REF_NODE:` |
|      - |  1103 | `	case XML_ENTITY_NODE:` |
|      - |  1104 | `	case XML_DOCUMENT_TYPE_NODE:` |
|      - |  1105 | `	case XML_NOTATION_NODE:` |
|      - |  1106 | `	case XML_DTD_NODE:` |
|      - |  1107 | `	case XML_ELEMENT_DECL:` |
|      - |  1108 | `	case XML_ATTRIBUTE_DECL:` |
|      - |  1109 | `	case XML_ENTITY_DECL:` |
|     11 |  1110 | `		return 1;` |
|    301 |  1111 | `	default:` |
|    605 |  1112 | `		return pNode->doc == 0;` |
|      - |  1113 | `	}` |
|    309 |  1114 | `}` |
|      - |  1115 | `/* The two screens every level-2 mutator opens with, in php's order: an` |
|      - |  1116 | ` * invalid-children receiver answers false in silence, then the read-only` |
|      - |  1117 | ` * refusal -- the receiver's own, or that of the parent the CHILD would be` |
|      - |  1118 | ` * taken from. Returns non-zero when the caller must stop (result already` |
|      - |  1119 | ` * set). */` |
|    430 |  1120 | `static int DomMutatorScreen(ph7_context *pCtx,xmlNodePtr pParent,xmlNodePtr pChild,int *pRc)` |
|      3 |  1121 | `{` |
|    433 |  1122 | `	if( !DomChildrenValid(pParent) ){` |
|     19 |  1123 | `		ph7_result_bool(pCtx,0);` |
|     19 |  1124 | `		*pRc = PH7_OK;` |
|     19 |  1125 | `		return 1;` |
|      - |  1126 | `	}` |
|    412 |  1127 | `	if( DomNodeReadOnly(pParent)` |
|    410 |  1128 | `	 \|\| (pChild->parent && DomNodeReadOnly(pChild->parent)) ){` |
|     11 |  1129 | `		*pRc = DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|     11 |  1130 | `		return 1;` |
|      - |  1131 | `	}` |
|    405 |  1132 | `	return 0;` |
|    218 |  1133 | `}` |
|      - |  1134 | `/*` |
|      - |  1135 | ` * The attribute HALF of appendChild/insertBefore: php hands an attribute` |
|      - |  1136 | ` * argument to xmlAddChild, which attaches it as a PROPERTY -- so` |
|      - |  1137 | `` * `$el->appendChild($attr)` is a spelling of setAttributeNode, not a child`` |
|      - |  1138 | `` * splice (the chunk spliced it among the children and serialized `<r> k=""`,`` |
|      - |  1139 | ` * bytes that are not XML). The receiver must be an ELEMENT: a document, a` |
|      - |  1140 | ` * fragment or an attribute answers the Hierarchy refusal. An existing` |
|      - |  1141 | `` * attribute of the same name AND namespace (a plain `k` leaves a namespaced`` |
|      - |  1142 | `` * `p:k` alone -- php 8.5.10's rule, both spellings; 8.5.9 matched the name`` |
|      - |  1143 | ` * alone) is displaced UNLESS it is the argument itself, and the argument always (re)enters at the` |
|      - |  1144 | ` * tail of the property list, which is observable: appending an element's own` |
|      - |  1145 | ` * first attribute moves it last.` |
|      - |  1146 | ` *` |
|      - |  1147 | ` * One deliberate divergence, recorded: php FREES the displaced` |
|      - |  1148 | ` * attribute, so a wrapper held across the call answers Invalid State from` |
|      - |  1149 | ` * every later read ("Couldn't fetch DOMAttr" from a method). PHL parks it` |
|      - |  1150 | ` * detached and alive -- the same after-state setAttributeNode leaves.` |
|      - |  1151 | ` */` |
|     16 |  1152 | `static int DomMutatorAttrAttach(ph7_context *pCtx,phl_domnode *pPar,phl_domnode *pChd,` |
|      - |  1153 | `	ph7_value *pArg)` |
|      2 |  1154 | `{` |
|     18 |  1155 | `	xmlNodePtr pElem = (xmlNodePtr)pPar->pNode;` |
|     18 |  1156 | `	xmlAttrPtr pAttr = (xmlAttrPtr)pChd->pNode;` |
|      - |  1157 | `	xmlAttrPtr pOld;` |
|     18 |  1158 | `	if( pElem->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  1159 | `		return DomThrow(pCtx,DOM_ERR_HIERARCHY);` |
|      - |  1160 | `	}` |
|     18 |  1161 | `	pOld = DomAttrByNs(pElem,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name);` |
|     18 |  1162 | `	if( pOld && pOld != pAttr ){` |
|      8 |  1163 | `		xmlUnlinkNode((xmlNodePtr)pOld);` |
|      8 |  1164 | `		DomOrphanAdd(pPar->pShell,(xmlNodePtr)pOld);` |
|      3 |  1165 | `	}` |
|     18 |  1166 | `	DomAttrDetach(pChd->pShell,pAttr);` |
|     18 |  1167 | `	DomAttrLinkLast(pElem,pAttr);` |
|     18 |  1168 | `	DomNsAttrArrive(pElem,pAttr);` |
|     18 |  1169 | `	ph7_result_value(pCtx,pArg);` |
|     18 |  1170 | `	return PH7_OK;` |
|     10 |  1171 | `}` |
|      - |  1172 |  |
|      - |  1173 | `/*` |
|      - |  1174 | ` * php's refusal taxonomy for linking pChild under pParent, or NULL when the` |
|      - |  1175 | ` * link is allowed. The chunk collapsed all of it into one message per method,` |
|      - |  1176 | ` * which cost more than a wording: nothing rejected making a node its own` |
|      - |  1177 | `` * DESCENDANT, so `$a->firstChild->appendChild($a)` spliced a CYCLE into the`` |
|      - |  1178 | ` * tree and every later walk of it ran away.` |
|      - |  1179 | ` */` |
|      - |  1180 | `/*` |
|      - |  1181 | ` * The refusal is in TWO halves because php's empty-fragment answer sits` |
|      - |  1182 | ` * between them: a foreign empty fragment is Wrong Document, an empty fragment` |
|      - |  1183 | ` * on an ATTRIBUTE receiver is the "Document Fragment is empty" warning plus` |
|      - |  1184 | ` * false -- so the document screen runs before the fragment check and the` |
|      - |  1185 | ` * receiver-kind screen after it.` |
|      - |  1186 | ` */` |
|    402 |  1187 | `static int DomLinkRefusalPre(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      3 |  1188 | `{` |
|      - |  1189 | `	/* A child with NO document is exempt: it is a constructed node, and the` |
|      - |  1190 | `	 * level-2 doors ADOPT it -- where the modern variadic family refuses it` |
|      - |  1191 | `	 * with this same code. */` |
|    405 |  1192 | `	if( pParent->doc != pChild->doc && pChild->doc != 0 ){` |
|     17 |  1193 | `		return DOM_ERR_WRONG_DOC;` |
|      - |  1194 | `	}` |
|      - |  1195 | `	/* A DOCUMENT is never a child, stated outright: the ancestor walk below` |
|      - |  1196 | `	 * only sees it from an ATTACHED receiver, and a detached one --` |
|      - |  1197 | ``	 * `$d->createElement('x')->appendChild($d)` -- spliced the document node`` |
|      - |  1198 | `	 * into its own orphan's child list, which teardown then freed twice. */` |
|    389 |  1199 | `	if( pChild->type == XML_DOCUMENT_NODE \|\| pChild->type == XML_HTML_DOCUMENT_NODE ){` |
|      3 |  1200 | `		return DOM_ERR_HIERARCHY;` |
|      - |  1201 | `	}` |
|    387 |  1202 | `	return 0;` |
|    204 |  1203 | `}` |
|      - |  1204 | `/* The ancestor-cycle walk. Walking UP from the parent also catches` |
|      - |  1205 | `` * pChild == pParent, so `$frag->appendChild($frag)` is Hierarchy even`` |
|      - |  1206 | ` * for an EMPTY fragment -- the cycle answers before the empty warning. */` |
|    418 |  1207 | `static int DomLinkCycle(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      3 |  1208 | `{` |
|      - |  1209 | `	xmlNodePtr p;` |
|   1243 |  1210 | `	for( p = pParent ; p ; p = p->parent ){` |
|    841 |  1211 | `		if( p == pChild ){` |
|     17 |  1212 | `			return DOM_ERR_HIERARCHY;` |
|      - |  1213 | `		}` |
|    414 |  1214 | `	}` |
|    405 |  1215 | `	return 0;` |
|    212 |  1216 | `}` |
|      - |  1217 | `/* An ATTRIBUTE takes text and entity references, nothing else -- not even a` |
|      - |  1218 | ` * fragment whose every child is text (though the EMPTY fragment's warning` |
|      - |  1219 | ` * answers before this). php's Hierarchy refusal. */` |
|    396 |  1220 | `static int DomAttrRecvKind(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      3 |  1221 | `{` |
|    396 |  1222 | `	if( pParent->type == XML_ATTRIBUTE_NODE` |
|    209 |  1223 | `	 && pChild->type != XML_TEXT_NODE && pChild->type != XML_ENTITY_REF_NODE ){` |
|      7 |  1224 | `		return DOM_ERR_HIERARCHY;` |
|      - |  1225 | `	}` |
|    393 |  1226 | `	return 0;` |
|    201 |  1227 | `}` |
|      - |  1228 | `/*` |
|      - |  1229 | ` * A DOCUMENT FRAGMENT is not linked, it is EMPTIED: php moves its children into` |
|      - |  1230 | ` * the target and answers the FIRST of them (the fragment itself is never a` |
|      - |  1231 | ` * child of anything, which is the whole point of the type -- it is how a` |
|      - |  1232 | ` * program builds a run of nodes and inserts it in one call). An EMPTY one is` |
|      - |  1233 | `` * php's warning plus `false`, not an exception.`` |
|      - |  1234 | ` *` |
|      - |  1235 | ` * *ppFirst takes the first node moved, or NULL when the argument was not a` |
|      - |  1236 | ` * fragment at all; the caller then links the node itself.` |
|      - |  1237 | ` */` |
|    796 |  1238 | `static int DomIsFragment(xmlNodePtr pNode)` |
|      3 |  1239 | `{` |
|    799 |  1240 | `	return pNode && pNode->type == XML_DOCUMENT_FRAG_NODE;` |
|      3 |  1241 | `}` |
|      6 |  1242 | `static int DomFragEmpty(ph7_context *pCtx)` |
|      1 |  1243 | `{` |
|      - |  1244 | ``	/* The context already qualifies the message with php's `DOMNode::method(): `. */`` |
|      7 |  1245 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Document Fragment is empty");` |
|      7 |  1246 | `	ph7_result_bool(pCtx,0);` |
|      7 |  1247 | `	return PH7_OK;` |
|      1 |  1248 | `}` |
|      - |  1249 | `/* Move every child of pFrag into pParent, before pRef or at the end. */` |
|     36 |  1250 | `static xmlNodePtr DomFragMove(phl_xmldoc *pShell,xmlNodePtr pParent,xmlNodePtr pFrag,xmlNodePtr pRef)` |
|      1 |  1251 | `{` |
|     37 |  1252 | `	xmlNodePtr pFirst = pFrag->children;` |
|     37 |  1253 | `	xmlNodePtr pChild = pFirst;` |
|     83 |  1254 | `	while( pChild ){` |
|     47 |  1255 | `		xmlNodePtr pNext = pChild->next;` |
|     47 |  1256 | `		DomDetach(pShell,pChild);` |
|     47 |  1257 | `		if( pRef ){` |
|      9 |  1258 | `			DomLinkBefore(pParent,pChild,pRef);` |
|      5 |  1259 | `		}else{` |
|     39 |  1260 | `			DomLinkLast(pParent,pChild);` |
|      - |  1261 | `		}` |
|      - |  1262 | `		/* php reconciles each node it MOVED, not the fragment they came from --` |
|      - |  1263 | `		 * and through this path it does so DEEPLY (see DomNsOnInsertEx). */` |
|     47 |  1264 | `		DomNsOnInsertEx(pChild,1);` |
|     47 |  1265 | `		pChild = pNext;` |
|      1 |  1266 | `	}` |
|     37 |  1267 | `	pFrag->children = pFrag->last = 0;` |
|     37 |  1268 | `	return pFirst;` |
|      1 |  1269 | `}` |
|      - |  1270 | `/*` |
|      - |  1271 | ` * DOMNode::appendChild(DOMNode $node): DOMNode` |
|      - |  1272 | ` *` |
|      - |  1273 | `` * Note the argument reaches C already screened -- `$n->appendChild(1)` is a`` |
|      - |  1274 | ``  * TypeError from the declared `DOMNode $node`, where the chunk read `->__res` `` |
|      - |  1275 | ` * off an int and warned.` |
|      - |  1276 | ` */` |
|    386 |  1277 | `DOM_METHOD(vm_builtin_DOMNode_appendChild)` |
|      3 |  1278 | `{` |
|    389 |  1279 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    389 |  1280 | `	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|      - |  1281 | `	int iErr,rc;` |
|    389 |  1282 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|    ! 0 |  1283 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  1284 | `	}` |
|    389 |  1285 | `	if( DomMutatorScreen(pCtx,(xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode,&rc) ){` |
|     27 |  1286 | `		return rc;` |
|      - |  1287 | `	}` |
|    363 |  1288 | `	iErr = DomLinkRefusalPre((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|    363 |  1289 | `	if( iErr == 0 ){` |
|    347 |  1290 | `		iErr = DomLinkCycle((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|    172 |  1291 | `	}` |
|      - |  1292 | `	/* The empty-fragment answer sits between the screens: a foreign empty` |
|      - |  1293 | `	 * fragment is Wrong Document, appending a fragment to ITSELF is the cycle's` |
|      - |  1294 | `	 * Hierarchy, and only an empty one on an attribute receiver reaches the` |
|      - |  1295 | `	 * warning plus false. */` |
|    360 |  1296 | `	if( iErr == 0 && DomIsFragment((xmlNodePtr)pChd->pNode)` |
|    184 |  1297 | `	 && ((xmlNodePtr)pChd->pNode)->children == 0 ){` |
|      5 |  1298 | `		return DomFragEmpty(pCtx);` |
|      - |  1299 | `	}` |
|    359 |  1300 | `	if( iErr == 0 ){` |
|    331 |  1301 | `		iErr = DomAttrRecvKind((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|    164 |  1302 | `	}` |
|      - |  1303 | `	/* An attribute lands on an ELEMENT or nowhere -- and BEFORE the adoption,` |
|      - |  1304 | ``	 * which is measurable: `$doc->appendChild(new DOMAttr('k'))` refuses with`` |
|      - |  1305 | `	 * the argument still ownerless. */` |
|    356 |  1306 | `	if( iErr == 0 && ((xmlNodePtr)pChd->pNode)->type == XML_ATTRIBUTE_NODE` |
|    173 |  1307 | `	 && ((xmlNodePtr)pPar->pNode)->type != XML_ELEMENT_NODE ){` |
|      3 |  1308 | `		iErr = DOM_ERR_HIERARCHY;` |
|      1 |  1309 | `	}` |
|    359 |  1310 | `	if( iErr ){` |
|     35 |  1311 | `		return DomThrow(pCtx,iErr);` |
|      - |  1312 | `	}` |
|      - |  1313 | `	/* Every screen passed: a document-less argument is ADOPTED here, php's` |
|      - |  1314 | `	 * constructed-node door -- wrappers, orphan entry and (for an owned` |
|      - |  1315 | `	 * receiver) the document itself all move before the link. */` |
|    325 |  1316 | `	DomAdoptIntoRecv(pCtx,apArg[0],pChd);` |
|    325 |  1317 | `	if( ((xmlNodePtr)pChd->pNode)->type == XML_ATTRIBUTE_NODE ){` |
|     16 |  1318 | `		return DomMutatorAttrAttach(pCtx,pPar,pChd,apArg[0]);` |
|      - |  1319 | `	}` |
|    310 |  1320 | `	if( DomIsFragment((xmlNodePtr)pChd->pNode) ){` |
|      - |  1321 | `		xmlNodePtr pFirst;` |
|     25 |  1322 | `		pFirst = DomFragMove(pChd->pShell,(xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode,0);` |
|     25 |  1323 | `		return DomResultNodeOf(pCtx,pPar,pFirst);` |
|      - |  1324 | `	}` |
|    286 |  1325 | `	DomDetach(pChd->pShell,(xmlNodePtr)pChd->pNode);` |
|    286 |  1326 | `	DomLinkLast((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|    286 |  1327 | `	DomNsOnInsertEx((xmlNodePtr)pChd->pNode,0);` |
|    286 |  1328 | `	ph7_result_value(pCtx,apArg[0]);` |
|    286 |  1329 | `	return PH7_OK;` |
|    196 |  1330 | `}` |
|      - |  1331 | `/* DOMNode::insertBefore(DOMNode $node, ?DOMNode $child = null): DOMNode --` |
|      - |  1332 | ` * a reference node that is not a child of the receiver is Not Found. */` |
|     44 |  1333 | `DOM_METHOD(vm_builtin_DOMNode_insertBefore)` |
|      2 |  1334 | `{` |
|     46 |  1335 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|     46 |  1336 | `	phl_domnode *pNew = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     46 |  1337 | `	phl_domnode *pRef = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;` |
|      - |  1338 | `	xmlNodePtr pParent,pChild,pAnchor;` |
|      - |  1339 | `	int iErr,rc;` |
|     46 |  1340 | `	if( pPar == 0 \|\| pNew == 0 ){` |
|    ! 0 |  1341 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1342 | `	}` |
|     46 |  1343 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|     46 |  1344 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|     46 |  1345 | `	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;` |
|     46 |  1346 | `	if( DomMutatorScreen(pCtx,pParent,pChild,&rc) ){` |
|      3 |  1347 | `		return rc;` |
|      - |  1348 | `	}` |
|     44 |  1349 | `	iErr = DomLinkRefusalPre(pParent,pChild);` |
|     44 |  1350 | `	if( iErr == 0 ){` |
|     42 |  1351 | `		iErr = DomLinkCycle(pParent,pChild);` |
|     20 |  1352 | `	}` |
|      - |  1353 | `	/* Between the screens, and BEFORE the reference-membership refusal: an` |
|      - |  1354 | `	 * empty fragment answers its warning even against a reference node that` |
|      - |  1355 | `	 * is no child of the receiver. */` |
|     44 |  1356 | `	if( iErr == 0 && DomIsFragment(pChild) && pChild->children == 0 ){` |
|      3 |  1357 | `		return DomFragEmpty(pCtx);` |
|      - |  1358 | `	}` |
|     42 |  1359 | `	if( iErr == 0 ){` |
|     40 |  1360 | `		iErr = DomAttrRecvKind(pParent,pChild);` |
|     19 |  1361 | `	}` |
|      - |  1362 | `	/* An attribute lands on an ELEMENT or nowhere, and php answers that` |
|      - |  1363 | `	 * Hierarchy refusal BEFORE the reference-membership one -- a fragment` |
|      - |  1364 | `	 * receiver with an attribute argument and a foreign reference is` |
|      - |  1365 | `	 * Hierarchy, not Not Found. */` |
|     40 |  1366 | `	if( iErr == 0 && pChild->type == XML_ATTRIBUTE_NODE` |
|     28 |  1367 | `	 && pParent->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  1368 | `		iErr = DOM_ERR_HIERARCHY;` |
|    ! 0 |  1369 | `	}` |
|     42 |  1370 | `	if( iErr == 0 && pAnchor && pAnchor->parent != pParent ){` |
|     11 |  1371 | `		iErr = DOM_ERR_NOT_FOUND;` |
|      5 |  1372 | `	}` |
|     42 |  1373 | `	if( iErr ){` |
|     13 |  1374 | `		return DomThrow(pCtx,iErr);` |
|      - |  1375 | `	}` |
|      - |  1376 | `	/* The constructed-node adoption, before ANY of the insertion tails --` |
|      - |  1377 | `	 * php's order, so even an argument the sibling Error is about to strand` |
|      - |  1378 | `	 * detached comes out of the call owned by this document. */` |
|     30 |  1379 | `	DomAdoptIntoRecv(pCtx,apArg[0],pNew);` |
|     30 |  1380 | `	if( pChild->type == XML_ATTRIBUTE_NODE ){` |
|      - |  1381 | `		/*` |
|      - |  1382 | `		 * The attribute half, with insertBefore's own tails. A NULL reference` |
|      - |  1383 | `		 * is the append spelling and attaches (the same-name displacement` |
|      - |  1384 | `		 * included). A reference that is itself an ATTRIBUTE of the receiver` |
|      - |  1385 | `		 * really does mean "before": the argument enters the property list at` |
|      - |  1386 | `		 * the reference's position. Any other reference runs the DISPLACEMENT` |
|      - |  1387 | `		 * and then fails the sibling link, php's own order, so` |
|      - |  1388 | `` 		 * `$el->insertBefore($attr, $child)` on an element carrying `k="old"` `` |
|      - |  1389 | `		 * LOSES the old attribute, attaches nothing, and raises the plain` |
|      - |  1390 | `		 * Error the self-sibling splice raises.` |
|      - |  1391 | `		 */` |
|     13 |  1392 | `		xmlAttrPtr pAttr = (xmlAttrPtr)pChild;` |
|      - |  1393 | `		xmlAttrPtr pOld;` |
|     13 |  1394 | `		if( pAnchor == 0 ){` |
|      3 |  1395 | `			return DomMutatorAttrAttach(pCtx,pPar,pNew,apArg[0]);` |
|      - |  1396 | `		}` |
|     11 |  1397 | `		pOld = DomAttrByNs(pParent,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name);` |
|     11 |  1398 | `		if( pOld && pOld != pAttr ){` |
|      5 |  1399 | `			xmlUnlinkNode((xmlNodePtr)pOld);` |
|      5 |  1400 | `			DomOrphanAdd(pPar->pShell,(xmlNodePtr)pOld);` |
|      2 |  1401 | `		}` |
|     10 |  1402 | `		if( pAnchor->type != XML_ATTRIBUTE_NODE` |
|      9 |  1403 | `		 \|\| pAnchor == (xmlNodePtr)pAttr \|\| pAnchor == (xmlNodePtr)pOld ){` |
|      - |  1404 | `			/* The argument is UNLINKED before the sibling link fails -- php's` |
|      - |  1405 | ``			 * own order, so `$r->insertBefore($cAttr, $child)` costs the other`` |
|      - |  1406 | `			 * element its attribute and attaches nothing here. The link fails` |
|      - |  1407 | `			 * for a non-attribute reference, for the argument AS its own` |
|      - |  1408 | `			 * reference, and for a reference the displacement just took --` |
|      - |  1409 | `			 * php frees it and the sibling link then refuses. */` |
|      9 |  1410 | `			DomAttrDetach(pNew->pShell,pAttr);` |
|      9 |  1411 | `			DomOrphanAdd(pNew->pShell,(xmlNodePtr)pAttr);` |
|      9 |  1412 | `			return PH7_VmThrowException(pCtx,"Error",` |
|      - |  1413 | `				"Cannot add newnode as the previous sibling of refnode");` |
|      - |  1414 | `		}` |
|      3 |  1415 | `		DomAttrDetach(pNew->pShell,pAttr);` |
|      3 |  1416 | `		DomAttrLinkBefore(pParent,pAttr,(xmlAttrPtr)pAnchor);` |
|      3 |  1417 | `		DomNsAttrArrive(pParent,pAttr);` |
|      3 |  1418 | `		ph7_result_value(pCtx,apArg[0]);` |
|      3 |  1419 | `		return PH7_OK;` |
|      - |  1420 | `	}` |
|     18 |  1421 | `	if( pAnchor && pAnchor->type == XML_ATTRIBUTE_NODE ){` |
|      - |  1422 | `		/*` |
|      - |  1423 | `		 * A non-attribute argument against an ATTRIBUTE reference: php hands` |
|      - |  1424 | `		 * the pair to xmlAddPrevSibling, which UNLINKS the argument and then` |
|      - |  1425 | `		 * splices it into the PROPERTY chain -- state no serializer or` |
|      - |  1426 | `		 * childNodes walk ever shows, whose exact shape is libxml's version's.` |
|      - |  1427 | `		 * The bytes agree when PHL simply DETACHES the argument and answers` |
|      - |  1428 | `		 * it; the one detail php answers differently afterwards is recorded in` |
|      - |  1429 | ``		 * Recorded (the argument's `parentNode` reads the receiver there).`` |
|      - |  1430 | `		 */` |
|      3 |  1431 | `		DomDetach(pNew->pShell,pChild);` |
|      3 |  1432 | `		DomOrphanAdd(pNew->pShell,pChild);` |
|      3 |  1433 | `		ph7_result_value(pCtx,apArg[0]);` |
|      3 |  1434 | `		return PH7_OK;` |
|      - |  1435 | `	}` |
|     15 |  1436 | `	if( pAnchor == pChild ){` |
|      - |  1437 | `		/*` |
|      - |  1438 | `		 * A node cannot be inserted before ITSELF, and linking it anyway made` |
|      - |  1439 | ``		 * it its own sibling: `$p->insertBefore($x,$x)` spliced a cycle into`` |
|      - |  1440 | `		 * the child list, and the next walk of the tree -- saveXML, a` |
|      - |  1441 | `		 * childNodes count, getNodePath -- never returned.` |
|      - |  1442 | `		 *` |
|      - |  1443 | `		 * php's refusal here is a plain Error with no DOM code, because there` |
|      - |  1444 | `		 * is no DOM error for it, and it comes AFTER the node is detached: the` |
|      - |  1445 | `		 * tree loses the node and the caller is told nothing more.` |
|      - |  1446 | `		 */` |
|      5 |  1447 | `		DomDetach(pNew->pShell,pChild);` |
|      5 |  1448 | `		DomOrphanAdd(pNew->pShell,pChild);` |
|      5 |  1449 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  1450 | `			"Cannot add newnode as the previous sibling of refnode");` |
|      - |  1451 | `	}` |
|     11 |  1452 | `	if( DomIsFragment(pChild) ){` |
|      - |  1453 | `		/* An EMPTY fragment answered its warning above, before the reference` |
|      - |  1454 | `		 * screen -- php's order. */` |
|      - |  1455 | `		xmlNodePtr pFirst;` |
|      3 |  1456 | `		pFirst = DomFragMove(pNew->pShell,pParent,pChild,pAnchor);` |
|      3 |  1457 | `		return DomResultNodeOf(pCtx,pPar,pFirst);` |
|      - |  1458 | `	}` |
|      9 |  1459 | `	DomDetach(pNew->pShell,pChild);` |
|      9 |  1460 | `	if( pAnchor ){` |
|      9 |  1461 | `		DomLinkBefore(pParent,pChild,pAnchor);` |
|      5 |  1462 | `	}else{` |
|    ! 0 |  1463 | `		DomLinkLast(pParent,pChild);` |
|      - |  1464 | `	}` |
|      9 |  1465 | `	DomNsOnInsertEx(pChild,0);` |
|      9 |  1466 | `	ph7_result_value(pCtx,apArg[0]);` |
|      9 |  1467 | `	return PH7_OK;` |
|     24 |  1468 | `}` |
|      - |  1469 | `/* DOMNode::removeChild(DOMNode $child): DOMNode */` |
|     36 |  1470 | `DOM_METHOD(vm_builtin_DOMNode_removeChild)` |
|      1 |  1471 | `{` |
|     37 |  1472 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|     37 |  1473 | `	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|      - |  1474 | `	xmlNodePtr pChild;` |
|     37 |  1475 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|    ! 0 |  1476 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1477 | `	}` |
|     37 |  1478 | `	pChild = (xmlNodePtr)pChd->pNode;` |
|      - |  1479 | ``	/* php's membership test is `no children at all, or the parent pointer`` |
|      - |  1480 | ``	 * disagrees` -- which lets an ATTACHED ATTRIBUTE through (its libxml`` |
|      - |  1481 | `	 * parent IS the element), so removeChild really does remove an attribute` |
|      - |  1482 | `	 * -- but only from an element that has at least one real child; on a` |
|      - |  1483 | `	 * childless one the same attribute is Not Found. (An entity reference's` |
|      - |  1484 | `	 * child fails the parent test: its parent is the DTD.) */` |
|     36 |  1485 | `	if( ((xmlNodePtr)pPar->pNode)->children == 0` |
|     36 |  1486 | `	 \|\| pChild->parent != (xmlNodePtr)pPar->pNode ){` |
|     17 |  1487 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1488 | `	}` |
|      - |  1489 | `	/* ...and only THEN the read-only refusal, php's order: an entity` |
|      - |  1490 | `	 * reference's child is Not Found territory never reached, while` |
|      - |  1491 | ``	 * `$ownerless->removeChild($its->child)` is the No Modification`` |
|      - |  1492 | `	 * refusal. */` |
|     21 |  1493 | `	if( DomNodeReadOnly((xmlNodePtr)pPar->pNode) ){` |
|      3 |  1494 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|      - |  1495 | `	}` |
|     19 |  1496 | `	xmlUnlinkNode(pChild);` |
|     19 |  1497 | `	DomOrphanAdd(pChd->pShell,pChild);` |
|     19 |  1498 | `	ph7_result_value(pCtx,apArg[0]);` |
|     19 |  1499 | `	return PH7_OK;` |
|     19 |  1500 | `}` |
|      - |  1501 | `/* DOMNode::replaceChild(DOMNode $node, DOMNode $child): DOMNode -- answers the` |
|      - |  1502 | ` * node it replaced, which is the SECOND argument. */` |
|     44 |  1503 | `DOM_METHOD(vm_builtin_DOMNode_replaceChild)` |
|      1 |  1504 | `{` |
|     45 |  1505 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|     45 |  1506 | `	phl_domnode *pNew = nArg > 1 ? DomObjArg(apArg[0]) : 0;` |
|     45 |  1507 | `	phl_domnode *pOld = nArg > 1 ? DomObjArg(apArg[1]) : 0;` |
|      - |  1508 | `	xmlNodePtr pParent,pChild,pVictim;` |
|      - |  1509 | `	int iErr;` |
|     45 |  1510 | `	if( pPar == 0 \|\| pNew == 0 \|\| pOld == 0 ){` |
|    ! 0 |  1511 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1512 | `	}` |
|     45 |  1513 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|     45 |  1514 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|     45 |  1515 | `	pVictim = (xmlNodePtr)pOld->pNode;` |
|      - |  1516 | `	/*` |
|      - |  1517 | `	 * php's replaceChild, in its own order (dom_node_replace_child) -- and it` |
|      - |  1518 | `	 * disagrees with appendChild's twice. The document screen answers FIRST (a` |
|      - |  1519 | `	 * text receiver or a read-only receiver with a foreign argument is Wrong` |
|      - |  1520 | `	 * Document here, where appendChild answers false and No Modification).` |
|      - |  1521 | `	 * Then the two silent-false answers: the invalid-children receiver and the` |
|      - |  1522 | `	 * CHILDLESS one -- nothing to replace, and php says nothing at all, even` |
|      - |  1523 | `	 * for an attribute or a document argument. Then the shared insertion` |
|      - |  1524 | `	 * validity: read-only, the ancestor cycle, the attribute receiver's` |
|      - |  1525 | `	 * child-kind rule, an attribute argument's element-only rule, the` |
|      - |  1526 | `	 * document-as-child rule. Then a rule of replaceChild's OWN: old and new` |
|      - |  1527 | `	 * must be attributes TOGETHER or not at all -- so an attribute argument` |
|      - |  1528 | `	 * against a foreign ATTRIBUTE victim reads Not Found from the membership` |
|      - |  1529 | `	 * check (both are attributes, the pair passes) while an element argument` |
|      - |  1530 | `	 * against the same victim is Hierarchy. The victim's membership answers` |
|      - |  1531 | `	 * last.` |
|      - |  1532 | `	 */` |
|     45 |  1533 | `	if( pChild->doc != pParent->doc && pChild->doc != 0 ){` |
|      3 |  1534 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  1535 | `	}` |
|     43 |  1536 | `	if( !DomChildrenValid(pParent) \|\| pParent->children == 0 ){` |
|      7 |  1537 | `		ph7_result_bool(pCtx,0);` |
|      7 |  1538 | `		return PH7_OK;` |
|      - |  1539 | `	}` |
|     36 |  1540 | `	if( DomNodeReadOnly(pParent)` |
|     36 |  1541 | `	 \|\| (pChild->parent && DomNodeReadOnly(pChild->parent)) ){` |
|      3 |  1542 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|      - |  1543 | `	}` |
|     35 |  1544 | `	iErr = DomLinkCycle(pParent,pChild);` |
|     35 |  1545 | `	if( iErr == 0 ){` |
|     31 |  1546 | `		iErr = DomAttrRecvKind(pParent,pChild);` |
|     15 |  1547 | `	}` |
|     34 |  1548 | `	if( iErr == 0 && pChild->type == XML_ATTRIBUTE_NODE` |
|     18 |  1549 | `	 && pParent->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  1550 | `		iErr = DOM_ERR_HIERARCHY;` |
|    ! 0 |  1551 | `	}` |
|     34 |  1552 | `	if( iErr == 0` |
|     32 |  1553 | `	 && (pChild->type == XML_DOCUMENT_NODE \|\| pChild->type == XML_HTML_DOCUMENT_NODE) ){` |
|    ! 0 |  1554 | `		iErr = DOM_ERR_HIERARCHY;` |
|    ! 0 |  1555 | `	}` |
|     34 |  1556 | `	if( iErr == 0` |
|     32 |  1557 | `	 && (pChild->type == XML_ATTRIBUTE_NODE) != (pVictim->type == XML_ATTRIBUTE_NODE) ){` |
|      5 |  1558 | `		iErr = DOM_ERR_HIERARCHY;` |
|      2 |  1559 | `	}` |
|     35 |  1560 | `	if( iErr == 0 && pVictim->parent != pParent ){` |
|      7 |  1561 | `		iErr = DOM_ERR_NOT_FOUND;` |
|      3 |  1562 | `	}` |
|     35 |  1563 | `	if( iErr ){` |
|     17 |  1564 | `		return DomThrow(pCtx,iErr);` |
|      - |  1565 | `	}` |
|      - |  1566 | `	/* The constructed-node adoption, php's "document assignment" step. */` |
|     19 |  1567 | `	DomAdoptIntoRecv(pCtx,apArg[0],pNew);` |
|     19 |  1568 | `	if( DomIsFragment(pChild) ){` |
|      - |  1569 | `		/* No empty-fragment refusal here, unlike the other two: php REMOVES the` |
|      - |  1570 | `		 * old child and inserts nothing, and answers it as any replaceChild` |
|      - |  1571 | `		 * does. */` |
|      5 |  1572 | `		DomFragMove(pNew->pShell,pParent,pChild,pVictim);` |
|      5 |  1573 | `		xmlUnlinkNode(pVictim);` |
|      5 |  1574 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|      5 |  1575 | `		ph7_result_value(pCtx,apArg[1]);` |
|      5 |  1576 | `		return PH7_OK;` |
|      - |  1577 | `	}` |
|     15 |  1578 | `	if( pChild != pVictim && pChild->type == XML_ATTRIBUTE_NODE ){` |
|      - |  1579 | `		/* Both sides are attributes (the XOR above let them through): the swap` |
|      - |  1580 | `		 * happens in the PROPERTY list, at the victim's position, with NO` |
|      - |  1581 | `		 * same-name displacement -- php hands the pair to xmlReplaceNode` |
|      - |  1582 | `		 * as-is, so a duplicate name is the caller's to answer for. */` |
|      5 |  1583 | `		DomAttrDetach(pNew->pShell,(xmlAttrPtr)pChild);` |
|      5 |  1584 | `		DomAttrLinkBefore(pParent,(xmlAttrPtr)pChild,(xmlAttrPtr)pVictim);` |
|      5 |  1585 | `		xmlUnlinkNode(pVictim);` |
|      5 |  1586 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|      5 |  1587 | `		DomNsAttrArrive(pParent,(xmlAttrPtr)pChild);` |
|     13 |  1588 | `	}else if( pChild != pVictim ){` |
|      9 |  1589 | `		DomDetach(pNew->pShell,pChild);` |
|      9 |  1590 | `		DomLinkBefore(pParent,pChild,pVictim);` |
|      9 |  1591 | `		xmlUnlinkNode(pVictim);` |
|      9 |  1592 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|      9 |  1593 | `		DomNsOnInsertEx(pChild,0);` |
|      4 |  1594 | `	}` |
|     15 |  1595 | `	ph7_result_value(pCtx,apArg[1]);` |
|     15 |  1596 | `	return PH7_OK;` |
|     23 |  1597 | `}` |
|      - |  1598 | `/* ===== The 8.3 parent/child-node family (DOMParentNode / DOMChildNode) ===== */` |
|      - |  1599 |  |
|      - |  1600 | `/*` |
|      - |  1601 | ` * The name a TypeError prints for a value that is neither a DOMNode nor a` |
|      - |  1602 | ` * string: the CLASS of an object, php's type keyword for anything else --` |
|      - |  1603 | ` * the same rendering DomWriteText uses for a typed property store.` |
|      - |  1604 | ` */` |
|     24 |  1605 | `static const char * DomGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|      1 |  1606 | `{` |
|     25 |  1607 | `	if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|      5 |  1608 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|      5 |  1609 | `		SyBufferFormat(zBuf,nBuf,"%z",&pObj->pClass->sDisp);` |
|      5 |  1610 | `		return zBuf;` |
|      - |  1611 | `	}` |
|     21 |  1612 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      3 |  1613 | `		return "null";` |
|      - |  1614 | `	}` |
|     19 |  1615 | `	return ph7_type_name(pVal);` |
|     13 |  1616 | `}` |
|      - |  1617 | `/*` |
|      - |  1618 | ` * php's variadic screen for the 8.0 insertion methods: every argument must be` |
|      - |  1619 | ` * a DOMNode or a STRING (nothing coerces -- an int is refused where an` |
|      - |  1620 | `` * ordinary `string $data` parameter would take it), the WHOLE list is checked`` |
|      - |  1621 | ` * before anything else runs, and the TypeError names the position with no` |
|      - |  1622 | ` * parameter name, because many values share the one variadic formal.  The` |
|      - |  1623 | ``  * method name it prints is the DECLARING class's (`DOMCharacterData::before()` `` |
|      - |  1624 | ` * for a comment), which is what pCtx->pFunc->sName already carries.` |
|      - |  1625 | ` * Answers 0 when every argument passed, non-zero after raising.` |
|      - |  1626 | ` */` |
|    160 |  1627 | `static int DomNodesScreen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  1628 | `{` |
|    161 |  1629 | `	ph7_class *pNodeCls = PH7_VmExtractClass(pCtx->pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);` |
|      - |  1630 | `	int i;` |
|    345 |  1631 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    209 |  1632 | `		ph7_value *pVal = apArg[i];` |
|      - |  1633 | `		char zBuf[128];` |
|    209 |  1634 | `		if( pVal->iFlags & MEMOBJ_OBJ ){` |
|    139 |  1635 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|    139 |  1636 | `			if( pNodeCls && PH7_VmInstanceOf(pObj->pClass,pNodeCls) ){` |
|    160 |  1637 | `				continue;` |
|      1 |  1638 | `			}` |
|     73 |  1639 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     51 |  1640 | `			continue;` |
|      - |  1641 | `		}` |
|     37 |  1642 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  1643 | `			"%z(): Argument #%d must be of type DOMNode\|string, %s given",` |
|     24 |  1644 | `			&pCtx->pFunc->sName,i+1,DomGivenName(pVal,zBuf,sizeof(zBuf)));` |
|     25 |  1645 | `		return -1;` |
|    ! 0 |  1646 | `	}` |
|    137 |  1647 | `	return 0;` |
|     81 |  1648 | `}` |
|      - |  1649 | `/*` |
|      - |  1650 | ` * php's "convert nodes into a node" (dom_zvals_to_single_node), transcribed` |
|      - |  1651 | ` * with its ONE-argument shortcut: a single node argument is handed through` |
|      - |  1652 | ` * whole -- nothing is unlinked, every check waits for the insertion -- while` |
|      - |  1653 | ` * two or more arguments really are appended one by one into an internal` |
|      - |  1654 | ` * fragment.  The difference is observable twice over.  A refusal DURING that` |
|      - |  1655 | ` * conversion (another document's node, a document, an attribute) leaves every` |
|      - |  1656 | `` * argument already converted DETACHED -- `$b->append($a, $attr)` costs the`` |
|      - |  1657 | ` * tree its $a -- and a refusal at the final insertion (the receiver was in` |
|      - |  1658 | ` * the converted set) leaves ALL of them detached, which is how` |
|      - |  1659 | `` * `$b->append($a, $b)` empties <r> of both children where `$r->append($r)`,`` |
|      - |  1660 | ` * one argument, moves nothing at all.  The CYCLE is checked only against the` |
|      - |  1661 | ` * conversion fragment (i.e. never fails there), NOT against the receiver --` |
|      - |  1662 | ` * that waits for the insertion step.` |
|      - |  1663 | ` *` |
|      - |  1664 | ` * The converted list is built in pList (xmlNodePtr entries, in order).  Every` |
|      - |  1665 | ` * node it takes is detached and parked in the receiver's orphan set, where a` |
|      - |  1666 | ` * failure leaves it alive for whatever PHP variable still wraps it -- php` |
|      - |  1667 | ` * frees the unwrapped ones instead, which no program can see.  A fragment` |
|      - |  1668 | ` * argument is emptied INTO the list (php unpacks it), so it stays empty even` |
|      - |  1669 | ` * when a later argument is refused.  Answers 0, or non-zero after raising.` |
|      - |  1670 | ` */` |
|     58 |  1671 | `static int DomNodesConvert(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pParent,` |
|      - |  1672 | `	int nArg,ph7_value **apArg,SySet *pList,int *pRc)` |
|      1 |  1673 | `{` |
|     59 |  1674 | `	ph7_class_instance *pDstHolder = DomThisDoc(pCtx);` |
|      - |  1675 | `	int i;` |
|    149 |  1676 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      - |  1677 | `		phl_domnode *pNd;` |
|      - |  1678 | `		xmlNodePtr pNode;` |
|      - |  1679 | `		ph7_class_instance *pSrcHolder;` |
|    101 |  1680 | `		if( (apArg[i]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     41 |  1681 | `			int nLen = 0;` |
|     41 |  1682 | `			const char *zText = ph7_value_to_string(apArg[i],&nLen);` |
|     41 |  1683 | `			pNode = xmlNewDocTextLen(pParent->doc,(const xmlChar *)zText,nLen);` |
|     41 |  1684 | `			if( pNode ){` |
|     41 |  1685 | `				DomOrphanAdd(pShell,pNode);` |
|     41 |  1686 | `				SySetPut(pList,(const void *)&pNode);` |
|     20 |  1687 | `			}` |
|     41 |  1688 | `			continue;` |
|      - |  1689 | `		}` |
|     61 |  1690 | `		pNd = DomObjArg(apArg[i]);` |
|     61 |  1691 | `		if( pNd == 0 ){` |
|      - |  1692 | `			/* A DOMNode-classed object with no node behind it. php's refusal` |
|      - |  1693 | `			 * ignores strictErrorChecking. */` |
|      3 |  1694 | `			*pRc = DomThrowAlways(pCtx,DOM_ERR_INVALID_STATE);` |
|      7 |  1695 | `			return -1;` |
|      - |  1696 | `		}` |
|     59 |  1697 | `		pNode = (xmlNodePtr)pNd->pNode;` |
|     59 |  1698 | `		if( pNode->doc != pParent->doc ){` |
|      - |  1699 | `			/* No adoption in the modern family: a constructed node's NULL` |
|      - |  1700 | `			 * document is a mismatch like any other and refuses -- only the` |
|      - |  1701 | `			 * OWNERLESS-to-OWNERLESS pair (both NULL) passes. */` |
|      5 |  1702 | `			*pRc = DomThrowVoid(pCtx,DOM_ERR_WRONG_DOC);` |
|      5 |  1703 | `			return -1;` |
|      - |  1704 | `		}` |
|      - |  1705 | `		/* Same document, possibly different HOLDER: two constructed trees` |
|      - |  1706 | `		 * merging. The wrappers move to the receiver's cache so identity` |
|      - |  1707 | `		 * keeps answering. */` |
|     55 |  1708 | `		pSrcHolder = DomObjArgDoc(apArg[i]);` |
|     55 |  1709 | `		if( pSrcHolder != pDstHolder ){` |
|      9 |  1710 | `			DomAdoptWrappers(pCtx->pVm,pSrcHolder,pDstHolder,pShell,pNode);` |
|      9 |  1711 | `			pNd->pShell = pShell;` |
|      4 |  1712 | `		}` |
|     54 |  1713 | `		if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE` |
|     55 |  1714 | `		 \|\| pNode->type == XML_ATTRIBUTE_NODE ){` |
|      5 |  1715 | `			*pRc = DomThrowVoid(pCtx,DOM_ERR_HIERARCHY);` |
|      5 |  1716 | `			return -1;` |
|      - |  1717 | `		}` |
|     51 |  1718 | `		if( DomIsFragment(pNode) ){` |
|    ! 0 |  1719 | `			xmlNodePtr pChild = pNode->children;` |
|    ! 0 |  1720 | `			while( pChild ){` |
|    ! 0 |  1721 | `				xmlNodePtr pNext = pChild->next;` |
|    ! 0 |  1722 | `				xmlUnlinkNode(pChild);` |
|    ! 0 |  1723 | `				DomOrphanAdd(pShell,pChild);` |
|    ! 0 |  1724 | `				SySetPut(pList,(const void *)&pChild);` |
|    ! 0 |  1725 | `				pChild = pNext;` |
|    ! 0 |  1726 | `			}` |
|    ! 0 |  1727 | `			pNode->children = pNode->last = 0;` |
|    ! 0 |  1728 | `			continue;` |
|      - |  1729 | `		}` |
|     51 |  1730 | `		DomDetach(pNd->pShell,pNode);` |
|     51 |  1731 | `		DomOrphanAdd(pShell,pNode);` |
|     51 |  1732 | `		SySetPut(pList,(const void *)&pNode);` |
|     26 |  1733 | `	}` |
|     49 |  1734 | `	return 0;` |
|     30 |  1735 | `}` |
|     84 |  1736 | `static int DomListHas(SySet *pList,xmlNodePtr pNode)` |
|      1 |  1737 | `{` |
|     85 |  1738 | `	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);` |
|      - |  1739 | `	sxu32 n;` |
|    211 |  1740 | `	for( n = 0 ; n < SySetUsed(pList) ; ++n ){` |
|    133 |  1741 | `		if( apNode[n] == pNode ){` |
|      7 |  1742 | `			return 1;` |
|      - |  1743 | `		}` |
|     64 |  1744 | `	}` |
|     79 |  1745 | `	return 0;` |
|     43 |  1746 | `}` |
|      - |  1747 | `/*` |
|      - |  1748 | ` * php's pre-insertion validity for what conversion produced, against the REAL` |
|      - |  1749 | ` * parent this time.  For a single node argument this is where every check` |
|      - |  1750 | ` * runs -- another document before the kind-or-ancestor Hierarchy refusal, the` |
|      - |  1751 | ` * same order the conversion pass uses -- and for a converted list the only` |
|      - |  1752 | ` * question left is whether the receiver is now INSIDE the set (its ancestor` |
|      - |  1753 | ` * chain passes through a detached argument).  Note what php never checks on` |
|      - |  1754 | ` * this path: a document receiver takes a second root element and bare text` |
|      - |  1755 | ` * without complaint, so the document it writes may not be well-formed XML --` |
|      - |  1756 | ` * measured, and matched.` |
|      - |  1757 | ` */` |
|    146 |  1758 | `static int DomInsertValidity(xmlNodePtr pParent,xmlNodePtr pSingle,SySet *pList)` |
|      1 |  1759 | `{` |
|      - |  1760 | `	xmlNodePtr p;` |
|    147 |  1761 | `	if( pSingle ){` |
|     99 |  1762 | `		if( pSingle->doc != pParent->doc ){` |
|     13 |  1763 | `			return DOM_ERR_WRONG_DOC;` |
|      - |  1764 | `		}` |
|     86 |  1765 | `		if( pSingle->type == XML_DOCUMENT_NODE \|\| pSingle->type == XML_HTML_DOCUMENT_NODE` |
|     85 |  1766 | `		 \|\| pSingle->type == XML_ATTRIBUTE_NODE ){` |
|      7 |  1767 | `			return DOM_ERR_HIERARCHY;` |
|      - |  1768 | `		}` |
|    215 |  1769 | `		for( p = pParent ; p ; p = p->parent ){` |
|    151 |  1770 | `			if( p == pSingle ){` |
|     17 |  1771 | `				return DOM_ERR_HIERARCHY;` |
|      - |  1772 | `			}` |
|     68 |  1773 | `		}` |
|     65 |  1774 | `		return 0;` |
|      - |  1775 | `	}` |
|    121 |  1776 | `	for( p = pParent ; p ; p = p->parent ){` |
|     79 |  1777 | `		if( DomListHas(pList,p) ){` |
|      7 |  1778 | `			return DOM_ERR_HIERARCHY;` |
|      - |  1779 | `		}` |
|     37 |  1780 | `	}` |
|     43 |  1781 | `	return 0;` |
|     74 |  1782 | `}` |
|      - |  1783 | `/*` |
|      - |  1784 | ` * The insertion itself (php's dom_insert_node_list_unchecked): everything in` |
|      - |  1785 | ` * pList goes before pRef -- at the end when NULL -- in order.  A list node` |
|      - |  1786 | ` * came through the conversion fragment, so its namespace reconcile is the` |
|      - |  1787 | ` * DEEP one (dom_reconcile_ns_list); a single node is php's dom_reconcile_ns,` |
|      - |  1788 | ` * the shallow appendChild rule.  A single node inserted before ITSELF slides` |
|      - |  1789 | ` * the reference to its next sibling first (the spec's step 3), which is what` |
|      - |  1790 | `` * makes `$r->prepend($r->firstChild)` a no-op instead of a cycle.`` |
|      - |  1791 | ` */` |
|     42 |  1792 | `static void DomNodesPlace(phl_xmldoc *pShell,xmlNodePtr pParent,xmlNodePtr pRef,SySet *pList)` |
|      1 |  1793 | `{` |
|     43 |  1794 | `	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);` |
|      - |  1795 | `	sxu32 n;` |
|    111 |  1796 | `	for( n = 0 ; n < SySetUsed(pList) ; ++n ){` |
|     69 |  1797 | `		xmlNodePtr pNode = apNode[n];` |
|     69 |  1798 | `		DomDetach(pShell,pNode);` |
|     69 |  1799 | `		if( pRef ){` |
|     23 |  1800 | `			DomLinkBefore(pParent,pNode,pRef);` |
|     12 |  1801 | `		}else{` |
|     47 |  1802 | `			DomLinkLast(pParent,pNode);` |
|      - |  1803 | `		}` |
|     69 |  1804 | `		DomNsOnInsertEx(pNode,1);` |
|     35 |  1805 | `	}` |
|     43 |  1806 | `}` |
|      - |  1807 | `/*` |
|      - |  1808 | ` * DOMParentNode::append / prepend / replaceChildren -- one body, three` |
|      - |  1809 | `` * insertion points.  php declares all three `void`, so a refusal in the`` |
|      - |  1810 | ` * non-strict mode is a warning and NOTHING is answered (DomThrowVoid).` |
|      - |  1811 | ` */` |
|      - |  1812 | `#define DOM_PN_APPEND   0` |
|      - |  1813 | `#define DOM_PN_PREPEND  1` |
|      - |  1814 | `#define DOM_PN_REPLACE  2` |
|    108 |  1815 | `static int DomParentNodeInsert(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)` |
|      1 |  1816 | `{` |
|    109 |  1817 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    109 |  1818 | `	phl_domnode *pOne = 0;` |
|    109 |  1819 | `	xmlNodePtr pParent,pSingle = 0,pRef = 0;` |
|      - |  1820 | `	SySet sList;` |
|    109 |  1821 | `	int iErr,rc = PH7_OK;` |
|    109 |  1822 | `	if( DomNodesScreen(pCtx,nArg,apArg) \|\| pPar == 0 ){` |
|     21 |  1823 | `		return PH7_OK;` |
|      - |  1824 | `	}` |
|     89 |  1825 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|     89 |  1826 | `	SySetInit(&sList,&pCtx->pVm->sAllocator,sizeof(xmlNodePtr));` |
|     89 |  1827 | `	if( nArg == 1 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      - |  1828 | `		/* The one-argument shortcut: the node itself, unconverted.  A shell` |
|      - |  1829 | `		 * with no node behind it is php's SILENT no-op here (the pre-insert` |
|      - |  1830 | `		 * NULL guard), where the multi-argument conversion raises Invalid` |
|      - |  1831 | `		 * State -- one more face of the shortcut. */` |
|     53 |  1832 | `		pOne = DomObjArg(apArg[0]);` |
|     53 |  1833 | `		if( pOne == 0 ){` |
|      3 |  1834 | `			return PH7_OK;` |
|      - |  1835 | `		}` |
|     51 |  1836 | `		pSingle = (xmlNodePtr)pOne->pNode;` |
|     62 |  1837 | `	}else if( DomNodesConvert(pCtx,pPar->pShell,pParent,nArg,apArg,&sList,&rc) ){` |
|      7 |  1838 | `		SySetRelease(&sList);` |
|      7 |  1839 | `		return rc;` |
|      - |  1840 | `	}` |
|     81 |  1841 | `	iErr = DomInsertValidity(pParent,pSingle,&sList);` |
|     81 |  1842 | `	if( iErr ){` |
|     31 |  1843 | `		SySetRelease(&sList);` |
|     31 |  1844 | `		return DomThrowVoid(pCtx,iErr);` |
|      - |  1845 | `	}` |
|     51 |  1846 | `	if( pOne ){` |
|      - |  1847 | `		/* The single-node shortcut skipped the conversion, so it re-homes its` |
|      - |  1848 | `		 * wrappers here: an ownerless argument merging into an ownerless` |
|      - |  1849 | `		 * receiver (the only mismatch the validity lets through). */` |
|     23 |  1850 | `		DomAdoptIntoRecv(pCtx,apArg[0],pOne);` |
|     11 |  1851 | `	}` |
|     51 |  1852 | `	if( iMode == DOM_PN_REPLACE ){` |
|      - |  1853 | `		/* Every remaining child goes -- through the wrapper-preserving drop a` |
|      - |  1854 | `		 * content write uses, so a PHP variable holding one keeps a live` |
|      - |  1855 | `		 * detached node rather than a dangling pointer.  After the validity` |
|      - |  1856 | `		 * check, as php orders it. */` |
|     11 |  1857 | `		DomDropChildren(pCtx,pPar->pShell,pParent);` |
|     46 |  1858 | `	}else if( iMode == DOM_PN_PREPEND ){` |
|      - |  1859 | `		/* The first child AFTER conversion has emptied the set out of the` |
|      - |  1860 | `		 * tree -- and never a member of the set. */` |
|      7 |  1861 | `		pRef = pParent->children;` |
|      3 |  1862 | `	}` |
|     51 |  1863 | `	if( pSingle ){` |
|     23 |  1864 | `		if( DomIsFragment(pSingle) ){` |
|      - |  1865 | `			/* A single fragment splices -- silently even when EMPTY, unlike` |
|      - |  1866 | `			 * appendChild's warning. */` |
|      5 |  1867 | `			DomFragMove(pOne->pShell,pParent,pSingle,pRef);` |
|      3 |  1868 | `		}else{` |
|     19 |  1869 | `			if( pRef == pSingle ){` |
|    ! 0 |  1870 | `				pRef = pSingle->next;` |
|    ! 0 |  1871 | `			}` |
|     19 |  1872 | `			DomDetach(pOne->pShell,pSingle);` |
|     19 |  1873 | `			if( pRef ){` |
|      3 |  1874 | `				DomLinkBefore(pParent,pSingle,pRef);` |
|      2 |  1875 | `			}else{` |
|     17 |  1876 | `				DomLinkLast(pParent,pSingle);` |
|      - |  1877 | `			}` |
|     19 |  1878 | `			DomNsOnInsertEx(pSingle,0);` |
|      - |  1879 | `		}` |
|     12 |  1880 | `	}else{` |
|     29 |  1881 | `		DomNodesPlace(pPar->pShell,pParent,pRef,&sList);` |
|      - |  1882 | `	}` |
|     51 |  1883 | `	SySetRelease(&sList);` |
|     51 |  1884 | `	return PH7_OK;` |
|     55 |  1885 | `}` |
|     86 |  1886 | `DOM_METHOD(vm_builtin_Dom_append)` |
|      1 |  1887 | `{` |
|     87 |  1888 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_APPEND);` |
|      1 |  1889 | `}` |
|      8 |  1890 | `DOM_METHOD(vm_builtin_Dom_prepend)` |
|      1 |  1891 | `{` |
|      9 |  1892 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_PREPEND);` |
|      1 |  1893 | `}` |
|     14 |  1894 | `DOM_METHOD(vm_builtin_Dom_replaceChildren)` |
|      1 |  1895 | `{` |
|     15 |  1896 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_REPLACE);` |
|      1 |  1897 | `}` |
|      - |  1898 | `/*` |
|      - |  1899 | ` * Is this xmlNodePtr one of the ARGUMENT nodes?  The viable-sibling walks ask` |
|      - |  1900 | ` * it about tree nodes, so only object arguments can match -- php's` |
|      - |  1901 | ` * dom_is_node_in_list does the same walk over the zval list.` |
|      - |  1902 | ` */` |
|     24 |  1903 | `static int DomArgListHasNode(int nArg,ph7_value **apArg,xmlNodePtr pNode)` |
|      1 |  1904 | `{` |
|      - |  1905 | `	int i;` |
|     47 |  1906 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     29 |  1907 | `		phl_domnode *pNd = DomObjArg(apArg[i]);` |
|     29 |  1908 | `		if( pNd && (xmlNodePtr)pNd->pNode == pNode ){` |
|      7 |  1909 | `			return 1;` |
|      - |  1910 | `		}` |
|     12 |  1911 | `	}` |
|     19 |  1912 | `	return 0;` |
|     13 |  1913 | `}` |
|      - |  1914 | `/*` |
|      - |  1915 | ` * DOMChildNode::before / after / replaceWith -- php's WHATWG transcription` |
|      - |  1916 | ` * (dom_parent_node_before/after, dom_child_replace_with), sharing the parent` |
|      - |  1917 | ` * side's conversion machinery.  The order is the measurable part: the TYPE` |
|      - |  1918 | ` * screen runs first even for a node with no parent; a parentless receiver` |
|      - |  1919 | ` * then returns in SILENCE -- around an argument that could never be inserted` |
|      - |  1920 | ` * -- and only then does conversion run, with the same mid-list detachment the` |
|      - |  1921 | ` * parent side has.  The reference sibling ("viable") is the nearest sibling` |
|      - |  1922 | ` * NOT in the argument set, read before anything moves; the insertion point is` |
|      - |  1923 | ` * derived from it after conversion, so a set member that was also the first` |
|      - |  1924 | ` * child no longer counts.` |
|      - |  1925 | ` */` |
|      - |  1926 | `#define DOM_CN_BEFORE   0` |
|      - |  1927 | `#define DOM_CN_AFTER    1` |
|      - |  1928 | `#define DOM_CN_REPLACE  2` |
|     52 |  1929 | `static int DomChildNodeOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)` |
|      1 |  1930 | `{` |
|     53 |  1931 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     53 |  1932 | `	phl_domnode *pOne = 0;` |
|     53 |  1933 | `	xmlNodePtr pThis,pParent,pViable,pRef,pSingle = 0;` |
|      - |  1934 | `	SySet sList;` |
|     53 |  1935 | `	int iErr,rc = PH7_OK;` |
|     53 |  1936 | `	if( DomNodesScreen(pCtx,nArg,apArg) \|\| pNd == 0 ){` |
|      5 |  1937 | `		return PH7_OK;` |
|      - |  1938 | `	}` |
|     49 |  1939 | `	pThis = (xmlNodePtr)pNd->pNode;` |
|     49 |  1940 | `	pParent = pThis->parent;` |
|     49 |  1941 | `	if( pParent == 0 ){` |
|     11 |  1942 | `		return PH7_OK;` |
|      - |  1943 | `	}` |
|     38 |  1944 | `	if( iMode == DOM_CN_REPLACE` |
|     25 |  1945 | `	 && (DomNodeReadOnly(pThis) \|\| DomNodeReadOnly(pParent)) ){` |
|      - |  1946 | `		/* replaceWith carries the read-only refusal (it REMOVES the receiver)` |
|      - |  1947 | `		 * where before/after do not: a text child of a constructed ownerless` |
|      - |  1948 | `		 * element takes before() and refuses replaceWith(). The parentless` |
|      - |  1949 | `		 * silence above still answers first -- a constructed ROOT is a silent` |
|      - |  1950 | `		 * no-op, not this refusal. */` |
|    ! 0 |  1951 | `		return DomThrowVoid(pCtx,DOM_ERR_NO_MOD);` |
|      - |  1952 | `	}` |
|     39 |  1953 | `	if( iMode == DOM_CN_BEFORE ){` |
|     19 |  1954 | `		pViable = pThis->prev;` |
|     21 |  1955 | `		while( pViable && DomArgListHasNode(nArg,apArg,pViable) ){` |
|      3 |  1956 | `			pViable = pViable->prev;` |
|      1 |  1957 | `		}` |
|     10 |  1958 | `	}else{` |
|     21 |  1959 | `		pViable = pThis->next;` |
|     25 |  1960 | `		while( pViable && DomArgListHasNode(nArg,apArg,pViable) ){` |
|      5 |  1961 | `			pViable = pViable->next;` |
|      1 |  1962 | `		}` |
|      - |  1963 | `	}` |
|     39 |  1964 | `	SySetInit(&sList,&pCtx->pVm->sAllocator,sizeof(xmlNodePtr));` |
|     39 |  1965 | `	if( nArg == 1 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     17 |  1966 | `		pOne = DomObjArg(apArg[0]);` |
|     17 |  1967 | `		if( pOne == 0 ){` |
|    ! 0 |  1968 | `			return PH7_OK;` |
|      - |  1969 | `		}` |
|     17 |  1970 | `		pSingle = (xmlNodePtr)pOne->pNode;` |
|     31 |  1971 | `	}else if( DomNodesConvert(pCtx,pNd->pShell,pParent,nArg,apArg,&sList,&rc) ){` |
|      5 |  1972 | `		SySetRelease(&sList);` |
|      5 |  1973 | `		return rc;` |
|      - |  1974 | `	}` |
|     35 |  1975 | `	iErr = DomInsertValidity(pParent,pSingle,&sList);` |
|     35 |  1976 | `	if( iErr ){` |
|      5 |  1977 | `		SySetRelease(&sList);` |
|      5 |  1978 | `		return DomThrowVoid(pCtx,iErr);` |
|      - |  1979 | `	}` |
|     31 |  1980 | `	if( pOne ){` |
|      - |  1981 | `		/* The single-node shortcut skipped the conversion's wrapper re-home:` |
|      - |  1982 | `		 * an ownerless argument merging into an ownerless receiver's tree, the` |
|      - |  1983 | `		 * only mismatch the validity lets through. */` |
|     17 |  1984 | `		DomAdoptIntoRecv(pCtx,apArg[0],pOne);` |
|      8 |  1985 | `	}` |
|     31 |  1986 | `	if( iMode == DOM_CN_BEFORE ){` |
|      - |  1987 | `		/* Step 5: the viable previous sibling's NEXT -- the parent's first` |
|      - |  1988 | `		 * child when there is none -- both read after conversion. */` |
|     15 |  1989 | `		pRef = pViable ? pViable->next : pParent->children;` |
|      8 |  1990 | `	}else{` |
|     17 |  1991 | `		pRef = pViable;` |
|      - |  1992 | `	}` |
|     31 |  1993 | `	if( iMode == DOM_CN_REPLACE ){` |
|      - |  1994 | `		/* php unlinks the receiver unless conversion already took it. */` |
|      9 |  1995 | `		if( pThis != pSingle && !DomListHas(&sList,pThis) ){` |
|      7 |  1996 | `			xmlUnlinkNode(pThis);` |
|      7 |  1997 | `			DomOrphanAdd(pNd->pShell,pThis);` |
|      3 |  1998 | `		}` |
|      4 |  1999 | `	}` |
|     31 |  2000 | `	if( pSingle ){` |
|     17 |  2001 | `		if( DomIsFragment(pSingle) ){` |
|      3 |  2002 | `			DomFragMove(pOne->pShell,pParent,pSingle,pRef);` |
|      2 |  2003 | `		}else{` |
|     15 |  2004 | `			if( pRef == pSingle ){` |
|      3 |  2005 | `				pRef = pSingle->next;` |
|      1 |  2006 | `			}` |
|     15 |  2007 | `			DomDetach(pOne->pShell,pSingle);` |
|     15 |  2008 | `			if( pRef ){` |
|     13 |  2009 | `				DomLinkBefore(pParent,pSingle,pRef);` |
|      7 |  2010 | `			}else{` |
|      3 |  2011 | `				DomLinkLast(pParent,pSingle);` |
|      - |  2012 | `			}` |
|     15 |  2013 | `			DomNsOnInsertEx(pSingle,0);` |
|      - |  2014 | `		}` |
|      9 |  2015 | `	}else{` |
|     15 |  2016 | `		DomNodesPlace(pNd->pShell,pParent,pRef,&sList);` |
|      - |  2017 | `	}` |
|     31 |  2018 | `	SySetRelease(&sList);` |
|     31 |  2019 | `	return PH7_OK;` |
|     27 |  2020 | `}` |
|     26 |  2021 | `DOM_METHOD(vm_builtin_Dom_before)` |
|      1 |  2022 | `{` |
|     27 |  2023 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_BEFORE);` |
|      1 |  2024 | `}` |
|     12 |  2025 | `DOM_METHOD(vm_builtin_Dom_after)` |
|      1 |  2026 | `{` |
|     13 |  2027 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_AFTER);` |
|      1 |  2028 | `}` |
|     14 |  2029 | `DOM_METHOD(vm_builtin_Dom_replaceWith)` |
|      1 |  2030 | `{` |
|     15 |  2031 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_REPLACE);` |
|      1 |  2032 | `}` |
|      - |  2033 | `/*` |
|      - |  2034 | ` * DOMChildNode::remove(): void -- and php's asymmetry: where before() on a` |
|      - |  2035 | ` * parentless node is a silent no-op, remove() is the Not Found refusal, in` |
|      - |  2036 | ` * whichever mode the document is in.` |
|      - |  2037 | ` */` |
|     10 |  2038 | `DOM_METHOD(vm_builtin_Dom_removeSelf)` |
|      1 |  2039 | `{` |
|     11 |  2040 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      - |  2041 | `	xmlNodePtr pNode;` |
|      5 |  2042 | `	SXUNUSED(nArg);` |
|      5 |  2043 | `	SXUNUSED(apArg);` |
|     11 |  2044 | `	if( pNd == 0 ){` |
|    ! 0 |  2045 | `		return PH7_OK;` |
|      - |  2046 | `	}` |
|     11 |  2047 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|      - |  2048 | `	/* The read-only refusal answers BEFORE the parentless one: a constructed` |
|      - |  2049 | `	 * ownerless node -- necessarily parentless -- is No Modification Allowed` |
|      - |  2050 | `	 * here, where an owned parentless node is Not Found. */` |
|     11 |  2051 | `	if( DomNodeReadOnly(pNode) \|\| (pNode->parent && DomNodeReadOnly(pNode->parent)) ){` |
|      3 |  2052 | `		return DomThrowVoid(pCtx,DOM_ERR_NO_MOD);` |
|      - |  2053 | `	}` |
|      9 |  2054 | `	if( pNode->parent == 0 ){` |
|      5 |  2055 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  2056 | `	}` |
|      5 |  2057 | `	xmlUnlinkNode(pNode);` |
|      5 |  2058 | `	DomOrphanAdd(pNd->pShell,pNode);` |
|      5 |  2059 | `	return PH7_OK;` |
|      6 |  2060 | `}` |
|      - |  2061 |  |
|      - |  2062 | `/* DOMNode::hasChildNodes(): bool / hasAttributes(): bool / getLineNo(): int */` |
|      4 |  2063 | `DOM_METHOD(vm_builtin_DOMNode_hasChildNodes)` |
|      1 |  2064 | `{` |
|      5 |  2065 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      2 |  2066 | `	SXUNUSED(nArg);` |
|      2 |  2067 | `	SXUNUSED(apArg);` |
|      5 |  2068 | `	ph7_result_bool(pCtx,pNd && DomNodeChildFirst((xmlNodePtr)pNd->pNode) != 0);` |
|      5 |  2069 | `	return PH7_OK;` |
|      1 |  2070 | `}` |
|      - |  2071 | `/* The attribute list of an element (empty for anything else). */` |
|    242 |  2072 | `static xmlAttrPtr DomAttrList(xmlNodePtr pNode)` |
|      2 |  2073 | `{` |
|    244 |  2074 | `	return (pNode && pNode->type == XML_ELEMENT_NODE) ? pNode->properties : 0;` |
|      2 |  2075 | `}` |
|     16 |  2076 | `static int DomAttrCount(xmlNodePtr pNode)` |
|      1 |  2077 | `{` |
|     17 |  2078 | `	xmlAttrPtr pAttr = DomAttrList(pNode);` |
|     17 |  2079 | `	int iCount = 0;` |
|     41 |  2080 | `	for( ; pAttr ; pAttr = pAttr->next ){` |
|     25 |  2081 | `		iCount++;` |
|     13 |  2082 | `	}` |
|     17 |  2083 | `	return iCount;` |
|      1 |  2084 | `}` |
|    208 |  2085 | `static xmlAttrPtr DomAttrAt(xmlNodePtr pNode,int iWant)` |
|      2 |  2086 | `{` |
|    210 |  2087 | `	xmlAttrPtr pAttr = DomAttrList(pNode);` |
|    330 |  2088 | `	for( ; pAttr && iWant > 0 ; pAttr = pAttr->next ){` |
|    122 |  2089 | `		iWant--;` |
|     62 |  2090 | `	}` |
|    210 |  2091 | `	return pAttr;` |
|      2 |  2092 | `}` |
|    ! 0 |  2093 | `DOM_METHOD(vm_builtin_DOMNode_hasAttributes)` |
|    ! 0 |  2094 | `{` |
|    ! 0 |  2095 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    ! 0 |  2096 | `	SXUNUSED(nArg);` |
|    ! 0 |  2097 | `	SXUNUSED(apArg);` |
|    ! 0 |  2098 | `	ph7_result_bool(pCtx,pNd && DomAttrCount((xmlNodePtr)pNd->pNode) > 0);` |
|    ! 0 |  2099 | `	return PH7_OK;` |
|    ! 0 |  2100 | `}` |
|      8 |  2101 | `DOM_METHOD(vm_builtin_DOMNode_getLineNo)` |
|      1 |  2102 | `{` |
|      9 |  2103 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      4 |  2104 | `	SXUNUSED(nArg);` |
|      4 |  2105 | `	SXUNUSED(apArg);` |
|      9 |  2106 | `	ph7_result_int64(pCtx,pNd ? (ph7_int64)xmlGetLineNo((xmlNodePtr)pNd->pNode) : 0);` |
|      9 |  2107 | `	return PH7_OK;` |
|      1 |  2108 | `}` |
|      - |  2109 | `/* DOMNode::isSameNode(DOMNode $otherNode): bool -- pointer identity, which is` |
|      - |  2110 | ` * also the identity the wrapper cache keys on. */` |
|      6 |  2111 | `DOM_METHOD(vm_builtin_DOMNode_isSameNode)` |
|      1 |  2112 | `{` |
|      7 |  2113 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  2114 | `	phl_domnode *pOther = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|      7 |  2115 | `	ph7_result_bool(pCtx,pNd != 0 && pOther != 0 && pNd->pNode == pOther->pNode);` |
|      7 |  2116 | `	return PH7_OK;` |
|      1 |  2117 | `}` |
|      - |  2118 |  |
|      - |  2119 | `/* ===== Position, containment and structural equality ===== */` |
|      - |  2120 |  |
|      - |  2121 | `/* php's DOMNode::DOCUMENT_POSITION_* -- the DOM's own bit values. */` |
|      - |  2122 | `#define DOM_POS_DISCONNECTED 1` |
|      - |  2123 | `#define DOM_POS_PRECEDING    2` |
|      - |  2124 | `#define DOM_POS_FOLLOWING    4` |
|      - |  2125 | `#define DOM_POS_CONTAINS     8` |
|      - |  2126 | `#define DOM_POS_CONTAINED_BY 16` |
|      - |  2127 | `#define DOM_POS_IMPL_SPEC    32` |
|      - |  2128 |  |
|      - |  2129 | `/* The topmost node reachable by parent links -- the DOCUMENT for a node in a` |
|      - |  2130 | ` * tree, and the outermost detached node otherwise. */` |
|     66 |  2131 | `static xmlNodePtr DomRootOf(xmlNodePtr pNode)` |
|      1 |  2132 | `{` |
|    155 |  2133 | `	while( pNode && pNode->parent ){` |
|     89 |  2134 | `		pNode = pNode->parent;` |
|      1 |  2135 | `	}` |
|     67 |  2136 | `	return pNode;` |
|      1 |  2137 | `}` |
|      - |  2138 | `/* Is pAnc a STRICT ancestor of pNode? libxml parents an attribute at its` |
|      - |  2139 | `` * element, which is how php answers true for `$el->contains($el->attr)`. */`` |
|     38 |  2140 | `static int DomIsAncestorOf(xmlNodePtr pAnc,xmlNodePtr pNode)` |
|      1 |  2141 | `{` |
|     39 |  2142 | `	xmlNodePtr p = pNode ? pNode->parent : 0;` |
|     75 |  2143 | `	for( ; p ; p = p->parent ){` |
|     51 |  2144 | `		if( p == pAnc ){` |
|     15 |  2145 | `			return 1;` |
|      - |  2146 | `		}` |
|     19 |  2147 | `	}` |
|     25 |  2148 | `	return 0;` |
|     20 |  2149 | `}` |
|     24 |  2150 | `static int DomDepthOf(xmlNodePtr pNode)` |
|      1 |  2151 | `{` |
|     25 |  2152 | `	int n = 0;` |
|     93 |  2153 | `	for( ; pNode ; pNode = pNode->parent ){` |
|     69 |  2154 | `		n++;` |
|     35 |  2155 | `	}` |
|     25 |  2156 | `	return n;` |
|      1 |  2157 | `}` |
|      - |  2158 | `/*` |
|      - |  2159 | ` * Does pA come before pB in document order? Both are distinct nodes of one` |
|      - |  2160 | ` * tree. Lifting each to the depth of the other either lands on the SAME node --` |
|      - |  2161 | ` * one is an ancestor of the other, and an ancestor comes first in a preorder` |
|      - |  2162 | ` * walk -- or, after stepping up in lockstep, on two distinct children of one` |
|      - |  2163 | ` * parent, whose child-list order is the answer.` |
|      - |  2164 | ` *` |
|      - |  2165 | ` * The ancestor case is reachable even though compareDocumentPosition answers` |
|      - |  2166 | ` * CONTAINS/CONTAINED_BY for it: an ATTRIBUTE folds onto its element first, so` |
|      - |  2167 | `` * `$root->attr` against `$child->attr` arrives here as the element PAIR with`` |
|      - |  2168 | ` * one of them an ancestor of the other.` |
|      - |  2169 | ` */` |
|     12 |  2170 | `static int DomPrecedesInTree(xmlNodePtr pA,xmlNodePtr pB)` |
|      1 |  2171 | `{` |
|     13 |  2172 | `	int nA = DomDepthOf(pA),nB = DomDepthOf(pB);` |
|     13 |  2173 | `	xmlNodePtr pUpA = pA,pUpB = pB,p;` |
|     15 |  2174 | `	while( nA > nB ){ pUpA = pUpA->parent; nA--; }` |
|     15 |  2175 | `	while( nB > nA ){ pUpB = pUpB->parent; nB--; }` |
|     13 |  2176 | `	if( pUpA == pUpB ){` |
|      5 |  2177 | `		return pUpA == pA;   /* pA was not lifted: it is the ancestor */` |
|      - |  2178 | `	}` |
|      9 |  2179 | `	while( pUpA && pUpB && pUpA->parent != pUpB->parent ){` |
|    ! 0 |  2180 | `		pUpA = pUpA->parent;` |
|    ! 0 |  2181 | `		pUpB = pUpB->parent;` |
|    ! 0 |  2182 | `	}` |
|      9 |  2183 | `	for( p = pUpA ? pUpA->prev : 0 ; p ; p = p->prev ){` |
|      5 |  2184 | `		if( p == pUpB ){` |
|      5 |  2185 | `			return 0;   /* pB is an earlier sibling */` |
|      - |  2186 | `		}` |
|    ! 0 |  2187 | `	}` |
|      5 |  2188 | `	return 1;` |
|      7 |  2189 | `}` |
|      - |  2190 | `/*` |
|      - |  2191 | ` * DOMNode::compareDocumentPosition(DOMNode $other): int` |
|      - |  2192 | ` *` |
|      - |  2193 | `` * The DOM's own algorithm, run with `other` as node1 and the receiver as node2.`` |
|      - |  2194 | ` * An ATTRIBUTE is folded onto its element first, which is what makes an` |
|      - |  2195 | `` * attribute answer `CONTAINED_BY\|FOLLOWING` against its own element and`` |
|      - |  2196 | `` * `IMPLEMENTATION_SPECIFIC` plus the attribute-list order against a sibling`` |
|      - |  2197 | ` * attribute -- and what makes it compare as its element against everything else.` |
|      - |  2198 | ` *` |
|      - |  2199 | ` * Two nodes in different trees are DISCONNECTED, and the direction bit there is` |
|      - |  2200 | ` * php's own: the raw node POINTERS, which is the only thing available and which` |
|      - |  2201 | ` * php marks IMPLEMENTATION_SPECIFIC for exactly that reason. The bit is stable` |
|      - |  2202 | ` * and antisymmetric within one process; it is not comparable ACROSS engines,` |
|      - |  2203 | ` * so no test pins it.` |
|      - |  2204 | ` */` |
|     32 |  2205 | `DOM_METHOD(vm_builtin_DOMNode_compareDocumentPosition)` |
|      1 |  2206 | `{` |
|     33 |  2207 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     33 |  2208 | `	phl_domnode *pOtherNd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     33 |  2209 | `	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     33 |  2210 | `	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;` |
|     33 |  2211 | `	xmlNodePtr pNode1,pNode2,pAttr1 = 0,pAttr2 = 0;` |
|     33 |  2212 | `	if( pThisNode == 0 \|\| pOther == 0 ){` |
|     10 |  2213 | `		ph7_result_int(pCtx,0);` |
|     10 |  2214 | `		return PH7_OK;` |
|      - |  2215 | `	}` |
|     43 |  2216 | `	if( pThisNode == pOther ){` |
|      3 |  2217 | `		ph7_result_int(pCtx,0);` |
|      3 |  2218 | `		return PH7_OK;` |
|      - |  2219 | `	}` |
|     41 |  2220 | `	pNode1 = pOther;` |
|     41 |  2221 | `	pNode2 = pThisNode;` |
|     41 |  2222 | `	if( pNode1->type == XML_ATTRIBUTE_NODE ){` |
|     13 |  2223 | `		pAttr1 = pNode1;` |
|     13 |  2224 | `		pNode1 = pNode1->parent;` |
|      6 |  2225 | `	}` |
|     41 |  2226 | `	if( pNode2->type == XML_ATTRIBUTE_NODE ){` |
|     13 |  2227 | `		pAttr2 = pNode2;` |
|     13 |  2228 | `		pNode2 = pNode2->parent;` |
|     13 |  2229 | `		if( pAttr1 && pNode1 && pNode1 == pNode2 ){` |
|      - |  2230 | `			xmlAttrPtr pAttr;` |
|      5 |  2231 | `			for( pAttr = pNode2->properties ; pAttr ; pAttr = pAttr->next ){` |
|      5 |  2232 | `				if( (xmlNodePtr)pAttr == pAttr1 ){` |
|      3 |  2233 | `					ph7_result_int(pCtx,DOM_POS_IMPL_SPEC\|DOM_POS_PRECEDING);` |
|      3 |  2234 | `					return PH7_OK;` |
|      - |  2235 | `				}` |
|      3 |  2236 | `				if( (xmlNodePtr)pAttr == pAttr2 ){` |
|      3 |  2237 | `					ph7_result_int(pCtx,DOM_POS_IMPL_SPEC\|DOM_POS_FOLLOWING);` |
|      3 |  2238 | `					return PH7_OK;` |
|      - |  2239 | `				}` |
|    ! 0 |  2240 | `			}` |
|    ! 0 |  2241 | `		}` |
|      4 |  2242 | `	}` |
|     37 |  2243 | `	if( pNode1 == 0 \|\| pNode2 == 0 \|\| DomRootOf(pNode1) != DomRootOf(pNode2) ){` |
|     47 |  2244 | `		ph7_result_int(pCtx,DOM_POS_DISCONNECTED\|DOM_POS_IMPL_SPEC` |
|     24 |  2245 | `			\|((sxuptr)pThisNode > (sxuptr)pOther ? DOM_POS_PRECEDING : DOM_POS_FOLLOWING));` |
|     25 |  2246 | `		return PH7_OK;` |
|      - |  2247 | `	}` |
|     36 |  2248 | `	if( (pAttr1 == 0 && DomIsAncestorOf(pNode1,pNode2))` |
|     36 |  2249 | `	 \|\| (pAttr2 != 0 && pNode1 == pNode2) ){` |
|     15 |  2250 | `		ph7_result_int(pCtx,DOM_POS_CONTAINS\|DOM_POS_PRECEDING);` |
|     15 |  2251 | `		return PH7_OK;` |
|      - |  2252 | `	}` |
|     24 |  2253 | `	if( (pAttr2 == 0 && DomIsAncestorOf(pNode2,pNode1))` |
|     23 |  2254 | `	 \|\| (pAttr1 != 0 && pNode1 == pNode2) ){` |
|     13 |  2255 | `		ph7_result_int(pCtx,DOM_POS_CONTAINED_BY\|DOM_POS_FOLLOWING);` |
|     13 |  2256 | `		return PH7_OK;` |
|      - |  2257 | `	}` |
|     13 |  2258 | `	ph7_result_int(pCtx,DomPrecedesInTree(pNode1,pNode2)` |
|      - |  2259 | `		? DOM_POS_PRECEDING : DOM_POS_FOLLOWING);` |
|     13 |  2260 | `	return PH7_OK;` |
|     27 |  2261 | `}` |
|      - |  2262 | `/* DOMNode::contains(DOMNode\|DOMNameSpaceNode\|null $other): bool -- INCLUSIVE` |
|      - |  2263 | ` * descendant, so a node contains itself, and (libxml parenting attributes) an` |
|      - |  2264 | ` * element contains its own attributes. A namespace DECLARATION is asked about` |
|      - |  2265 | ` * through the element that MAKES it: its own pointer is an xmlNs, which is in` |
|      - |  2266 | ` * no tree at all. */` |
|     18 |  2267 | `DOM_METHOD(vm_builtin_DOMNode_contains)` |
|      1 |  2268 | `{` |
|     19 |  2269 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     19 |  2270 | `	ph7_value *pArg = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? apArg[0] : 0;` |
|     19 |  2271 | `	phl_domnode *pOtherNd = pArg ? DomObjArg(pArg) : 0;` |
|     19 |  2272 | `	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     19 |  2273 | `	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;` |
|     19 |  2274 | `	phl_domnode *pOwnerNd = pArg ? DomNsNodeOwner(pArg) : 0;` |
|     19 |  2275 | `	if( pOwnerNd ){` |
|      3 |  2276 | `		pOther = (xmlNodePtr)pOwnerNd->pNode;` |
|      1 |  2277 | `	}` |
|     33 |  2278 | `	ph7_result_bool(pCtx,pThisNode != 0 && pOther != 0` |
|     23 |  2279 | `		&& (pThisNode == pOther \|\| DomIsAncestorOf(pThisNode,pOther)));` |
|     19 |  2280 | `	return PH7_OK;` |
|      1 |  2281 | `}` |
|      - |  2282 | `/* DOMNode::getRootNode(?array $options = null): DOMNode -- php declares the` |
|      - |  2283 | `` * options array (the shadow-DOM `composed` key) and reads nothing from it. */`` |
|     14 |  2284 | `DOM_METHOD(vm_builtin_DOMNode_getRootNode)` |
|      1 |  2285 | `{` |
|     15 |  2286 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  2287 | `	SXUNUSED(nArg);` |
|      7 |  2288 | `	SXUNUSED(apArg);` |
|     15 |  2289 | `	return DomResultNodeOf(pCtx,pNd,DomRootOf(pNd ? (xmlNodePtr)pNd->pNode : 0));` |
|      1 |  2290 | `}` |
|      - |  2291 | `/*` |
|      - |  2292 | ` * DOMNode::isSupported(string $feature, string $version): bool -- the DOM Level` |
|      - |  2293 | `` * 1 feature test, and php's whole table is two rows: `XML` at 1.0 or 2.0 and`` |
|      - |  2294 | `` * `Core` at 1.0 (never `Core` at 2.0). The feature name folds case, the version`` |
|      - |  2295 | ` * does not.` |
|      - |  2296 | ` */` |
|     20 |  2297 | `DOM_METHOD(vm_builtin_DOMNode_isSupported)` |
|      1 |  2298 | `{` |
|     21 |  2299 | `	const char *zFeature = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|     21 |  2300 | `	const char *zVersion = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     21 |  2301 | `	int bOne = DomNameIs(zVersion,"1.0");` |
|     21 |  2302 | `	int bTwo = DomNameIs(zVersion,"2.0");` |
|     21 |  2303 | `	int bXml = SyStrlen(zFeature) == 3 && SyStrnicmp(zFeature,"XML",3) == 0;` |
|     21 |  2304 | `	int bCore = SyStrlen(zFeature) == 4 && SyStrnicmp(zFeature,"Core",4) == 0;` |
|     21 |  2305 | `	ph7_result_bool(pCtx,(bXml && (bOne \|\| bTwo)) \|\| (bCore && bOne));` |
|     21 |  2306 | `	return PH7_OK;` |
|      1 |  2307 | `}` |
|      - |  2308 | `/*` |
|      - |  2309 | ` * php's structural equality, which is NOT the DOM spec's to the letter.` |
|      - |  2310 | ` *` |
|      - |  2311 | ` * The type has to match, then the per-kind identity: an ELEMENT compares its` |
|      - |  2312 | `` * namespace URI, its PREFIX and its local name (so `p:m` and `q:m` bound to the`` |
|      - |  2313 | ` * one URI are NOT equal) plus its attributes as a SET -- same count, and every` |
|      - |  2314 | ` * attribute matched by namespace, local name and value regardless of order. An` |
|      - |  2315 | ` * ATTRIBUTE compares its namespace URI, its name and its value and NOT its` |
|      - |  2316 | ` * prefix, which is the asymmetry no reading of the spec predicts. A PI compares` |
|      - |  2317 | ` * target and data, character data its content, an entity REFERENCE its name.` |
|      - |  2318 | ` * Then the children, in order and in the same number.` |
|      - |  2319 | ` */` |
|    228 |  2320 | `static int DomStrEqOrBothNull(const xmlChar *zA,const xmlChar *zB)` |
|      1 |  2321 | `{` |
|    229 |  2322 | `	if( zA == 0 \|\| zB == 0 ){` |
|     91 |  2323 | `		return zA == zB;` |
|      - |  2324 | `	}` |
|    139 |  2325 | `	return xmlStrEqual(zA,zB) != 0;` |
|    115 |  2326 | `}` |
|    128 |  2327 | `static void DomNsHrefOf(xmlNodePtr pNode,const xmlChar **pzHref,const xmlChar **pzPrefix)` |
|      1 |  2328 | `{` |
|    129 |  2329 | `	*pzHref = (pNode->ns && pNode->ns->href) ? pNode->ns->href : 0;` |
|    129 |  2330 | `	*pzPrefix = (pNode->ns && pNode->ns->prefix) ? pNode->ns->prefix : 0;` |
|    129 |  2331 | `}` |
|     26 |  2332 | `static int DomAttrValueEq(xmlNodePtr pA,xmlNodePtr pB)` |
|      1 |  2333 | `{` |
|     27 |  2334 | `	xmlChar *zA = xmlNodeGetContent(pA);` |
|     27 |  2335 | `	xmlChar *zB = xmlNodeGetContent(pB);` |
|     27 |  2336 | `	int bEq = DomStrEqOrBothNull(zA,zB);` |
|     27 |  2337 | `	if( zA ){ xmlFree(zA); }` |
|     27 |  2338 | `	if( zB ){ xmlFree(zB); }` |
|     27 |  2339 | `	return bEq;` |
|      1 |  2340 | `}` |
|     28 |  2341 | `static int DomAttrSetEqual(xmlNodePtr pA,xmlNodePtr pB)` |
|      1 |  2342 | `{` |
|      - |  2343 | `	xmlAttrPtr pOne,pTwo;` |
|     29 |  2344 | `	int nA = 0,nB = 0;` |
|     63 |  2345 | `	for( pOne = pA->properties ; pOne ; pOne = pOne->next ){ nA++; }` |
|     57 |  2346 | `	for( pTwo = pB->properties ; pTwo ; pTwo = pTwo->next ){ nB++; }` |
|     29 |  2347 | `	if( nA != nB ){` |
|      5 |  2348 | `		return 0;` |
|      - |  2349 | `	}` |
|     47 |  2350 | `	for( pOne = pA->properties ; pOne ; pOne = pOne->next ){` |
|      - |  2351 | `		const xmlChar *zHrefA,*zPfxA,*zHrefB,*zPfxB;` |
|     27 |  2352 | `		DomNsHrefOf((xmlNodePtr)pOne,&zHrefA,&zPfxA);` |
|     35 |  2353 | `		for( pTwo = pB->properties ; pTwo ; pTwo = pTwo->next ){` |
|     31 |  2354 | `			DomNsHrefOf((xmlNodePtr)pTwo,&zHrefB,&zPfxB);` |
|     30 |  2355 | `			if( DomStrEqOrBothNull(zHrefA,zHrefB)` |
|     29 |  2356 | `			 && DomStrEqOrBothNull(pOne->name,pTwo->name)` |
|     27 |  2357 | `			 && DomAttrValueEq((xmlNodePtr)pOne,(xmlNodePtr)pTwo) ){` |
|     23 |  2358 | `				break;` |
|      - |  2359 | `			}` |
|      5 |  2360 | `		}` |
|     27 |  2361 | `		if( pTwo == 0 ){` |
|      5 |  2362 | `			return 0;` |
|      - |  2363 | `		}` |
|     12 |  2364 | `	}` |
|     21 |  2365 | `	return 1;` |
|     15 |  2366 | `}` |
|     86 |  2367 | `static int DomNodesEqual(xmlNodePtr pA,xmlNodePtr pB)` |
|      1 |  2368 | `{` |
|      - |  2369 | `	xmlNodePtr pKidA,pKidB;` |
|      - |  2370 | `	const xmlChar *zHrefA,*zPfxA,*zHrefB,*zPfxB;` |
|     87 |  2371 | `	if( pA == 0 \|\| pB == 0 ){` |
|    ! 0 |  2372 | `		return pA == pB;` |
|      - |  2373 | `	}` |
|     87 |  2374 | `	if( pA->type != pB->type ){` |
|      3 |  2375 | `		return 0;` |
|      - |  2376 | `	}` |
|     85 |  2377 | `	switch( pA->type ){` |
|     17 |  2378 | `	case XML_ELEMENT_NODE:` |
|     35 |  2379 | `		DomNsHrefOf(pA,&zHrefA,&zPfxA);` |
|     35 |  2380 | `		DomNsHrefOf(pB,&zHrefB,&zPfxB);` |
|     34 |  2381 | `		if( !DomStrEqOrBothNull(zHrefA,zHrefB) \|\| !DomStrEqOrBothNull(zPfxA,zPfxB)` |
|     33 |  2382 | `		 \|\| !DomStrEqOrBothNull(pA->name,pB->name) \|\| !DomAttrSetEqual(pA,pB) ){` |
|     15 |  2383 | `			return 0;` |
|      - |  2384 | `		}` |
|     21 |  2385 | `		break;` |
|      1 |  2386 | `	case XML_ATTRIBUTE_NODE:` |
|      3 |  2387 | `		DomNsHrefOf(pA,&zHrefA,&zPfxA);` |
|      3 |  2388 | `		DomNsHrefOf(pB,&zHrefB,&zPfxB);` |
|      2 |  2389 | `		if( !DomStrEqOrBothNull(zHrefA,zHrefB)` |
|      2 |  2390 | `		 \|\| !DomStrEqOrBothNull(pA->name,pB->name)` |
|      3 |  2391 | `		 \|\| !DomAttrValueEq(pA,pB) ){` |
|    ! 0 |  2392 | `			return 0;` |
|      - |  2393 | `		}` |
|      - |  2394 | `		/* An attribute's value IS its child list; comparing it twice would only` |
|      - |  2395 | `		 * refuse a value split across nodes that reads the same. */` |
|      3 |  2396 | `		return 1;` |
|      4 |  2397 | `	case XML_PI_NODE:` |
|      8 |  2398 | `		if( !DomStrEqOrBothNull(pA->name,pB->name)` |
|      8 |  2399 | `		 \|\| !DomStrEqOrBothNull(pA->content,pB->content) ){` |
|      5 |  2400 | `			return 0;` |
|      - |  2401 | `		}` |
|      5 |  2402 | `		break;` |
|     12 |  2403 | `	case XML_TEXT_NODE:` |
|      - |  2404 | `	case XML_CDATA_SECTION_NODE:` |
|      - |  2405 | `	case XML_COMMENT_NODE:` |
|     25 |  2406 | `		if( !DomStrEqOrBothNull(pA->content,pB->content) ){` |
|      3 |  2407 | `			return 0;` |
|      - |  2408 | `		}` |
|     23 |  2409 | `		break;` |
|      2 |  2410 | `	case XML_ENTITY_REF_NODE:` |
|      - |  2411 | `		/* The reference's NAME is what a program wrote; its children are the` |
|      - |  2412 | `		 * DECLARATION libxml resolved it to, which is not part of the node. */` |
|      5 |  2413 | `		return DomStrEqOrBothNull(pA->name,pB->name);` |
|      6 |  2414 | `	default:` |
|     12 |  2415 | `		break;` |
|      - |  2416 | `	}` |
|     59 |  2417 | `	pKidA = pA->children;` |
|     59 |  2418 | `	pKidB = pB->children;` |
|     91 |  2419 | `	while( pKidA && pKidB ){` |
|     35 |  2420 | `		if( !DomNodesEqual(pKidA,pKidB) ){` |
|      3 |  2421 | `			return 0;` |
|      - |  2422 | `		}` |
|     33 |  2423 | `		pKidA = pKidA->next;` |
|     33 |  2424 | `		pKidB = pKidB->next;` |
|      1 |  2425 | `	}` |
|     57 |  2426 | `	return pKidA == 0 && pKidB == 0;` |
|     44 |  2427 | `}` |
|      - |  2428 | `/* DOMNode::isEqualNode(?DOMNode $otherNode): bool */` |
|     54 |  2429 | `DOM_METHOD(vm_builtin_DOMNode_isEqualNode)` |
|      1 |  2430 | `{` |
|     55 |  2431 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     55 |  2432 | `	phl_domnode *pOtherNd = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|    107 |  2433 | `	ph7_result_bool(pCtx,pNd != 0 && pOtherNd != 0` |
|     53 |  2434 | `		&& DomNodesEqual((xmlNodePtr)pNd->pNode,(xmlNodePtr)pOtherNd->pNode));` |
|     55 |  2435 | `	return PH7_OK;` |
|      1 |  2436 | `}` |
|      - |  2437 |  |
|      - |  2438 | `/* ===== Copying: cloneNode ===== */` |
|      - |  2439 |  |
|      - |  2440 | `/*` |
|      - |  2441 | ` * One node copied the way php copies it.` |
|      - |  2442 | ` *` |
|      - |  2443 | ` * libxml's generic copier has no case for a DTD node and answers NULL there, so` |
|      - |  2444 | `` * `$doc->doctype->cloneNode()` was `false` -- php reaches for xmlCopyDtd`` |
|      - |  2445 | ` * instead, which carries the whole internal subset (its declarations, entities` |
|      - |  2446 | ``  * and notations) across. The copy keeps the SOURCE's document in its `doc` `` |
|      - |  2447 | ` * slot without being linked into it, which is what makes php's cloned doctype` |
|      - |  2448 | `` * still answer an `internalSubset` while its `parentNode` is null.`` |
|      - |  2449 | ` */` |
|     50 |  2450 | `static xmlNodePtr DomCopyNode(xmlNodePtr pNode,xmlDocPtr pDoc,int iExtended)` |
|      1 |  2451 | `{` |
|      - |  2452 | `	xmlNodePtr pCopy;` |
|     51 |  2453 | `	if( pNode->type == XML_DTD_NODE \|\| pNode->type == XML_DOCUMENT_TYPE_NODE ){` |
|      5 |  2454 | `		pCopy = (xmlNodePtr)xmlCopyDtd((xmlDtdPtr)pNode);` |
|      5 |  2455 | `		if( pCopy ){` |
|      5 |  2456 | `			pCopy->doc = pNode->doc;` |
|      2 |  2457 | `		}` |
|      5 |  2458 | `		return pCopy;` |
|      - |  2459 | `	}` |
|     47 |  2460 | `	return xmlDocCopyNode(pNode,pDoc,iExtended);` |
|     26 |  2461 | `}` |
|      - |  2462 |  |
|      - |  2463 | `/*` |
|      - |  2464 | ` * Cloning a DOCUMENT is not cloning a node: php builds a SECOND document --` |
|      - |  2465 | ` * its own tree, its own wrapper, its own identity cache -- so the copy's` |
|      - |  2466 | `` * `documentElement` answers the copy as its `ownerDocument` and appending a`` |
|      - |  2467 | ` * node of the ORIGINAL into it is the Wrong Document Error it would be between` |
|      - |  2468 | ` * any two documents. Everything else is one xmlDocCopyNode into the SAME tree,` |
|      - |  2469 | ` * parked as an orphan like every other node this file creates.` |
|      - |  2470 | ` *` |
|      - |  2471 | ` * The parser directives ride along: php's copy answers the receiver's whole` |
|      - |  2472 | ` * flag block, not the class defaults.` |
|      - |  2473 | ` */` |
|     12 |  2474 | `static int DomCloneDocument(ph7_context *pCtx,phl_domnode *pNd,int bDeep)` |
|      1 |  2475 | `{` |
|     13 |  2476 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  2477 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  2478 | `	ph7_class *pClass;` |
|      - |  2479 | `	ph7_class_instance *pObj;` |
|      - |  2480 | `	phl_xmldoc *pShell;` |
|      - |  2481 | `	phl_domnode *pRes;` |
|      - |  2482 | `	xmlDocPtr pCopy;` |
|     13 |  2483 | `	sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     13 |  2484 | `	pCopy = xmlCopyDoc((xmlDocPtr)pNd->pNode,bDeep ? 1 : 0);` |
|     13 |  2485 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::cloneNode");` |
|     13 |  2486 | `	if( pCopy == 0 ){` |
|    ! 0 |  2487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  2488 | `		return PH7_OK;` |
|      - |  2489 | `	}` |
|      - |  2490 | `	/* php answers a plain DOMDocument even when the receiver is a subclass of` |
|      - |  2491 | ``	 * one: the copy is built by the extension, not by `new static`. */`` |
|     13 |  2492 | `	pClass = PH7_VmExtractClass(pVm,"DOMDocument",sizeof("DOMDocument")-1,FALSE,0);` |
|     13 |  2493 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|     13 |  2494 | `	pShell = pObj ? PH7_LibxmlNewDoc(pVm,pCopy) : 0;` |
|     13 |  2495 | `	pRes = pShell ? DomNewRes(pVm,pShell,pCopy) : 0;` |
|     13 |  2496 | `	if( pRes == 0 ){` |
|    ! 0 |  2497 | `		if( pShell == 0 ){` |
|    ! 0 |  2498 | `			xmlFreeDoc(pCopy);   /* not registered: nothing else will free it */` |
|    ! 0 |  2499 | `		}` |
|    ! 0 |  2500 | `		if( pObj ){` |
|    ! 0 |  2501 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  2502 | `		}` |
|    ! 0 |  2503 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2504 | `	}` |
|     13 |  2505 | `	DomSetRes(pVm,pObj,pRes);` |
|     13 |  2506 | `	PH7_NativeSetAttrObj(pVm,pObj,DOM_DOC,pObj);` |
|     13 |  2507 | `	if( pThis ){` |
|      - |  2508 | `		ph7_value *pFrom,*pTo;` |
|      - |  2509 | `		/* The seven directives travel as the one word they are stored in. */` |
|     13 |  2510 | `		PH7_NativeSetAttrInt(pVm,pObj,DOM_DFLAGS,PH7_NativeAttrInt(pThis,DOM_DFLAGS));` |
|      - |  2511 | `		/* ...and the registerNodeClass table, which php's copy answers too --` |
|      - |  2512 | `		 * shared copy-on-write, which the map's own writer separates. */` |
|     13 |  2513 | `		pFrom = PH7_NativeAttr(pThis,DOM_NCLS);` |
|     13 |  2514 | `		pTo = PH7_NativeAttr(pObj,DOM_NCLS);` |
|     13 |  2515 | `		if( pFrom && pTo && (pFrom->iFlags & MEMOBJ_HASHMAP) ){` |
|      3 |  2516 | `			PH7_MemObjStore(pFrom,pTo);` |
|      1 |  2517 | `		}` |
|      6 |  2518 | `	}` |
|     13 |  2519 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     13 |  2520 | `	return PH7_OK;` |
|      7 |  2521 | `}` |
|      - |  2522 | `/*` |
|      - |  2523 | ` * DOMNode::cloneNode(bool $deep = false): DOMNode\|false` |
|      - |  2524 | ` *` |
|      - |  2525 | `` * The SHALLOW copy is not libxml's shallow copy: php asks for `extended = 2`,`` |
|      - |  2526 | `` * which carries an element's attributes and its own `xmlns` declarations across`` |
|      - |  2527 | `` * while leaving the children behind -- so `$el->cloneNode()` is a usable`` |
|      - |  2528 | `` * template row, not a bare tag. A deep one is `extended = 1`, and libxml then`` |
|      - |  2529 | ` * reconciles whatever namespace the descendants were using onto the copy.` |
|      - |  2530 | ` */` |
|     44 |  2531 | `DOM_METHOD(vm_builtin_DOMNode_cloneNode)` |
|      1 |  2532 | `{` |
|     45 |  2533 | `	ph7_vm *pVm = pCtx->pVm;` |
|     45 |  2534 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     45 |  2535 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     45 |  2536 | `	int bDeep = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|      - |  2537 | `	xmlNodePtr pCopy;` |
|      - |  2538 | `	sxu32 nMark;` |
|     45 |  2539 | `	if( pNode == 0 ){` |
|    ! 0 |  2540 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  2541 | `		return PH7_OK;` |
|      - |  2542 | `	}` |
|     45 |  2543 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|     13 |  2544 | `		return DomCloneDocument(pCtx,pNd,bDeep);` |
|      - |  2545 | `	}` |
|     33 |  2546 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     33 |  2547 | `	pCopy = DomCopyNode(pNode,pNode->doc,bDeep ? 1 : 2);` |
|     33 |  2548 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::cloneNode");` |
|     33 |  2549 | `	if( pCopy == 0 ){` |
|      5 |  2550 | `		ph7_result_bool(pCtx,0);` |
|      5 |  2551 | `		return PH7_OK;` |
|      - |  2552 | `	}` |
|      - |  2553 | `	/* An ATTRIBUTE copy comes back in NO namespace: libxml resolves an` |
|      - |  2554 | `	 * attribute's prefix against the element it is being copied ONTO, and there` |
|      - |  2555 | ``	 * is no element here. php's clone keeps the namespace, so `p:at="1"` cloned`` |
|      - |  2556 | ``	 * stays `p:at="1"` rather than turning into `at="1"` -- a silent rename of`` |
|      - |  2557 | `	 * the very attribute a namespaced document is keyed on. The copy borrows the` |
|      - |  2558 | `	 * SOURCE's declaration, which is the only thing it can do: an attribute` |
|      - |  2559 | ``	 * carries no `nsDef` of its own, and the declaration outlives it (nothing in`` |
|      - |  2560 | `	 * this file frees a node before its document). */` |
|     29 |  2561 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pCopy->ns == 0 ){` |
|      3 |  2562 | `		pCopy->ns = pNode->ns;` |
|      1 |  2563 | `	}` |
|     29 |  2564 | `	DomOrphanAdd(pNd->pShell,pCopy);` |
|     29 |  2565 | `	return DomResultNodeOf(pCtx,pNd,pCopy);` |
|     23 |  2566 | `}` |
|      - |  2567 | `/* Enter one wrapper into a holder's identity cache, keyed by the node pointer.` |
|      - |  2568 | ` * BORROWED, like every entry: the object's own release takes it back out. */` |
|    254 |  2569 | `static void DomCacheStore(ph7_vm *pVm,ph7_class_instance *pDoc,xmlNodePtr pNode,` |
|      - |  2570 | `	ph7_class_instance *pObj)` |
|      1 |  2571 | `{` |
|    255 |  2572 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|      - |  2573 | `	ph7_value sKey,sVal;` |
|    255 |  2574 | `	if( pCache == 0 ){` |
|    ! 0 |  2575 | `		return;` |
|      - |  2576 | `	}` |
|    255 |  2577 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|    255 |  2578 | `	PH7_MemObjInitFromInt(&(*pVm),&sVal,(sxi64)(sxuptr)pObj);` |
|    255 |  2579 | `	PH7_HashmapInsert(pCache,&sKey,&sVal);` |
|    255 |  2580 | `	PH7_MemObjRelease(&sKey);` |
|    255 |  2581 | `	DomNodeMarkHeld(pNode,pObj);` |
|    128 |  2582 | `}` |
|      - |  2583 | `/* Empty a slot the instance copied from its clone source: the null value. */` |
|     12 |  2584 | `static void DomSetSlotNull(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxu32 nName)` |
|      1 |  2585 | `{` |
|      - |  2586 | `	ph7_value sNull;` |
|     13 |  2587 | `	PH7_MemObjInit(&(*pVm),&sNull);` |
|     13 |  2588 | `	PH7_NativeSetProp(&(*pVm),pObj,zName,nName,&sNull);` |
|     13 |  2589 | `}` |
|      - |  2590 | `/*` |
|      - |  2591 | `` * `clone $node` / `clone $doc` -- ph7_class::xClone for the DOM classes.`` |
|      - |  2592 | ` *` |
|      - |  2593 | ` * php's clone_obj handler copies the NODE, so the clone is a second SUBTREE and` |
|      - |  2594 | ` * not a second object over the same one.  The slot-by-slot copy that runs` |
|      - |  2595 | ` * before this hook duplicated $__res, and stopping there is the XMLWriter clone` |
|      - |  2596 | ` * bug one family later: a write through either object shows through both.` |
|      - |  2597 | ` *` |
|      - |  2598 | `` * php's rules, measured: the copy is always DEEP (`clone $el` carries the whole`` |
|      - |  2599 | ` * subtree where cloneNode() defaults shallow), always DETACHED, and stays in` |
|      - |  2600 | `` * the SAME document -- `$c->ownerDocument === $d` -- while a DOCUMENT is copied`` |
|      - |  2601 | ` * whole into a second document, directives, declaration and URI included, so` |
|      - |  2602 | ` * mutating the copy's tree leaves the original's bytes alone.  A user subclass` |
|      - |  2603 | ` * clones through the inherited hook and keeps its class and its own properties,` |
|      - |  2604 | ` * php's handler inheritance (the ENGINE's chain walk serves that).` |
|      - |  2605 | ` */` |
|     18 |  2606 | `static void DomInstanceClone(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)` |
|      1 |  2607 | `{` |
|     19 |  2608 | `	phl_domnode *pNd = DomResOf(pSrc);` |
|     19 |  2609 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     19 |  2610 | `	ph7_class_instance *pDoc = PH7_NativeAttrObj(pClone,DOM_DOC);` |
|      - |  2611 | `	xmlNodePtr pCopy;` |
|      - |  2612 | `	phl_domnode *pRes;` |
|     19 |  2613 | `	if( pNode == 0 ){` |
|    ! 0 |  2614 | `		return;   /* no node behind the source: the copy has none either */` |
|      - |  2615 | `	}` |
|     19 |  2616 | `	pCopy = DomCopyNode(pNode,pNode->doc,1);` |
|     19 |  2617 | `	pRes = pCopy ? DomNewRes(&(*pVm),pNd->pShell,pCopy) : 0;` |
|     19 |  2618 | `	if( pRes == 0 ){` |
|      - |  2619 | `		/* Never leave the slot-copied handle in place: two objects over one` |
|      - |  2620 | `		 * node is the exact aliasing this hook exists to prevent. */` |
|      3 |  2621 | `		if( pCopy ){` |
|    ! 0 |  2622 | `			xmlFreeNode(pCopy);` |
|    ! 0 |  2623 | `		}` |
|      3 |  2624 | `		DomSetSlotNull(&(*pVm),pClone,DOM_RES,sizeof(DOM_RES)-1);` |
|      3 |  2625 | `		return;` |
|      - |  2626 | `	}` |
|      - |  2627 | `	/* The same namespace borrow cloneNode() does: an attribute copied with no` |
|      - |  2628 | `	 * element to resolve against comes back in NO namespace. */` |
|     17 |  2629 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pCopy->ns == 0 ){` |
|      3 |  2630 | `		pCopy->ns = pNode->ns;` |
|      1 |  2631 | `	}` |
|     17 |  2632 | `	DomOrphanAdd(pNd->pShell,pCopy);` |
|     17 |  2633 | `	DomSetRes(&(*pVm),pClone,pRes);` |
|      - |  2634 | `	/* The clone IS the copy's wrapper: enter it into the identity cache so` |
|      - |  2635 | ``	 * `$c->firstChild->parentNode === $c` holds. ($__doc rode the slot copy.) */`` |
|     17 |  2636 | `	DomCacheStore(&(*pVm),pDoc,pCopy,pClone);` |
|     10 |  2637 | `}` |
|     10 |  2638 | `static void DomInstanceCloneDoc(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)` |
|      1 |  2639 | `{` |
|     11 |  2640 | `	phl_domnode *pNd = DomResOf(pSrc);` |
|      - |  2641 | `	xmlDocPtr pCopy;` |
|      - |  2642 | `	phl_xmldoc *pShell;` |
|      - |  2643 | `	phl_domnode *pRes;` |
|     11 |  2644 | `	if( pNd == 0 \|\| pNd->pNode == 0 ){` |
|    ! 0 |  2645 | `		return;` |
|      - |  2646 | `	}` |
|     11 |  2647 | `	pCopy = xmlCopyDoc((xmlDocPtr)pNd->pNode,1);` |
|     11 |  2648 | `	pShell = pCopy ? PH7_LibxmlNewDoc(&(*pVm),pCopy) : 0;` |
|     11 |  2649 | `	pRes = pShell ? DomNewRes(&(*pVm),pShell,pCopy) : 0;` |
|     11 |  2650 | `	if( pRes == 0 ){` |
|    ! 0 |  2651 | `		if( pCopy && pShell == 0 ){` |
|    ! 0 |  2652 | `			xmlFreeDoc(pCopy);   /* not registered: nothing else will free it */` |
|    ! 0 |  2653 | `		}` |
|    ! 0 |  2654 | `		DomSetSlotNull(&(*pVm),pClone,DOM_RES,sizeof(DOM_RES)-1);` |
|    ! 0 |  2655 | `		return;` |
|      - |  2656 | `	}` |
|     11 |  2657 | `	DomSetRes(&(*pVm),pClone,pRes);` |
|      - |  2658 | `	/* Its own document, its own identity cache: the slot copy pointed both at` |
|      - |  2659 | `	 * the SOURCE's, so the copy's documentElement would have answered the` |
|      - |  2660 | `	 * original document as its owner. (The directive slots the copy carried` |
|      - |  2661 | `	 * across are php's answer and stay.) */` |
|     11 |  2662 | `	PH7_NativeSetAttrObj(&(*pVm),pClone,DOM_DOC,pClone);` |
|     11 |  2663 | `	DomSetSlotNull(&(*pVm),pClone,DOM_NODES,sizeof(DOM_NODES)-1);` |
|      6 |  2664 | `}` |
|      - |  2665 |  |
|      - |  2666 | `/* ===== The node CONSTRUCTORS: php's ownerless nodes ===== */` |
|      - |  2667 |  |
|      - |  2668 | `/*` |
|      - |  2669 | `` * php gives a constructed node NO document at all -- `(new DOMText('t'))->`` |
|      - |  2670 | `` * ownerDocument` is null and the libxml node's doc is NULL -- and adopts it on`` |
|      - |  2671 | ` * the first insertion.  Until then the node has to be OWNED by something that` |
|      - |  2672 | ` * frees it: the limbo shell, one per VM, a phl_xmldoc with no xmlDoc whose` |
|      - |  2673 | ` * orphan set carries every constructed-and-never-adopted node to teardown.` |
|      - |  2674 | ` */` |
|    238 |  2675 | `static phl_xmldoc * DomLimboShell(ph7_vm *pVm)` |
|      1 |  2676 | `{` |
|    239 |  2677 | `	if( pVm->pXmlLimbo == 0 ){` |
|      3 |  2678 | `		pVm->pXmlLimbo = PH7_LibxmlNewDoc(&(*pVm),0);` |
|      1 |  2679 | `	}` |
|    239 |  2680 | `	return (phl_xmldoc *)pVm->pXmlLimbo;` |
|      1 |  2681 | `}` |
|      - |  2682 | `/*` |
|      - |  2683 | ` * The shared constructor tail: park the fresh node on the limbo shell, wire` |
|      - |  2684 | ` * the instance's two slots, and make the instance its OWN holder -- $__doc` |
|      - |  2685 | ` * points at itself and the identity cache lives on it, exactly the document's` |
|      - |  2686 | `` * own arrangement, so `$e->firstChild->parentNode === $e` holds for a tree`` |
|      - |  2687 | ` * that belongs to no document.  (ownerDocument still answers null: the getter` |
|      - |  2688 | ` * reads the NODE's document, not the slot.)  Takes ownership of pNode either` |
|      - |  2689 | `` * way; a re-run constructor -- `$t->__construct('b')`, which php allows --`` |
|      - |  2690 | ` * simply re-points the slots and leaves the old node parked.` |
|      - |  2691 | ` */` |
|    218 |  2692 | `static int DomCtorInstall(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |  2693 | `{` |
|    219 |  2694 | `	ph7_vm *pVm = pCtx->pVm;` |
|    219 |  2695 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  2696 | `	phl_xmldoc *pShell;` |
|      - |  2697 | `	phl_domnode *pRes;` |
|    219 |  2698 | `	if( pNode == 0 ){` |
|    ! 0 |  2699 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2700 | `	}` |
|    219 |  2701 | `	pShell = pThis ? DomLimboShell(pVm) : 0;` |
|    219 |  2702 | `	pRes = pShell ? DomNewRes(pVm,pShell,pNode) : 0;` |
|    219 |  2703 | `	if( pRes == 0 ){` |
|    ! 0 |  2704 | `		xmlFreeNode(pNode);` |
|    ! 0 |  2705 | `		return pThis ? PH7_ContextMemoryError(pCtx) : PH7_OK;` |
|      - |  2706 | `	}` |
|    219 |  2707 | `	DomOrphanAdd(pShell,pNode);` |
|    219 |  2708 | `	DomSetRes(pVm,pThis,pRes);` |
|    219 |  2709 | `	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);` |
|    219 |  2710 | `	DomCacheStore(pVm,pThis,pNode,pThis);` |
|    219 |  2711 | `	return PH7_OK;` |
|    110 |  2712 | `}` |
|      - |  2713 | `/* The content of the character-data three: php passes NULL for an OMITTED` |
|      - |  2714 | ` * argument and the string -- even the empty one -- for a given one, which is` |
|      - |  2715 | `` * why `new DOMText()` has a NULL nodeValue where `new DOMText('')` reads "". */`` |
|     66 |  2716 | `DOM_METHOD(vm_builtin_DOMText_construct)` |
|      1 |  2717 | `{` |
|     67 |  2718 | `	int nData = 0;` |
|     67 |  2719 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : 0;` |
|     67 |  2720 | `	xmlNodePtr pNode = zData` |
|     56 |  2721 | `		? xmlNewDocTextLen(0,(const xmlChar *)zData,nData)` |
|     38 |  2722 | `		: xmlNewDocText(0,0);` |
|     67 |  2723 | `	return DomCtorInstall(pCtx,pNode);` |
|      1 |  2724 | `}` |
|     16 |  2725 | `DOM_METHOD(vm_builtin_DOMComment_construct)` |
|      1 |  2726 | `{` |
|     17 |  2727 | `	int nData = 0;` |
|     17 |  2728 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : 0;` |
|      - |  2729 | `	xmlNodePtr pNode;` |
|     17 |  2730 | `	if( zData ){` |
|      - |  2731 | `		/* libxml has no length-taking comment constructor and` |
|      - |  2732 | `		 * xmlNewDocComment measures with strlen, so a NUL-carrying PHP string` |
|      - |  2733 | `		 * goes through a bounded copy. */` |
|     13 |  2734 | `		xmlChar *zCopy = xmlStrndup((const xmlChar *)zData,nData);` |
|     13 |  2735 | `		pNode = zCopy ? xmlNewDocComment(0,zCopy) : 0;` |
|     13 |  2736 | `		if( zCopy ){` |
|     13 |  2737 | `			xmlFree(zCopy);` |
|      6 |  2738 | `		}` |
|      7 |  2739 | `	}else{` |
|      5 |  2740 | `		pNode = xmlNewDocComment(0,0);` |
|      - |  2741 | `	}` |
|     17 |  2742 | `	return DomCtorInstall(pCtx,pNode);` |
|      1 |  2743 | `}` |
|     10 |  2744 | `DOM_METHOD(vm_builtin_DOMCdataSection_construct)` |
|      1 |  2745 | `{` |
|     11 |  2746 | `	int nData = 0;` |
|     11 |  2747 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : "";` |
|     16 |  2748 | `	return DomCtorInstall(pCtx,` |
|      5 |  2749 | `		xmlNewCDataBlock(0,(const xmlChar *)zData,nData));` |
|      1 |  2750 | `}` |
|      - |  2751 | `/*` |
|      - |  2752 | ` * DOMElement::__construct(string $qualifiedName, ?string $value = null,` |
|      - |  2753 | ` *                         string $namespace = '')` |
|      - |  2754 | ` *` |
|      - |  2755 | ` * The constructor's name grammar is its OWN, not createElementNS's, each cell` |
|      - |  2756 | `` * measured: the whole name must be an XML Name first (so `1:a` is Invalid`` |
|      - |  2757 | ` * Character where createElementNS answers Namespace), a prefix without a` |
|      - |  2758 | ` * namespace is the Namespace refusal, and WITH one the name must be a QName` |
|      - |  2759 | `` * whose prefix is neither `xml` nor `xmlns` -- php refuses `xml:a` here even`` |
|      - |  2760 | ` * against the xml namespace's own URI, where createElementNS allows it. A` |
|      - |  2761 | `` * plain `xmlns` passes as an ordinary name and binds the DEFAULT namespace.`` |
|      - |  2762 | ` *` |
|      - |  2763 | ``  * The $value rides libxml's entity parser, createElement's own quirk: `&amp;` `` |
|      - |  2764 | `` * becomes `&`, and an unterminated reference warns (under this constructor's`` |
|      - |  2765 | ` * name) and drops the whole value. An attribute's value -- the constructor` |
|      - |  2766 | `` * below -- is LITERAL instead: `&amp;` stays five characters.`` |
|      - |  2767 | ` */` |
|     96 |  2768 | `DOM_METHOD(vm_builtin_DOMElement_construct)` |
|      1 |  2769 | `{` |
|     97 |  2770 | `	ph7_vm *pVm = pCtx->pVm;` |
|     97 |  2771 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     97 |  2772 | `	int nVal = 0;` |
|     68 |  2773 | `	const char *zVal = (nArg > 1 && !ph7_value_is_null(apArg[1]))` |
|     73 |  2774 | `		? ph7_value_to_string(apArg[1],&nVal) : 0;` |
|     97 |  2775 | `	const char *zUri = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|     97 |  2776 | `	int bHasUri = zUri[0] != 0;` |
|     97 |  2777 | `	xmlChar *zPrefix = 0;` |
|      - |  2778 | `	xmlChar *zLocal;` |
|      - |  2779 | `	xmlNodePtr pNode;` |
|      - |  2780 | `	sxu32 nMark;` |
|     97 |  2781 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     13 |  2782 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  2783 | `	}` |
|      - |  2784 | `	/* The split is BY HAND, at the first colon, with a leading colon meaning` |
|      - |  2785 | `	 * no prefix at all: libxml's xmlSplitQName2 changed its answer for a name` |
|      - |  2786 | `	 * that ENDS in the colon between 2.9 and 2.13 (the Windows gate caught` |
|      - |  2787 | ``	 * `new DOMElement('a:')` constructing there), and the grammar must answer`` |
|      - |  2788 | `	 * the same on every platform. */` |
|      - |  2789 | `	{` |
|     85 |  2790 | `		const xmlChar *zColon = xmlStrchr((const xmlChar *)zName,':');` |
|     85 |  2791 | `		if( zColon && zColon != (const xmlChar *)zName ){` |
|     43 |  2792 | `			zPrefix = xmlStrndup((const xmlChar *)zName,` |
|     28 |  2793 | `				(int)(zColon - (const xmlChar *)zName));` |
|     29 |  2794 | `			zLocal = xmlStrdup(zColon + 1);` |
|     15 |  2795 | `		}else{` |
|     57 |  2796 | `			zLocal = 0;` |
|      - |  2797 | `		}` |
|      - |  2798 | `	}` |
|     85 |  2799 | `	if( !bHasUri ){` |
|     61 |  2800 | `		if( zPrefix ){` |
|      - |  2801 | `			/* A prefix names a namespace, and none came. */` |
|     13 |  2802 | `			xmlFree(zPrefix);` |
|     13 |  2803 | `			xmlFree(zLocal);` |
|     13 |  2804 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  2805 | `		}` |
|     49 |  2806 | `		if( zLocal ){` |
|    ! 0 |  2807 | `			xmlFree(zLocal);` |
|    ! 0 |  2808 | `		}` |
|     49 |  2809 | ``		zLocal = 0;   /* the whole name, `:a` included */`` |
|     25 |  2810 | `	}else{` |
|     24 |  2811 | `		if( xmlValidateQName((const xmlChar *)zName,0) != 0` |
|     21 |  2812 | `		 \|\| (zPrefix && (xmlStrEqual(zPrefix,(const xmlChar *)"xml")` |
|      9 |  2813 | `		              \|\| xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){` |
|     13 |  2814 | `			if( zPrefix ){` |
|     11 |  2815 | `				xmlFree(zPrefix);` |
|      5 |  2816 | `			}` |
|     13 |  2817 | `			if( zLocal ){` |
|     11 |  2818 | `				xmlFree(zLocal);` |
|      5 |  2819 | `			}` |
|     13 |  2820 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  2821 | `		}` |
|      - |  2822 | `	}` |
|     61 |  2823 | `	pNode = xmlNewNode(0,zLocal ? zLocal : (const xmlChar *)zName);` |
|     61 |  2824 | `	if( zLocal ){` |
|      7 |  2825 | `		xmlFree(zLocal);` |
|      3 |  2826 | `	}` |
|     61 |  2827 | `	if( pNode == 0 ){` |
|    ! 0 |  2828 | `		if( zPrefix ){` |
|    ! 0 |  2829 | `			xmlFree(zPrefix);` |
|    ! 0 |  2830 | `		}` |
|    ! 0 |  2831 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2832 | `	}` |
|     61 |  2833 | `	if( bHasUri ){` |
|      - |  2834 | `		/* On the node's OWN nsDef, so the declaration serializes here once an` |
|      - |  2835 | `		 * insertion adopts the element and lookupNamespaceURI answers it` |
|      - |  2836 | `		 * meanwhile; the reconcile strips it wherever an ancestor already` |
|      - |  2837 | `		 * declares the binding. */` |
|     13 |  2838 | `		xmlNsPtr pNs = xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);` |
|     13 |  2839 | `		if( pNs ){` |
|     13 |  2840 | `			xmlSetNs(pNode,pNs);` |
|      6 |  2841 | `		}` |
|      6 |  2842 | `	}` |
|     61 |  2843 | `	if( zPrefix ){` |
|      7 |  2844 | `		xmlFree(zPrefix);` |
|      3 |  2845 | `	}` |
|     61 |  2846 | `	if( zVal && nVal > 0 ){` |
|      - |  2847 | `` 		/* The EMPTY value is skipped whole -- php's `new DOMElement('a','')` `` |
|      - |  2848 | `		 * has no text child at all, where libxml's setter would leave one. */` |
|     11 |  2849 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     11 |  2850 | `		xmlNodeSetContentLen(pNode,(const xmlChar *)zVal,nVal);` |
|     11 |  2851 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMElement::__construct");` |
|      5 |  2852 | `	}` |
|     61 |  2853 | `	return DomCtorInstall(pCtx,pNode);` |
|     49 |  2854 | `}` |
|      - |  2855 | `/*` |
|      - |  2856 | ` * The last three: a FRAGMENT takes nothing at all; a PROCESSING INSTRUCTION` |
|      - |  2857 | `` * validates its target as a plain XML Name (`xml`, `XML` and `p:a` all pass --`` |
|      - |  2858 | ` * php never asks whether the target is reserved) and stores its data` |
|      - |  2859 | ` * literally, NULL when omitted like the character-data three; an ENTITY` |
|      - |  2860 | ` * REFERENCE validates its name and takes libxml's answer for the content: a` |
|      - |  2861 | `` * PREDEFINED name (`amp`) arrives with the shared entity declaration as its`` |
|      - |  2862 | ` * child -- a STATIC libxml global, wrapped but never owned, which is why the` |
|      - |  2863 | ` * constructor parks only the reference node itself on the limbo shell.` |
|      - |  2864 | ` */` |
|     14 |  2865 | `DOM_METHOD(vm_builtin_DOMDocumentFragment_construct)` |
|      1 |  2866 | `{` |
|      7 |  2867 | `	SXUNUSED(nArg);` |
|      7 |  2868 | `	SXUNUSED(apArg);` |
|     15 |  2869 | `	return DomCtorInstall(pCtx,xmlNewDocFragment(0));` |
|      1 |  2870 | `}` |
|     22 |  2871 | `DOM_METHOD(vm_builtin_DOMProcessingInstruction_construct)` |
|      1 |  2872 | `{` |
|     23 |  2873 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     23 |  2874 | `	const char *zData = nArg > 1 ? ph7_value_to_string(apArg[1],0) : 0;` |
|     23 |  2875 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  2876 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  2877 | `	}` |
|     25 |  2878 | `	return DomCtorInstall(pCtx,` |
|      8 |  2879 | `		xmlNewPI((const xmlChar *)zName,(const xmlChar *)zData));` |
|     12 |  2880 | `}` |
|     24 |  2881 | `DOM_METHOD(vm_builtin_DOMEntityReference_construct)` |
|      1 |  2882 | `{` |
|     25 |  2883 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     25 |  2884 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  2885 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  2886 | `	}` |
|     19 |  2887 | `	return DomCtorInstall(pCtx,xmlNewReference(0,(const xmlChar *)zName));` |
|     13 |  2888 | `}` |
|      - |  2889 | `/*` |
|      - |  2890 | ` * DOMAttr::__construct(string $name, string $value = '')` |
|      - |  2891 | ` *` |
|      - |  2892 | `` * The name is a plain XML Name -- NO QName split at all, so `p:a` and even`` |
|      - |  2893 | `` * `xmlns:x` pass whole and carry no namespace (`prefix` reads "" and`` |
|      - |  2894 | `` * `localName` the full spelling). The value is LITERAL: php builds the text`` |
|      - |  2895 | ` * child directly rather than through the entity parser, which is what keeps` |
|      - |  2896 | `` * `&amp;` five characters where the element constructor's value collapses it.`` |
|      - |  2897 | ` */` |
|     24 |  2898 | `DOM_METHOD(vm_builtin_DOMAttr_construct)` |
|      1 |  2899 | `{` |
|     25 |  2900 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     25 |  2901 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     25 |  2902 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  2903 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  2904 | `	}` |
|     28 |  2905 | `	return DomCtorInstall(pCtx,` |
|     18 |  2906 | `		(xmlNodePtr)xmlNewProp(0,(const xmlChar *)zName,(const xmlChar *)zVal));` |
|     13 |  2907 | `}` |
|      - |  2908 |  |
|      - |  2909 | `/* ===== Namespaces ===== */` |
|      - |  2910 |  |
|      - |  2911 | `/*` |
|      - |  2912 | ` * Where a namespace lookup starts. php resolves a DOCUMENT to its root element` |
|      - |  2913 | `` * first -- so `$doc->lookupPrefix($uri)` answers what the document element`` |
|      - |  2914 | ` * would, and an empty document answers nothing at all -- and starts from the` |
|      - |  2915 | ` * node itself for everything else, because libxml's own search walks up the` |
|      - |  2916 | ` * parent chain (which is how a text node or a PI reaches its element's` |
|      - |  2917 | ` * declarations, and how a detached one reaches none).` |
|      - |  2918 | ` */` |
|    130 |  2919 | `static xmlNodePtr DomNsAnchor(xmlNodePtr pNode)` |
|      1 |  2920 | `{` |
|    131 |  2921 | `	if( pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE) ){` |
|     29 |  2922 | `		return (xmlNodePtr)xmlDocGetRootElement((xmlDocPtr)pNode);` |
|      - |  2923 | `	}` |
|    103 |  2924 | `	return pNode;` |
|     66 |  2925 | `}` |
|      - |  2926 | ``/* A `?string` argument: its bytes, or NULL for a null one. */`` |
|    508 |  2927 | `static const char * DomArgStrOrNull(int nArg,ph7_value **apArg,int iArg)` |
|      1 |  2928 | `{` |
|    509 |  2929 | `	if( iArg >= nArg \|\| ph7_value_is_null(apArg[iArg]) ){` |
|     93 |  2930 | `		return 0;` |
|      - |  2931 | `	}` |
|    417 |  2932 | `	return ph7_value_to_string(apArg[iArg],0);` |
|    255 |  2933 | `}` |
|      - |  2934 | `/* DOMNode::lookupNamespaceURI(?string $prefix): ?string */` |
|     62 |  2935 | `DOM_METHOD(vm_builtin_DOMNode_lookupNamespaceURI)` |
|      1 |  2936 | `{` |
|     63 |  2937 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     63 |  2938 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|     63 |  2939 | `	const char *zPrefix = DomArgStrOrNull(nArg,apArg,0);` |
|     63 |  2940 | `	xmlNsPtr pNs = pNode ? xmlSearchNs(pNode->doc,pNode,(const xmlChar *)zPrefix) : 0;` |
|     63 |  2941 | `	if( pNs && pNs->href ){` |
|     35 |  2942 | `		ph7_result_string(pCtx,(const char *)pNs->href,-1);` |
|     18 |  2943 | `	}else{` |
|     29 |  2944 | `		ph7_result_null(pCtx);` |
|      - |  2945 | `	}` |
|     63 |  2946 | `	return PH7_OK;` |
|      1 |  2947 | `}` |
|      - |  2948 | `/* DOMNode::lookupPrefix(string $namespace): ?string -- the DEFAULT namespace has` |
|      - |  2949 | `` * no prefix, so a document whose only declaration is `xmlns="..."` answers null`` |
|      - |  2950 | ` * for the very URI lookupNamespaceURI(null) hands back. */` |
|     36 |  2951 | `DOM_METHOD(vm_builtin_DOMNode_lookupPrefix)` |
|      1 |  2952 | `{` |
|     37 |  2953 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     37 |  2954 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|     37 |  2955 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     37 |  2956 | `	xmlNsPtr pNs = (pNode && zUri[0]) ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;` |
|     37 |  2957 | `	if( pNs && pNs->prefix ){` |
|     15 |  2958 | `		ph7_result_string(pCtx,(const char *)pNs->prefix,-1);` |
|      8 |  2959 | `	}else{` |
|     23 |  2960 | `		ph7_result_null(pCtx);` |
|      - |  2961 | `	}` |
|     37 |  2962 | `	return PH7_OK;` |
|      1 |  2963 | `}` |
|      - |  2964 | `/* DOMNode::isDefaultNamespace(string $namespace): bool -- php tests the URI` |
|      - |  2965 | ` * against the default declaration in scope, and answers FALSE for the empty` |
|      - |  2966 | ` * string rather than "this node is in no namespace". */` |
|     32 |  2967 | `DOM_METHOD(vm_builtin_DOMNode_isDefaultNamespace)` |
|      1 |  2968 | `{` |
|     33 |  2969 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     33 |  2970 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|     33 |  2971 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     33 |  2972 | `	xmlNsPtr pNs = (pNode && zUri[0]) ? xmlSearchNs(pNode->doc,pNode,0) : 0;` |
|     45 |  2973 | `	ph7_result_bool(pCtx,pNs != 0 && pNs->href != 0` |
|     22 |  2974 | `		&& xmlStrEqual(pNs->href,(const xmlChar *)zUri));` |
|     33 |  2975 | `	return PH7_OK;` |
|      1 |  2976 | `}` |
|      - |  2977 |  |
|      - |  2978 | `/* ===== Element attributes ===== */` |
|      - |  2979 |  |
|      - |  2980 | `/* Document-order successor within pRoot's subtree (pRoot excluded) */` |
|   1576 |  2981 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot)` |
|      1 |  2982 | `{` |
|   1577 |  2983 | `	if( pCur->children ){` |
|    660 |  2984 | `		return pCur->children;` |
|      - |  2985 | `	}` |
|   1453 |  2986 | `	while( pCur && pCur != pRoot ){` |
|   1133 |  2987 | `		if( pCur->next ){` |
|    598 |  2988 | `			return pCur->next;` |
|      - |  2989 | `		}` |
|    536 |  2990 | `		pCur = pCur->parent;` |
|      1 |  2991 | `	}` |
|    321 |  2992 | `	return 0;` |
|    790 |  2993 | `}` |
|      - |  2994 | `/*` |
|      - |  2995 | ` * ===== Qualified names and the namespaces they need =====` |
|      - |  2996 | ` *` |
|      - |  2997 | ` * The grammar php screens a created name against, and the rule by which it` |
|      - |  2998 | ` * finds or declares the namespace behind it.  Both were missing here, and what` |
|      - |  2999 | `` * stood in for them wrote documents that are not XML: `setAttributeNS('urn:b',`` |
|      - |  3000 | ``  * '1:x', 'v')` emitted `xmlns:1="urn:b" 1:x="v"`, `('urn:b','a:b:c','v')` `` |
|      - |  3001 | ` * emitted an attribute with two colons in its name, and a prefixed name with a` |
|      - |  3002 | `` * NULL namespace emitted `xmlns:q=""`.  php refuses all three with a Namespace`` |
|      - |  3003 | ` * Error before the element is touched.` |
|      - |  3004 | ` */` |
|      - |  3005 | `#define DOM_XML_NS_URI   "http://www.w3.org/XML/1998/namespace"` |
|      - |  3006 | `#define DOM_XMLNS_NS_URI "http://www.w3.org/2000/xmlns/"` |
|      - |  3007 |  |
|      - |  3008 | `typedef struct dom_qname dom_qname;` |
|      - |  3009 | `struct dom_qname {` |
|      - |  3010 | `	xmlChar *zPrefix;   /* NULL when the name carries none */` |
|      - |  3011 | `	xmlChar *zLocal;    /* always allocated */` |
|      - |  3012 | `};` |
|    240 |  3013 | `static void DomQNameRelease(dom_qname *pQ)` |
|      1 |  3014 | `{` |
|    241 |  3015 | `	if( pQ->zPrefix ){` |
|    148 |  3016 | `		xmlFree(pQ->zPrefix);` |
|     73 |  3017 | `	}` |
|    241 |  3018 | `	if( pQ->zLocal ){` |
|    241 |  3019 | `		xmlFree(pQ->zLocal);` |
|    120 |  3020 | `	}` |
|    241 |  3021 | `	pQ->zPrefix = pQ->zLocal = 0;` |
|    241 |  3022 | `}` |
|    272 |  3023 | `static int DomUriIs(const char *zUri,const char *zWant)` |
|      1 |  3024 | `{` |
|    273 |  3025 | `	return zUri != 0 && DomNameIs(zUri,zWant);` |
|      1 |  3026 | `}` |
|      - |  3027 | `/*` |
|      - |  3028 | `` * php's `dom_check_qname`: the name has to be a QName, and a prefix demands a`` |
|      - |  3029 | ` * namespace.  Three callers ask three different questions of the same name, so` |
|      - |  3030 | ` * iMode says which:` |
|      - |  3031 | ` *` |
|      - |  3032 | ` *   DOM_QN_SET   setAttributeNS -- the loosest. A prefixed name is judged as two` |
|      - |  3033 | ` *                NCNames and every failure is the Namespace Error; an unprefixed` |
|      - |  3034 | ` *                one is a plain Name, where a character libxml will not take is` |
|      - |  3035 | `` *                the Invalid Character Error. The unprefixed `xmlns` is how a`` |
|      - |  3036 | ` *                program writes a namespace DECLARATION, so nothing about the` |
|      - |  3037 | ` *                xmlns namespace is checked here.` |
|      - |  3038 | ` *   DOM_QN_ATTR  createAttributeNS -- a QName and nothing else, plus the DOM` |
|      - |  3039 | ` *                spec's pairing (the xmlns namespace may only be spelled by an` |
|      - |  3040 | ` *                xmlns name and an xmlns name may name nothing else) and the` |
|      - |  3041 | `` *                `xml` prefix's own URI.`` |
|      - |  3042 | ` *   DOM_QN_ELEM  createElementNS -- a QName when a namespace came with it, and` |
|      - |  3043 | `` *                the SET side's split when none did (so `createElementNS(null,`` |
|      - |  3044 | `` *                'x y')` is the Invalid Character Error where the attribute`` |
|      - |  3045 | `` *                factory says Namespace Error, and `:x` is an element named`` |
|      - |  3046 | `` *                `:x` there and a refusal here). No reserved rule at all: php`` |
|      - |  3047 | ` *                checks those where it RESOLVES the namespace, which is after` |
|      - |  3048 | ` *                any binding the document already has, so` |
|      - |  3049 | `` *                `createElementNS($XML_NS, 'xmlns:x')` is an `xml:x` element`` |
|      - |  3050 | ` *                rather than a refusal.` |
|      - |  3051 | ` *` |
|      - |  3052 | ` * Answers 0, or the DOM error code to raise.` |
|      - |  3053 | ` */` |
|      - |  3054 | `#define DOM_QN_SET  0` |
|      - |  3055 | `#define DOM_QN_ATTR 1` |
|      - |  3056 | `#define DOM_QN_ELEM 2` |
|    268 |  3057 | `static int DomQNameParse(const char *zQname,const char *zUri,int iMode,dom_qname *pOut)` |
|      1 |  3058 | `{` |
|    269 |  3059 | `	int bHasUri = zUri != 0 && zUri[0] != 0;` |
|      - |  3060 | `	int bXmlnsName;` |
|    269 |  3061 | `	pOut->zPrefix = pOut->zLocal = 0;` |
|    269 |  3062 | `	if( zQname == 0 \|\| zQname[0] == 0 ){` |
|    ! 0 |  3063 | `		return DOM_ERR_NAMESPACE;` |
|      - |  3064 | `	}` |
|    269 |  3065 | `	if( iMode == DOM_QN_ATTR \|\| (iMode == DOM_QN_ELEM && bHasUri) ){` |
|      - |  3066 | `		/* A created name that names a namespace has to be a QName, and every` |
|      - |  3067 | `		 * failure there is the Namespace Error. */` |
|    157 |  3068 | `		if( xmlValidateQName((const xmlChar *)zQname,0) != 0 ){` |
|     29 |  3069 | `			return DOM_ERR_NAMESPACE;` |
|      - |  3070 | `		}` |
|     64 |  3071 | `	}` |
|    241 |  3072 | `	pOut->zLocal = xmlSplitQName2((const xmlChar *)zQname,&pOut->zPrefix);` |
|    241 |  3073 | `	if( pOut->zLocal == 0 ){` |
|      - |  3074 | `		/* No prefix -- or a name that BEGINS with the colon, which libxml hands` |
|      - |  3075 | ``		 * back whole and php then writes literally (`:x`) as long as no`` |
|      - |  3076 | `		 * namespace came with it. */` |
|     94 |  3077 | `		pOut->zLocal = xmlStrdup((const xmlChar *)zQname);` |
|     94 |  3078 | `		if( pOut->zLocal == 0 ){` |
|    ! 0 |  3079 | `			return DOM_ERR_NAMESPACE;` |
|      - |  3080 | `		}` |
|     47 |  3081 | `	}` |
|    241 |  3082 | `	if( iMode == DOM_QN_SET \|\| (iMode == DOM_QN_ELEM && !bHasUri) ){` |
|      - |  3083 | `		/* The SET side separates the two failures php separates. A name with a` |
|      - |  3084 | `		 * PREFIX is judged as two NCNames and every failure there is the` |
|      - |  3085 | `		 * Namespace Error; an unprefixed one is judged as a plain Name, and a` |
|      - |  3086 | `		 * name libxml will not take at all is the Invalid Character Error. A` |
|      - |  3087 | `		 * namespace then demands that the local part be an NCName too, which is` |
|      - |  3088 | ``		 * what refuses `:x` once a URI comes with it. */`` |
|    113 |  3089 | `		if( pOut->zPrefix ){` |
|     63 |  3090 | `			if( xmlValidateNCName(pOut->zPrefix,0) != 0` |
|     61 |  3091 | `			 \|\| xmlValidateNCName(pOut->zLocal,0) != 0 ){` |
|     22 |  3092 | `				DomQNameRelease(pOut);` |
|     22 |  3093 | `				return DOM_ERR_NAMESPACE;` |
|      1 |  3094 | `			}` |
|     71 |  3095 | `		}else if( xmlValidateName((const xmlChar *)zQname,0) != 0 ){` |
|     15 |  3096 | `			DomQNameRelease(pOut);` |
|     15 |  3097 | `			return DOM_ERR_INVALID_CHAR;` |
|      - |  3098 | `		}` |
|     78 |  3099 | `		if( bHasUri && xmlValidateNCName(pOut->zLocal,0) != 0 ){` |
|      4 |  3100 | `			DomQNameRelease(pOut);` |
|      4 |  3101 | `			return DOM_ERR_NAMESPACE;` |
|      - |  3102 | `		}` |
|     37 |  3103 | `	}` |
|    203 |  3104 | `	bXmlnsName = pOut->zPrefix == 0 && xmlStrEqual(pOut->zLocal,(const xmlChar *)"xmlns");` |
|    203 |  3105 | `	if( pOut->zPrefix && !bHasUri ){` |
|      - |  3106 | `		/* A prefix names a namespace, so there has to be one. (Whether the` |
|      - |  3107 | `		 * prefix may be USED is the resolution's question, not the grammar's:` |
|      - |  3108 | `		 * php reuses a binding the document already has whatever prefix was` |
|      - |  3109 | `		 * asked for, and only refuses when it would have to declare one.) */` |
|     21 |  3110 | `		DomQNameRelease(pOut);` |
|     21 |  3111 | `		return DOM_ERR_NAMESPACE;` |
|      - |  3112 | `	}` |
|    182 |  3113 | `	if( iMode == DOM_QN_ATTR && pOut->zPrefix` |
|     24 |  3114 | `	 && xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xml")` |
|     11 |  3115 | `	 && !DomUriIs(zUri,DOM_XML_NS_URI) ){` |
|    ! 0 |  3116 | `		DomQNameRelease(pOut);` |
|    ! 0 |  3117 | `		return DOM_ERR_NAMESPACE;` |
|      - |  3118 | `	}` |
|    183 |  3119 | `	if( iMode == DOM_QN_ATTR ){` |
|      - |  3120 | `		/* The DOM spec's pairing, which php applies to a created ATTRIBUTE: the` |
|      - |  3121 | `		 * xmlns namespace may only be spelled by an xmlns name, and an xmlns` |
|      - |  3122 | `		 * name may name nothing else. */` |
|     55 |  3123 | `		int bXmlnsPrefix = pOut->zPrefix != 0` |
|     30 |  3124 | `			&& xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xmlns");` |
|     31 |  3125 | `		if( (bXmlnsName \|\| bXmlnsPrefix) != DomUriIs(zUri,DOM_XMLNS_NS_URI) ){` |
|      5 |  3126 | `			DomQNameRelease(pOut);` |
|      5 |  3127 | `			return DOM_ERR_NAMESPACE;` |
|      - |  3128 | `		}` |
|     13 |  3129 | `	}` |
|    179 |  3130 | `	return 0;` |
|    135 |  3131 | `}` |
|      - |  3132 | `/*` |
|      - |  3133 | ` * The namespace a node in zUri should carry, declared on pAnchor when the` |
|      - |  3134 | ` * document has none.  php REUSES a binding it can find by URI as long as that` |
|      - |  3135 | ` * binding has a prefix, takes the caller's prefix when it has to declare and` |
|      - |  3136 | `` * the prefix is free, and otherwise generates `default`, `default1`, ... --`` |
|      - |  3137 | ` * which is why asking for a prefix another URI already owns quietly answers` |
|      - |  3138 | `` * `default:x` rather than refusing.`` |
|      - |  3139 | ` *` |
|      - |  3140 | `` * bNeedPrefix is the CREATE side (`createAttributeNS`), where an unprefixed`` |
|      - |  3141 | ` * name still gets a generated prefix; the SET side may declare the DEFAULT` |
|      - |  3142 | ` * namespace instead.` |
|      - |  3143 | ` */` |
|      2 |  3144 | `static xmlNsPtr DomFindPrefixedNs(xmlNodePtr pNode,const char *zUri)` |
|      1 |  3145 | `{` |
|      - |  3146 | `	xmlNodePtr p;` |
|      5 |  3147 | `	for( p = pNode ; p ; p = p->parent ){` |
|      - |  3148 | `		xmlNsPtr pNs;` |
|      5 |  3149 | `		if( p->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  3150 | `			continue;` |
|      - |  3151 | `		}` |
|      7 |  3152 | `		for( pNs = p->nsDef ; pNs ; pNs = pNs->next ){` |
|      4 |  3153 | `			if( pNs->prefix == 0 \|\| pNs->href == 0` |
|      3 |  3154 | `			 \|\| !xmlStrEqual(pNs->href,(const xmlChar *)zUri) ){` |
|      3 |  3155 | `				continue;` |
|      - |  3156 | `			}` |
|      - |  3157 | `			/* ...and only if a nearer declaration has not taken the prefix. */` |
|      3 |  3158 | `			if( xmlSearchNs(pNode->doc,pNode,pNs->prefix) == pNs ){` |
|      3 |  3159 | `				return pNs;` |
|      - |  3160 | `			}` |
|    ! 0 |  3161 | `		}` |
|      2 |  3162 | `	}` |
|    ! 0 |  3163 | `	return 0;` |
|      2 |  3164 | `}` |
|      - |  3165 | `/*` |
|      - |  3166 | ` * Declare a binding of zUri on pAnchor under a prefix nothing there has taken:` |
|      - |  3167 | `` * zBase, then zBase1, zBase2...  php starts from `default` for a namespace`` |
|      - |  3168 | ` * with no prefix of its own and from the prefix ITSELF when it is re-spelling` |
|      - |  3169 | `` * one an inner declaration has shadowed (which is where `p1` comes from).`` |
|      - |  3170 | ` *` |
|      - |  3171 | ` * bScope is what "nothing there has taken" means. xmlNewNs only refuses a second` |
|      - |  3172 | ` * declaration on the SAME element, which is the whole test for a re-spelling` |
|      - |  3173 | ` * (the shadowing declaration is the one being written). An attribute ARRIVING` |
|      - |  3174 | ` * needs the stronger one -- a prefix bound anywhere in scope is taken, or the` |
|      - |  3175 | ` * declaration written here would shadow it and re-point every node under it.` |
|      - |  3176 | ` */` |
|     28 |  3177 | `static xmlNsPtr DomNsGenerateEx(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase,` |
|      - |  3178 | `	int bScope)` |
|      1 |  3179 | `{` |
|     29 |  3180 | `	xmlNsPtr pNs = 0;` |
|      - |  3181 | `	int i;` |
|     35 |  3182 | `	for( i = 0 ; i < 1000 ; i++ ){` |
|      - |  3183 | `		char zGen[256];` |
|     35 |  3184 | `		const char *zB = zBase ? (const char *)zBase : "default";` |
|     35 |  3185 | `		if( SyStrlen(zB) > sizeof(zGen)-16 ){` |
|    ! 0 |  3186 | `			zB = "default";` |
|    ! 0 |  3187 | `		}` |
|     35 |  3188 | `		if( i == 0 ){` |
|     29 |  3189 | `			SyBufferFormat(zGen,sizeof(zGen),"%s",zB);` |
|     15 |  3190 | `		}else{` |
|      7 |  3191 | `			SyBufferFormat(zGen,sizeof(zGen),"%s%d",zB,i);` |
|      - |  3192 | `		}` |
|     35 |  3193 | `		if( bScope && xmlSearchNs(pAnchor->doc,pAnchor,(const xmlChar *)zGen) != 0 ){` |
|      - |  3194 | `			/* Taken -- by a declaration IN SCOPE, which xmlNewNs does not see:` |
|      - |  3195 | `			 * it only refuses a second one on the same element. */` |
|      5 |  3196 | `			continue;` |
|      - |  3197 | `		}` |
|     31 |  3198 | `		pNs = xmlNewNs(pAnchor,(const xmlChar *)zUri,(const xmlChar *)zGen);` |
|     31 |  3199 | `		if( pNs ){` |
|     29 |  3200 | `			return pNs;` |
|      - |  3201 | `		}` |
|      2 |  3202 | `	}` |
|    ! 0 |  3203 | `	return 0;` |
|     15 |  3204 | `}` |
|     14 |  3205 | `static xmlNsPtr DomNsGenerate(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase)` |
|      1 |  3206 | `{` |
|     15 |  3207 | `	return DomNsGenerateEx(pAnchor,zUri,zBase,0);` |
|      1 |  3208 | `}` |
|      - |  3209 | `/* A binding of this URI an ATTRIBUTE can use: one that carries a prefix. */` |
|     44 |  3210 | `static xmlNsPtr DomNsReuse(xmlNodePtr pAnchor,const char *zUri)` |
|      1 |  3211 | `{` |
|     45 |  3212 | `	xmlNsPtr pNs = xmlSearchNsByHref(pAnchor->doc,pAnchor,(const xmlChar *)zUri);` |
|     45 |  3213 | `	if( pNs && pNs->prefix ){` |
|     19 |  3214 | ``		return pNs;   /* including libxml's implicit `xml` binding */`` |
|      - |  3215 | `	}` |
|      - |  3216 | `	/* Bound, but only WITHOUT a prefix, which does not serve an attribute: a` |
|      - |  3217 | `	 * prefixed binding of the same URI further out still does. */` |
|     27 |  3218 | `	return pNs ? DomFindPrefixedNs(pAnchor,zUri) : 0;` |
|     23 |  3219 | `}` |
|     62 |  3220 | `static xmlNsPtr DomNsResolve(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zPrefix,int bNeedPrefix)` |
|      1 |  3221 | `{` |
|      - |  3222 | `	xmlNsPtr pNs;` |
|     63 |  3223 | `	if( !bNeedPrefix ){` |
|     33 |  3224 | `		pNs = DomNsReuse(pAnchor,zUri);` |
|     33 |  3225 | `		if( pNs ){` |
|     15 |  3226 | `			return pNs;` |
|      - |  3227 | `		}` |
|      9 |  3228 | `	}` |
|      - |  3229 | `	/* The CREATE side asks libxml's own question and no more: a document that` |
|      - |  3230 | `	 * binds this URI to the default namespace AND to a prefix answers the` |
|      - |  3231 | `	 * default one there, and php then declares its own rather than looking for` |
|      - |  3232 | `	 * the prefixed binding the SET side would have found. */` |
|     49 |  3233 | `	pNs = xmlSearchNsByHref(pAnchor->doc,pAnchor,(const xmlChar *)zUri);` |
|     49 |  3234 | `	if( bNeedPrefix ){` |
|     31 |  3235 | `		if( pNs && pNs->prefix ){` |
|     11 |  3236 | `			return pNs;` |
|      - |  3237 | `		}` |
|     21 |  3238 | `		pNs = 0;   /* a prefix-less binding is no use to an attribute */` |
|     10 |  3239 | `	}` |
|      - |  3240 | `	/* A prefix-less binding stops php from declaring another one under the` |
|      - |  3241 | `	 * caller's prefix -- what happens then is a generated one. */` |
|     39 |  3242 | `	if( pNs == 0 && (zPrefix != 0 \|\| !bNeedPrefix) ){` |
|     34 |  3243 | `		if( !bNeedPrefix && zPrefix` |
|     16 |  3244 | `		 && (xmlStrEqual(zPrefix,(const xmlChar *)"xml")` |
|     11 |  3245 | `		  \|\| xmlStrEqual(zPrefix,(const xmlChar *)"xmlns")) ){` |
|      - |  3246 | `			/* A RESERVED prefix cannot be declared, and php does not paper over` |
|      - |  3247 | `			 * that with a generated one: it refuses. (Nothing is refused when` |
|      - |  3248 | `			 * the URI already had a binding -- the prefix is never consulted` |
|      - |  3249 | ``			 * then, which is why `setAttributeNS($uri,'xml:id',..)` succeeds on`` |
|      - |  3250 | `			 * a document that binds $uri and fails on one that does not.) */` |
|      5 |  3251 | `			return 0;` |
|      - |  3252 | `		}` |
|     31 |  3253 | `		pNs = xmlNewNs(pAnchor,(const xmlChar *)zUri,zPrefix);` |
|     31 |  3254 | `		if( pNs ){` |
|     27 |  3255 | `			return pNs;` |
|      - |  3256 | `		}` |
|      2 |  3257 | `	}` |
|      9 |  3258 | `	return DomNsGenerate(pAnchor,zUri,0);` |
|     32 |  3259 | `}` |
|      - |  3260 | `/*` |
|      - |  3261 | ` * The namespace a node CREATED in zUri carries, which is a different rule from` |
|      - |  3262 | ` * either side above and php's smallest one: a binding already in scope is used` |
|      - |  3263 | ` * whatever prefix was asked for -- for a fresh node that means only libxml's own` |
|      - |  3264 | `` * `xml` declaration, which is why every `createElementNS($XML_NS, ...)` comes`` |
|      - |  3265 | `` * back spelled `xml:` -- and otherwise the node declares zUri on ITSELF under`` |
|      - |  3266 | ` * the caller's prefix, with no generated prefix and no fallback: the three` |
|      - |  3267 | `` * reserved-name rules php checks here (`dom_get_ns`) are a refusal, not a`` |
|      - |  3268 | ` * rename. NULL means Namespace Error.` |
|      - |  3269 | ` */` |
|     92 |  3270 | `static xmlNsPtr DomNsForCreate(xmlNodePtr pNode,const char *zUri,const xmlChar *zPrefix)` |
|      1 |  3271 | `{` |
|     93 |  3272 | `	xmlNsPtr pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);` |
|     93 |  3273 | `	if( pNs ){` |
|     17 |  3274 | `		return pNs;` |
|      - |  3275 | `	}` |
|     76 |  3276 | `	if( zPrefix != 0` |
|     64 |  3277 | `	 && ((xmlStrEqual(zPrefix,(const xmlChar *)"xml") && !DomUriIs(zUri,DOM_XML_NS_URI))` |
|     46 |  3278 | `	  \|\| (xmlStrEqual(zPrefix,(const xmlChar *)"xmlns") && !DomUriIs(zUri,DOM_XMLNS_NS_URI))` |
|     43 |  3279 | `	  \|\| (DomUriIs(zUri,DOM_XMLNS_NS_URI)` |
|     27 |  3280 | `	   && !xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){` |
|     15 |  3281 | `		return 0;` |
|      - |  3282 | `	}` |
|     69 |  3283 | `	return xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);` |
|     50 |  3284 | `}` |
|      - |  3285 | `/*` |
|      - |  3286 | ` * A declaration that lands on pElem takes the SPELLING away from every node` |
|      - |  3287 | ` * under it that reached its namespace through a declaration this one now` |
|      - |  3288 | ` * shadows -- the node still points at a binding nothing can name from there, so` |
|      - |  3289 | ` * a re-parse of the serialized document reads it in the wrong namespace (or in` |
|      - |  3290 | ` * none).  php re-points those nodes at a binding of their OWN URI: one still in` |
|      - |  3291 | ` * scope when there is one, and otherwise a fresh declaration on pElem under` |
|      - |  3292 | `` * their own prefix numbered up (`p` -> `p1`), or `default` when they had none.`` |
|      - |  3293 | ` *` |
|      - |  3294 | ` * Only called when a write actually declared something, which is what keeps it` |
|      - |  3295 | ` * off the ordinary path.` |
|      - |  3296 | ` */` |
|     76 |  3297 | `static void DomNsRespell(xmlNodePtr pNode,int bAttr)` |
|      1 |  3298 | `{` |
|     77 |  3299 | `	xmlNsPtr pNs = pNode->ns,pAlt;` |
|      - |  3300 | `	/* php declares what it needs on the node that NEEDS it -- the element` |
|      - |  3301 | `	 * itself, or the element an attribute belongs to. */` |
|     77 |  3302 | `	xmlNodePtr pSite = bAttr ? pNode->parent : pNode;` |
|     77 |  3303 | `	if( pNs == 0 \|\| pNs->href == 0 \|\| pSite == 0 ){` |
|     33 |  3304 | `		return;` |
|      - |  3305 | `	}` |
|     45 |  3306 | `	if( xmlSearchNs(pNode->doc,pNode,pNs->prefix) == pNs ){` |
|     29 |  3307 | `		return;   /* the prefix still names this very binding */` |
|      - |  3308 | `	}` |
|      - |  3309 | `	/* An attribute needs a PREFIXED binding; an element is happy with the` |
|      - |  3310 | `	 * default one. */` |
|     13 |  3311 | `	pAlt = bAttr ? DomNsReuse(pNode,(const char *)pNs->href)` |
|     12 |  3312 | `	             : xmlSearchNsByHref(pNode->doc,pNode,pNs->href);` |
|     17 |  3313 | `	if( pAlt == 0 ){` |
|      - |  3314 | `		/* Its own prefix first -- a declaration that was REMOVED leaves that` |
|      - |  3315 | `		 * prefix free again, and php re-declares it unchanged there. */` |
|     11 |  3316 | `		pAlt = xmlNewNs(pSite,pNs->href,pNs->prefix);` |
|      5 |  3317 | `	}` |
|     17 |  3318 | `	if( pAlt == 0 ){` |
|      7 |  3319 | `		pAlt = DomNsGenerate(pSite,(const char *)pNs->href,pNs->prefix);` |
|      3 |  3320 | `	}` |
|     17 |  3321 | `	if( pAlt ){` |
|     17 |  3322 | `		pNode->ns = pAlt;` |
|      8 |  3323 | `	}` |
|     39 |  3324 | `}` |
|     78 |  3325 | `static int DomNsDefCount(xmlNodePtr pElem)` |
|      1 |  3326 | `{` |
|      - |  3327 | `	xmlNsPtr pNs;` |
|     79 |  3328 | `	int n = 0;` |
|     99 |  3329 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|     21 |  3330 | `		n++;` |
|     11 |  3331 | `	}` |
|     79 |  3332 | `	return n;` |
|      1 |  3333 | `}` |
|     34 |  3334 | `static void DomNsReconcile(xmlNodePtr pElem)` |
|      1 |  3335 | `{` |
|     35 |  3336 | `	xmlNodePtr pCur = pElem;` |
|     77 |  3337 | `	while( pCur ){` |
|      - |  3338 | `		xmlAttrPtr pAttr;` |
|     43 |  3339 | `		if( pCur->type == XML_ELEMENT_NODE ){` |
|     43 |  3340 | `			DomNsRespell(pCur,0);` |
|     77 |  3341 | `			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){` |
|     35 |  3342 | `				if( pAttr->type == XML_ATTRIBUTE_NODE ){` |
|     35 |  3343 | `					DomNsRespell((xmlNodePtr)pAttr,1);` |
|     17 |  3344 | `				}` |
|     18 |  3345 | `			}` |
|     21 |  3346 | `		}` |
|     43 |  3347 | `		pCur = DomWalkNext(pCur,pElem);` |
|      1 |  3348 | `	}` |
|     35 |  3349 | `}` |
|      - |  3350 | `/*` |
|      - |  3351 | ` * A declaration is freed with the element that carries it, so one REMOVED from` |
|      - |  3352 | ` * an element cannot simply be dropped: a node further down may still point at` |
|      - |  3353 | ` * it. It goes where libxml's own document teardown will free it and nothing` |
|      - |  3354 | ``  * resolves through it -- `doc->oldNs`, which is what php's `dom_set_old_ns` `` |
|      - |  3355 | ` * writes to.` |
|      - |  3356 | ` */` |
|     32 |  3357 | `static void DomNsPark(xmlNodePtr pOwner,xmlNsPtr pNs)` |
|      1 |  3358 | `{` |
|     33 |  3359 | `	xmlDocPtr pDoc = pOwner->doc;` |
|      - |  3360 | `	xmlNsPtr pTail;` |
|     33 |  3361 | `	pNs->next = 0;` |
|      - |  3362 | ``	/* The list's HEAD must stay libxml's own `xml` declaration, because`` |
|      - |  3363 | ``	 * xmlSearchNs answers doc->oldNs DIRECTLY for the `xml` prefix. Asking for`` |
|      - |  3364 | `	 * it is what builds it. */` |
|     33 |  3365 | `	xmlSearchNs(pDoc,pOwner,(const xmlChar *)"xml");` |
|     33 |  3366 | `	if( pDoc->oldNs == 0 ){` |
|    ! 0 |  3367 | `		pDoc->oldNs = pNs;` |
|    ! 0 |  3368 | `		return;` |
|      - |  3369 | `	}` |
|     37 |  3370 | `	for( pTail = pDoc->oldNs ; pTail->next ; pTail = pTail->next ){}` |
|     33 |  3371 | `	pTail->next = pNs;` |
|     17 |  3372 | `}` |
|      - |  3373 | `/*` |
|      - |  3374 | `` * php's `dom_reconcile_ns`, which every mutator runs on the node it LINKED.`` |
|      - |  3375 | ` * Without it a move wrote documents that are not XML in both directions:` |
|      - |  3376 | ` * appending a node whose namespace was declared on the ancestor it just left` |
|      - |  3377 | `` * emitted `<p:b k="1"/>` with the prefix bound nowhere, and appending one that`` |
|      - |  3378 | `` * carries its own declaration (`createElementNS`, or a chunk `appendXML` built)`` |
|      - |  3379 | ` * emitted a second copy of a declaration the new parent already makes.` |
|      - |  3380 | ` *` |
|      - |  3381 | ` * The strip is php's own test: same URI, and either the node's declaration` |
|      - |  3382 | ` * carries NO prefix -- then any binding of that URI in scope replaces it, even` |
|      - |  3383 | `` * a prefixed one, which is how an appended `createElementNS($uri,'y')` comes`` |
|      - |  3384 | `` * out spelled `p:y` -- or the in-scope binding spells it the same way.`` |
|      - |  3385 | ` *` |
|      - |  3386 | `` * The re-pointing after it is libxml's own `xmlReconciliateNs`, called here`` |
|      - |  3387 | ` * rather than paraphrased: it re-points EVERY node of the subtree at the first` |
|      - |  3388 | ` * in-scope binding of its URI found from the inserted node -- so a URI two` |
|      - |  3389 | ` * prefixes bind is respelled to the first of them, which the setAttributeNS` |
|      - |  3390 | ` * respeller (DomNsReconcile, which only touches a node whose spelling BROKE)` |
|      - |  3391 | ` * does not do -- and declares one on the inserted node for a URI nothing in` |
|      - |  3392 | ` * scope binds any more (including one a DESCENDANT declares, since the search` |
|      - |  3393 | ` * only ever looks up).` |
|      - |  3394 | ` */` |
|    302 |  3395 | `static void DomNsStrip(xmlNodePtr pNode,xmlNodePtr pScopeAt)` |
|      2 |  3396 | `{` |
|    304 |  3397 | `	xmlNsPtr pCur = pNode->nsDef,pPrev = 0;` |
|    304 |  3398 | `	if( pNode->doc == 0 ){` |
|      - |  3399 | `		/* Nothing would own a removed declaration, and a node under it may` |
|      - |  3400 | `		 * still point at one: leave the element's list alone. */` |
|     15 |  3401 | `		return;` |
|      - |  3402 | `	}` |
|    410 |  3403 | `	while( pCur ){` |
|    121 |  3404 | `		xmlNsPtr pNext = pCur->next;` |
|    181 |  3405 | `		xmlNsPtr pScope = pCur->href` |
|    120 |  3406 | `			? xmlSearchNsByHref(pNode->doc,pScopeAt,pCur->href) : 0;` |
|    120 |  3407 | `		if( pScope != 0` |
|     96 |  3408 | `		 && (pCur->prefix == 0` |
|     38 |  3409 | `		  \|\| (pScope->prefix != 0 && xmlStrEqual(pScope->prefix,pCur->prefix))) ){` |
|     31 |  3410 | `			if( pPrev ){` |
|    ! 0 |  3411 | `				pPrev->next = pNext;` |
|    ! 0 |  3412 | `			}else{` |
|     31 |  3413 | `				pNode->nsDef = pNext;` |
|      - |  3414 | `			}` |
|     31 |  3415 | `			DomNsPark(pNode,pCur);` |
|     16 |  3416 | `		}else{` |
|     91 |  3417 | `			pPrev = pCur;` |
|      - |  3418 | `		}` |
|    121 |  3419 | `		pCur = pNext;` |
|      1 |  3420 | `	}` |
|    153 |  3421 | `}` |
|      - |  3422 | `/*` |
|      - |  3423 | ` * php runs the strip on the node it linked and NO deeper -- a redundant` |
|      - |  3424 | `` * declaration one level down survives an `appendChild` -- but a FRAGMENT is`` |
|      - |  3425 | ` * spliced by a second function that walks each moved child WHOLE, so the same` |
|      - |  3426 | ` * subtree arriving that way comes out stripped at every depth. bDeep is that` |
|      - |  3427 | ` * difference, and both halves are measurable.` |
|      - |  3428 | ` *` |
|      - |  3429 | ` * The deep walk judges every node against the same scope -- the INSERTION` |
|      - |  3430 | ` * POINT, not each node's own parent -- so a declaration duplicated inside the` |
|      - |  3431 | ` * moved subtree survives when the new parent does not make it too.` |
|      - |  3432 | ` */` |
|    472 |  3433 | `static void DomNsOnInsertEx(xmlNodePtr pNode,int bDeep)` |
|      2 |  3434 | `{` |
|    474 |  3435 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|    186 |  3436 | `		return;` |
|      - |  3437 | `	}` |
|    290 |  3438 | `	if( bDeep ){` |
|     73 |  3439 | `		xmlNodePtr pCur = pNode,pAt = pNode->parent;` |
|    163 |  3440 | `		while( pCur ){` |
|     91 |  3441 | `			if( pCur->type == XML_ELEMENT_NODE ){` |
|     87 |  3442 | `				DomNsStrip(pCur,pAt);` |
|     43 |  3443 | `			}` |
|     91 |  3444 | `			pCur = DomWalkNext(pCur,pNode);` |
|      1 |  3445 | `		}` |
|     37 |  3446 | `	}else{` |
|    218 |  3447 | `		DomNsStrip(pNode,pNode->parent);` |
|      - |  3448 | `	}` |
|    290 |  3449 | `	xmlReconciliateNs(pNode->doc,pNode);` |
|    238 |  3450 | `}` |
|      - |  3451 | `/*` |
|      - |  3452 | ` * The namespace an attribute NODE carries once it is linked onto pElem. Its own` |
|      - |  3453 | ` * ns struct is a declaration of wherever it came FROM, and moving the node does` |
|      - |  3454 | `` * not move that: written unchanged it emitted `p:k="1"` with the prefix bound`` |
|      - |  3455 | ` * NOWHERE, and -- when the target's scope binds that prefix to something else --` |
|      - |  3456 | ` * bound to the WRONG URI, which is the worse half, because those bytes parse` |
|      - |  3457 | ` * back cleanly as an attribute in a namespace the program never wrote.` |
|      - |  3458 | ` *` |
|      - |  3459 | ` * php keeps the spelling when the attribute's own declaration is still in scope` |
|      - |  3460 | ` * here, even if a nearer one binds the same URI under another prefix. Otherwise` |
|      - |  3461 | ` * it takes any binding of the URI in scope -- INCLUDING a prefix-less one, where` |
|      - |  3462 | ` * the attribute then serializes with no prefix at all and still answers the URI,` |
|      - |  3463 | ` * which is NOT how the by-NAME writes resolve (there an attribute always wants a` |
|      - |  3464 | ` * prefixed binding, DomNsReuse) -- and otherwise declares one here under the` |
|      - |  3465 | `` * attribute's own prefix, numbered up when that prefix is taken (`p` -> `p1`).`` |
|      - |  3466 | ` */` |
|     78 |  3467 | `static void DomNsAttrArrive(xmlNodePtr pElem,xmlAttrPtr pAttr)` |
|      2 |  3468 | `{` |
|     80 |  3469 | `	xmlNsPtr pNs = pAttr->ns;` |
|     80 |  3470 | `	if( pNs == 0 \|\| pNs->href == 0 ){` |
|     32 |  3471 | `		return;` |
|      - |  3472 | `	}` |
|     49 |  3473 | `	if( DomUriIs((const char *)pNs->href,DOM_XMLNS_NS_URI) ){` |
|      - |  3474 | `		/* An attribute in the xmlns namespace IS a declaration, and php resolves` |
|      - |  3475 | ``		 * nothing for it: `xmlns="urn:z"` stays spelled that way wherever it is`` |
|      - |  3476 | `		 * written, and never acquires a declaration of the xmlns namespace. */` |
|      7 |  3477 | `		return;` |
|      - |  3478 | `	}` |
|     43 |  3479 | `	if( xmlSearchNs(pElem->doc,pElem,pNs->prefix) == pNs ){` |
|     21 |  3480 | `		return;` |
|      - |  3481 | `	}` |
|     23 |  3482 | `	pNs = xmlSearchNsByHref(pElem->doc,pElem,pAttr->ns->href);` |
|     23 |  3483 | `	if( pNs ){` |
|      9 |  3484 | `		pAttr->ns = pNs;` |
|      9 |  3485 | `		return;` |
|      - |  3486 | `	}` |
|     15 |  3487 | `	pNs = DomNsGenerateEx(pElem,(const char *)pAttr->ns->href,pAttr->ns->prefix,1);` |
|     15 |  3488 | `	if( pNs == 0 ){` |
|    ! 0 |  3489 | `		return;` |
|      - |  3490 | `	}` |
|     15 |  3491 | `	pAttr->ns = pNs;` |
|      - |  3492 | `	/*` |
|      - |  3493 | `	 * A declaration LANDED on this element, and php then judges its whole` |
|      - |  3494 | `	 * subtree from HERE: a descendant whose namespace is declared further down` |
|      - |  3495 | `	 * is re-pointed at a fresh declaration on this element -- even though the` |
|      - |  3496 | `	 * one it had is still in scope where it stands. That is libxml's` |
|      - |  3497 | `	 * xmlReconciliateNs, and it runs ONLY on this path: an arriving attribute` |
|      - |  3498 | `	 * that needed no declaration leaves the subtree exactly as it was, which is` |
|      - |  3499 | `	 * measurable both ways.` |
|      - |  3500 | `	 */` |
|     15 |  3501 | `	xmlReconciliateNs(pElem->doc,pElem);` |
|     41 |  3502 | `}` |
|      - |  3503 | `/* A namespace DECLARATION on this element: php's setAttributeNS writes one` |
|      - |  3504 | `` * when the name is `xmlns` or its prefix is, and REBINDS the one already`` |
|      - |  3505 | ` * there rather than adding a second. */` |
|     14 |  3506 | `static void DomNsDeclare(xmlNodePtr pElem,const xmlChar *zPrefix,const char *zHref)` |
|      1 |  3507 | `{` |
|      - |  3508 | `	xmlNsPtr pNs;` |
|     17 |  3509 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|      7 |  3510 | `		int bSame = zPrefix == 0 ? pNs->prefix == 0` |
|      3 |  3511 | `			: (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix));` |
|      5 |  3512 | `		if( bSame ){` |
|      3 |  3513 | `			xmlChar *zNew = xmlStrdup((const xmlChar *)zHref);` |
|      3 |  3514 | `			if( zNew == 0 ){` |
|    ! 0 |  3515 | `				return;` |
|      - |  3516 | `			}` |
|      3 |  3517 | `			if( pNs->href ){` |
|      3 |  3518 | `				xmlFree((xmlChar *)pNs->href);` |
|      1 |  3519 | `			}` |
|      3 |  3520 | `			pNs->href = zNew;` |
|      3 |  3521 | `			return;` |
|      - |  3522 | `		}` |
|      2 |  3523 | `	}` |
|     13 |  3524 | `	xmlNewNs(pElem,(const xmlChar *)zHref,zPrefix);` |
|      8 |  3525 | `}` |
|      - |  3526 |  |
|      - |  3527 | `/*` |
|      - |  3528 | ` * ===== Namespace DECLARATIONS, which php answers from the attribute surface =====` |
|      - |  3529 | ` *` |
|      - |  3530 | `` * `xmlns:x="urn:x"` is not an attribute in libxml -- it is an xmlNs on the`` |
|      - |  3531 | ``  * element's nsDef chain -- but php answers it from `getAttributeNode('xmlns:x')` `` |
|      - |  3532 | `` * (as a DOMNameSpaceNode), from `getAttribute`, `hasAttribute`,`` |
|      - |  3533 | `` * `removeAttribute`, `toggleAttribute` and `getAttributeNames`, which is how a`` |
|      - |  3534 | ` * program reads or drops one.  Here every one of those said the declaration was` |
|      - |  3535 | ``  * not there: `hasAttribute('xmlns:x')` was false and `getAttribute('xmlns:x')` `` |
|      - |  3536 | ` * was "" on a document whose root declares it.` |
|      - |  3537 | ` *` |
|      - |  3538 | ` * The lookup is the element's OWN declarations, not the ones in scope: a child` |
|      - |  3539 | ` * answers false for a prefix its parent declared.` |
|      - |  3540 | ` */` |
|      - |  3541 | `#define DOM_XMLNS_NAME "xmlns"` |
|      - |  3542 |  |
|      - |  3543 | `/* The declaration this element makes for zPrefix (NULL for the DEFAULT one). */` |
|     68 |  3544 | `static xmlNsPtr DomNsDeclOf(xmlNodePtr pElem,const xmlChar *zPrefix)` |
|      1 |  3545 | `{` |
|      - |  3546 | `	xmlNsPtr pNs;` |
|     69 |  3547 | `	if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  3548 | `		return 0;` |
|      - |  3549 | `	}` |
|    111 |  3550 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|    148 |  3551 | `		if( zPrefix == 0 ? pNs->prefix == 0` |
|     70 |  3552 | `		                 : (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix)) ){` |
|     45 |  3553 | `			return pNs;` |
|      - |  3554 | `		}` |
|     22 |  3555 | `	}` |
|     25 |  3556 | `	return 0;` |
|     35 |  3557 | `}` |
|      - |  3558 | ``/* ...under the NAME php spells it with: `xmlns` or `xmlns:<prefix>`. */`` |
|    114 |  3559 | `static xmlNsPtr DomNsDeclByName(xmlNodePtr pElem,const char *zName)` |
|      3 |  3560 | `{` |
|    117 |  3561 | `	sxu32 n = (sxu32)SyStrlen(DOM_XMLNS_NAME);` |
|    117 |  3562 | `	if( SyStrlen(zName) < n \|\| SyStrncmp(zName,DOM_XMLNS_NAME,n) != 0 ){` |
|     73 |  3563 | `		return 0;` |
|      - |  3564 | `	}` |
|     45 |  3565 | `	if( zName[n] == 0 ){` |
|     15 |  3566 | `		return DomNsDeclOf(pElem,0);` |
|      - |  3567 | `	}` |
|     31 |  3568 | `	if( zName[n] != ':' ){` |
|    ! 0 |  3569 | `		return 0;` |
|      - |  3570 | `	}` |
|     31 |  3571 | `	return DomNsDeclOf(pElem,(const xmlChar *)(zName+n+1));` |
|     60 |  3572 | `}` |
|      - |  3573 | `/*` |
|      - |  3574 | ` * Dropping one: the declaration leaves the element's chain, but the xmlNs` |
|      - |  3575 | ` * itself must NOT be freed -- nodes below can still point at it, and php's own` |
|      - |  3576 | ` * answer for that case is a document that keeps saying what it said. The` |
|      - |  3577 | ` * document's oldNs chain owns it from here, so it dies with the document.` |
|      - |  3578 | ` */` |
|      8 |  3579 | `static void DomNsDeclRemove(xmlNodePtr pElem,xmlNsPtr pNs)` |
|      1 |  3580 | `{` |
|      9 |  3581 | `	xmlNsPtr pPrev = 0,pCur;` |
|      9 |  3582 | `	xmlDocPtr pDoc = pElem->doc;` |
|      9 |  3583 | `	for( pCur = pElem->nsDef ; pCur ; pPrev = pCur,pCur = pCur->next ){` |
|      9 |  3584 | `		if( pCur != pNs ){` |
|    ! 0 |  3585 | `			continue;` |
|      - |  3586 | `		}` |
|      9 |  3587 | `		if( pPrev ){` |
|    ! 0 |  3588 | `			pPrev->next = pCur->next;` |
|    ! 0 |  3589 | `		}else{` |
|      9 |  3590 | `			pElem->nsDef = pCur->next;` |
|      - |  3591 | `		}` |
|      9 |  3592 | `		pCur->next = 0;` |
|      9 |  3593 | `		if( pDoc == 0 ){` |
|    ! 0 |  3594 | `			return;` |
|      - |  3595 | `		}` |
|      9 |  3596 | `		if( pDoc->oldNs == 0 ){` |
|      9 |  3597 | `			pDoc->oldNs = pCur;` |
|      5 |  3598 | `		}else{` |
|    ! 0 |  3599 | `			xmlNsPtr pTail = pDoc->oldNs;` |
|    ! 0 |  3600 | `			while( pTail->next ){` |
|    ! 0 |  3601 | `				pTail = pTail->next;` |
|    ! 0 |  3602 | `			}` |
|    ! 0 |  3603 | `			pTail->next = pCur;` |
|      - |  3604 | `		}` |
|      9 |  3605 | `		return;` |
|    ! 0 |  3606 | `	}` |
|      5 |  3607 | `}` |
|      - |  3608 |  |
|      - |  3609 | `/*` |
|      - |  3610 | ` * php's wrapper for one: DOMNameSpaceNode, a class of its own that does NOT` |
|      - |  3611 | ` * extend DOMNode and answers ten properties.  A FRESH object every time (php's` |
|      - |  3612 | `` * two calls are never `===`), so it needs none of the identity cache.`` |
|      - |  3613 | ` */` |
|      - |  3614 | `/*` |
|      - |  3615 | ` * The handle for one declaration, kept in the document's identity cache under` |
|      - |  3616 | `` * the xmlNs pointer.  php's two lookups answer two OBJECTS (`===` is false) but`` |
|      - |  3617 | `` * the same declaration, and a shared handle is what makes them `==` -- and what`` |
|      - |  3618 | `` * stops a loop over `getAttributeNode('xmlns:x')` from allocating one per call.`` |
|      - |  3619 | ` */` |
|     60 |  3620 | `static phl_domnode * DomNsRes(ph7_vm *pVm,ph7_class_instance *pDoc,phl_xmldoc *pShell,xmlNsPtr pNs)` |
|      1 |  3621 | `{` |
|     61 |  3622 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|     61 |  3623 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  3624 | `	phl_domnode *pRes;` |
|      - |  3625 | `	ph7_value sKey,sVal;` |
|     61 |  3626 | `	if( pCache == 0 ){` |
|    ! 0 |  3627 | `		return DomNewRes(&(*pVm),pShell,pNs);` |
|      - |  3628 | `	}` |
|     61 |  3629 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNs);` |
|     61 |  3630 | `	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|     33 |  3631 | `		ph7_value *pHit = HashmapExtractNodeValue(pEntry);` |
|     33 |  3632 | `		if( pHit && (pHit->iFlags & MEMOBJ_RES) ){` |
|     33 |  3633 | `			PH7_MemObjRelease(&sKey);` |
|     33 |  3634 | `			return (phl_domnode *)pHit->x.pOther;` |
|      - |  3635 | `		}` |
|    ! 0 |  3636 | `	}` |
|     29 |  3637 | `	pRes = DomNewRes(&(*pVm),pShell,pNs);` |
|     29 |  3638 | `	if( pRes ){` |
|     29 |  3639 | `		PH7_MemObjInit(&(*pVm),&sVal);` |
|     29 |  3640 | `		sVal.x.pOther = pRes;` |
|     29 |  3641 | `		sVal.iFlags = MEMOBJ_RES;` |
|     29 |  3642 | `		PH7_HashmapInsert(pCache,&sKey,&sVal);` |
|     14 |  3643 | `	}` |
|     29 |  3644 | `	PH7_MemObjRelease(&sKey);` |
|     29 |  3645 | `	return pRes;` |
|     31 |  3646 | `}` |
|     60 |  3647 | `static ph7_class_instance * DomNewNsNode(ph7_vm *pVm,ph7_class_instance *pDoc,` |
|      - |  3648 | `	phl_xmldoc *pShell,xmlNsPtr pNs,xmlNodePtr pElem)` |
|      1 |  3649 | `{` |
|     91 |  3650 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DOMNameSpaceNode",` |
|     60 |  3651 | `		(sxu32)SyStrlen("DOMNameSpaceNode"),FALSE,0);` |
|     61 |  3652 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|     61 |  3653 | `	phl_domnode *pRes = pObj ? DomNsRes(&(*pVm),pDoc,pShell,pNs) : 0;` |
|     61 |  3654 | `	if( pRes == 0 ){` |
|    ! 0 |  3655 | `		if( pObj ){` |
|    ! 0 |  3656 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  3657 | `		}` |
|    ! 0 |  3658 | `		return 0;` |
|      - |  3659 | `	}` |
|     61 |  3660 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|     61 |  3661 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|      - |  3662 | `	{` |
|     61 |  3663 | `		ph7_class_instance *pOwn = DomWrap(&(*pVm),pDoc,pShell,pElem);` |
|     61 |  3664 | `		PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_NS_OWNER,pOwn);` |
|     61 |  3665 | `		if( pOwn ){` |
|     61 |  3666 | `			PH7_ClassInstanceUnref(pOwn);   /* the slot took its own */` |
|     30 |  3667 | `		}` |
|      - |  3668 | `	}` |
|     61 |  3669 | `	return pObj;   /* the CALLER owns this reference */` |
|     31 |  3670 | `}` |
|      - |  3671 | `/* Answer one from a native method (php hands back a fresh object every time). */` |
|     18 |  3672 | `static int DomResultNsNode(ph7_context *pCtx,phl_domnode *pNd,xmlNsPtr pNs,xmlNodePtr pElem)` |
|      1 |  3673 | `{` |
|     19 |  3674 | `	ph7_class_instance *pObj = DomNewNsNode(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNs,pElem);` |
|     19 |  3675 | `	if( pObj == 0 ){` |
|    ! 0 |  3676 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3677 | `	}` |
|     19 |  3678 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     19 |  3679 | `	return PH7_OK;` |
|     10 |  3680 | `}` |
|      - |  3681 |  |
|      - |  3682 | `/*` |
|      - |  3683 | ` * An attribute of pElem by QUALIFIED name, php's own two-step lookup.` |
|      - |  3684 | ` *` |
|      - |  3685 | ` * libxml's xmlHasProp() matches the stored name and ignores namespaces` |
|      - |  3686 | ` * entirely, which is wrong in both directions and was the answer the whole` |
|      - |  3687 | `` * by-name surface gave: `getAttribute('x:b')` found NOTHING (the stored name is`` |
|      - |  3688 | `` * `b`, the prefix lives in the node's ns) while `getAttribute('b')` found the`` |
|      - |  3689 | `` * NAMESPACED one and `removeAttribute('b')` deleted it.  php looks for an`` |
|      - |  3690 | ` * attribute in NO namespace under the whole name first -- which is what finds` |
|      - |  3691 | ` * one written under an unresolvable prefix, stored with the colon in its name --` |
|      - |  3692 | ` * and only then splits the prefix, resolves it in the element's scope and asks` |
|      - |  3693 | ` * again by (local name, URI).  An unresolvable prefix finds nothing.` |
|      - |  3694 | ` *` |
|      - |  3695 | ` * A DTD-declared DEFAULT is not an attribute here: xmlHasNsProp answers the` |
|      - |  3696 | ` * DECLARATION node for those, which is a different node kind entirely.` |
|      - |  3697 | ` */` |
|    240 |  3698 | `static xmlAttrPtr DomAttrNoNs(xmlNodePtr pElem,const char *zName)` |
|      3 |  3699 | `{` |
|      - |  3700 | `	xmlAttrPtr pAttr;` |
|    243 |  3701 | `	if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  3702 | `		return 0;` |
|      - |  3703 | `	}` |
|    243 |  3704 | `	pAttr = xmlHasNsProp(pElem,(const xmlChar *)zName,0);` |
|    243 |  3705 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|    123 |  3706 | `}` |
|    214 |  3707 | `static xmlAttrPtr DomAttrByName(xmlNodePtr pElem,const char *zName)` |
|      3 |  3708 | `{` |
|    217 |  3709 | `	xmlAttrPtr pAttr = DomAttrNoNs(pElem,zName);` |
|    217 |  3710 | `	if( pAttr == 0 && pElem ){` |
|     69 |  3711 | `		xmlChar *zPrefix = 0;` |
|     69 |  3712 | `		xmlChar *zLocal = xmlSplitQName2((const xmlChar *)zName,&zPrefix);` |
|     69 |  3713 | `		if( zLocal ){` |
|     41 |  3714 | `			if( zPrefix ){` |
|     41 |  3715 | `				xmlNsPtr pNs = xmlSearchNs(pElem->doc,pElem,zPrefix);` |
|     41 |  3716 | `				if( pNs ){` |
|     15 |  3717 | `					pAttr = xmlHasNsProp(pElem,zLocal,pNs->href);` |
|      7 |  3718 | `				}` |
|     41 |  3719 | `				xmlFree(zPrefix);` |
|     20 |  3720 | `			}` |
|     41 |  3721 | `			xmlFree(zLocal);` |
|     20 |  3722 | `		}` |
|     34 |  3723 | `	}` |
|    217 |  3724 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|      3 |  3725 | `}` |
|    158 |  3726 | `static xmlAttrPtr DomAttrByNs(xmlNodePtr pElem,const xmlChar *zUri,const char *zLocal)` |
|      2 |  3727 | `{` |
|    160 |  3728 | `	xmlAttrPtr pAttr = pElem ? xmlHasNsProp(pElem,(const xmlChar *)zLocal,zUri) : 0;` |
|    160 |  3729 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|      2 |  3730 | `}` |
|      - |  3731 |  |
|      - |  3732 | `/* DOMElement::getAttribute(string $qualifiedName): string -- "" when absent */` |
|     44 |  3733 | `DOM_METHOD(vm_builtin_DOMElement_getAttribute)` |
|      1 |  3734 | `{` |
|     45 |  3735 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     45 |  3736 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     45 |  3737 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     45 |  3738 | `	xmlNsPtr pDecl = DomNsDeclByName(pElem,zName);` |
|     45 |  3739 | `	xmlAttrPtr pAttr = pDecl ? 0 : DomAttrByName(pElem,zName);` |
|     45 |  3740 | `	xmlChar *zVal = pAttr ? xmlNodeListGetString(pAttr->doc,pAttr->children,1) : 0;` |
|     45 |  3741 | `	if( pDecl ){` |
|      - |  3742 | `		/* A declaration's "value" is the URI it binds. */` |
|      7 |  3743 | `		ph7_result_string(pCtx,pDecl->href ? (const char *)pDecl->href : "",-1);` |
|      7 |  3744 | `		return PH7_OK;` |
|      - |  3745 | `	}` |
|     39 |  3746 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|     39 |  3747 | `	if( zVal ){` |
|     33 |  3748 | `		xmlFree(zVal);` |
|     16 |  3749 | `	}` |
|     39 |  3750 | `	return PH7_OK;` |
|     23 |  3751 | `}` |
|      - |  3752 | `/* DOMElement::hasAttribute(string $qualifiedName): bool */` |
|     18 |  3753 | `DOM_METHOD(vm_builtin_DOMElement_hasAttribute)` |
|      1 |  3754 | `{` |
|     19 |  3755 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     19 |  3756 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     43 |  3757 | `	ph7_result_bool(pCtx,pNd != 0` |
|     27 |  3758 | `		&& (DomAttrByName((xmlNodePtr)pNd->pNode,zName) != 0` |
|     15 |  3759 | `		 \|\| DomNsDeclByName((xmlNodePtr)pNd->pNode,zName) != 0));` |
|     19 |  3760 | `	return PH7_OK;` |
|      1 |  3761 | `}` |
|      - |  3762 | `/* DOMElement::setAttribute(string $qualifiedName, string $value): DOMAttr -- php` |
|      - |  3763 | ` * answers the attribute NODE it wrote, so the write is followed by a wrap. */` |
|     28 |  3764 | `DOM_METHOD(vm_builtin_DOMElement_setAttribute)` |
|      3 |  3765 | `{` |
|     31 |  3766 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     31 |  3767 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|     31 |  3768 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  3769 | `	xmlAttrPtr pAttr;` |
|     31 |  3770 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  3771 | `		return DomThrowAlways(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  3772 | `	}` |
|     25 |  3773 | `	if( DomNsDeclByName((xmlNodePtr)pNd->pNode,zName) ){` |
|      - |  3774 | `		/* php will not write THROUGH a declaration this element already makes:` |
|      - |  3775 | `		 * the write is dropped and the answer is false. (A name that is not one` |
|      - |  3776 | `		 * yet becomes an ordinary attribute, colon and all.) */` |
|      3 |  3777 | `		ph7_result_bool(pCtx,0);` |
|      3 |  3778 | `		return PH7_OK;` |
|      - |  3779 | `	}` |
|     23 |  3780 | `	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);` |
|     23 |  3781 | `	pAttr = DomAttrByName((xmlNodePtr)pNd->pNode,zName);` |
|     23 |  3782 | `	if( pAttr == 0 ){` |
|    ! 0 |  3783 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3784 | `		return PH7_OK;` |
|      - |  3785 | `	}` |
|     23 |  3786 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     17 |  3787 | `}` |
|      - |  3788 | `/* DOMElement::removeAttribute(string $qualifiedName): bool */` |
|     10 |  3789 | `DOM_METHOD(vm_builtin_DOMElement_removeAttribute)` |
|      1 |  3790 | `{` |
|     11 |  3791 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     11 |  3792 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     11 |  3793 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     11 |  3794 | `	xmlAttrPtr pAttr = DomAttrByName(pElem,zName);` |
|     11 |  3795 | `	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|     11 |  3796 | `	if( pDecl ){` |
|      - |  3797 | `		/* The declaration goes, and whatever still needs it gets it back: php` |
|      - |  3798 | `		 * answers TRUE either way, and a binding nothing uses simply vanishes. */` |
|      5 |  3799 | `		DomNsDeclRemove(pElem,pDecl);` |
|      5 |  3800 | `		DomNsReconcile(pElem);` |
|      5 |  3801 | `		ph7_result_bool(pCtx,1);` |
|      5 |  3802 | `		return PH7_OK;` |
|      - |  3803 | `	}` |
|      7 |  3804 | `	if( pAttr == 0 ){` |
|      - |  3805 | `		/* Absent (or a DTD default): php returns false */` |
|      3 |  3806 | `		ph7_result_bool(pCtx,0);` |
|      3 |  3807 | `		return PH7_OK;` |
|      - |  3808 | `	}` |
|      5 |  3809 | `	xmlRemoveProp(pAttr);` |
|      5 |  3810 | `	ph7_result_bool(pCtx,1);` |
|      5 |  3811 | `	return PH7_OK;` |
|      6 |  3812 | `}` |
|      - |  3813 | `/*` |
|      - |  3814 | `` * A `?string $namespace` argument as libxml wants it: NULL for php's null,`` |
|      - |  3815 | ` * which is how a caller asks for the attribute in NO namespace, and the bytes` |
|      - |  3816 | ` * otherwise.  Handing libxml "" for a null instead is not the same question --` |
|      - |  3817 | ` * "" matches a namespace whose URI is the empty string, which no document has,` |
|      - |  3818 | `` * so `getAttributeNS(null, 'href')` answered "" for every plain attribute.`` |
|      - |  3819 | ` */` |
|    132 |  3820 | `static const xmlChar * DomArgUri(int nArg,ph7_value **apArg,int iArg)` |
|      1 |  3821 | `{` |
|    133 |  3822 | `	const char *z = DomArgStrOrNull(nArg,apArg,iArg);` |
|    133 |  3823 | `	return (const xmlChar *)z;` |
|      1 |  3824 | `}` |
|      - |  3825 | `/* DOMElement::getAttributeNS(?string $namespace, string $localName): string */` |
|     20 |  3826 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNS)` |
|      1 |  3827 | `{` |
|     21 |  3828 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     21 |  3829 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     21 |  3830 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     21 |  3831 | `	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,zUri) : 0;` |
|     21 |  3832 | `	if( zVal == 0 && pNd && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|      - |  3833 | `		/* The other door, the one hasAttributeNS already knew about: a` |
|      - |  3834 | ``		 * DECLARATION answers its URI here. `getAttributeNS($XMLNS, 'p')` was ""`` |
|      - |  3835 | ``		 * on an element declaring `xmlns:p`, where php answers the namespace. */`` |
|      9 |  3836 | `		xmlNsPtr pDecl = DomNsDeclOf((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal);` |
|      9 |  3837 | `		if( pDecl && pDecl->href ){` |
|      3 |  3838 | `			ph7_result_string(pCtx,(const char *)pDecl->href,-1);` |
|      3 |  3839 | `			return PH7_OK;` |
|      - |  3840 | `		}` |
|      3 |  3841 | `	}` |
|     19 |  3842 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|     19 |  3843 | `	if( zVal ){` |
|     11 |  3844 | `		xmlFree(zVal);` |
|      5 |  3845 | `	}` |
|     19 |  3846 | `	return PH7_OK;` |
|     11 |  3847 | `}` |
|      - |  3848 | `/* DOMElement::setAttributeNS(?string $namespace, string $qualifiedName, string $value): void */` |
|     66 |  3849 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNS)` |
|      1 |  3850 | `{` |
|     67 |  3851 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     67 |  3852 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|     67 |  3853 | `	const char *zQname = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|     67 |  3854 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|     67 |  3855 | `	int bHasUri = zUri != 0 && zUri[0] != 0;` |
|      - |  3856 | `	xmlNodePtr pNode;` |
|     67 |  3857 | `	xmlNsPtr pNs = 0;` |
|      - |  3858 | `	dom_qname sQ;` |
|      - |  3859 | `	int rc,bDecl,nOldDefs;` |
|     67 |  3860 | `	if( pNd == 0 ){` |
|    ! 0 |  3861 | `		return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  3862 | `	}` |
|     67 |  3863 | `	if( zQname[0] == 0 ){` |
|      - |  3864 | `		/* php screens the EMPTY name at the parameter, before the DOM sees it. */` |
|      3 |  3865 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  3866 | `			"DOMElement::setAttributeNS(): Argument #2 ($qualifiedName) must not be empty");` |
|      - |  3867 | `	}` |
|     65 |  3868 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_SET,&sQ);` |
|     65 |  3869 | `	if( rc ){` |
|     17 |  3870 | `		return DomThrowVoid(pCtx,rc);` |
|      - |  3871 | `	}` |
|     49 |  3872 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|     49 |  3873 | `	nOldDefs = DomNsDefCount(pNode);` |
|      - |  3874 | `	/* The DECLARATION spelling is the xmlns NAMESPACE plus an xmlns name --` |
|      - |  3875 | ``	 * `xmlns:z` binds z, plain `xmlns` binds the default one. Everything else`` |
|      - |  3876 | ``	 * is an ordinary attribute, including a bare `xmlns` under some other URI:`` |
|      - |  3877 | ``	 * php writes it as an ATTRIBUTE whose name happens to be `xmlns`, which`` |
|      - |  3878 | `	 * serializes beside the declaration already there. */` |
|     57 |  3879 | `	bDecl = DomUriIs(zUri,DOM_XMLNS_NS_URI)` |
|     53 |  3880 | `		&& (sQ.zPrefix ? xmlStrEqual(sQ.zPrefix,(const xmlChar *)"xmlns")` |
|     10 |  3881 | `		               : xmlStrEqual(sQ.zLocal,(const xmlChar *)"xmlns"));` |
|     49 |  3882 | `	if( bHasUri && !bDecl ){` |
|      - |  3883 | `		/* An ordinary attribute: find or declare the namespace it names. The` |
|      - |  3884 | `		 * xmlns URI is not special here -- php declares it like any other,` |
|      - |  3885 | `		 * EXCEPT under a prefix, which is the one binding it will not write.` |
|      - |  3886 | ``		 * The same goes for the prefix `xmlns` itself: php will REUSE a binding`` |
|      - |  3887 | ``		 * for it (which is how `setAttributeNS(XML_NS,'xmlns:z')` ends up as`` |
|      - |  3888 | ``		 * `xml:z`) and refuses to declare one. */`` |
|     35 |  3889 | `		if( sQ.zPrefix && DomUriIs(zUri,DOM_XMLNS_NS_URI) ){` |
|      3 |  3890 | `			DomQNameRelease(&sQ);` |
|      3 |  3891 | `			return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  3892 | `		}` |
|     33 |  3893 | `		pNs = DomNsResolve(pNode,zUri,sQ.zPrefix,0);` |
|     33 |  3894 | `		if( pNs == 0 ){` |
|      5 |  3895 | `			DomQNameRelease(&sQ);` |
|      5 |  3896 | `			return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  3897 | `		}` |
|     14 |  3898 | `	}` |
|     43 |  3899 | `	if( bDecl ){` |
|      - |  3900 | ``		/* The declaration spelling: `xmlns:z` binds z, plain `xmlns` binds the`` |
|      - |  3901 | `		 * default namespace, and the VALUE is the URI being bound. */` |
|     13 |  3902 | `		DomNsDeclare(pNode,sQ.zPrefix ? sQ.zLocal : 0,zVal);` |
|      7 |  3903 | `	}else{` |
|     31 |  3904 | `		xmlSetNsProp(pNode,pNs,sQ.zLocal,(const xmlChar *)zVal);` |
|      - |  3905 | `	}` |
|      - |  3906 | `	/* Either path may have declared something here -- and a declaration, new or` |
|      - |  3907 | `	 * rebound, can take the spelling away from what is already below it. */` |
|     43 |  3908 | `	if( bDecl \|\| DomNsDefCount(pNode) != nOldDefs ){` |
|     27 |  3909 | `		DomNsReconcile(pNode);` |
|     13 |  3910 | `	}` |
|     43 |  3911 | `	DomQNameRelease(&sQ);` |
|     43 |  3912 | `	return PH7_OK;` |
|     34 |  3913 | `}` |
|      - |  3914 |  |
|      - |  3915 | `/* ===== Attribute NODES ===== */` |
|      - |  3916 |  |
|      - |  3917 | `/*` |
|      - |  3918 | ` * The half of the attribute surface that hands out (and takes) the attribute` |
|      - |  3919 | ` * NODE rather than its string.  It is how a program moves an attribute between` |
|      - |  3920 | ` * elements, reads one it has held across an edit, or asks which attributes an` |
|      - |  3921 | ``  * element carries at all -- and none of it existed here, so `getAttributeNode` `` |
|      - |  3922 | `` * was a `Call to undefined method` and every idiom built on it stopped at the`` |
|      - |  3923 | ` * first line.` |
|      - |  3924 | ` */` |
|      - |  3925 |  |
|      - |  3926 | `/* The xmlAttr a DOMAttr-typed argument stands for (already screened by ZPP:` |
|      - |  3927 | ` * anything that is not a DOMAttr never reaches the body). */` |
|     84 |  3928 | `static xmlAttrPtr DomAttrArg(ph7_value *pVal)` |
|      1 |  3929 | `{` |
|     85 |  3930 | `	phl_domnode *pNd = DomObjArg(pVal);` |
|     85 |  3931 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     85 |  3932 | `	return (pNode && pNode->type == XML_ATTRIBUTE_NODE) ? (xmlAttrPtr)pNode : 0;` |
|      1 |  3933 | `}` |
|      - |  3934 | `/* libxml's own name-only question, which the getAttributeNode family asks.` |
|      - |  3935 | ` * php 8.5.9's setAttributeNode asked it too; 8.5.10 matches the namespace as` |
|      - |  3936 | ` * well, so the displacement doors no longer come here. */` |
|     48 |  3937 | `static xmlAttrPtr DomAttrByLocal(xmlNodePtr pElem,const char *zName)` |
|      1 |  3938 | `{` |
|     49 |  3939 | `	xmlAttrPtr pAttr = pElem ? xmlHasProp(pElem,(const xmlChar *)zName) : 0;` |
|     49 |  3940 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|      1 |  3941 | `}` |
|      - |  3942 | `/* Append an attribute to an element's property list. The list is its own chain` |
|      - |  3943 | ` * (pElem->properties), not the child chain, which is why DomLinkLast will not` |
|      - |  3944 | ` * do: an attribute spliced among the CHILDREN serializes inside the tag body. */` |
|     72 |  3945 | `static void DomAttrLinkLast(xmlNodePtr pElem,xmlAttrPtr pAttr)` |
|      2 |  3946 | `{` |
|     74 |  3947 | `	xmlAttrPtr pLast = pElem->properties;` |
|     74 |  3948 | `	pAttr->parent = pElem;` |
|     74 |  3949 | `	pAttr->doc = pElem->doc;` |
|     74 |  3950 | `	pAttr->next = 0;` |
|     74 |  3951 | `	if( pLast == 0 ){` |
|     58 |  3952 | `		pElem->properties = pAttr;` |
|     58 |  3953 | `		pAttr->prev = 0;` |
|     58 |  3954 | `		return;` |
|      - |  3955 | `	}` |
|     23 |  3956 | `	while( pLast->next ){` |
|      7 |  3957 | `		pLast = pLast->next;` |
|      1 |  3958 | `	}` |
|     17 |  3959 | `	pLast->next = pAttr;` |
|     17 |  3960 | `	pAttr->prev = pLast;` |
|     38 |  3961 | `}` |
|      - |  3962 | `/* ...and at a POSITION: before pRef, which must be one of pElem's own --` |
|      - |  3963 | ` * insertBefore against an attribute reference, and replaceChild's` |
|      - |  3964 | ` * attribute-for-attribute swap, the two places php lets a caller state the` |
|      - |  3965 | ` * property list's order. */` |
|      6 |  3966 | `static void DomAttrLinkBefore(xmlNodePtr pElem,xmlAttrPtr pAttr,xmlAttrPtr pRef)` |
|      1 |  3967 | `{` |
|      7 |  3968 | `	pAttr->parent = pElem;` |
|      7 |  3969 | `	pAttr->doc = pElem->doc;` |
|      7 |  3970 | `	pAttr->next = pRef;` |
|      7 |  3971 | `	pAttr->prev = pRef->prev;` |
|      7 |  3972 | `	if( pRef->prev ){` |
|    ! 0 |  3973 | `		pRef->prev->next = pAttr;` |
|    ! 0 |  3974 | `	}else{` |
|      7 |  3975 | `		pElem->properties = pAttr;` |
|      - |  3976 | `	}` |
|      7 |  3977 | `	pRef->prev = pAttr;` |
|      7 |  3978 | `}` |
|      - |  3979 | `/* Detach an attribute from its element (or the orphan set) without freeing it:` |
|      - |  3980 | ` * php hands the caller back the node it displaced, alive. */` |
|     86 |  3981 | `static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr)` |
|      2 |  3982 | `{` |
|     88 |  3983 | `	if( pAttr->parent ){` |
|     31 |  3984 | `		xmlUnlinkNode((xmlNodePtr)pAttr);` |
|     16 |  3985 | `	}else{` |
|     58 |  3986 | `		DomOrphanRemove(pShell,(xmlNodePtr)pAttr);` |
|      - |  3987 | `	}` |
|     88 |  3988 | `}` |
|      - |  3989 | `/* DOMElement::getAttributeNode(string $qualifiedName): DOMAttr\|false -- FALSE` |
|      - |  3990 | ` * for an absent one, where the NS spelling below answers null. */` |
|    114 |  3991 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNode)` |
|      3 |  3992 | `{` |
|    117 |  3993 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    117 |  3994 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    117 |  3995 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    117 |  3996 | `	xmlAttrPtr pAttr = DomAttrByName(pElem,zName);` |
|    117 |  3997 | `	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|    117 |  3998 | `	if( pDecl ){` |
|     17 |  3999 | `		return DomResultNsNode(pCtx,pNd,pDecl,pElem);` |
|      - |  4000 | `	}` |
|    101 |  4001 | `	if( pAttr == 0 ){` |
|      7 |  4002 | `		ph7_result_bool(pCtx,0);` |
|      7 |  4003 | `		return PH7_OK;` |
|      - |  4004 | `	}` |
|     95 |  4005 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     60 |  4006 | `}` |
|      - |  4007 | `/* DOMElement::getAttributeNodeNS(?string $namespace, string $localName): ?DOMAttr */` |
|     34 |  4008 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNodeNS)` |
|      1 |  4009 | `{` |
|     35 |  4010 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     35 |  4011 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     35 |  4012 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     35 |  4013 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     35 |  4014 | `	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|      - |  4015 | `		/* Here the LOCAL name is the prefix being declared -- and the DEFAULT` |
|      - |  4016 | ``		 * declaration, whose local name would be `xmlns`, is not reachable this`` |
|      - |  4017 | `		 * way at all. */` |
|      5 |  4018 | `		xmlNsPtr pDecl = DomNsDeclOf(pElem,(const xmlChar *)zLocal);` |
|      5 |  4019 | `		if( pDecl == 0 ){` |
|      3 |  4020 | `			ph7_result_null(pCtx);` |
|      3 |  4021 | `			return PH7_OK;` |
|      - |  4022 | `		}` |
|      3 |  4023 | `		return DomResultNsNode(pCtx,pNd,pDecl,pElem);` |
|      - |  4024 | `	}` |
|     31 |  4025 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)DomAttrByNs(pElem,zUri,zLocal));` |
|     18 |  4026 | `}` |
|      - |  4027 | `/*` |
|      - |  4028 | ` * DOMElement::setAttributeNode(DOMAttr $attr): ?DOMAttr and its NS spelling.` |
|      - |  4029 | ` *` |
|      - |  4030 | ` * php answers the attribute it DISPLACED (alive and ownerless) or null. Since` |
|      - |  4031 | ` * 8.5.10 both spellings decide "the same attribute" the same way, on the local` |
|      - |  4032 | ` * name and the namespace URI together (8.5.9's plain spelling matched the name` |
|      - |  4033 | `` * alone, so a plain `k` displaced a namespaced `p:k`).  An attribute that already belongs to`` |
|      - |  4034 | ` * another element of the same document is MOVED, not copied.` |
|      - |  4035 | ` */` |
|     64 |  4036 | `static int DomSetAttrNode(ph7_context *pCtx,int nArg,ph7_value **apArg,int bNS)` |
|      1 |  4037 | `{` |
|     65 |  4038 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     65 |  4039 | `	xmlAttrPtr pAttr = nArg > 0 ? DomAttrArg(apArg[0]) : 0;` |
|     65 |  4040 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  4041 | `	xmlAttrPtr pOld;` |
|     65 |  4042 | `	if( pElem == 0 \|\| pAttr == 0 ){` |
|    ! 0 |  4043 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  4044 | `	}` |
|     65 |  4045 | `	if( pAttr->doc != pElem->doc && pAttr->doc != 0 ){` |
|      7 |  4046 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  4047 | `	}` |
|      - |  4048 | `	/* A constructed, document-less attribute is ADOPTED, like every insertion` |
|      - |  4049 | ``	 * door -- `$el->setAttributeNode(new DOMAttr('k','v'))` is how a built`` |
|      - |  4050 | `	 * attribute reaches a real document. */` |
|     59 |  4051 | `	DomAdoptIntoRecv(pCtx,apArg[0],DomObjArg(apArg[0]));` |
|     59 |  4052 | `	pOld = DomAttrByNs(pElem,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name);` |
|     29 |  4053 | `	SXUNUSED(bNS);` |
|     59 |  4054 | `	if( pOld == pAttr ){` |
|      - |  4055 | `		/* Already this element's, under this spelling: php does nothing at all` |
|      - |  4056 | `		 * and answers null rather than handing the node back to itself. */` |
|      3 |  4057 | `		ph7_result_null(pCtx);` |
|      3 |  4058 | `		return PH7_OK;` |
|      - |  4059 | `	}` |
|     57 |  4060 | `	if( pOld ){` |
|      5 |  4061 | `		xmlUnlinkNode((xmlNodePtr)pOld);` |
|      5 |  4062 | `		DomOrphanAdd(pNd->pShell,(xmlNodePtr)pOld);` |
|      2 |  4063 | `	}` |
|     57 |  4064 | `	DomAttrDetach(pNd->pShell,pAttr);` |
|     57 |  4065 | `	DomAttrLinkLast(pElem,pAttr);` |
|     57 |  4066 | `	DomNsAttrArrive(pElem,pAttr);` |
|     57 |  4067 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pOld);` |
|     33 |  4068 | `}` |
|     18 |  4069 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNode)` |
|      1 |  4070 | `{` |
|     19 |  4071 | `	return DomSetAttrNode(pCtx,nArg,apArg,0);` |
|      1 |  4072 | `}` |
|     46 |  4073 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNodeNS)` |
|      1 |  4074 | `{` |
|     47 |  4075 | `	return DomSetAttrNode(pCtx,nArg,apArg,1);` |
|      1 |  4076 | `}` |
|      - |  4077 | `/* DOMElement::removeAttributeNode(DOMAttr $attr): DOMAttr -- an attribute that` |
|      - |  4078 | ` * is not THIS element's is php's Not Found, whichever element owns it. */` |
|     10 |  4079 | `DOM_METHOD(vm_builtin_DOMElement_removeAttributeNode)` |
|      1 |  4080 | `{` |
|     11 |  4081 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     11 |  4082 | `	xmlAttrPtr pAttr = nArg > 0 ? DomAttrArg(apArg[0]) : 0;` |
|     11 |  4083 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     11 |  4084 | `	if( pElem == 0 \|\| pAttr == 0 \|\| pAttr->parent != pElem ){` |
|      9 |  4085 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  4086 | `	}` |
|      3 |  4087 | `	xmlUnlinkNode((xmlNodePtr)pAttr);` |
|      3 |  4088 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|      3 |  4089 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|      6 |  4090 | `}` |
|      - |  4091 | `/* The qualified name a serializer would write for an attribute. */` |
|     20 |  4092 | `static int DomAttrQName(xmlAttrPtr pAttr,SyBlob *pOut)` |
|      1 |  4093 | `{` |
|     21 |  4094 | `	if( pAttr->ns && pAttr->ns->prefix ){` |
|      3 |  4095 | `		SyBlobAppend(pOut,(const void *)pAttr->ns->prefix,SyStrlen((const char *)pAttr->ns->prefix));` |
|      3 |  4096 | `		SyBlobAppend(pOut,(const void *)":",sizeof(char));` |
|      1 |  4097 | `	}` |
|     21 |  4098 | `	SyBlobAppend(pOut,(const void *)pAttr->name,SyStrlen((const char *)pAttr->name));` |
|     21 |  4099 | `	return PH7_OK;` |
|      1 |  4100 | `}` |
|      - |  4101 | `/* DOMElement::getAttributeNames(): array -- the qualified names, in document` |
|      - |  4102 | ` * order, as a LIST (php re-keys from zero). */` |
|     18 |  4103 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNames)` |
|      1 |  4104 | `{` |
|     19 |  4105 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     19 |  4106 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|     19 |  4107 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|      - |  4108 | `	xmlAttrPtr pAttr;` |
|      - |  4109 | `	xmlNsPtr pNs;` |
|      9 |  4110 | `	SXUNUSED(nArg);` |
|      9 |  4111 | `	SXUNUSED(apArg);` |
|     19 |  4112 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  4113 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4114 | `	}` |
|      - |  4115 | `	/* php lists the element's own DECLARATIONS first, in the order it makes` |
|      - |  4116 | `	 * them, and the attributes after. */` |
|     35 |  4117 | `	for( pNs = pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE` |
|     60 |  4118 | `			? ((xmlNodePtr)pNd->pNode)->nsDef : 0 ; pNs ; pNs = pNs->next ){` |
|      - |  4119 | `		SyBlob sName;` |
|     15 |  4120 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|     15 |  4121 | `		SyBlobAppend(&sName,(const void *)DOM_XMLNS_NAME,SyStrlen(DOM_XMLNS_NAME));` |
|     15 |  4122 | `		if( pNs->prefix ){` |
|      9 |  4123 | `			SyBlobAppend(&sName,(const void *)":",sizeof(char));` |
|      9 |  4124 | `			SyBlobAppend(&sName,(const void *)pNs->prefix,SyStrlen((const char *)pNs->prefix));` |
|      4 |  4125 | `		}` |
|     15 |  4126 | `		ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|     15 |  4127 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     15 |  4128 | `		ph7_value_reset_string_cursor(pVal);` |
|     15 |  4129 | `		SyBlobRelease(&sName);` |
|      8 |  4130 | `	}` |
|     39 |  4131 | `	for( pAttr = DomAttrList(pNd ? (xmlNodePtr)pNd->pNode : 0) ; pAttr ; pAttr = pAttr->next ){` |
|      - |  4132 | `		SyBlob sName;` |
|     21 |  4133 | `		if( pAttr->type != XML_ATTRIBUTE_NODE ){` |
|    ! 0 |  4134 | `			continue;` |
|      - |  4135 | `		}` |
|     21 |  4136 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|     21 |  4137 | `		DomAttrQName(pAttr,&sName);` |
|     21 |  4138 | `		ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|     21 |  4139 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     21 |  4140 | `		ph7_value_reset_string_cursor(pVal);` |
|     21 |  4141 | `		SyBlobRelease(&sName);` |
|     11 |  4142 | `	}` |
|     19 |  4143 | `	ph7_result_value(pCtx,pArray);` |
|     19 |  4144 | `	return PH7_OK;` |
|     10 |  4145 | `}` |
|      - |  4146 | `/* DOMElement::hasAttributeNS(?string $namespace, string $localName): bool */` |
|     22 |  4147 | `DOM_METHOD(vm_builtin_DOMElement_hasAttributeNS)` |
|      1 |  4148 | `{` |
|     23 |  4149 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     23 |  4150 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     23 |  4151 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     23 |  4152 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     23 |  4153 | `	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|      - |  4154 | `		/*` |
|      - |  4155 | `		 * Two doors onto that namespace, and php answers about EITHER: a` |
|      - |  4156 | `		 * DECLARATION, which is not an attribute in libxml at all, and a real` |
|      - |  4157 | `` 		 * attribute in it -- which is what `createAttributeNS($XMLNS, ...)` `` |
|      - |  4158 | `		 * makes, and which this only asked the first door about. (A DEFAULT` |
|      - |  4159 | `		 * declaration is not one of them: php answers false for the local name` |
|      - |  4160 | ``		 * `xmlns`, and the prefix comparison below never matches it.)`` |
|      - |  4161 | `		 */` |
|     29 |  4162 | `		ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0` |
|     14 |  4163 | `			\|\| DomNsDeclOf(pElem,(const xmlChar *)zLocal) != 0);` |
|     17 |  4164 | `		return PH7_OK;` |
|      - |  4165 | `	}` |
|      7 |  4166 | `	ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0);` |
|      7 |  4167 | `	return PH7_OK;` |
|     12 |  4168 | `}` |
|      - |  4169 | `/* DOMElement::removeAttributeNS(?string $namespace, string $localName): void --` |
|      - |  4170 | ` * an absent one is silence, as php's is. */` |
|      6 |  4171 | `DOM_METHOD(vm_builtin_DOMElement_removeAttributeNS)` |
|      1 |  4172 | `{` |
|      7 |  4173 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  4174 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      7 |  4175 | `	xmlAttrPtr pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,DomArgUri(nArg,apArg,0),zLocal) : 0;` |
|      7 |  4176 | `	if( pAttr ){` |
|      5 |  4177 | `		xmlRemoveProp(pAttr);` |
|      2 |  4178 | `	}` |
|      7 |  4179 | `	return PH7_OK;` |
|      1 |  4180 | `}` |
|      - |  4181 | `/*` |
|      - |  4182 | ` * DOMElement::toggleAttribute(string $qualifiedName, ?bool $force = null): bool` |
|      - |  4183 | ` *` |
|      - |  4184 | ` * php's 8.3 verb: with no $force it flips (removing answers false, adding a` |
|      - |  4185 | ` * value-less attribute answers true), and with one it only ADDS or only` |
|      - |  4186 | ` * REMOVES -- an add that finds the attribute already there leaves its value` |
|      - |  4187 | ` * alone.  The name is validated first, so a bad one is refused before the` |
|      - |  4188 | ` * element is touched.` |
|      - |  4189 | ` */` |
|     20 |  4190 | `DOM_METHOD(vm_builtin_DOMElement_toggleAttribute)` |
|      1 |  4191 | `{` |
|     21 |  4192 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     21 |  4193 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     21 |  4194 | `	int bForceGiven = nArg > 1 && !ph7_value_is_null(apArg[1]);` |
|     21 |  4195 | `	int bForce = bForceGiven && ph7_value_to_bool(apArg[1]);` |
|     21 |  4196 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  4197 | `	xmlAttrPtr pAttr;` |
|      - |  4198 | `	xmlNsPtr pDecl;` |
|     21 |  4199 | `	if( pElem == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      5 |  4200 | `		return DomThrowAlways(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  4201 | `	}` |
|     17 |  4202 | `	pAttr = DomAttrByName(pElem,zName);` |
|     17 |  4203 | `	pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|     17 |  4204 | `	if( bForceGiven ? bForce : (pAttr == 0 && pDecl == 0) ){` |
|      7 |  4205 | `		if( pAttr == 0 && pDecl == 0 ){` |
|      5 |  4206 | `			sxu32 nXmlns = (sxu32)SyStrlen(DOM_XMLNS_NAME);` |
|      6 |  4207 | `			int bXmlnsName = SyStrlen(zName) >= nXmlns` |
|      3 |  4208 | `				&& SyStrncmp(zName,DOM_XMLNS_NAME,nXmlns) == 0` |
|      6 |  4209 | `				&& (zName[nXmlns] == 0 \|\| zName[nXmlns] == ':');` |
|      5 |  4210 | `			if( bXmlnsName ){` |
|      - |  4211 | `				/* An xmlns name toggled ON becomes a DECLARATION bound to the` |
|      - |  4212 | `				 * empty URI, not an attribute -- which is why it comes out` |
|      - |  4213 | `				 * before the attributes rather than after them. */` |
|      3 |  4214 | `				DomNsDeclare(pElem,zName[nXmlns] == ':' ? (const xmlChar *)(zName+nXmlns+1) : 0,"");` |
|      2 |  4215 | `			}else{` |
|      3 |  4216 | `				xmlSetProp(pElem,(const xmlChar *)zName,(const xmlChar *)"");` |
|      - |  4217 | `			}` |
|      2 |  4218 | `		}` |
|      7 |  4219 | `		ph7_result_bool(pCtx,1);` |
|      7 |  4220 | `		return PH7_OK;` |
|      - |  4221 | `	}` |
|     11 |  4222 | `	if( pAttr ){` |
|      5 |  4223 | `		xmlRemoveProp(pAttr);` |
|      7 |  4224 | `	}else if( pDecl ){` |
|      5 |  4225 | `		DomNsDeclRemove(pElem,pDecl);` |
|      5 |  4226 | `		DomNsReconcile(pElem);` |
|      2 |  4227 | `	}` |
|      9 |  4228 | `	ph7_result_bool(pCtx,0);` |
|      9 |  4229 | `	return PH7_OK;` |
|      9 |  4230 | `}` |
|      - |  4231 | `/*` |
|      - |  4232 | `` * The ID three.  php's `setIdAttribute*` is what makes an attribute the one`` |
|      - |  4233 | `` * `getElementById()` answers by, in a document with no DTD to say so, and`` |
|      - |  4234 | `` * `DOMAttr::isId()` is how a caller reads the flag back.  All three refuse a`` |
|      - |  4235 | ` * name the element does not carry with Not Found -- including an attribute that` |
|      - |  4236 | ` * belongs to a DIFFERENT element, which is why the node spelling checks the` |
|      - |  4237 | ` * owner rather than trusting the argument.` |
|      - |  4238 | ` */` |
|     26 |  4239 | `static int DomMarkId(xmlAttrPtr pAttr,int bIsId)` |
|      1 |  4240 | `{` |
|     27 |  4241 | `	if( bIsId ){` |
|     23 |  4242 | `		if( pAttr->atype != XML_ATTRIBUTE_ID ){` |
|     23 |  4243 | `			xmlChar *zVal = xmlNodeListGetString(pAttr->doc,pAttr->children,1);` |
|     23 |  4244 | `			if( zVal ){` |
|     23 |  4245 | `				xmlAddID(0,pAttr->doc,zVal,pAttr);` |
|     23 |  4246 | `				xmlFree(zVal);` |
|     11 |  4247 | `			}` |
|     11 |  4248 | `		}` |
|     23 |  4249 | `		pAttr->atype = XML_ATTRIBUTE_ID;` |
|     12 |  4250 | `	}else{` |
|      5 |  4251 | `		if( pAttr->atype == XML_ATTRIBUTE_ID ){` |
|      5 |  4252 | `			xmlRemoveID(pAttr->doc,pAttr);` |
|      2 |  4253 | `		}` |
|      5 |  4254 | `		pAttr->atype = (xmlAttributeType)0;` |
|      - |  4255 | `	}` |
|     27 |  4256 | `	return PH7_OK;` |
|      1 |  4257 | `}` |
|     26 |  4258 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttribute)` |
|      1 |  4259 | `{` |
|     27 |  4260 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     27 |  4261 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|      - |  4262 | `	/* php's ID lookup is the STRICT one -- the whole name in NO namespace, with` |
|      - |  4263 | ``	 * no prefix resolution, so `setIdAttribute('p:k')` is Not Found even on an`` |
|      - |  4264 | ``	 * element that carries `p:k`. */`` |
|     27 |  4265 | `	xmlAttrPtr pAttr = pNd ? DomAttrNoNs((xmlNodePtr)pNd->pNode,zName) : 0;` |
|     27 |  4266 | `	if( pAttr == 0 ){` |
|      9 |  4267 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  4268 | `	}` |
|     19 |  4269 | `	return DomMarkId(pAttr,nArg > 1 && ph7_value_to_bool(apArg[1]));` |
|     14 |  4270 | `}` |
|      - |  4271 | `/* php's second parameter is spelled $qualifiedName and matched as a LOCAL one:` |
|      - |  4272 | ` * the namespace decides the rest. */` |
|      8 |  4273 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttributeNS)` |
|      1 |  4274 | `{` |
|      9 |  4275 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      9 |  4276 | `	const char *zLocal = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|      9 |  4277 | `	xmlAttrPtr pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,DomArgUri(nArg,apArg,0),zLocal) : 0;` |
|      9 |  4278 | `	if( pAttr == 0 ){` |
|      5 |  4279 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  4280 | `	}` |
|      5 |  4281 | `	return DomMarkId(pAttr,nArg > 2 && ph7_value_to_bool(apArg[2]));` |
|      5 |  4282 | `}` |
|     10 |  4283 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttributeNode)` |
|      1 |  4284 | `{` |
|     11 |  4285 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     11 |  4286 | `	xmlAttrPtr pAttr = nArg > 1 ? DomAttrArg(apArg[0]) : 0;` |
|     11 |  4287 | `	if( pNd == 0 \|\| pAttr == 0 \|\| pAttr->parent != (xmlNodePtr)pNd->pNode ){` |
|      7 |  4288 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  4289 | `	}` |
|      5 |  4290 | `	return DomMarkId(pAttr,nArg > 1 && ph7_value_to_bool(apArg[1]));` |
|      6 |  4291 | `}` |
|      - |  4292 | `/* DOMAttr::isId(): bool */` |
|     42 |  4293 | `DOM_METHOD(vm_builtin_DOMAttr_isId)` |
|      1 |  4294 | `{` |
|     43 |  4295 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     43 |  4296 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     21 |  4297 | `	SXUNUSED(nArg);` |
|     21 |  4298 | `	SXUNUSED(apArg);` |
|     85 |  4299 | `	ph7_result_bool(pCtx,pNode != 0 && pNode->type == XML_ATTRIBUTE_NODE` |
|     42 |  4300 | `		&& ((xmlAttrPtr)pNode)->atype == XML_ATTRIBUTE_ID);` |
|     43 |  4301 | `	return PH7_OK;` |
|      1 |  4302 | `}` |
|      - |  4303 | `/*` |
|      - |  4304 | ` * DOMDocument::getElementById(string $elementId): ?DOMElement` |
|      - |  4305 | ` *` |
|      - |  4306 | `` * The other half of the ID three: what `setIdAttribute()` is FOR. libxml keeps`` |
|      - |  4307 | `` * the table (a DTD `ATTLIST ... ID` fills it at parse time, `xmlAddID` fills it`` |
|      - |  4308 | ` * when a program marks one), and php answers the attribute's element -- but` |
|      - |  4309 | ` * only while that element is still IN the document, so an element removed from` |
|      - |  4310 | ` * the tree stops being findable even though its attribute still carries the` |
|      - |  4311 | ` * flag. A DTD-declared DEFAULT has no attribute node behind it and libxml says` |
|      - |  4312 | ` * so with its own sentinel.` |
|      - |  4313 | ` */` |
|     44 |  4314 | `DOM_METHOD(vm_builtin_DOMDocument_getElementById)` |
|      1 |  4315 | `{` |
|     45 |  4316 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     45 |  4317 | `	const char *zId = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     45 |  4318 | `	xmlAttrPtr pAttr = pNd ? xmlGetID((xmlDocPtr)pNd->pNode,(const xmlChar *)zId) : 0;` |
|     44 |  4319 | `	if( pAttr == 0 \|\| pAttr == (xmlAttrPtr)-1 \|\| pAttr->type != XML_ATTRIBUTE_NODE` |
|     26 |  4320 | `	 \|\| pAttr->parent == 0 \|\| !DomIsConnected(pAttr->parent) ){` |
|     23 |  4321 | `		ph7_result_null(pCtx);` |
|     23 |  4322 | `		return PH7_OK;` |
|      - |  4323 | `	}` |
|     23 |  4324 | `	return DomResultNodeOf(pCtx,pNd,pAttr->parent);` |
|     23 |  4325 | `}` |
|      - |  4326 | `/* DOMDocument::createAttribute(string $localName): DOMAttr -- ownerless: php` |
|      - |  4327 | ` * gives it this document but NO element until it is set on one. */` |
|     66 |  4328 | `DOM_METHOD(vm_builtin_DOMDocument_createAttribute)` |
|      2 |  4329 | `{` |
|     68 |  4330 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     68 |  4331 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|      - |  4332 | `	xmlAttrPtr pAttr;` |
|     68 |  4333 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      9 |  4334 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  4335 | `	}` |
|     60 |  4336 | `	pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,(const xmlChar *)zName,0);` |
|     60 |  4337 | `	if( pAttr == 0 ){` |
|    ! 0 |  4338 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4339 | `	}` |
|     60 |  4340 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|     60 |  4341 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     35 |  4342 | `}` |
|      - |  4343 | `/*` |
|      - |  4344 | ` * DOMDocument::createAttributeNS(?string $namespace, string $qualifiedName): DOMAttr` |
|      - |  4345 | ` *` |
|      - |  4346 | ` * The namespace is declared on the document's ROOT ELEMENT, not on the` |
|      - |  4347 | ` * attribute -- which is why a document that has no root element yet cannot` |
|      - |  4348 | ` * answer at all, and says so with php's warning and a false.` |
|      - |  4349 | ` */` |
|     42 |  4350 | `DOM_METHOD(vm_builtin_DOMDocument_createAttributeNS)` |
|      1 |  4351 | `{` |
|     43 |  4352 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     43 |  4353 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|     43 |  4354 | `	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  4355 | `	xmlNodePtr pRoot;` |
|      - |  4356 | `	xmlAttrPtr pAttr;` |
|      - |  4357 | `	dom_qname sQ;` |
|      - |  4358 | `	int rc;` |
|     43 |  4359 | `	if( pNd == 0 ){` |
|    ! 0 |  4360 | `		return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  4361 | `	}` |
|     43 |  4362 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_ATTR,&sQ);` |
|     43 |  4363 | `	if( rc ){` |
|     17 |  4364 | `		return DomThrow(pCtx,rc);` |
|      - |  4365 | `	}` |
|     27 |  4366 | `	pRoot = xmlDocGetRootElement((xmlDocPtr)pNd->pNode);` |
|     27 |  4367 | `	if( pRoot == 0 ){` |
|      3 |  4368 | `		DomQNameRelease(&sQ);` |
|      - |  4369 | ``		/* The context prints php's `DOMDocument::createAttributeNS(): ` itself. */`` |
|      3 |  4370 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Document Missing Root Element");` |
|      3 |  4371 | `		ph7_result_bool(pCtx,0);` |
|      3 |  4372 | `		return PH7_OK;` |
|      - |  4373 | `	}` |
|     25 |  4374 | `	pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,sQ.zLocal,0);` |
|     25 |  4375 | `	if( pAttr == 0 ){` |
|    ! 0 |  4376 | `		DomQNameRelease(&sQ);` |
|    ! 0 |  4377 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4378 | `	}` |
|     25 |  4379 | `	if( zUri && zUri[0] && sQ.zPrefix ){` |
|     15 |  4380 | `		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,sQ.zPrefix,1));` |
|     18 |  4381 | `	}else if( zUri && zUri[0]` |
|     11 |  4382 | `	 && xmlStrEqual(sQ.zLocal,(const xmlChar *)DOM_XMLNS_NAME) ){` |
|      - |  4383 | `		/*` |
|      - |  4384 | ``		 * The unprefixed `xmlns` -- the only name the grammar lets through with`` |
|      - |  4385 | `		 * the xmlns namespace, and the name a DEFAULT declaration is written` |
|      - |  4386 | `		 * with. It IS in that namespace and php answers so, but nothing is` |
|      - |  4387 | `		 * DECLARED for it: the binding is the attribute's alone, so it is parked` |
|      - |  4388 | `		 * on the document, which is what frees it. Without this the attribute` |
|      - |  4389 | ``		 * answered namespaceURI null, `hasAttributeNS($XMLNS, 'xmlns')` was`` |
|      - |  4390 | ``		 * false for it once written, and `getAttribute('xmlns')` -- a by-NAME`` |
|      - |  4391 | `		 * lookup, which skips a namespaced attribute -- answered its value where` |
|      - |  4392 | `		 * php answers "".` |
|      - |  4393 | `		 */` |
|      5 |  4394 | `		xmlNsPtr pNs = DomNsReuse(pRoot,zUri);` |
|      5 |  4395 | `		if( pNs == 0 ){` |
|      3 |  4396 | `			pNs = xmlNewNs(0,(const xmlChar *)zUri,0);` |
|      3 |  4397 | `			if( pNs ){` |
|      3 |  4398 | `				DomNsPark((xmlNodePtr)pAttr,pNs);` |
|      1 |  4399 | `			}` |
|      1 |  4400 | `		}` |
|      5 |  4401 | `		if( pNs ){` |
|      5 |  4402 | `			xmlSetNs((xmlNodePtr)pAttr,pNs);` |
|      3 |  4403 | `		}` |
|      9 |  4404 | `	}else if( zUri && zUri[0] ){` |
|      5 |  4405 | `		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,0,1));` |
|      2 |  4406 | `	}` |
|     25 |  4407 | `	DomQNameRelease(&sQ);` |
|     25 |  4408 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|     25 |  4409 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     22 |  4410 | `}` |
|      - |  4411 |  |
|      - |  4412 | `/* ===== getElementsByTagName (live) ===== */` |
|      - |  4413 |  |
|      - |  4414 | `/* Length-carrying: the name comes from a declared string SLOT, whose bytes are` |
|      - |  4415 | ` * NOT NUL-terminated (PH7_NativeAttrStr borrows the blob as-is). */` |
|   1314 |  4416 | `static int DomLenEq(const xmlChar *zHave,const char *zWant,int nWant)` |
|      1 |  4417 | `{` |
|   1870 |  4418 | `	return zHave != 0 && (int)SyStrlen((const char *)zHave) == nWant` |
|   1971 |  4419 | `		&& SyMemcmp((const void *)zHave,(const void *)zWant,(sxu32)nWant) == 0;` |
|      1 |  4420 | `}` |
|      - |  4421 | `/*` |
|      - |  4422 | ` * One element against a (namespace, local name) pair. nUri < 0 is the name-only` |
|      - |  4423 | `` * query, `getElementsByTagName` -- the sentinel is the LENGTH and not the`` |
|      - |  4424 | ` * pointer because an empty declared string slot reads back as a NULL one, which` |
|      - |  4425 | ` * is exactly the namespace-aware "in NO namespace" case. Under the` |
|      - |  4426 | ` * namespace-aware query php's three cases are NOT symmetric, and that asymmetry` |
|      - |  4427 | ` * is the whole content of the rule:` |
|      - |  4428 | ` *` |
|      - |  4429 | `` *   `*`          every element, in a namespace or in none`` |
|      - |  4430 | ` *   null or ""   only the elements in NO namespace (php maps its null argument` |
|      - |  4431 | ` *                and the empty string to the same question)` |
|      - |  4432 | ` *   a URI        only the elements in it` |
|      - |  4433 | ` *` |
|      - |  4434 | `` * The local name is `*` for every name, and an exact match otherwise -- against`` |
|      - |  4435 | `` * libxml's `name`, which is the LOCAL name, so a prefix never enters into it.`` |
|      - |  4436 | ` */` |
|   1874 |  4437 | `static int DomGebtnMatch(xmlNodePtr pNode,const char *zUri,int nUri,` |
|      - |  4438 | `	const char *zName,int nName)` |
|      1 |  4439 | `{` |
|   1875 |  4440 | `	if( pNode->type != XML_ELEMENT_NODE ){` |
|     47 |  4441 | `		return 0;` |
|      - |  4442 | `	}` |
|   1829 |  4443 | `	if( !(nName == 1 && zName[0] == '*') && !DomLenEq(pNode->name,zName,nName) ){` |
|    581 |  4444 | `		return 0;` |
|      - |  4445 | `	}` |
|   1249 |  4446 | `	if( nUri < 0 \|\| (nUri == 1 && zUri[0] == '*') ){` |
|    375 |  4447 | `		return 1;` |
|      - |  4448 | `	}` |
|    503 |  4449 | `	if( nUri == 0 ){` |
|    193 |  4450 | `		return pNode->ns == 0;` |
|      - |  4451 | `	}` |
|    311 |  4452 | `	return pNode->ns != 0 && DomLenEq(pNode->ns->href,zUri,nUri);` |
|    752 |  4453 | `}` |
|      - |  4454 | `/* The list is LIVE: nothing is snapshotted, both queries re-walk the subtree` |
|      - |  4455 | ` * every time DOMNodeList asks. Passing iWant < 0 counts instead of indexing. */` |
|    338 |  4456 | `static xmlNodePtr DomGebtnWalk(xmlNodePtr pRoot,const char *zUri,int nUri,` |
|      - |  4457 | `	const char *zName,int nName,int iWant,int *pnCount)` |
|      1 |  4458 | `{` |
|    339 |  4459 | `	xmlNodePtr pCur = pRoot ? pRoot->children : 0;` |
|    339 |  4460 | `	int iCount = 0;` |
|   1633 |  4461 | `	while( pCur ){` |
|   1503 |  4462 | `		if( DomGebtnMatch(pCur,zUri,nUri,zName,nName) ){` |
|    547 |  4463 | `			if( iWant >= 0 && iCount == iWant ){` |
|    209 |  4464 | `				return pCur;` |
|      - |  4465 | `			}` |
|    339 |  4466 | `			iCount++;` |
|    169 |  4467 | `		}` |
|   1295 |  4468 | `		pCur = DomWalkNext(pCur,pRoot);` |
|      1 |  4469 | `	}` |
|    131 |  4470 | `	if( pnCount ){` |
|     71 |  4471 | `		*pnCount = iCount;` |
|     35 |  4472 | `	}` |
|    131 |  4473 | `	return 0;` |
|    170 |  4474 | `}` |
|      - |  4475 |  |
|      - |  4476 | `/* ===== DOMDocument ===== */` |
|      - |  4477 |  |
|      - |  4478 | `/*` |
|      - |  4479 | ` * DOMDocument::__construct(string $version = '1.0', string $encoding = '')` |
|      - |  4480 | ` *` |
|      - |  4481 | `` * The chunk reached the document through `parent::__construct(__dom_doc_new(..))`;`` |
|      - |  4482 | ` * a native constructor writes its own two slots, and $__doc is the document` |
|      - |  4483 | ` * ITSELF (php's ownerDocument is null on a document, which __get answers).` |
|      - |  4484 | ` */` |
|   1664 |  4485 | `DOM_METHOD(vm_builtin_DOMDocument_construct)` |
|      5 |  4486 | `{` |
|   1669 |  4487 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1669 |  4488 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1669 |  4489 | `	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";` |
|   1669 |  4490 | `	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  4491 | `	xmlDocPtr pDoc;` |
|      - |  4492 | `	phl_xmldoc *pShell;` |
|      - |  4493 | `	phl_domnode *pRes;` |
|   1669 |  4494 | `	if( pThis == 0 ){` |
|    ! 0 |  4495 | `		return PH7_OK;` |
|      - |  4496 | `	}` |
|      - |  4497 | ``	/* The version goes through as WRITTEN -- `new DOMDocument('')` is a document`` |
|      - |  4498 | ``	 * whose `version` reads "" and whose declaration says `version=""`, which is`` |
|      - |  4499 | `	 * php's answer (only an omitted argument takes the "1.0" default, and that` |
|      - |  4500 | `	 * one is the signature's). */` |
|   1669 |  4501 | `	pDoc = xmlNewDoc((const xmlChar *)zVersion);` |
|   1669 |  4502 | `	if( pDoc == 0 ){` |
|    ! 0 |  4503 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4504 | `	}` |
|   1669 |  4505 | `	if( zEncoding[0] ){` |
|      5 |  4506 | `		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);` |
|      2 |  4507 | `	}` |
|   1669 |  4508 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|   1669 |  4509 | `	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|   1669 |  4510 | `	if( pRes == 0 ){` |
|    ! 0 |  4511 | `		if( pShell == 0 ){` |
|    ! 0 |  4512 | `			xmlFreeDoc(pDoc);` |
|    ! 0 |  4513 | `		}` |
|    ! 0 |  4514 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4515 | `	}` |
|   1669 |  4516 | `	DomSetRes(pVm,pThis,pRes);` |
|   1669 |  4517 | `	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);` |
|   1669 |  4518 | `	return PH7_OK;` |
|    837 |  4519 | `}` |
|      - |  4520 | `/*` |
|      - |  4521 | ` * The URI php stamps on a document parsed from MEMORY.` |
|      - |  4522 | ` *` |
|      - |  4523 | ` * A file parse takes its URI from the file; a memory parse has none, and php` |
|      - |  4524 | ` * gives it the process's CURRENT DIRECTORY with a trailing separator so that a` |
|      - |  4525 | `` * relative `xml:base` (and every `baseURI` under it) resolves against the same`` |
|      - |  4526 | ` * place a relative include would. The path is the bytes getcwd() answers, not a` |
|      - |  4527 | ` * URI: a space stays a space. Asks the VFS rather than the C library so the` |
|      - |  4528 | ` * win32 backend answers its own spelling.` |
|      - |  4529 | ` *` |
|      - |  4530 | ` * The answer travels through the context's RESULT slot, which is where the VFS` |
|      - |  4531 | ` * writes it -- every caller sets its own return value afterwards.` |
|      - |  4532 | ` */` |
|   1562 |  4533 | `static void DomCwdUri(ph7_context *pCtx,SyBlob *pOut)` |
|      5 |  4534 | `{` |
|   1567 |  4535 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|   1567 |  4536 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|   1567 |  4537 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   1562 |  4538 | `	if( pVfs && pVfs->xGetcwd && pVfs->xGetcwd(pCtx) == PH7_OK` |
|   1567 |  4539 | `	 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0 ){` |
|   1567 |  4540 | `		const char *zDir = (const char *)SyBlobData(&pCtx->pRet->sBlob);` |
|   1567 |  4541 | `		sxu32 nDir = SyBlobLength(&pCtx->pRet->sBlob);` |
|   1567 |  4542 | `		SyBlobAppend(pOut,zDir,nDir);` |
|   1567 |  4543 | `		if( nDir < 1 \|\| (zDir[nDir - 1] != '/' && zDir[nDir - 1] != '\\') ){` |
|   1567 |  4544 | `			SyBlobAppend(pOut,"/",sizeof(char));` |
|    781 |  4545 | `		}` |
|    781 |  4546 | `	}` |
|   1567 |  4547 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   1567 |  4548 | `	SyBlobNullAppend(pOut);` |
|   1567 |  4549 | `}` |
|      - |  4550 | `/* Stamp it, unless the parse already gave the document one. */` |
|   1572 |  4551 | `static void DomStampCwd(ph7_context *pCtx,xmlDocPtr pDoc)` |
|      5 |  4552 | `{` |
|      - |  4553 | `	SyBlob sDir;` |
|   1577 |  4554 | `	if( pDoc == 0 \|\| pDoc->URL ){` |
|     15 |  4555 | `		return;` |
|      - |  4556 | `	}` |
|   1563 |  4557 | `	DomCwdUri(pCtx,&sDir);` |
|   1563 |  4558 | `	if( SyBlobLength(&sDir) > 0 ){` |
|   1563 |  4559 | `		pDoc->URL = xmlStrdup((const xmlChar *)SyBlobData(&sDir));` |
|    779 |  4560 | `	}` |
|   1563 |  4561 | `	SyBlobRelease(&sDir);` |
|    791 |  4562 | `}` |
|      - |  4563 | `/*` |
|      - |  4564 | ` * The parse options one of php's load methods actually runs with: the caller's` |
|      - |  4565 | `` * `$options`, OR'd with what the document's own directives ask for.`` |
|      - |  4566 | ` *` |
|      - |  4567 | ` *   preserveWhiteSpace = false  ->  NOBLANKS   (drop ignorable whitespace)` |
|      - |  4568 | ` *   substituteEntities = true   ->  NOENT      (expand entity references)` |
|      - |  4569 | ` *   validateOnParse    = true   ->  DTDVALID   (validate against the DTD)` |
|      - |  4570 | ` *   resolveExternals   = true   ->  DTDATTR    (apply the DTD's default` |
|      - |  4571 | ` *                                               attributes -- which is also` |
|      - |  4572 | ` *                                               what makes libxml LOAD an` |
|      - |  4573 | ` *                                               external subset)` |
|      - |  4574 | ` *   recover            = true   ->  RECOVER    (keep what parsed)` |
|      - |  4575 | ` *` |
|      - |  4576 | ` * The two directions never cancel: a directive can only ADD to the argument,` |
|      - |  4577 | `` * which is why `loadXML($s, LIBXML_NOENT)` expands entities on a document whose`` |
|      - |  4578 | `` * `substituteEntities` is false.`` |
|      - |  4579 | ` */` |
|   1584 |  4580 | `static int DomParseOptions(ph7_class_instance *pThis,int iOpts)` |
|      5 |  4581 | `{` |
|   1589 |  4582 | `	if( pThis == 0 ){` |
|    ! 0 |  4583 | `		return iOpts;` |
|      - |  4584 | `	}` |
|   1589 |  4585 | `	if( !DomDocFlag(pThis,DOM_F_PRESERVE_WS) ){` |
|     19 |  4586 | `		iOpts \|= XML_PARSE_NOBLANKS;` |
|      9 |  4587 | `	}` |
|   1589 |  4588 | `	if( DomDocFlag(pThis,DOM_F_SUBST_ENT) ){` |
|     11 |  4589 | `		iOpts \|= XML_PARSE_NOENT;` |
|      5 |  4590 | `	}` |
|   1589 |  4591 | `	if( DomDocFlag(pThis,DOM_F_VALIDATE) ){` |
|     39 |  4592 | `		iOpts \|= XML_PARSE_DTDVALID;` |
|     19 |  4593 | `	}` |
|   1589 |  4594 | `	if( DomDocFlag(pThis,DOM_F_RESOLVE_EXT) ){` |
|      9 |  4595 | `		iOpts \|= XML_PARSE_DTDATTR;` |
|      4 |  4596 | `	}` |
|   1589 |  4597 | `	if( DomDocFlag(pThis,DOM_F_RECOVER) ){` |
|     13 |  4598 | `		iOpts \|= XML_PARSE_RECOVER;` |
|      6 |  4599 | `	}` |
|   1589 |  4600 | `	return iOpts;` |
|    797 |  4601 | `}` |
|      - |  4602 | `/*` |
|      - |  4603 | `` * A RECOVERING parse reports its diagnostics whatever `error_reporting()` says.`` |
|      - |  4604 | ` *` |
|      - |  4605 | ` * php forces E_WARNING back into the mask for the duration of a parse it is` |
|      - |  4606 | ` * recovering from -- the point being that a document which came back DAMAGED` |
|      - |  4607 | ` * must not do so in silence, however the script has configured reporting. It` |
|      - |  4608 | ` * forces that one level only: a libxml WARNING (an E_NOTICE) stays suppressed.` |
|      - |  4609 | ` * Answers the previous state, which the caller restores.` |
|      - |  4610 | ` */` |
|      - |  4611 | `typedef struct { sxi32 iMask; int bOn; } phl_dom_errsave;` |
|   1584 |  4612 | `static phl_dom_errsave DomForceWarnings(ph7_vm *pVm,int bRecover)` |
|      5 |  4613 | `{` |
|      - |  4614 | `	phl_dom_errsave sSave;` |
|   1589 |  4615 | `	sSave.iMask = pVm->iErrMask;` |
|   1589 |  4616 | `	sSave.bOn = pVm->bErrReport;` |
|   1589 |  4617 | `	if( bRecover ){` |
|     15 |  4618 | `		pVm->iErrMask \|= E_WARNING;` |
|     15 |  4619 | `		pVm->bErrReport = 1;` |
|      7 |  4620 | `	}` |
|   1589 |  4621 | `	return sSave;` |
|      5 |  4622 | `}` |
|   1584 |  4623 | `static void DomRestoreWarnings(ph7_vm *pVm,phl_dom_errsave sSave)` |
|      5 |  4624 | `{` |
|   1589 |  4625 | `	pVm->iErrMask = sSave.iMask;` |
|   1589 |  4626 | `	pVm->bErrReport = sSave.bOn;` |
|   1589 |  4627 | `}` |
|      - |  4628 | `/*` |
|      - |  4629 | ` * The path php names a loaded file by: the VFS's canonical absolute name, or` |
|      - |  4630 | ` * the working directory joined to it when the file does not exist and there is` |
|      - |  4631 | ` * nothing to canonicalize. Answers it NUL-terminated in *pOut.` |
|      - |  4632 | ` */` |
|     18 |  4633 | `static void DomAbsPath(ph7_context *pCtx,const char *zFile,SyBlob *pOut)` |
|      1 |  4634 | `{` |
|     19 |  4635 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     14 |  4636 | `	int bAbs = zFile[0] == '/' \|\| zFile[0] == '\\'` |
|     20 |  4637 | `		\|\| (zFile[0] && zFile[1] == ':');   /* the win32 spelling */` |
|     19 |  4638 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|     19 |  4639 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     18 |  4640 | `	if( pVfs && pVfs->xRealpath && pVfs->xRealpath(zFile,pCtx) == PH7_OK` |
|     14 |  4641 | `	 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0` |
|     11 |  4642 | `	 && SyBlobLength(&pCtx->pRet->sBlob) > 0 ){` |
|     11 |  4643 | `		SyBlobAppend(pOut,SyBlobData(&pCtx->pRet->sBlob),SyBlobLength(&pCtx->pRet->sBlob));` |
|      6 |  4644 | `	}else{` |
|      - |  4645 | `		/* Not there to canonicalize -- but php still resolves the part that IS` |
|      - |  4646 | `		 * there (expand_filepath walks each existing component through its` |
|      - |  4647 | `		 * links), so a missing file under a linked directory is named by the` |
|      - |  4648 | `		 * directory's real path; on macOS every temp path is one (/var ->` |
|      - |  4649 | `		 * /private/var). The longest existing prefix is resolved and the missing` |
|      - |  4650 | `		 * tail kept as written. A bare drive ("C:") is never a prefix: it would` |
|      - |  4651 | `		 * resolve to that drive's working directory. */` |
|      - |  4652 | `		SyBlob sFull,sPre;` |
|      - |  4653 | `		const char *zFull;` |
|      9 |  4654 | `		sxu32 nFull,i,nTail = 0;` |
|      9 |  4655 | `		SyBlobInit(&sFull,&pCtx->pVm->sAllocator);` |
|      9 |  4656 | `		SyBlobInit(&sPre,&pCtx->pVm->sAllocator);` |
|      9 |  4657 | `		if( !bAbs ){` |
|      - |  4658 | `			SyBlob sDir;` |
|      4 |  4659 | `			DomCwdUri(pCtx,&sDir);` |
|      4 |  4660 | `			SyBlobAppend(&sFull,SyBlobData(&sDir),SyBlobLength(&sDir));` |
|      4 |  4661 | `			SyBlobRelease(&sDir);` |
|      2 |  4662 | `		}` |
|      9 |  4663 | `		SyBlobAppend(&sFull,zFile,(sxu32)SyStrlen(zFile));` |
|      9 |  4664 | `		SyBlobNullAppend(&sFull);` |
|      9 |  4665 | `		zFull = (const char *)SyBlobData(&sFull);` |
|      9 |  4666 | `		nFull = (sxu32)SyStrlen(zFull);` |
|    173 |  4667 | `		for( i = nFull ; i > 1 && pVfs && pVfs->xRealpath ; --i ){` |
|    173 |  4668 | `			if( (zFull[i-1] != '/' && zFull[i-1] != '\\') \|\| zFull[i-2] == ':' ){` |
|    153 |  4669 | `				continue;` |
|      - |  4670 | `			}` |
|     21 |  4671 | `			SyBlobReset(&sPre);` |
|     21 |  4672 | `			SyBlobAppend(&sPre,zFull,i-1);` |
|     21 |  4673 | `			SyBlobNullAppend(&sPre);` |
|     21 |  4674 | `			PH7_MemObjRelease(pCtx->pRet);` |
|     20 |  4675 | `			if( pVfs->xRealpath((const char *)SyBlobData(&sPre),pCtx) == PH7_OK` |
|     14 |  4676 | `			 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0` |
|      9 |  4677 | `			 && SyBlobLength(&pCtx->pRet->sBlob) > 0 ){` |
|      9 |  4678 | `				SyBlobAppend(pOut,SyBlobData(&pCtx->pRet->sBlob),SyBlobLength(&pCtx->pRet->sBlob));` |
|      9 |  4679 | `				nTail = nFull - (i-1);` |
|      9 |  4680 | `				break;` |
|      - |  4681 | `			}` |
|      6 |  4682 | `		}` |
|      9 |  4683 | `		if( nTail > 0 ){` |
|      9 |  4684 | `			SyBlobAppend(pOut,zFull + (nFull - nTail),nTail);` |
|      5 |  4685 | `		}else{` |
|    ! 0 |  4686 | `			SyBlobAppend(pOut,zFull,nFull);` |
|      - |  4687 | `		}` |
|      9 |  4688 | `		SyBlobRelease(&sPre);` |
|      9 |  4689 | `		SyBlobRelease(&sFull);` |
|      - |  4690 | `	}` |
|     19 |  4691 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     19 |  4692 | `	SyBlobNullAppend(pOut);` |
|     19 |  4693 | `}` |
|      - |  4694 | `/*` |
|      - |  4695 | ` * Point the receiver at a freshly parsed tree, as both load methods do: the` |
|      - |  4696 | ` * document object keeps its identity and everything under the OLD tree becomes` |
|      - |  4697 | ` * stale, so the per-document wrapper cache is dropped with it. Answers 0 when` |
|      - |  4698 | `` * there is no tree to install, which is each method's `false`; the previous`` |
|      - |  4699 | ` * tree is left alone in that case, as php leaves it.` |
|      - |  4700 | ` */` |
|   1604 |  4701 | `static int DomInstallParsed(ph7_context *pCtx,ph7_class_instance *pThis,xmlDocPtr pDoc)` |
|      5 |  4702 | `{` |
|   1609 |  4703 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1609 |  4704 | `	phl_xmldoc *pShell = pDoc ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;` |
|   1609 |  4705 | `	phl_domnode *pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|      - |  4706 | `	ph7_value *pNodes;` |
|   1609 |  4707 | `	if( pRes == 0 ){` |
|     15 |  4708 | `		if( pDoc && pShell == 0 ){` |
|    ! 0 |  4709 | `			xmlFreeDoc(pDoc);` |
|    ! 0 |  4710 | `		}` |
|     15 |  4711 | `		return 0;` |
|      - |  4712 | `	}` |
|   1595 |  4713 | `	DomSetRes(pVm,pThis,pRes);` |
|   1595 |  4714 | `	pNodes = PH7_NativeAttr(pThis,DOM_NODES);` |
|   1595 |  4715 | `	if( pNodes ){` |
|   1595 |  4716 | `		PH7_MemObjRelease(pNodes);` |
|   1595 |  4717 | `		PH7_MemObjToHashmap(pNodes);` |
|    795 |  4718 | `	}` |
|   1595 |  4719 | `	return 1;` |
|    807 |  4720 | `}` |
|      - |  4721 | `/* DOMDocument::loadXML(string $source, int $options = 0): bool -- the receiver` |
|      - |  4722 | ` * is REPOINTED at a new tree, so its identity cache is dropped with it. */` |
|   1574 |  4723 | `DOM_METHOD(vm_builtin_DOMDocument_loadXML)` |
|      5 |  4724 | `{` |
|   1579 |  4725 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1579 |  4726 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1579 |  4727 | `	int nLen = 0;` |
|   1579 |  4728 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";` |
|   1579 |  4729 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|      - |  4730 | `	phl_dom_errsave sErr;` |
|      - |  4731 | `	xmlDocPtr pDoc;` |
|      - |  4732 | `	sxu32 nMark;` |
|   1579 |  4733 | `	if( pThis == 0 ){` |
|    ! 0 |  4734 | `		return PH7_OK;` |
|      - |  4735 | `	}` |
|   1579 |  4736 | `	if( nLen < 1 ){` |
|      3 |  4737 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  4738 | `			"DOMDocument::loadXML(): Argument #1 ($source) must not be empty");` |
|      - |  4739 | `	}` |
|   1577 |  4740 | `	iOpts = DomParseOptions(pThis,iOpts);` |
|   1577 |  4741 | `	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);` |
|   1577 |  4742 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   1577 |  4743 | `	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);` |
|   1577 |  4744 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,"DOMDocument::loadXML",iOpts);` |
|   1577 |  4745 | `	DomRestoreWarnings(pVm,sErr);` |
|   1577 |  4746 | `	DomStampCwd(pCtx,pDoc);` |
|   1577 |  4747 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|   1577 |  4748 | `	return PH7_OK;` |
|    792 |  4749 | `}` |
|      - |  4750 | `/*` |
|      - |  4751 | ` * DOMDocument::load(string $filename, int $options = 0): bool` |
|      - |  4752 | ` *` |
|      - |  4753 | ` * The same parse as loadXML from a FILE, and php reads that file through its` |
|      - |  4754 | ` * own stream layer (which is what makes a wrapper and a userland stream valid` |
|      - |  4755 | ` * destinations there, and what this does too) while letting libxml word the` |
|      - |  4756 | ` * failure. What only a differential decides:` |
|      - |  4757 | ` *` |
|      - |  4758 | `` *   * a file that is not THERE is libxml's own `I/O warning : failed to load`` |
|      - |  4759 | `` *     external entity "<path>"` and nothing else, while one that exists and`` |
|      - |  4760 | ` *     cannot be opened ALSO gets php's stream warning in front of it;` |
|      - |  4761 | ``  *   * the document's URI is the RESOLVED absolute path -- so `load('a/../b.xml')` `` |
|      - |  4762 | ` *     answers the canonical name -- and it is a URI, not a path: a space in it` |
|      - |  4763 | `` *     comes back as `%20`;`` |
|      - |  4764 | ` *   * a failed load leaves the receiver's previous tree exactly where it was.` |
|      - |  4765 | ` */` |
|      - |  4766 | `/*` |
|      - |  4767 | ` * Read a file for one of the two file-loading methods: the bytes into *pBody,` |
|      - |  4768 | ` * the name libxml is to know it by into *pPath. Answers 0 when the file could` |
|      - |  4769 | ` * not be read at all, with php's diagnostics already raised -- the receiver is` |
|      - |  4770 | ` * left alone then, and the method answers false.` |
|      - |  4771 | ` */` |
|      - |  4772 | `static int DomReadFileAs(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|      - |  4773 | `	SyBlob *pBody,SyBlob *pPath,int bVerbatim);` |
|     18 |  4774 | `PH7_PRIVATE int PH7_DomReadFile(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|      - |  4775 | `	SyBlob *pBody,SyBlob *pPath)` |
|      1 |  4776 | `{` |
|     19 |  4777 | `	return DomReadFileAs(pCtx,zFile,nFile,zFn,pBody,pPath,0);` |
|      1 |  4778 | `}` |
|      - |  4779 | `/*` |
|      - |  4780 | ` * bVerbatim: libxml is to know the file by the path AS WRITTEN rather than by` |
|      - |  4781 | ` * its canonical absolute name -- loadHTMLFile's rule (see its call).` |
|      - |  4782 | ` */` |
|     24 |  4783 | `static int DomReadFileAs(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|      - |  4784 | `	SyBlob *pBody,SyBlob *pPath,int bVerbatim)` |
|      1 |  4785 | `{` |
|     25 |  4786 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  4787 | `	const ph7_io_stream *pStream;` |
|      - |  4788 | `	void *pHandle;` |
|     25 |  4789 | `	if( bVerbatim ){` |
|      7 |  4790 | `		SyBlobInit(pPath,&pVm->sAllocator);` |
|      7 |  4791 | `		SyBlobAppend(pPath,zFile,(sxu32)nFile);` |
|      7 |  4792 | `		SyBlobNullAppend(pPath);` |
|      4 |  4793 | `	}else{` |
|     19 |  4794 | `		DomAbsPath(pCtx,zFile,pPath);` |
|      - |  4795 | `	}` |
|     25 |  4796 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|      - |  4797 | `	/* php hands its document loaders the context libxml_set_streams_context()` |
|      - |  4798 | ``	 * left, which is what lets a `load('http://…')` carry a script's own headers`` |
|      - |  4799 | `	 * and user agent. The slot was stored and answered and READ BY NOTHING until` |
|      - |  4800 | `	 * there was an http:// wrapper to read it; a file:// open ignores it exactly` |
|      - |  4801 | `	 * as php's does. */` |
|     25 |  4802 | `	PH7_StreamCtxArm(pVm,PH7_StreamCtxFromValue(&pVm->sXmlStreamsCtx));` |
|     25 |  4803 | `	pHandle = (pStream && pStream->xRead) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|     24 |  4804 | `		PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx)) : 0;` |
|     25 |  4805 | `	if( pHandle == 0 ){` |
|      - |  4806 | `		/* php's stream layer says nothing about a file that is simply absent --` |
|      - |  4807 | `		 * only libxml does, in its own words and with no source location. A file` |
|      - |  4808 | `		 * that IS there and would not open (a mode, a lock) gets both. */` |
|      7 |  4809 | `		const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|      - |  4810 | `		SyBlob sMsg;` |
|      - |  4811 | `		sxu32 nMark;` |
|      7 |  4812 | `		if( pVfs && pVfs->xFileExists && pVfs->xFileExists(zFile) == PH7_OK ){` |
|    ! 0 |  4813 | `			VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 |  4814 | `		}` |
|      7 |  4815 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      7 |  4816 | `		SyBlobFormat(&sMsg,"failed to load external entity \"%s\"\n",` |
|      6 |  4817 | `			(const char *)SyBlobData(pPath));` |
|      7 |  4818 | `		SyBlobNullAppend(&sMsg);` |
|      - |  4819 | `		/* Both of libxml's channels, as php feeds them: the structured copy is` |
|      - |  4820 | ``		 * what `libxml_get_errors()`/`libxml_get_last_error()` answer (level`` |
|      - |  4821 | `		 * WARNING, no file, no line), and the generic one is the text that gets` |
|      - |  4822 | `		 * PRINTED -- with the severity spelled into it and at E_WARNING. */` |
|      7 |  4823 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     10 |  4824 | `		PH7_LibxmlQueueError(pVm,XML_ERR_WARNING,XML_IO_LOAD_ERROR,0,0,` |
|      6 |  4825 | `			(const char *)SyBlobData(&sMsg),0);` |
|      7 |  4826 | `		if( pVm->bLibxmlInternalErr ){` |
|      3 |  4827 | `			xmlSetStructuredErrorFunc(0,0);   /* nothing to drain: it stays queued */` |
|      2 |  4828 | `		}else{` |
|      - |  4829 | `			SyBlob sGen;` |
|      5 |  4830 | `			PH7_LibxmlDropErrors(pVm,nMark);` |
|      5 |  4831 | `			SyBlobInit(&sGen,&pVm->sAllocator);` |
|      5 |  4832 | `			SyBlobFormat(&sGen,"I/O warning : %s",(const char *)SyBlobData(&sMsg));` |
|      5 |  4833 | `			SyBlobNullAppend(&sGen);` |
|      5 |  4834 | `			PH7_LibxmlRaiseGeneric(pVm,zFn,(const char *)SyBlobData(&sGen));` |
|      5 |  4835 | `			SyBlobRelease(&sGen);` |
|      - |  4836 | `		}` |
|      7 |  4837 | `		SyBlobRelease(&sMsg);` |
|      7 |  4838 | `		SyBlobRelease(pPath);` |
|      7 |  4839 | `		return 0;` |
|      - |  4840 | `	}` |
|     19 |  4841 | `	SyBlobInit(pBody,&pVm->sAllocator);` |
|     19 |  4842 | `	PH7_StreamReadWholeFile(pHandle,pStream,pBody);` |
|     19 |  4843 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     19 |  4844 | `	return 1;` |
|     13 |  4845 | `}` |
|     22 |  4846 | `DOM_METHOD(vm_builtin_DOMDocument_load)` |
|      1 |  4847 | `{` |
|     23 |  4848 | `	ph7_vm *pVm = pCtx->pVm;` |
|     23 |  4849 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  4850 | `	const char *zFile;` |
|     23 |  4851 | `	int nFile = 0;` |
|     23 |  4852 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|      - |  4853 | `	phl_dom_errsave sErr;` |
|      - |  4854 | `	SyBlob sBody,sPath;` |
|      - |  4855 | `	xmlDocPtr pDoc;` |
|      - |  4856 | `	sxu32 nMark;` |
|     23 |  4857 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|     23 |  4858 | `	if( pThis == 0 ){` |
|    ! 0 |  4859 | `		return PH7_OK;` |
|      - |  4860 | `	}` |
|     23 |  4861 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|      3 |  4862 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  4863 | `			"DOMDocument::load(): Argument #1 ($filename) must not contain any null bytes");` |
|      - |  4864 | `	}` |
|     21 |  4865 | `	if( nFile < 1 ){` |
|      3 |  4866 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  4867 | `			"DOMDocument::load(): Argument #1 ($filename) must not be empty");` |
|      - |  4868 | `	}` |
|     19 |  4869 | `	if( !PH7_DomReadFile(pCtx,zFile,nFile,"DOMDocument::load",&sBody,&sPath) ){` |
|      5 |  4870 | `		ph7_result_bool(pCtx,0);` |
|      5 |  4871 | `		return PH7_OK;` |
|      - |  4872 | `	}` |
|     15 |  4873 | `	if( SyBlobLength(&sBody) < 1 ){` |
|      - |  4874 | `		/* libxml's memory parser will not even start on nothing, so the error` |
|      - |  4875 | `		 * php's FILE parser raises there is queued by hand -- same level, same` |
|      - |  4876 | ``		 * code, same wording, so `libxml_get_errors()` reports what php's does`` |
|      - |  4877 | `		 * and the drain prints php's sentence. */` |
|      3 |  4878 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      4 |  4879 | `		PH7_LibxmlQueueError(pVm,XML_ERR_FATAL,XML_ERR_DOCUMENT_EMPTY,1,1,` |
|      2 |  4880 | `			"Document is empty\n",(const char *)SyBlobData(&sPath));` |
|      3 |  4881 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::load");` |
|      3 |  4882 | `		SyBlobRelease(&sBody);` |
|      3 |  4883 | `		SyBlobRelease(&sPath);` |
|      3 |  4884 | `		ph7_result_bool(pCtx,0);` |
|      3 |  4885 | `		return PH7_OK;` |
|      - |  4886 | `	}` |
|     13 |  4887 | `	iOpts = DomParseOptions(pThis,iOpts);` |
|     13 |  4888 | `	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);` |
|     13 |  4889 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      - |  4890 | `	/* The path is the parse's URL: libxml turns it into the document's URI and` |
|      - |  4891 | `	 * names it in every diagnostic the parse raises. */` |
|     19 |  4892 | `	pDoc = xmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),` |
|     12 |  4893 | `		(const char *)SyBlobData(&sPath),0,iOpts);` |
|     13 |  4894 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,"DOMDocument::load",iOpts);` |
|     13 |  4895 | `	DomRestoreWarnings(pVm,sErr);` |
|     13 |  4896 | `	SyBlobRelease(&sBody);` |
|     13 |  4897 | `	SyBlobRelease(&sPath);` |
|     13 |  4898 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|     13 |  4899 | `	return PH7_OK;` |
|     12 |  4900 | `}` |
|      - |  4901 | `/*` |
|      - |  4902 | ` * DOMDocument::loadHTML(string $source, int $options = 0): bool` |
|      - |  4903 | ` * DOMDocument::loadHTMLFile(string $filename, int $options = 0): bool` |
|      - |  4904 | ` *` |
|      - |  4905 | ` * The other parser: HTML is not XML and libxml has a second one for it, which` |
|      - |  4906 | `` * closes what the markup left open, supplies the `html`/`body` php's`` |
|      - |  4907 | `` * `LIBXML_HTML_NOIMPLIED` asks it not to, and stamps the DTD`` |
|      - |  4908 | `` * `LIBXML_HTML_NODEFDTD` asks it not to. What comes out is an HTML DOCUMENT --`` |
|      - |  4909 | ` * node type 13, its own serializer -- and the differences from the XML side are` |
|      - |  4910 | ` * measured ones: the document's own directives reach NOTHING here (only` |
|      - |  4911 | `` * `$options` does), a document parsed from a STRING is given no URI at all`` |
|      - |  4912 | ` * (where loadXML stamps the working directory), and there is no well-formedness` |
|      - |  4913 | ` * to fail on, so the answer is true for anything that is not empty.` |
|      - |  4914 | ` */` |
|     26 |  4915 | `static int DomLoadHtml(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile)` |
|      1 |  4916 | `{` |
|     27 |  4917 | `	ph7_vm *pVm = pCtx->pVm;` |
|     27 |  4918 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     27 |  4919 | `	const char *zFn = bFile ? "DOMDocument::loadHTMLFile" : "DOMDocument::loadHTML";` |
|     27 |  4920 | `	const char *zArg = bFile ? "filename" : "source";` |
|      - |  4921 | `	const char *zSrc;` |
|     27 |  4922 | `	int nSrc = 0;` |
|     27 |  4923 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|      - |  4924 | `	SyBlob sBody,sPath;` |
|      - |  4925 | `	xmlDocPtr pDoc;` |
|      - |  4926 | `	sxu32 nMark;` |
|     27 |  4927 | `	zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|     27 |  4928 | `	if( pThis == 0 ){` |
|    ! 0 |  4929 | `		return PH7_OK;` |
|      - |  4930 | `	}` |
|     27 |  4931 | `	if( bFile && nSrc != (int)SyStrlen(zSrc) ){` |
|    ! 0 |  4932 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    ! 0 |  4933 | `			"%s(): Argument #1 ($filename) must not contain any null bytes",zFn);` |
|      - |  4934 | `	}` |
|     27 |  4935 | `	if( nSrc < 1 ){` |
|      7 |  4936 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      2 |  4937 | `			"%s(): Argument #1 ($%s) must not be empty",zFn,zArg);` |
|      - |  4938 | `	}` |
|     23 |  4939 | `	if( bFile ){` |
|      - |  4940 | `		/* loadHTMLFile hands libxml the path AS WRITTEN -- php's` |
|      - |  4941 | `		 * htmlCreateFileParserCtxt(source): no working directory joined and no` |
|      - |  4942 | `		 * link resolved, unlike load(), which canonicalizes first. It is the name` |
|      - |  4943 | `		 * the document's URI and every diagnostic carry. */` |
|      7 |  4944 | `		if( !DomReadFileAs(pCtx,zSrc,nSrc,zFn,&sBody,&sPath,1) ){` |
|      3 |  4945 | `			ph7_result_bool(pCtx,0);` |
|      3 |  4946 | `			return PH7_OK;` |
|      - |  4947 | `		}` |
|      3 |  4948 | `	}else{` |
|      - |  4949 | `		/* A string has no URI: php leaves the document's null. */` |
|     17 |  4950 | `		SyBlobInit(&sBody,&pVm->sAllocator);` |
|     17 |  4951 | `		SyBlobAppend(&sBody,zSrc,(sxu32)nSrc);` |
|     17 |  4952 | `		SyBlobInit(&sPath,&pVm->sAllocator);` |
|     17 |  4953 | `		SyBlobNullAppend(&sPath);` |
|      - |  4954 | `	}` |
|     21 |  4955 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     21 |  4956 | `	if( SyBlobLength(&sBody) > 0 ){` |
|     28 |  4957 | `		pDoc = htmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),` |
|      9 |  4958 | `			bFile ? (const char *)SyBlobData(&sPath) : 0,0,iOpts);` |
|     10 |  4959 | `	}else{` |
|      - |  4960 | `		/* An empty FILE is still a document here, unlike on the XML side: php's` |
|      - |  4961 | ``		 * HTML parser says `Document is empty` and hands back the DTD-only`` |
|      - |  4962 | ``		 * document `htmlNewDoc` builds. libxml's memory parser will not start on`` |
|      - |  4963 | `		 * nothing, so both halves are made by hand. (An empty STRING never gets` |
|      - |  4964 | `		 * this far -- it is the ValueError above.) */` |
|      4 |  4965 | `		PH7_LibxmlQueueError(pVm,XML_ERR_ERROR,XML_ERR_DOCUMENT_EMPTY,1,1,` |
|      2 |  4966 | `			"Document is empty\n",(const char *)SyBlobData(&sPath));` |
|      3 |  4967 | `		pDoc = htmlNewDoc(0,0);` |
|      3 |  4968 | `		if( pDoc ){` |
|      3 |  4969 | `			pDoc->URL = xmlStrdup((const xmlChar *)SyBlobData(&sPath));` |
|      1 |  4970 | `		}` |
|      - |  4971 | `	}` |
|     21 |  4972 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFn,iOpts);` |
|     21 |  4973 | `	SyBlobRelease(&sBody);` |
|     21 |  4974 | `	SyBlobRelease(&sPath);` |
|     21 |  4975 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|     21 |  4976 | `	return PH7_OK;` |
|     14 |  4977 | `}` |
|     18 |  4978 | `DOM_METHOD(vm_builtin_DOMDocument_loadHTML)` |
|      1 |  4979 | `{` |
|     19 |  4980 | `	return DomLoadHtml(pCtx,nArg,apArg,FALSE);` |
|      1 |  4981 | `}` |
|      8 |  4982 | `DOM_METHOD(vm_builtin_DOMDocument_loadHTMLFile)` |
|      1 |  4983 | `{` |
|      9 |  4984 | `	return DomLoadHtml(pCtx,nArg,apArg,TRUE);` |
|      1 |  4985 | `}` |
|      - |  4986 | `/*` |
|      - |  4987 | ` * The two save options php reads, and what they mean to libxml.` |
|      - |  4988 | ` *` |
|      - |  4989 | `` * `LIBXML_NOEMPTYTAG` turns `<e/>` into `<e></e>` and reaches BOTH dumps -- a`` |
|      - |  4990 | `` * node's as much as a document's -- while `LIBXML_NOXMLDECL` only reaches the`` |
|      - |  4991 | ` * whole-document one (a node's output has no declaration to drop). Every other` |
|      - |  4992 | `` * bit of `$options` is ignored, unknown ones included. php spells the first one`` |
|      - |  4993 | ` * with libxml's library-wide switch; this file asks for it per dump instead,` |
|      - |  4994 | ` * which says the same thing without touching global state (and without the` |
|      - |  4995 | ` * deprecated symbol: the MSVC gate refuses it under /WX).` |
|      - |  4996 | ` *` |
|      - |  4997 | ` * Both dumps therefore run through libxml's save API. The DOCUMENT's goes out` |
|      - |  4998 | ` * in the encoding its declaration names -- which is also how a document whose` |
|      - |  4999 | ` * encoding has no converter fails, with no context to write through -- and a` |
|      - |  5000 | ` * NODE's is always UTF-8, as php's is.` |
|      - |  5001 | ` */` |
|      - |  5002 | `#define DOM_SAVE_NOXMLDECL  2` |
|      - |  5003 | `#define DOM_SAVE_NOEMPTYTAG 4` |
|   1100 |  5004 | `static int DomSaveFlags(int bFormat,int iOpts,int bDoc)` |
|      3 |  5005 | `{` |
|      - |  5006 | ``	/* AS_XML because the receiver may be an HTML document (`loadHTML` makes`` |
|      - |  5007 | `	 * one): libxml's save context would hand such a document to the HTML` |
|      - |  5008 | `	 * serializer, and php's XML savers write XML whatever the document is --` |
|      - |  5009 | ``	 * declaration, `<br/>` and all. */`` |
|   1103 |  5010 | `	int iSave = XML_SAVE_AS_XML \| (bFormat ? XML_SAVE_FORMAT : 0);` |
|   1103 |  5011 | `	if( iOpts & DOM_SAVE_NOEMPTYTAG ){` |
|     13 |  5012 | `		iSave \|= XML_SAVE_NO_EMPTY;` |
|      6 |  5013 | `	}` |
|   1103 |  5014 | `	if( bDoc && (iOpts & DOM_SAVE_NOXMLDECL) ){` |
|      5 |  5015 | `		iSave \|= XML_SAVE_NO_DECL;` |
|      2 |  5016 | `	}` |
|   1103 |  5017 | `	return iSave;` |
|      3 |  5018 | `}` |
|      - |  5019 | `/*` |
|      - |  5020 | ` * Serialize a whole document (pNode == 0) or one node the way php's savers do.` |
|      - |  5021 | ` * Answers the bytes in *pzOut (xmlFree'd by the caller) and their count, or -1.` |
|      - |  5022 | ` */` |
|   1100 |  5023 | `static int DomDumpTree(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,int iOpts,xmlChar **pzOut)` |
|      3 |  5024 | `{` |
|   1103 |  5025 | `	xmlBufferPtr pBuf = xmlBufferCreate();` |
|      - |  5026 | `	xmlSaveCtxtPtr pSave;` |
|   1103 |  5027 | `	int nOut = 0;` |
|   1103 |  5028 | `	*pzOut = 0;` |
|   1103 |  5029 | `	if( pBuf == 0 ){` |
|    ! 0 |  5030 | `		return -1;` |
|      - |  5031 | `	}` |
|      - |  5032 | `	/* A NODE's dump is UTF-8 whatever the document declares, and naming that` |
|      - |  5033 | `	 * encoding is also what keeps libxml from ESCAPING every non-ASCII character` |
|      - |  5034 | ``	 * (its no-encoding path writes `&#xE9;`, which is right for a document that`` |
|      - |  5035 | `	 * declares nothing and wrong for a node). A DOCUMENT's goes out in its own` |
|      - |  5036 | `	 * declared encoding, or in that escaping form when it declares none -- which` |
|      - |  5037 | `	 * is what php answers there. */` |
|   1103 |  5038 | `	pSave = xmlSaveToBuffer(pBuf,pNode ? "UTF-8" : (const char *)pDoc->encoding,` |
|    550 |  5039 | `		DomSaveFlags(bFormat,iOpts,pNode == 0));` |
|   1103 |  5040 | `	if( pSave == 0 ){` |
|    ! 0 |  5041 | `		xmlBufferFree(pBuf);` |
|    ! 0 |  5042 | `		return -1;` |
|      - |  5043 | `	}` |
|   1103 |  5044 | `	if( (pNode ? xmlSaveTree(pSave,pNode) : xmlSaveDoc(pSave,pDoc)) < 0 ){` |
|    ! 0 |  5045 | `		nOut = -1;` |
|    ! 0 |  5046 | `	}` |
|   1103 |  5047 | `	if( xmlSaveClose(pSave) < 0 ){` |
|    ! 0 |  5048 | `		nOut = -1;` |
|    ! 0 |  5049 | `	}` |
|   1103 |  5050 | `	if( nOut == 0 ){` |
|   1103 |  5051 | `		nOut = (int)xmlBufferLength(pBuf);` |
|   1103 |  5052 | `		*pzOut = xmlStrndup(xmlBufferContent(pBuf),nOut);` |
|   1103 |  5053 | `		if( *pzOut == 0 ){` |
|    ! 0 |  5054 | `			nOut = -1;` |
|    ! 0 |  5055 | `		}` |
|    550 |  5056 | `	}` |
|   1103 |  5057 | `	xmlBufferFree(pBuf);` |
|   1103 |  5058 | `	return nOut;` |
|    553 |  5059 | `}` |
|      - |  5060 | `/* DOMDocument::saveXML(?DOMNode $node = null, int $options = 0): string\|false */` |
|   1090 |  5061 | `DOM_METHOD(vm_builtin_DOMDocument_saveXML)` |
|      3 |  5062 | `{` |
|   1093 |  5063 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1093 |  5064 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1093 |  5065 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|   1093 |  5066 | `	phl_domnode *pTgt = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|   1093 |  5067 | `	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);` |
|   1093 |  5068 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|      - |  5069 | `	int bWhole;` |
|   1093 |  5070 | `	xmlChar *zOut = 0;` |
|      - |  5071 | `	int nOut;` |
|      - |  5072 | `	sxu32 nMark;` |
|   1093 |  5073 | `	if( pDocNd == 0 ){` |
|    ! 0 |  5074 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5075 | `		return PH7_OK;` |
|      - |  5076 | `	}` |
|   1090 |  5077 | `	if( pTgt && pTgt->pNode` |
|    689 |  5078 | `	 && ((xmlNodePtr)pTgt->pNode)->doc != (xmlDocPtr)pDocNd->pNode ){` |
|      - |  5079 | `		/* Another document's node -- or a constructed one that belongs to none` |
|      - |  5080 | `		 * yet -- is not this document's to serialize. */` |
|      5 |  5081 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  5082 | `	}` |
|   1089 |  5083 | `	bWhole = pTgt == 0 \|\| pTgt->pNode == pDocNd->pNode;` |
|   1089 |  5084 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   1089 |  5085 | `	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,bWhole ? 0 : (xmlNodePtr)pTgt->pNode,` |
|    543 |  5086 | `		bFormat,iOpts,&zOut);` |
|   1089 |  5087 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|   1089 |  5088 | `	if( nOut < 0 ){` |
|      - |  5089 | `		/* php says so rather than answering an empty document: the encoding the` |
|      - |  5090 | `		 * declaration names has no converter and nothing was written. */` |
|    ! 0 |  5091 | `		if( bWhole ){` |
|    ! 0 |  5092 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Could not save document");` |
|    ! 0 |  5093 | `		}` |
|    ! 0 |  5094 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5095 | `		return PH7_OK;` |
|      - |  5096 | `	}` |
|   1089 |  5097 | `	ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|   1089 |  5098 | `	xmlFree(zOut);` |
|   1089 |  5099 | `	return PH7_OK;` |
|    548 |  5100 | `}` |
|      - |  5101 | `/*` |
|      - |  5102 | ` * DOMDocument::saveHTML(?DOMNode $node = null): string\|false` |
|      - |  5103 | ` * DOMDocument::saveHTMLFile(string $filename): int\|false` |
|      - |  5104 | ` *` |
|      - |  5105 | ` * The HTML serializer, which is a different one: a void element comes out` |
|      - |  5106 | `` * `<br>` rather than `<br/>`, a character with an HTML entity name comes out`` |
|      - |  5107 | ` * under that name, and the whole document carries its DOCTYPE and no XML` |
|      - |  5108 | ` * declaration. Neither method takes save OPTIONS -- php declares one parameter` |
|      - |  5109 | `` * each -- but both read `formatOutput`, a NODE's dump included.`` |
|      - |  5110 | ` *` |
|      - |  5111 | ` * A node from ANOTHER document is php's Wrong Document Error (in whichever mode` |
|      - |  5112 | ` * this document is in), which is the only refusal either one has.` |
|      - |  5113 | ` */` |
|     26 |  5114 | `static int DomDumpHtml(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,xmlChar **pzOut)` |
|      1 |  5115 | `{` |
|      - |  5116 | `	xmlBufferPtr pBuf;` |
|      - |  5117 | `	xmlOutputBufferPtr pOut;` |
|      - |  5118 | `	int nOut;` |
|     27 |  5119 | `	*pzOut = 0;` |
|     27 |  5120 | `	if( pNode == 0 ){` |
|     23 |  5121 | `		nOut = 0;` |
|     23 |  5122 | `		htmlDocDumpMemoryFormat(pDoc,pzOut,&nOut,bFormat ? 1 : 0);` |
|     23 |  5123 | `		return *pzOut ? nOut : -1;` |
|      - |  5124 | `	}` |
|      5 |  5125 | `	pBuf = xmlBufferCreate();` |
|      - |  5126 | `	/* The buffer is the write TARGET, not the output buffer's own storage:` |
|      - |  5127 | `	 * closing the latter leaves it to us to free. */` |
|      5 |  5128 | `	pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|      5 |  5129 | `	if( pOut == 0 ){` |
|    ! 0 |  5130 | `		if( pBuf ){` |
|    ! 0 |  5131 | `			xmlBufferFree(pBuf);` |
|    ! 0 |  5132 | `		}` |
|    ! 0 |  5133 | `		return -1;` |
|      - |  5134 | `	}` |
|      5 |  5135 | `	htmlNodeDumpFormatOutput(pOut,pDoc,pNode,0,bFormat ? 1 : 0);` |
|      5 |  5136 | `	xmlOutputBufferFlush(pOut);` |
|      5 |  5137 | `	nOut = (int)xmlBufferLength(pBuf);` |
|      5 |  5138 | `	*pzOut = xmlStrndup(xmlBufferContent(pBuf),nOut);` |
|      5 |  5139 | `	xmlOutputBufferClose(pOut);` |
|      5 |  5140 | `	xmlBufferFree(pBuf);` |
|      5 |  5141 | `	return *pzOut ? nOut : -1;` |
|     14 |  5142 | `}` |
|     32 |  5143 | `static int DomSaveHtml(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile)` |
|      1 |  5144 | `{` |
|     33 |  5145 | `	ph7_vm *pVm = pCtx->pVm;` |
|     33 |  5146 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     33 |  5147 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     33 |  5148 | `	phl_domnode *pTgt = (!bFile && nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|     33 |  5149 | `	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);` |
|     33 |  5150 | `	const ph7_io_stream *pStream = 0;` |
|     33 |  5151 | `	const char *zFile = "";` |
|     33 |  5152 | `	int nFile = 0,nOut;` |
|     33 |  5153 | `	xmlChar *zOut = 0;` |
|      - |  5154 | `	void *pHandle;` |
|      - |  5155 | `	sxu32 nMark;` |
|     33 |  5156 | `	if( bFile ){` |
|      7 |  5157 | `		zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|      7 |  5158 | `		if( nFile != (int)SyStrlen(zFile) ){` |
|    ! 0 |  5159 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  5160 | `				"DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not contain any null bytes");` |
|      - |  5161 | `		}` |
|      7 |  5162 | `		if( nFile < 1 ){` |
|      3 |  5163 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  5164 | `				"DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not be empty");` |
|      - |  5165 | `		}` |
|      2 |  5166 | `	}` |
|     31 |  5167 | `	if( pDocNd == 0 ){` |
|    ! 0 |  5168 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5169 | `		return PH7_OK;` |
|      - |  5170 | `	}` |
|     31 |  5171 | `	if( pTgt && pTgt->pShell != pDocNd->pShell ){` |
|      5 |  5172 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  5173 | `	}` |
|     27 |  5174 | `	if( bFile ){` |
|      - |  5175 | `		/* Writing to a FILE goes through libxml's file saver, which stamps the` |
|      - |  5176 | `` 		 * document with the encoding it is about to use: an `http-equiv` `` |
|      - |  5177 | ``		 * Content-Type meta appears in `<head>` -- in the DOCUMENT, not just in`` |
|      - |  5178 | ``		 * the output, so the next `saveHTML()` shows it too -- and it always`` |
|      - |  5179 | `		 * says UTF-8, whatever the document's own encoding is. php inherits` |
|      - |  5180 | `		 * that; the string saver READS the same meta and adds none. */` |
|      5 |  5181 | `		htmlSetMetaEncoding((xmlDocPtr)pDocNd->pNode,(const xmlChar *)"UTF-8");` |
|      2 |  5182 | `	}` |
|     27 |  5183 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     42 |  5184 | `	nOut = DomDumpHtml((xmlDocPtr)pDocNd->pNode,` |
|     15 |  5185 | `		(pTgt && pTgt->pNode != pDocNd->pNode) ? (xmlNodePtr)pTgt->pNode : 0,bFormat,&zOut);` |
|     27 |  5186 | `	PH7_LibxmlDropErrors(pVm,nMark);` |
|     27 |  5187 | `	if( nOut < 0 ){` |
|    ! 0 |  5188 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5189 | `		return PH7_OK;` |
|      - |  5190 | `	}` |
|     27 |  5191 | `	if( !bFile ){` |
|     23 |  5192 | `		ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|     23 |  5193 | `		xmlFree(zOut);` |
|     23 |  5194 | `		return PH7_OK;` |
|      - |  5195 | `	}` |
|      5 |  5196 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|      5 |  5197 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|      - |  5198 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|      4 |  5199 | `		ph7_function_name(pCtx)) : 0;` |
|      5 |  5200 | `	if( pHandle == 0 ){` |
|      3 |  5201 | `		xmlFree(zOut);` |
|      3 |  5202 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      - |  5203 | `		/* php answers the bytes it WROTE, which is none of them -- not false. */` |
|      3 |  5204 | `		ph7_result_int(pCtx,0);` |
|      3 |  5205 | `		return PH7_OK;` |
|      - |  5206 | `	}` |
|      3 |  5207 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|    ! 0 |  5208 | `		nOut = 0;` |
|    ! 0 |  5209 | `	}` |
|      3 |  5210 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      3 |  5211 | `	xmlFree(zOut);` |
|      3 |  5212 | `	ph7_result_int(pCtx,nOut);` |
|      3 |  5213 | `	return PH7_OK;` |
|     17 |  5214 | `}` |
|     26 |  5215 | `DOM_METHOD(vm_builtin_DOMDocument_saveHTML)` |
|      1 |  5216 | `{` |
|     27 |  5217 | `	return DomSaveHtml(pCtx,nArg,apArg,FALSE);` |
|      1 |  5218 | `}` |
|      6 |  5219 | `DOM_METHOD(vm_builtin_DOMDocument_saveHTMLFile)` |
|      1 |  5220 | `{` |
|      7 |  5221 | `	return DomSaveHtml(pCtx,nArg,apArg,TRUE);` |
|      1 |  5222 | `}` |
|      - |  5223 | `/*` |
|      - |  5224 | ` * DOMDocument::save(string $filename, int $options = 0): int\|false` |
|      - |  5225 | ` *` |
|      - |  5226 | ` * saveXML's bytes written to a file, and the COUNT of them rather than the` |
|      - |  5227 | ` * bytes -- through the stream layer, which is where php's` |
|      - |  5228 | `` * `save(<path>): Failed to open stream: <reason>` comes from. Two rules only a`` |
|      - |  5229 | `` * differential decides: `LIBXML_NOXMLDECL` does NOT reach this one (php reads`` |
|      - |  5230 | ` * it in saveXML only, so a saved document always carries its declaration),` |
|      - |  5231 | ``  * and a document whose declared encoding has no converter is a silent `false` `` |
|      - |  5232 | ` * here where saveXML says "Could not save document".` |
|      - |  5233 | ` */` |
|     18 |  5234 | `DOM_METHOD(vm_builtin_DOMDocument_save)` |
|      1 |  5235 | `{` |
|     19 |  5236 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 |  5237 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     19 |  5238 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|      - |  5239 | `	const ph7_io_stream *pStream;` |
|      - |  5240 | `	const char *zFile;` |
|     19 |  5241 | `	int nFile = 0;` |
|     19 |  5242 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     19 |  5243 | `	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);` |
|      - |  5244 | `	int nOut;` |
|     19 |  5245 | `	xmlChar *zOut = 0;` |
|      - |  5246 | `	void *pHandle;` |
|      - |  5247 | `	sxu32 nMark;` |
|     19 |  5248 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|     19 |  5249 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|      3 |  5250 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  5251 | `			"DOMDocument::save(): Argument #1 ($filename) must not contain any null bytes");` |
|      - |  5252 | `	}` |
|     17 |  5253 | `	if( nFile < 1 ){` |
|      3 |  5254 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  5255 | `			"DOMDocument::save(): Argument #1 ($filename) must not be empty");` |
|      - |  5256 | `	}` |
|     15 |  5257 | `	if( pDocNd == 0 ){` |
|    ! 0 |  5258 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5259 | `		return PH7_OK;` |
|      - |  5260 | `	}` |
|     15 |  5261 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     15 |  5262 | `	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,0,bFormat,iOpts & ~DOM_SAVE_NOXMLDECL,&zOut);` |
|      - |  5263 | `	/* php reports this failure through the return value alone. */` |
|     15 |  5264 | `	PH7_LibxmlDropErrors(pVm,nMark);` |
|     15 |  5265 | `	if( nOut < 0 ){` |
|    ! 0 |  5266 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5267 | `		return PH7_OK;` |
|      - |  5268 | `	}` |
|     15 |  5269 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|     15 |  5270 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|      - |  5271 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|     14 |  5272 | `		ph7_function_name(pCtx)) : 0;` |
|     15 |  5273 | `	if( pHandle == 0 ){` |
|      3 |  5274 | `		xmlFree(zOut);` |
|      3 |  5275 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 |  5276 | `		ph7_result_bool(pCtx,0);` |
|      3 |  5277 | `		return PH7_OK;` |
|      - |  5278 | `	}` |
|     13 |  5279 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|    ! 0 |  5280 | `		nOut = -1;` |
|    ! 0 |  5281 | `	}` |
|     13 |  5282 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     13 |  5283 | `	xmlFree(zOut);` |
|     13 |  5284 | `	if( nOut < 0 ){` |
|    ! 0 |  5285 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5286 | `		return PH7_OK;` |
|      - |  5287 | `	}` |
|     13 |  5288 | `	ph7_result_int(pCtx,nOut);` |
|     13 |  5289 | `	return PH7_OK;` |
|     10 |  5290 | `}` |
|      - |  5291 | `/*` |
|      - |  5292 | ` * The four DOMDocument::create* methods, which differ only in the node kind` |
|      - |  5293 | ` * they ask libxml for. Fresh nodes start as orphans, so a node that is created` |
|      - |  5294 | ` * and never appended is still freed with its document.` |
|      - |  5295 | ` */` |
|    440 |  5296 | `static int DomDocCreate(ph7_context *pCtx,int iKind,const char *zName,const char *zVal,int nVal)` |
|      3 |  5297 | `{` |
|    443 |  5298 | `	ph7_vm *pVm = pCtx->pVm;` |
|    443 |  5299 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|      - |  5300 | `	xmlDocPtr pDoc;` |
|    443 |  5301 | `	xmlNodePtr pNode = 0;` |
|      - |  5302 | `	sxu32 nMark;` |
|    443 |  5303 | `	if( pDocNd == 0 ){` |
|    ! 0 |  5304 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  5305 | `	}` |
|    443 |  5306 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|    443 |  5307 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    443 |  5308 | `	switch( iKind ){` |
|     91 |  5309 | `	case XML_ELEMENT_NODE:` |
|    185 |  5310 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  5311 | `			break; /* Invalid Character Error */` |
|      - |  5312 | `		}` |
|      - |  5313 | `		/* php passes the value through xmlNewDocNode, which entity-parses` |
|      - |  5314 | `		 * it (quirk preserved: bad entities warn and drop the content). */` |
|    179 |  5315 | `		pNode = xmlNewDocNode(pDoc,0,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|    179 |  5316 | `		break;` |
|     56 |  5317 | `	case XML_TEXT_NODE:` |
|    114 |  5318 | `		pNode = xmlNewDocText(pDoc,(const xmlChar *)zVal);` |
|    114 |  5319 | `		break;` |
|      4 |  5320 | `	case XML_CDATA_SECTION_NODE:` |
|      9 |  5321 | `		pNode = xmlNewCDataBlock(pDoc,(const xmlChar *)zVal,nVal);` |
|      9 |  5322 | `		break;` |
|      5 |  5323 | `	case XML_COMMENT_NODE:` |
|     11 |  5324 | `		pNode = xmlNewDocComment(pDoc,(const xmlChar *)zVal);` |
|     11 |  5325 | `		break;` |
|     10 |  5326 | `	case XML_PI_NODE:` |
|      - |  5327 | `		/* php validates the TARGET the same way it validates an element name,` |
|      - |  5328 | ``		 * so `createProcessingInstruction('a b')` is Invalid Character Error`` |
|      - |  5329 | `		 * rather than a document that will not parse back. */` |
|     21 |  5330 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     11 |  5331 | `			break;` |
|      - |  5332 | `		}` |
|      - |  5333 | `		/* Empty data stays a NULL content pointer, matching php's node state:` |
|      - |  5334 | ``		 * `<?bare?>` serializes with no separator space, `nodeValue` reads`` |
|      - |  5335 | ``		 * null -- and `data` reads "", because THAT getter coerces. */`` |
|     11 |  5336 | `		pNode = xmlNewDocPI(pDoc,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|     11 |  5337 | `		break;` |
|     12 |  5338 | `	case XML_ENTITY_REF_NODE:` |
|     25 |  5339 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      9 |  5340 | `			break;` |
|      - |  5341 | `		}` |
|     17 |  5342 | `		pNode = xmlNewReference(pDoc,(const xmlChar *)zName);` |
|     17 |  5343 | `		break;` |
|     42 |  5344 | `	case XML_DOCUMENT_FRAG_NODE:` |
|     85 |  5345 | `		pNode = xmlNewDocFragment(pDoc);` |
|     84 |  5346 | `		break;` |
|      - |  5347 | `	}` |
|    663 |  5348 | `	PH7_LibxmlCaptureEnd(pVm,nMark,` |
|    220 |  5349 | `		iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");` |
|    443 |  5350 | `	if( pNode == 0 ){` |
|     25 |  5351 | `		if( iKind == XML_ELEMENT_NODE \|\| iKind == XML_PI_NODE \|\| iKind == XML_ENTITY_REF_NODE ){` |
|      - |  5352 | `			/* The three factories that take a NAME are the three that can be` |
|      - |  5353 | `			 * handed one libxml refuses. */` |
|     25 |  5354 | `			return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  5355 | `		}` |
|    ! 0 |  5356 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5357 | `		return PH7_OK;` |
|      - |  5358 | `	}` |
|    419 |  5359 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|    419 |  5360 | `	return DomResultNodeOf(pCtx,pDocNd,pNode);` |
|    223 |  5361 | `}` |
|      - |  5362 | `/* DOMDocument::createElement(string $localName, string $value = ''): DOMElement */` |
|    182 |  5363 | `DOM_METHOD(vm_builtin_DOMDocument_createElement)` |
|      3 |  5364 | `{` |
|    185 |  5365 | `	int nVal = 0;` |
|    185 |  5366 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    185 |  5367 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";` |
|    185 |  5368 | `	return DomDocCreate(pCtx,XML_ELEMENT_NODE,zName,zVal,nVal);` |
|      3 |  5369 | `}` |
|      - |  5370 | `/*` |
|      - |  5371 | ` * DOMDocument::createElementNS(?string $namespace, string $qualifiedName,` |
|      - |  5372 | ` *                              string $value = '')` |
|      - |  5373 | ` *` |
|      - |  5374 | ` * The only way to build a namespaced ELEMENT -- until this existed a program` |
|      - |  5375 | `` * could read a namespaced document and not write one, and `Call to undefined`` |
|      - |  5376 | `` * method` was the answer to the first line of every modern DOM example.`` |
|      - |  5377 | ` *` |
|      - |  5378 | ` * php's rules, measured:` |
|      - |  5379 | ` *` |
|      - |  5380 | ` *   * A NULL namespace is a plain element; an EMPTY-STRING one is not the same` |
|      - |  5381 | `` *     thing, it declares `xmlns=""` on the element and answers `''` for`` |
|      - |  5382 | ` *     namespaceURI. Either with a PREFIXED name is a Namespace Error, since a` |
|      - |  5383 | ` *     prefix names a namespace.` |
|      - |  5384 | ` *   * The declaration lands on the NEW element, always: a fresh node has no` |
|      - |  5385 | ` *     parent, so nothing the document declares elsewhere is in scope yet. What` |
|      - |  5386 | ` *     the document already makes is settled later, when the element is linked` |
|      - |  5387 | ` *     in and the redundant declaration is stripped (DomNsOnInsertEx).` |
|      - |  5388 | ` *   * The $value is not text -- php hands it to libxml, which entity-parses it,` |
|      - |  5389 | `` *     so `&amp;` becomes `&`, an undefined entity is a warning and `<` is`` |
|      - |  5390 | ` *     escaped. The same quirk createElement already carries.` |
|      - |  5391 | ` */` |
|    162 |  5392 | `DOM_METHOD(vm_builtin_DOMDocument_createElementNS)` |
|      1 |  5393 | `{` |
|    163 |  5394 | `	ph7_vm *pVm = pCtx->pVm;` |
|    163 |  5395 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|    163 |  5396 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|    163 |  5397 | `	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    163 |  5398 | `	int nVal = 0;` |
|    163 |  5399 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],&nVal) : "";` |
|      - |  5400 | `	xmlNodePtr pNode;` |
|      - |  5401 | `	dom_qname sQ;` |
|      - |  5402 | `	sxu32 nMark;` |
|      - |  5403 | `	int rc;` |
|    163 |  5404 | `	if( pDocNd == 0 ){` |
|    ! 0 |  5405 | `		return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  5406 | `	}` |
|    163 |  5407 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_ELEM,&sQ);` |
|    163 |  5408 | `	if( rc ){` |
|     59 |  5409 | `		return DomThrow(pCtx,rc);` |
|      - |  5410 | `	}` |
|    105 |  5411 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    157 |  5412 | `	pNode = xmlNewDocNode((xmlDocPtr)pDocNd->pNode,0,sQ.zLocal,` |
|    104 |  5413 | `		nVal ? (const xmlChar *)zVal : 0);` |
|    105 |  5414 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::createElementNS");` |
|    105 |  5415 | `	if( pNode == 0 ){` |
|    ! 0 |  5416 | `		DomQNameRelease(&sQ);` |
|    ! 0 |  5417 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5418 | `	}` |
|    105 |  5419 | `	if( zUri != 0 ){` |
|     99 |  5420 | `		xmlNsPtr pNs = DomNsForCreate(pNode,zUri,sQ.zPrefix);` |
|     99 |  5421 | `		if( pNs == 0 ){` |
|     15 |  5422 | `			DomQNameRelease(&sQ);` |
|     15 |  5423 | `			xmlFreeNode(pNode);   /* never handed out, never an orphan */` |
|     15 |  5424 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  5425 | `		}` |
|     85 |  5426 | `		xmlSetNs(pNode,pNs);` |
|     42 |  5427 | `	}` |
|     91 |  5428 | `	DomQNameRelease(&sQ);` |
|     91 |  5429 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|     91 |  5430 | `	return DomResultNodeOf(pCtx,pDocNd,pNode);` |
|     82 |  5431 | `}` |
|      - |  5432 | `/*` |
|      - |  5433 | ` * DOMDocument::importNode(DOMNode $node, bool $deep = false): DOMNode\|false` |
|      - |  5434 | ` *` |
|      - |  5435 | ` * A node of ANOTHER document copied into this one, which is the only way to` |
|      - |  5436 | ` * carry a subtree across: every mutator refuses a node whose document is not` |
|      - |  5437 | ` * the parent's with php's Wrong Document Error, so without this a program that` |
|      - |  5438 | ` * read two files could not build a third out of them.` |
|      - |  5439 | ` *` |
|      - |  5440 | ` * php's rules, measured:` |
|      - |  5441 | ` *` |
|      - |  5442 | ` *   * A node ALREADY of this document is answered unchanged -- the same object,` |
|      - |  5443 | ` *     not a copy, and not detached from wherever it is.` |
|      - |  5444 | `` *   * A DOCUMENT is refused with a warning and `false`, not an exception.`` |
|      - |  5445 | ` *   * Shallow does not mean bare: an element brings its attributes and its` |
|      - |  5446 | ` *     namespace declarations, only its children stay behind. A fragment brings` |
|      - |  5447 | ` *     nothing but itself, and an attribute brings its value whatever $deep says.` |
|      - |  5448 | ` *   * The copy is an ORPHAN of this document (no parent, and freed with it), and` |
|      - |  5449 | ` *     a second import of the same node is a second copy.` |
|      - |  5450 | ` *   * A namespaced ATTRIBUTE is the one kind libxml cannot finish: its copy` |
|      - |  5451 | ` *     arrives with no namespace at all, and php re-points it at a PREFIXED` |
|      - |  5452 | ` *     binding of the same URI on the target's ROOT -- reusing one the root` |
|      - |  5453 | `` *     already has (so the prefix can change, `p:b` arriving as `z:b`) and`` |
|      - |  5454 | ` *     declaring it there otherwise.` |
|      - |  5455 | ` */` |
|     48 |  5456 | `DOM_METHOD(vm_builtin_DOMDocument_importNode)` |
|      1 |  5457 | `{` |
|     49 |  5458 | `	ph7_vm *pVm = pCtx->pVm;` |
|     49 |  5459 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     49 |  5460 | `	phl_domnode *pSrc = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     49 |  5461 | `	int bDeep = nArg > 1 && ph7_value_to_bool(apArg[1]);` |
|      - |  5462 | `	xmlDocPtr pDoc;` |
|      - |  5463 | `	xmlNodePtr pNode,pCopy;` |
|      - |  5464 | `	sxu32 nMark;` |
|     49 |  5465 | `	if( pDocNd == 0 \|\| pSrc == 0 ){` |
|    ! 0 |  5466 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5467 | `		return PH7_OK;` |
|      - |  5468 | `	}` |
|     49 |  5469 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|     49 |  5470 | `	pNode = (xmlNodePtr)pSrc->pNode;` |
|     49 |  5471 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|      - |  5472 | ``		/* The context prints php's `DOMDocument::importNode(): ` itself. */`` |
|      3 |  5473 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Cannot import: Node Type Not Supported");` |
|      3 |  5474 | `		ph7_result_bool(pCtx,0);` |
|      3 |  5475 | `		return PH7_OK;` |
|      - |  5476 | `	}` |
|     47 |  5477 | `	if( pNode->doc == pDoc ){` |
|      5 |  5478 | `		ph7_result_value(pCtx,apArg[0]);` |
|      5 |  5479 | `		return PH7_OK;` |
|      - |  5480 | `	}` |
|     43 |  5481 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      - |  5482 | ``	/* 2 is libxml's `node + namespaces + attributes, no children`, which is what`` |
|      - |  5483 | `	 * makes a shallow import carry the attributes; cloneNode asks the same way. */` |
|     43 |  5484 | `	pCopy = xmlDocCopyNode(pNode,pDoc,bDeep ? 1 : 2);` |
|     43 |  5485 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::importNode");` |
|     43 |  5486 | `	if( pCopy == 0 ){` |
|    ! 0 |  5487 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5488 | `		return PH7_OK;` |
|      - |  5489 | `	}` |
|     43 |  5490 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pNode->ns != 0 && pNode->ns->href != 0 ){` |
|     13 |  5491 | `		xmlNodePtr pRoot = xmlDocGetRootElement(pDoc);` |
|     13 |  5492 | `		xmlNsPtr pNs = pRoot` |
|     12 |  5493 | `			? DomNsResolve(pRoot,(const char *)pNode->ns->href,pNode->ns->prefix,1) : 0;` |
|     13 |  5494 | `		if( pNs == 0 ){` |
|      - |  5495 | `			/* No root element to declare on. php answers an attribute that IS in` |
|      - |  5496 | `			 * the namespace anyway, through a declaration no element makes; the` |
|      - |  5497 | `			 * document owns it so that it is freed with it. */` |
|    ! 0 |  5498 | `			pNs = xmlNewNs(0,pNode->ns->href,pNode->ns->prefix);` |
|    ! 0 |  5499 | `			if( pNs ){` |
|    ! 0 |  5500 | `				DomNsPark(pCopy,pNs);` |
|    ! 0 |  5501 | `			}` |
|    ! 0 |  5502 | `		}` |
|     13 |  5503 | `		xmlSetNs(pCopy,pNs);` |
|      6 |  5504 | `	}` |
|     43 |  5505 | `	DomOrphanAdd(pDocNd->pShell,pCopy);` |
|     43 |  5506 | `	return DomResultNodeOf(pCtx,pDocNd,pCopy);` |
|     25 |  5507 | `}` |
|      - |  5508 | `/*` |
|      - |  5509 | `` * Re-home one node's WRAPPER. `adoptNode` moves the node itself between`` |
|      - |  5510 | ` * documents and answers the SAME object, which has to keep working: its $__doc` |
|      - |  5511 | `` * slot is what `ownerDocument` reads, its handle's shell is what will free the`` |
|      - |  5512 | ` * node, and its place in a document's identity cache is what makes` |
|      - |  5513 | `` * `$doc->documentElement === $doc->documentElement` true. All three move.`` |
|      - |  5514 | ` *` |
|      - |  5515 | ` * Both entries are borrowed pointers, so the move is two edits and no` |
|      - |  5516 | ` * reference changes hands.` |
|      - |  5517 | ` */` |
|    174 |  5518 | `static void DomAdoptWrapper(ph7_vm *pVm,ph7_hashmap *pFrom,ph7_hashmap *pTo,` |
|      - |  5519 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode)` |
|      1 |  5520 | `{` |
|    175 |  5521 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  5522 | `	ph7_class_instance *pObj;` |
|      - |  5523 | `	ph7_value sKey,*pHit;` |
|    175 |  5524 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|    175 |  5525 | `	if( PH7_HashmapLookup(pFrom,&sKey,&pEntry) != SXRET_OK \|\| pEntry == 0 ){` |
|     79 |  5526 | `		PH7_MemObjRelease(&sKey);` |
|     79 |  5527 | `		return;   /* PHP never asked for this node: nothing to move */` |
|      - |  5528 | `	}` |
|     97 |  5529 | `	pHit = HashmapExtractNodeValue(pEntry);` |
|     97 |  5530 | `	pObj = (pHit && (pHit->iFlags & MEMOBJ_INT))` |
|    144 |  5531 | `		? (ph7_class_instance *)(sxuptr)pHit->x.iVal : 0;` |
|     97 |  5532 | `	if( pObj ){` |
|     97 |  5533 | `		phl_domnode *pRes = DomResOf(pObj);` |
|      - |  5534 | `		ph7_value sVal;` |
|     97 |  5535 | `		PH7_MemObjInitFromInt(&(*pVm),&sVal,(sxi64)(sxuptr)pObj);` |
|     97 |  5536 | `		PH7_HashmapInsert(pTo,&sKey,&sVal);` |
|     97 |  5537 | `		if( pRes ){` |
|     97 |  5538 | `			pRes->pShell = pDstShell;` |
|     48 |  5539 | `		}` |
|     97 |  5540 | `		PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDstDoc);` |
|     48 |  5541 | `	}` |
|     97 |  5542 | `	PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|     97 |  5543 | `	PH7_MemObjRelease(&sKey);` |
|     89 |  5544 | `}` |
|      - |  5545 | `/* ...for every node of the adopted subtree, attributes and their text included:` |
|      - |  5546 | `` * php's adoption reaches all of them, which `$kid->ownerDocument` shows. */`` |
|     86 |  5547 | `static void DomAdoptWrappers(ph7_vm *pVm,ph7_class_instance *pSrcDoc,` |
|      - |  5548 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode)` |
|      1 |  5549 | `{` |
|     87 |  5550 | `	ph7_hashmap *pFrom = DomCache(&(*pVm),pSrcDoc);` |
|     87 |  5551 | `	ph7_hashmap *pTo = DomCache(&(*pVm),pDstDoc);` |
|     87 |  5552 | `	xmlNodePtr pCur = pNode;` |
|     87 |  5553 | `	if( pFrom == 0 \|\| pTo == 0 \|\| pFrom == pTo ){` |
|    ! 0 |  5554 | `		return;` |
|      - |  5555 | `	}` |
|    237 |  5556 | `	while( pCur ){` |
|    151 |  5557 | `		DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,pCur);` |
|    151 |  5558 | `		if( pCur->type == XML_ELEMENT_NODE ){` |
|      - |  5559 | `			xmlAttrPtr pAttr;` |
|     60 |  5560 | `			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){` |
|      - |  5561 | `				xmlNodePtr pKid;` |
|     13 |  5562 | `				DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,(xmlNodePtr)pAttr);` |
|     25 |  5563 | `				for( pKid = pAttr->children ; pKid ; pKid = pKid->next ){` |
|     13 |  5564 | `					DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,pKid);` |
|      7 |  5565 | `				}` |
|      7 |  5566 | `			}` |
|     24 |  5567 | `		}` |
|    151 |  5568 | `		pCur = DomWalkNext(pCur,pNode);` |
|      1 |  5569 | `	}` |
|     44 |  5570 | `}` |
|      - |  5571 | `/*` |
|      - |  5572 | ` * DOMDocument::adoptNode(DOMNode $node): DOMNode\|false` |
|      - |  5573 | ` *` |
|      - |  5574 | ` * The other half of importNode: the node is MOVED rather than copied, so the` |
|      - |  5575 | ` * source loses it and every wrapper PHP holds onto it keeps working and starts` |
|      - |  5576 | ` * answering this document.` |
|      - |  5577 | ` *` |
|      - |  5578 | ` * php's rules, measured:` |
|      - |  5579 | ` *` |
|      - |  5580 | ` *   * The answer is the SAME object, and it is always UNLINKED first -- even` |
|      - |  5581 | ` *     when it already belongs to this document, which is observable:` |
|      - |  5582 | `` *     `$d->adoptNode($d->documentElement)` leaves the document empty.`` |
|      - |  5583 | ` *   * A DOCUMENT is the Not Supported refusal (raised in the mode the ARGUMENT's` |
|      - |  5584 | `` *     document is in, not the receiver's); a FRAGMENT is a plain `false` with`` |
|      - |  5585 | ` *     no error at all.` |
|      - |  5586 | ` *   * An attribute is taken off its element. Every node under what moved changes` |
|      - |  5587 | ` *     document too, wrappers included.` |
|      - |  5588 | ` *   * NOTHING is re-declared: an adopted element keeps pointing at its old` |
|      - |  5589 | ` *     namespace and answers the same namespaceURI while carrying no declaration` |
|      - |  5590 | ` *     of it -- the declaration appears when it is LINKED, from the reconcile.` |
|      - |  5591 | ` */` |
|     52 |  5592 | `DOM_METHOD(vm_builtin_DOMDocument_adoptNode)` |
|      1 |  5593 | `{` |
|     53 |  5594 | `	ph7_vm *pVm = pCtx->pVm;` |
|     53 |  5595 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     53 |  5596 | `	phl_domnode *pSrc = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     53 |  5597 | `	ph7_class_instance *pSrcDoc = nArg > 0 ? DomObjArgDoc(apArg[0]) : 0;` |
|      - |  5598 | `	xmlNodePtr pNode;` |
|     53 |  5599 | `	if( pDocNd == 0 \|\| pSrc == 0 ){` |
|    ! 0 |  5600 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5601 | `		return PH7_OK;` |
|      - |  5602 | `	}` |
|     53 |  5603 | `	pNode = (xmlNodePtr)pSrc->pNode;` |
|     53 |  5604 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|      - |  5605 | `		/* The one refusal in this file that consults the ARGUMENT's document` |
|      - |  5606 | `		 * rather than the receiver's: php reaches for the strictness of the` |
|      - |  5607 | ``		 * node it was handed, so `$strict->adoptNode($lax)` warns and`` |
|      - |  5608 | ``		 * `$lax->adoptNode($strict)` throws. */`` |
|     11 |  5609 | `		return DomThrowFor(pCtx,pSrcDoc,DOM_ERR_NOT_SUPPORTED,DOM_REFUSE_FALSE);` |
|      - |  5610 | `	}` |
|     43 |  5611 | `	if( pNode->type == XML_DOCUMENT_FRAG_NODE ){` |
|      3 |  5612 | `		ph7_result_bool(pCtx,0);` |
|      3 |  5613 | `		return PH7_OK;` |
|      - |  5614 | `	}` |
|     41 |  5615 | `	DomDetach(pSrc->pShell,pNode);` |
|     41 |  5616 | `	if( pNode->doc != (xmlDocPtr)pDocNd->pNode ){` |
|      - |  5617 | `		/*` |
|      - |  5618 | `		 * NOT xmlSetTreeDoc: a parsed document interns its node names in its` |
|      - |  5619 | `		 * own dictionary, so a node re-homed by hand keeps names owned by the` |
|      - |  5620 | `		 * document it LEFT -- and freeing the target document then frees` |
|      - |  5621 | `		 * strings the source's dictionary owns. ASan called it what it is, a` |
|      - |  5622 | `		 * bad free. xmlDOMWrapAdoptNode is libxml's own re-homing: it moves the` |
|      - |  5623 | `		 * strings, the attribute values and the ID table entries with the node.` |
|      - |  5624 | `		 * (A CONSTRUCTED node has no document and no dictionary at all, and` |
|      - |  5625 | `		 * xmlSetTreeDoc IS its whole move.)` |
|      - |  5626 | `		 */` |
|     39 |  5627 | `		if( pNode->doc == 0 ){` |
|      3 |  5628 | `			xmlSetTreeDoc(pNode,(xmlDocPtr)pDocNd->pNode);` |
|      2 |  5629 | `		}else{` |
|     37 |  5630 | `			sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     37 |  5631 | `			xmlDOMWrapAdoptNode(0,pNode->doc,pNode,(xmlDocPtr)pDocNd->pNode,0,0);` |
|     37 |  5632 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::adoptNode");` |
|      - |  5633 | `		}` |
|     39 |  5634 | `		DomAdoptWrappers(pVm,pSrcDoc,DomThisDoc(pCtx),pDocNd->pShell,pNode);` |
|     19 |  5635 | `	}` |
|     41 |  5636 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|     41 |  5637 | `	ph7_result_value(pCtx,apArg[0]);` |
|     41 |  5638 | `	return PH7_OK;` |
|     27 |  5639 | `}` |
|      - |  5640 | `/*` |
|      - |  5641 | ` * DOMElement::insertAdjacentElement(string $where, DOMElement $element): ?DOMElement` |
|      - |  5642 | ` * DOMElement::insertAdjacentText(string $where, string $data): void` |
|      - |  5643 | ` *` |
|      - |  5644 | ` * php's dom_insert_adjacent, transcribed.  The WHERE word is matched` |
|      - |  5645 | ` * case-insensitively against the four positions and anything else is the` |
|      - |  5646 | ` * Syntax refusal (code 12, new to DomErrText) -- even on a receiver no` |
|      - |  5647 | ` * position could serve; beforebegin/afterend on a parentless receiver answer` |
|      - |  5648 | ` * null BEFORE anything moves; and then the argument is ADOPTED into this` |
|      - |  5649 | ` * document -- a node of another document is MOVED here, wrappers and all,` |
|      - |  5650 | ` * where every other insertion method refuses it with Wrong Document Error.` |
|      - |  5651 | ` * Only then does the pre-insertion validity run, so a refusal (the receiver` |
|      - |  5652 | ` * inside the argument) leaves the adopted argument DETACHED --` |
|      - |  5653 | `` * `$in->insertAdjacentElement('afterbegin',$host)` costs the tree the whole`` |
|      - |  5654 | ` * host subtree, php's own answer -- and the insertion point is read AFTER the` |
|      - |  5655 | ` * adopt unlinked the argument, which is what makes inserting one's own next` |
|      - |  5656 | `` * sibling `afterend` a no-op rather than a swap.`` |
|      - |  5657 | ` *` |
|      - |  5658 | ` * One deliberate divergence: php SEGFAULTS on` |
|      - |  5659 | `` * `$a->insertAdjacentElement('beforebegin',$a)` -- its adopt unlinks the`` |
|      - |  5660 | ` * receiver and the insertion then walks a NULL parent.  PHL answers the` |
|      - |  5661 | ` * Hierarchy refusal its validity was about to reach.` |
|      - |  5662 | ` */` |
|     42 |  5663 | `static int DomInsertAdjacentOp(ph7_context *pCtx,phl_domnode *pRecv,const char *zWhere,` |
|      - |  5664 | `	phl_xmldoc *pArgShell,xmlNodePtr pOther,ph7_class_instance *pArgDoc)` |
|      1 |  5665 | `{` |
|     43 |  5666 | `	ph7_vm *pVm = pCtx->pVm;` |
|     43 |  5667 | `	xmlNodePtr pThis = (xmlNodePtr)pRecv->pNode;` |
|      - |  5668 | `	xmlNodePtr pParent,pRef;` |
|      - |  5669 | `	int iPos,iErr;` |
|     43 |  5670 | `	if( DomNameIsCi(zWhere,"beforebegin") ){` |
|     11 |  5671 | `		iPos = 0;` |
|     38 |  5672 | `	}else if( DomNameIsCi(zWhere,"afterbegin") ){` |
|     19 |  5673 | `		iPos = 1;` |
|     24 |  5674 | `	}else if( DomNameIsCi(zWhere,"beforeend") ){` |
|      5 |  5675 | `		iPos = 2;` |
|     13 |  5676 | `	}else if( DomNameIsCi(zWhere,"afterend") ){` |
|      7 |  5677 | `		iPos = 3;` |
|      4 |  5678 | `	}else{` |
|      5 |  5679 | `		DomThrowVoid(pCtx,DOM_ERR_SYNTAX);` |
|      5 |  5680 | `		return -1;` |
|      - |  5681 | `	}` |
|     39 |  5682 | `	if( (iPos == 0 \|\| iPos == 3) && pThis->parent == 0 ){` |
|      7 |  5683 | `		return 1;   /* the null answer, nothing moved */` |
|      - |  5684 | `	}` |
|      - |  5685 | `	/* The adopt: detach, re-home across documents (adoptNode's machinery),` |
|      - |  5686 | `	 * and park until linked. A document-less argument -- a constructed node --` |
|      - |  5687 | `	 * has no dict-interned strings to move, so xmlSetTreeDoc is its whole` |
|      - |  5688 | `	 * move; the wrappers cross either way. */` |
|     33 |  5689 | `	DomDetach(pArgShell,pOther);` |
|     33 |  5690 | `	if( pOther->doc != pThis->doc ){` |
|      5 |  5691 | `		if( pOther->doc == 0 ){` |
|    ! 0 |  5692 | `			xmlSetTreeDoc(pOther,pThis->doc);` |
|    ! 0 |  5693 | `		}else{` |
|      5 |  5694 | `			sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      5 |  5695 | `			xmlDOMWrapAdoptNode(0,pOther->doc,pOther,pThis->doc,0,0);` |
|      5 |  5696 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMElement::insertAdjacentElement");` |
|      - |  5697 | `		}` |
|      5 |  5698 | `		DomAdoptWrappers(pVm,pArgDoc,DomThisDoc(pCtx),pRecv->pShell,pOther);` |
|      2 |  5699 | `	}` |
|     33 |  5700 | `	DomOrphanAdd(pRecv->pShell,pOther);` |
|     33 |  5701 | `	switch( iPos ){` |
|      7 |  5702 | `	case 0:  pParent = pThis->parent;  pRef = pThis;            break;` |
|     19 |  5703 | `	case 1:  pParent = pThis;          pRef = pThis->children;  break;` |
|      5 |  5704 | `	case 2:  pParent = pThis;          pRef = 0;                break;` |
|      5 |  5705 | `	default: pParent = pThis->parent;  pRef = pThis->next;      break;` |
|      - |  5706 | `	}` |
|     33 |  5707 | `	if( pParent == 0 ){` |
|      - |  5708 | `		/* The argument WAS the receiver: adopting it took the parent away. */` |
|    ! 0 |  5709 | `		DomThrowVoid(pCtx,DOM_ERR_HIERARCHY);` |
|    ! 0 |  5710 | `		return -1;` |
|      - |  5711 | `	}` |
|     33 |  5712 | `	iErr = DomInsertValidity(pParent,pOther,0);` |
|     33 |  5713 | `	if( iErr ){` |
|      7 |  5714 | `		DomThrowVoid(pCtx,iErr);` |
|      7 |  5715 | `		return -1;` |
|      - |  5716 | `	}` |
|     27 |  5717 | `	if( pRef == pOther ){` |
|    ! 0 |  5718 | `		pRef = pOther->next;` |
|    ! 0 |  5719 | `	}` |
|     27 |  5720 | `	DomDetach(pRecv->pShell,pOther);` |
|     27 |  5721 | `	if( pRef ){` |
|     11 |  5722 | `		DomLinkBefore(pParent,pOther,pRef);` |
|      6 |  5723 | `	}else{` |
|     17 |  5724 | `		DomLinkLast(pParent,pOther);` |
|      - |  5725 | `	}` |
|     27 |  5726 | `	DomNsOnInsertEx(pOther,0);` |
|     27 |  5727 | `	return 0;` |
|     22 |  5728 | `}` |
|     30 |  5729 | `DOM_METHOD(vm_builtin_DOMElement_insertAdjacentElement)` |
|      1 |  5730 | `{` |
|     31 |  5731 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     31 |  5732 | `	phl_domnode *pOther = nArg > 1 ? DomObjArg(apArg[1]) : 0;` |
|     31 |  5733 | `	const char *zWhere = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     31 |  5734 | `	if( pNd == 0 \|\| pOther == 0 ){` |
|    ! 0 |  5735 | `		return PH7_OK;` |
|      - |  5736 | `	}` |
|     45 |  5737 | `	if( DomInsertAdjacentOp(pCtx,pNd,zWhere,pOther->pShell,(xmlNodePtr)pOther->pNode,` |
|     46 |  5738 | `		DomObjArgDoc(apArg[1])) == 0 ){` |
|      - |  5739 | `		/* The answer is the argument itself, now linked. */` |
|     19 |  5740 | `		ph7_result_value(pCtx,apArg[1]);` |
|      9 |  5741 | `	}` |
|     31 |  5742 | `	return PH7_OK;` |
|     16 |  5743 | `}` |
|     12 |  5744 | `DOM_METHOD(vm_builtin_DOMElement_insertAdjacentText)` |
|      1 |  5745 | `{` |
|     13 |  5746 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     13 |  5747 | `	const char *zWhere = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|      - |  5748 | `	const char *zData;` |
|     13 |  5749 | `	int nData = 0;` |
|      - |  5750 | `	xmlNodePtr pText;` |
|     13 |  5751 | `	if( pNd == 0 ){` |
|    ! 0 |  5752 | `		return PH7_OK;` |
|      - |  5753 | `	}` |
|     13 |  5754 | `	zData = nArg > 1 ? ph7_value_to_string(apArg[1],&nData) : "";` |
|     13 |  5755 | `	pText = xmlNewDocTextLen(((xmlNodePtr)pNd->pNode)->doc,(const xmlChar *)zData,nData);` |
|     13 |  5756 | `	if( pText == 0 ){` |
|    ! 0 |  5757 | `		return PH7_OK;` |
|      - |  5758 | `	}` |
|      - |  5759 | `	/* Park it FIRST. The op's two earliest refusals -- the Syntax word and` |
|      - |  5760 | `	 * the parentless beforebegin/afterend null -- return before its own` |
|      - |  5761 | `	 * DomOrphanAdd runs, and an unparked fresh node outlives every owner` |
|      - |  5762 | `	 * (the leak checker is what noticed). Parking is idempotent, the op's` |
|      - |  5763 | `	 * detach removes exactly one entry, and the linked node ends OFF the` |
|      - |  5764 | `	 * orphan list -- so the early paths leave it parked in the shell where` |
|      - |  5765 | `	 * teardown frees it, unobservable, which is php's answer. */` |
|     13 |  5766 | `	DomOrphanAdd(pNd->pShell,pText);` |
|     13 |  5767 | `	DomInsertAdjacentOp(pCtx,pNd,zWhere,pNd->pShell,pText,0);` |
|     13 |  5768 | `	return PH7_OK;` |
|      7 |  5769 | `}` |
|      - |  5770 | `/* DOMDocument::createTextNode / createComment / createCDATASection(string $data) */` |
|    130 |  5771 | `static int DomDocCreateData(ph7_context *pCtx,int iKind,int nArg,ph7_value **apArg)` |
|      2 |  5772 | `{` |
|    132 |  5773 | `	int nVal = 0;` |
|    132 |  5774 | `	const char *zVal = nArg > 0 ? ph7_value_to_string(apArg[0],&nVal) : "";` |
|    132 |  5775 | `	return DomDocCreate(pCtx,iKind,"",zVal,nVal);` |
|      2 |  5776 | `}` |
|    112 |  5777 | `DOM_METHOD(vm_builtin_DOMDocument_createTextNode)` |
|      2 |  5778 | `{` |
|    114 |  5779 | `	return DomDocCreateData(pCtx,XML_TEXT_NODE,nArg,apArg);` |
|      2 |  5780 | `}` |
|      - |  5781 | `/* DOMDocument::createProcessingInstruction(string $target, string $data = '')` |
|      - |  5782 | ` * / createEntityReference(string $name) / createDocumentFragment() */` |
|     20 |  5783 | `DOM_METHOD(vm_builtin_DOMDocument_createPI)` |
|      1 |  5784 | `{` |
|     21 |  5785 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     21 |  5786 | `	int nVal = 0;` |
|     21 |  5787 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";` |
|     21 |  5788 | `	return DomDocCreate(pCtx,XML_PI_NODE,zName,zVal,nVal);` |
|      1 |  5789 | `}` |
|     24 |  5790 | `DOM_METHOD(vm_builtin_DOMDocument_createEntityRef)` |
|      1 |  5791 | `{` |
|     25 |  5792 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     25 |  5793 | `	return DomDocCreate(pCtx,XML_ENTITY_REF_NODE,zName,"",0);` |
|      1 |  5794 | `}` |
|     84 |  5795 | `DOM_METHOD(vm_builtin_DOMDocument_createFragment)` |
|      1 |  5796 | `{` |
|     42 |  5797 | `	SXUNUSED(nArg);` |
|     42 |  5798 | `	SXUNUSED(apArg);` |
|     85 |  5799 | `	return DomDocCreate(pCtx,XML_DOCUMENT_FRAG_NODE,"","",0);` |
|      1 |  5800 | `}` |
|     10 |  5801 | `DOM_METHOD(vm_builtin_DOMDocument_createComment)` |
|      1 |  5802 | `{` |
|     11 |  5803 | `	return DomDocCreateData(pCtx,XML_COMMENT_NODE,nArg,apArg);` |
|      1 |  5804 | `}` |
|      8 |  5805 | `DOM_METHOD(vm_builtin_DOMDocument_createCDATASection)` |
|      1 |  5806 | `{` |
|      9 |  5807 | `	return DomDocCreateData(pCtx,XML_CDATA_SECTION_NODE,nArg,apArg);` |
|      1 |  5808 | `}` |
|      - |  5809 | `/*` |
|      - |  5810 | `` * php's normalization, which both `DOMNode::normalize()` and`` |
|      - |  5811 | `` * `DOMDocument::normalizeDocument()` are: adjacent text nodes merge into the`` |
|      - |  5812 | ` * FIRST of the run, and a text node left EMPTY is then dropped from the tree` |
|      - |  5813 | ` * entirely -- including one that was empty to begin with, which is what makes` |
|      - |  5814 | `` * `$el->normalize()` the way a program gets rid of the zero-length text nodes an`` |
|      - |  5815 | ` * edit leaves behind. Dropping them was the half missing here: a document that` |
|      - |  5816 | `` * had been normalized still serialized `<k></k>` where php writes `<k/>`, and`` |
|      - |  5817 | `` * still counted the empty node in `childNodes->length`.`` |
|      - |  5818 | ` *` |
|      - |  5819 | ` * Merged-away and dropped siblings are PARKED as orphans, never freed, so any` |
|      - |  5820 | ` * PHP wrapper to them stays valid -- php keeps exactly those alive too, through` |
|      - |  5821 | ` * its own wrapper refcount, and a variable holding one reads its old content and` |
|      - |  5822 | `` * a NULL `parentNode` in both engines.`` |
|      - |  5823 | ` *` |
|      - |  5824 | ` * The walk descends into a child ELEMENT and into that element's ATTRIBUTES` |
|      - |  5825 | ` * (an attribute's value is a child text list of its own, and a program that` |
|      - |  5826 | ` * built one in pieces has the same run of nodes to merge). What it does NOT` |
|      - |  5827 | ` * touch is the RECEIVER's own attributes -- php's switch reaches an attribute` |
|      - |  5828 | `` * only through a child element -- so `$el->normalize()` leaves `$el`'s`` |
|      - |  5829 | `` * attributes alone while `$el->parentNode->normalize()` normalizes them.`` |
|      - |  5830 | ` */` |
|     56 |  5831 | `static void DomNormalizeTree(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |  5832 | `{` |
|     57 |  5833 | `	xmlNodePtr pChild = pNode->children;` |
|    127 |  5834 | `	while( pChild ){` |
|     71 |  5835 | `		if( pChild->type == XML_TEXT_NODE ){` |
|      - |  5836 | `			xmlNodePtr pNext;` |
|     37 |  5837 | `			while( pChild->next && pChild->next->type == XML_TEXT_NODE ){` |
|      9 |  5838 | `				pNext = pChild->next;` |
|      9 |  5839 | `				if( pNext->content ){` |
|      9 |  5840 | `					xmlNodeAddContent(pChild,pNext->content);` |
|      4 |  5841 | `				}` |
|      9 |  5842 | `				xmlUnlinkNode(pNext);` |
|      9 |  5843 | `				DomOrphanAdd(pShell,pNext);` |
|      1 |  5844 | `			}` |
|     29 |  5845 | `			if( pChild->content == 0 \|\| pChild->content[0] == 0 ){` |
|      5 |  5846 | `				pNext = pChild->next;` |
|      5 |  5847 | `				xmlUnlinkNode(pChild);` |
|      5 |  5848 | `				DomOrphanAdd(pShell,pChild);` |
|      5 |  5849 | `				pChild = pNext;` |
|      5 |  5850 | `				continue;` |
|      1 |  5851 | `			}` |
|     55 |  5852 | `		}else if( pChild->type == XML_ELEMENT_NODE ){` |
|      - |  5853 | `			xmlAttrPtr pAttr;` |
|     29 |  5854 | `			DomNormalizeTree(pShell,pChild);` |
|     43 |  5855 | `			for( pAttr = pChild->properties ; pAttr ; pAttr = pAttr->next ){` |
|     15 |  5856 | `				DomNormalizeTree(pShell,(xmlNodePtr)pAttr);` |
|      8 |  5857 | `			}` |
|     29 |  5858 | `		}else if( pChild->type == XML_ATTRIBUTE_NODE ){` |
|      - |  5859 | `			/* Unreachable from a tree walk (attributes are not children), but` |
|      - |  5860 | `			 * php's switch states it and a fragment/DTD shape could reach it. */` |
|    ! 0 |  5861 | `			DomNormalizeTree(pShell,pChild);` |
|    ! 0 |  5862 | `		}` |
|     67 |  5863 | `		pChild = pChild->next;` |
|      1 |  5864 | `	}` |
|     57 |  5865 | `}` |
|      - |  5866 | `/* DOMDocument::normalizeDocument(): void and DOMNode::normalize(): void -- php` |
|      - |  5867 | ` * runs the same walk from the receiver, so the two share one body. */` |
|     14 |  5868 | `DOM_METHOD(vm_builtin_DOMDocument_normalizeDocument)` |
|      1 |  5869 | `{` |
|     15 |  5870 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  5871 | `	SXUNUSED(nArg);` |
|      7 |  5872 | `	SXUNUSED(apArg);` |
|     15 |  5873 | `	if( pNd ){` |
|     15 |  5874 | `		DomNormalizeTree(pNd->pShell,(xmlNodePtr)pNd->pNode);` |
|      7 |  5875 | `	}` |
|     15 |  5876 | `	return PH7_OK;` |
|      1 |  5877 | `}` |
|      - |  5878 | `/* DOMNode::getNodePath(): ?string -- the XPath that selects this node, or null` |
|      - |  5879 | ` * for one that is not addressable at all (anything under a fragment). php hands` |
|      - |  5880 | ` * libxml's answer straight back, positional predicate and all. */` |
|     60 |  5881 | `DOM_METHOD(vm_builtin_DOMNode_getNodePath)` |
|      1 |  5882 | `{` |
|     61 |  5883 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     61 |  5884 | `	xmlChar *zPath = pNd ? xmlGetNodePath((xmlNodePtr)pNd->pNode) : 0;` |
|     30 |  5885 | `	SXUNUSED(nArg);` |
|     30 |  5886 | `	SXUNUSED(apArg);` |
|     61 |  5887 | `	if( zPath == 0 ){` |
|     15 |  5888 | `		ph7_result_null(pCtx);` |
|     15 |  5889 | `		return PH7_OK;` |
|      - |  5890 | `	}` |
|     47 |  5891 | `	ph7_result_string(pCtx,(const char *)zPath,-1);` |
|     47 |  5892 | `	xmlFree(zPath);` |
|     47 |  5893 | `	return PH7_OK;` |
|     31 |  5894 | `}` |
|      - |  5895 |  |
|      - |  5896 | `/* ===== Character data: the in-place edit family ===== */` |
|      - |  5897 |  |
|      - |  5898 | `/*` |
|      - |  5899 | ` * Every offset and count on this surface is measured in UTF-8 CHARACTERS, not` |
|      - |  5900 | `` * bytes -- php runs `xmlUTF8Strlen` over the content and `xmlUTF8Strsub` to cut`` |
|      - |  5901 | `` * it -- so `$t->length` on "áé漢字" is 4 and `substringData(0,1)` is one`` |
|      - |  5902 | `` * character rather than one byte. PHL measured `length` with strlen(), which is`` |
|      - |  5903 | ` * a silently wrong answer for every non-ASCII document: 10 where php says 4,` |
|      - |  5904 | ` * and every offset a program then computed from it landed mid-character.` |
|      - |  5905 | ` *` |
|      - |  5906 | ` * libxml's own UTF-8 helpers are used rather than PHL's, so malformed content` |
|      - |  5907 | ` * counts and cuts identically in both engines.` |
|      - |  5908 | ` */` |
|    120 |  5909 | `static int DomCharLength(xmlNodePtr pNode)` |
|      1 |  5910 | `{` |
|    121 |  5911 | `	return (pNode && pNode->content) ? xmlUTF8Strlen(pNode->content) : 0;` |
|      1 |  5912 | `}` |
|      - |  5913 | `/*` |
|      - |  5914 | ` * php's Index Size Error: a negative bound, or an offset past the end. The` |
|      - |  5915 | ` * COUNT is clamped rather than refused once the offset is in range.` |
|      - |  5916 | ` *` |
|      - |  5917 | ` * The upper bound is compared UNSIGNED on three of the five and SIGNED on the` |
|      - |  5918 | ``  * other two, and only malformed content tells them apart: `xmlUTF8Strlen` `` |
|      - |  5919 | `` * answers -1 for content that is not valid UTF-8 (`$t->length` reports that`` |
|      - |  5920 | ` * -1), and as an UNSIGNED bound a -1 means "no limit" -- so substringData,` |
|      - |  5921 | ` * insertData and splitText all work on such a node and let libxml's own cutting` |
|      - |  5922 | ` * decide what comes back, while deleteData and replaceData refuse it outright,` |
|      - |  5923 | ` * for every offset and every count. php's own split, kept because a program` |
|      - |  5924 | ` * handed a byte string that is not UTF-8 gets a value back from three of these` |
|      - |  5925 | ` * and an exception from the other two.` |
|      - |  5926 | ` */` |
|     88 |  5927 | `static int DomCharRange(ph7_context *pCtx,xmlNodePtr pNode,ph7_int64 iOffset,` |
|      - |  5928 | `	ph7_int64 iCount,int bHasCount,int bUnsignedBound,int *pnLen,int *pRc)` |
|      1 |  5929 | `{` |
|     89 |  5930 | `	int nLen = DomCharLength(pNode);` |
|    114 |  5931 | `	int bPastEnd = bUnsignedBound ? (sxu32)iOffset > (sxu32)nLen` |
|     63 |  5932 | `	                              : iOffset > (ph7_int64)nLen;` |
|     89 |  5933 | `	*pnLen = nLen;` |
|     88 |  5934 | `	if( iOffset < 0 \|\| (bHasCount && iCount < 0)` |
|     79 |  5935 | `	 \|\| iOffset > (ph7_int64)SXI32_HIGH \|\| iCount > (ph7_int64)SXI32_HIGH` |
|     77 |  5936 | `	 \|\| bPastEnd ){` |
|     33 |  5937 | `		*pRc = DomThrow(pCtx,DOM_ERR_INDEX_SIZE);` |
|     33 |  5938 | `		return -1;` |
|      - |  5939 | `	}` |
|     57 |  5940 | `	return 0;` |
|     45 |  5941 | `}` |
|      - |  5942 | `/* DOMCharacterData::substringData(int $offset, int $count): string */` |
|     36 |  5943 | `DOM_METHOD(vm_builtin_DOMCharacterData_substringData)` |
|      1 |  5944 | `{` |
|     37 |  5945 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     37 |  5946 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     37 |  5947 | `	ph7_int64 iOffset = nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     37 |  5948 | `	ph7_int64 iCount = nArg > 1 ? ph7_value_to_int64(apArg[1]) : 0;` |
|      - |  5949 | `	xmlChar *zSub;` |
|     37 |  5950 | `	int nLen,rc = PH7_OK;` |
|     37 |  5951 | `	if( pNode == 0 ){` |
|    ! 0 |  5952 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5953 | `		return PH7_OK;` |
|      - |  5954 | `	}` |
|     37 |  5955 | `	if( DomCharRange(pCtx,pNode,iOffset,iCount,TRUE,TRUE,&nLen,&rc) != 0 ){` |
|     15 |  5956 | `		return rc;` |
|      - |  5957 | `	}` |
|     23 |  5958 | `	if( pNode->content == 0 ){` |
|      - |  5959 | `		/* php reads a NULL content pointer -- the omitted-argument` |
|      - |  5960 | `		 * constructor's node -- as "": the range still screens (so an offset` |
|      - |  5961 | `		 * past zero is Index Size), and what is left of nothing is "". */` |
|      3 |  5962 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  5963 | `		return PH7_OK;` |
|      - |  5964 | `	}` |
|     21 |  5965 | `	if( (sxu32)(iOffset+iCount) > (sxu32)nLen ){` |
|      7 |  5966 | `		iCount = (ph7_int64)nLen - iOffset;` |
|      3 |  5967 | `	}` |
|     21 |  5968 | `	zSub = xmlUTF8Strsub(pNode->content,(int)iOffset,(int)iCount);` |
|     21 |  5969 | `	ph7_result_string(pCtx,zSub ? (const char *)zSub : "",-1);` |
|     21 |  5970 | `	if( zSub ){` |
|     21 |  5971 | `		xmlFree(zSub);` |
|     10 |  5972 | `	}` |
|     21 |  5973 | `	return PH7_OK;` |
|     19 |  5974 | `}` |
|      - |  5975 | `/* DOMCharacterData::appendData(string $data): true -- raw bytes, no entity` |
|      - |  5976 | `` * parsing, which is why `appendData('&amp;')` stores those five characters. */`` |
|      6 |  5977 | `DOM_METHOD(vm_builtin_DOMCharacterData_appendData)` |
|      1 |  5978 | `{` |
|      7 |  5979 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  5980 | `	int nData = 0;` |
|      7 |  5981 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : "";` |
|      7 |  5982 | `	if( pNd ){` |
|      7 |  5983 | `		xmlTextConcat((xmlNodePtr)pNd->pNode,(const xmlChar *)zData,nData);` |
|      3 |  5984 | `	}` |
|      7 |  5985 | `	ph7_result_bool(pCtx,1);` |
|      7 |  5986 | `	return PH7_OK;` |
|      1 |  5987 | `}` |
|      - |  5988 | `/*` |
|      - |  5989 | ` * The three writers, which php builds the same way: the head up to $offset, the` |
|      - |  5990 | ` * replacement, then whatever the count left of the tail.` |
|      - |  5991 | ` *` |
|      - |  5992 | ` * insertData is (offset, 0, data), deleteData is (offset, count, ""), and` |
|      - |  5993 | ` * replaceData is both -- php's own three bodies say the same thing three times.` |
|      - |  5994 | ` */` |
|     52 |  5995 | `static int DomCharSplice(ph7_context *pCtx,ph7_int64 iOffset,ph7_int64 iCount,` |
|      - |  5996 | `	int bHasCount,const char *zData,int nData)` |
|      1 |  5997 | `{` |
|     53 |  5998 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     53 |  5999 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     53 |  6000 | `	xmlChar *zHead,*zTail = 0;` |
|     53 |  6001 | `	int nLen,rc = PH7_OK;` |
|     53 |  6002 | `	if( pNode == 0 ){` |
|    ! 0 |  6003 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  6004 | `		return PH7_OK;` |
|      - |  6005 | `	}` |
|     53 |  6006 | `	if( pNode->content == 0 ){` |
|      - |  6007 | `		/* A writer normalizes the omitted-argument constructor's NULL content` |
|      - |  6008 | ``		 * to "" and proceeds, php's own answer: `insertData(0,'i')` on a`` |
|      - |  6009 | ``		 * `new DOMComment()` writes "i", and its nodeValue reads "" after. */`` |
|      5 |  6010 | `		xmlNodeSetContent(pNode,(const xmlChar *)"");` |
|      2 |  6011 | `	}` |
|      - |  6012 | `	/* insertData has no count and takes the unsigned bound; the two that DO` |
|      - |  6013 | `	 * take one take the signed bound. */` |
|     53 |  6014 | `	if( DomCharRange(pCtx,pNode,iOffset,iCount,bHasCount,!bHasCount,&nLen,&rc) != 0 ){` |
|     19 |  6015 | `		return rc;` |
|      - |  6016 | `	}` |
|     35 |  6017 | `	if( (sxu32)(iOffset+iCount) > (sxu32)nLen ){` |
|     11 |  6018 | `		iCount = (ph7_int64)nLen - iOffset;` |
|      5 |  6019 | `	}` |
|     27 |  6020 | `	zHead = iOffset > 0 ? xmlUTF8Strndup(pNode->content,(int)iOffset)` |
|     25 |  6021 | `	                    : xmlStrdup((const xmlChar *)"");` |
|     35 |  6022 | `	if( iOffset + iCount < (ph7_int64)nLen ){` |
|     31 |  6023 | `		zTail = xmlUTF8Strsub(pNode->content,(int)(iOffset+iCount),` |
|     20 |  6024 | `			(int)((ph7_int64)nLen - iOffset - iCount));` |
|     10 |  6025 | `	}` |
|     35 |  6026 | `	xmlNodeSetContent(pNode,zHead ? zHead : (const xmlChar *)"");` |
|     35 |  6027 | `	if( nData > 0 ){` |
|     21 |  6028 | `		xmlNodeAddContentLen(pNode,(const xmlChar *)zData,nData);` |
|     10 |  6029 | `	}` |
|     35 |  6030 | `	if( zTail ){` |
|     21 |  6031 | `		xmlNodeAddContent(pNode,zTail);` |
|     10 |  6032 | `	}` |
|     35 |  6033 | `	if( zHead ){` |
|     35 |  6034 | `		xmlFree(zHead);` |
|     17 |  6035 | `	}` |
|     35 |  6036 | `	if( zTail ){` |
|     21 |  6037 | `		xmlFree(zTail);` |
|     10 |  6038 | `	}` |
|     35 |  6039 | `	ph7_result_bool(pCtx,1);` |
|     35 |  6040 | `	return PH7_OK;` |
|     27 |  6041 | `}` |
|      - |  6042 | `/* DOMCharacterData::insertData(int $offset, string $data): true */` |
|     14 |  6043 | `DOM_METHOD(vm_builtin_DOMCharacterData_insertData)` |
|      1 |  6044 | `{` |
|     15 |  6045 | `	int nData = 0;` |
|     15 |  6046 | `	const char *zData = nArg > 1 ? ph7_value_to_string(apArg[1],&nData) : "";` |
|     15 |  6047 | `	return DomCharSplice(pCtx,nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0,0,FALSE,zData,nData);` |
|      1 |  6048 | `}` |
|      - |  6049 | `/* DOMCharacterData::deleteData(int $offset, int $count): true */` |
|     22 |  6050 | `DOM_METHOD(vm_builtin_DOMCharacterData_deleteData)` |
|      1 |  6051 | `{` |
|     45 |  6052 | `	return DomCharSplice(pCtx,nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0,` |
|     22 |  6053 | `		nArg > 1 ? ph7_value_to_int64(apArg[1]) : 0,TRUE,"",0);` |
|      1 |  6054 | `}` |
|      - |  6055 | `/* DOMCharacterData::replaceData(int $offset, int $count, string $data): true */` |
|     16 |  6056 | `DOM_METHOD(vm_builtin_DOMCharacterData_replaceData)` |
|      1 |  6057 | `{` |
|     17 |  6058 | `	int nData = 0;` |
|     17 |  6059 | `	const char *zData = nArg > 2 ? ph7_value_to_string(apArg[2],&nData) : "";` |
|     33 |  6060 | `	return DomCharSplice(pCtx,nArg > 2 ? ph7_value_to_int64(apArg[0]) : 0,` |
|     16 |  6061 | `		nArg > 2 ? ph7_value_to_int64(apArg[1]) : 0,TRUE,zData,nData);` |
|      1 |  6062 | `}` |
|      - |  6063 | `/*` |
|      - |  6064 | ` * DOMText::splitText(int $offset): DOMText\|false` |
|      - |  6065 | ` *` |
|      - |  6066 | ` * The receiver keeps the head and a SECOND node takes the tail, spliced in` |
|      - |  6067 | ` * right after it. Two details only the oracle states: an offset past the end is` |
|      - |  6068 | `` * plain `false` where a negative one is a ValueError, and splitting a CDATA`` |
|      - |  6069 | `` * section produces a TEXT node -- so `<![CDATA[abcdef]]>` split at 2 serializes`` |
|      - |  6070 | `` * as `<![CDATA[ab]]>cdef`.`` |
|      - |  6071 | ` */` |
|     22 |  6072 | `DOM_METHOD(vm_builtin_DOMText_splitText)` |
|      1 |  6073 | `{` |
|     23 |  6074 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     23 |  6075 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     23 |  6076 | `	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|      - |  6077 | `	xmlChar *zHead,*zTail;` |
|      - |  6078 | `	xmlNodePtr pNew;` |
|      - |  6079 | `	int nLen;` |
|     23 |  6080 | `	if( iOffset < 0 ){` |
|      3 |  6081 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  6082 | `			"DOMText::splitText(): Argument #1 ($offset) must be greater than or equal to 0");` |
|      - |  6083 | `	}` |
|     20 |  6084 | `	if( pNode == 0` |
|     21 |  6085 | `	 \|\| (pNode->type != XML_TEXT_NODE && pNode->type != XML_CDATA_SECTION_NODE) ){` |
|    ! 0 |  6086 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  6087 | `		return PH7_OK;` |
|      - |  6088 | `	}` |
|     21 |  6089 | `	if( pNode->content == 0 ){` |
|      - |  6090 | `		/* The omitted-argument constructor's node splits as "": both halves` |
|      - |  6091 | `		 * empty, php's answer. The split WRITES, so normalizing is its own. */` |
|      3 |  6092 | `		xmlNodeSetContent(pNode,(const xmlChar *)"");` |
|      1 |  6093 | `	}` |
|     21 |  6094 | `	nLen = DomCharLength(pNode);` |
|     21 |  6095 | `	if( iOffset > (ph7_int64)nLen ){` |
|      3 |  6096 | `		ph7_result_bool(pCtx,0);` |
|      3 |  6097 | `		return PH7_OK;` |
|      - |  6098 | `	}` |
|     19 |  6099 | `	zHead = xmlUTF8Strndup(pNode->content,(int)iOffset);` |
|     19 |  6100 | `	zTail = xmlUTF8Strsub(pNode->content,(int)iOffset,(int)((ph7_int64)nLen - iOffset));` |
|     19 |  6101 | `	xmlNodeSetContent(pNode,zHead ? zHead : (const xmlChar *)"");` |
|     19 |  6102 | `	pNew = xmlNewDocText(pNode->doc,zTail ? zTail : (const xmlChar *)"");` |
|     19 |  6103 | `	if( zHead ){` |
|     19 |  6104 | `		xmlFree(zHead);` |
|      9 |  6105 | `	}` |
|     19 |  6106 | `	if( zTail ){` |
|     19 |  6107 | `		xmlFree(zTail);` |
|      9 |  6108 | `	}` |
|     19 |  6109 | `	if( pNew == 0 ){` |
|    ! 0 |  6110 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  6111 | `		return PH7_OK;` |
|      - |  6112 | `	}` |
|     19 |  6113 | `	if( pNode->parent ){` |
|      - |  6114 | `		/* Spliced by hand, as everything in this file is: xmlAddNextSibling` |
|      - |  6115 | `		 * MERGES two adjacent text nodes and frees one of them. */` |
|     13 |  6116 | `		if( pNode->next ){` |
|      9 |  6117 | `			DomLinkBefore(pNode->parent,pNew,pNode->next);` |
|      5 |  6118 | `		}else{` |
|      5 |  6119 | `			DomLinkLast(pNode->parent,pNew);` |
|      - |  6120 | `		}` |
|      7 |  6121 | `	}else{` |
|      7 |  6122 | `		DomOrphanAdd(pNd->pShell,pNew);` |
|      - |  6123 | `	}` |
|     19 |  6124 | `	return DomResultNodeOf(pCtx,pNd,pNew);` |
|     12 |  6125 | `}` |
|      - |  6126 | `/* DOMText::isWhitespaceInElementContent() and its 8.x rename` |
|      - |  6127 | ` * isElementContentWhitespace(): one body, libxml's blank-node test. */` |
|     12 |  6128 | `DOM_METHOD(vm_builtin_DOMText_isWhitespace)` |
|      1 |  6129 | `{` |
|     13 |  6130 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      6 |  6131 | `	SXUNUSED(nArg);` |
|      6 |  6132 | `	SXUNUSED(apArg);` |
|     13 |  6133 | `	ph7_result_bool(pCtx,pNd && xmlIsBlankNode((xmlNodePtr)pNd->pNode));` |
|     13 |  6134 | `	return PH7_OK;` |
|      1 |  6135 | `}` |
|      - |  6136 |  |
|      - |  6137 | `/* ===== C14N ===== */` |
|      - |  6138 |  |
|      - |  6139 | `/*` |
|      - |  6140 | ` * php canonicalizes a NODE by handing libxml the node SET an XPath produces` |
|      - |  6141 | `` * from it -- `(.//. \| .//@* \| .//namespace::*)` with the node as context -- and`` |
|      - |  6142 | ` * a DOCUMENT by handing it no set at all, which is how a document's top-level` |
|      - |  6143 | ` * comments reach the output where a node's cannot. Running a VISIBILITY` |
|      - |  6144 | ` * callback instead (the shape this file had) is close but not the same: an` |
|      - |  6145 | `` * ATTRIBUTE canonicalizes to its own ` b="2"` under php, where a "keep the`` |
|      - |  6146 | ` * target's subtree" callback answers the empty string.` |
|      - |  6147 | ` *` |
|      - |  6148 | `` * All four of php's parameters are read here. `$exclusive` picks Exclusive`` |
|      - |  6149 | `` * C14N, `$withComments` keeps comments, `$xpath` REPLACES the default node set`` |
|      - |  6150 | ``  * with the caller's query (and may register prefixes for it), and `$nsPrefixes` `` |
|      - |  6151 | ` * lists the namespace prefixes an exclusive canonicalization must declare even` |
|      - |  6152 | ` * where they are unused. Only the two bools were honoured before, so` |
|      - |  6153 | `` * `C14N(true)` -- the mode every XML-DSig signer asks for -- silently`` |
|      - |  6154 | ` * canonicalized inclusively and produced bytes that will not verify.` |
|      - |  6155 | ` */` |
|      - |  6156 |  |
|      - |  6157 | ``/* The `namespaces` sub-array of `$xpath`: prefix => URI, string pairs only. */`` |
|    108 |  6158 | `static int DomC14NRegisterNs(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 |  6159 | `{` |
|    109 |  6160 | `	xmlXPathContextPtr pXCtx = (xmlXPathContextPtr)pUserData;` |
|    109 |  6161 | `	if( pKey && pVal && ph7_value_is_string(pKey) && ph7_value_is_string(pVal) ){` |
|    163 |  6162 | `		xmlXPathRegisterNs(pXCtx,(const xmlChar *)ph7_value_to_string(pKey,0),` |
|    108 |  6163 | `			(const xmlChar *)ph7_value_to_string(pVal,0));` |
|     54 |  6164 | `	}` |
|    109 |  6165 | `	return PH7_OK;` |
|      1 |  6166 | `}` |
|      - |  6167 | `/*` |
|      - |  6168 | `` * The `$nsPrefixes` list, collected into the NULL-terminated array libxml`` |
|      - |  6169 | ` * wants. Non-string entries are skipped, exactly as php skips them.` |
|      - |  6170 | ` *` |
|      - |  6171 | ` * The bytes are COPIED. ph7_array_walk hands its callback a temporary copy of` |
|      - |  6172 | ` * each value and releases it the moment the callback returns, so keeping the` |
|      - |  6173 | ` * pointer leaves a dangling one -- which libxml then compares against real` |
|      - |  6174 | `` * prefixes and matches at random, so `C14N(true,false,null,['u','p'])` declared`` |
|      - |  6175 | ` * whichever prefix the freed memory happened to still read as.` |
|      - |  6176 | ` */` |
|      - |  6177 | `typedef struct DomC14NPrefixes DomC14NPrefixes;` |
|      - |  6178 | `struct DomC14NPrefixes {` |
|      - |  6179 | `	SyBlob sPool;    /* the prefix bytes, NUL-terminated one after another */` |
|      - |  6180 | `	SySet aOfs;      /* each prefix's offset into sPool */` |
|      - |  6181 | `	xmlChar **apPrefix;` |
|      - |  6182 | `};` |
|     18 |  6183 | `static int DomC14NCollectPrefix(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 |  6184 | `{` |
|     19 |  6185 | `	DomC14NPrefixes *pList = (DomC14NPrefixes *)pUserData;` |
|      9 |  6186 | `	SXUNUSED(pKey);` |
|     19 |  6187 | `	if( pVal && ph7_value_is_string(pVal) ){` |
|     17 |  6188 | `		sxu32 nOfs = SyBlobLength(&pList->sPool);` |
|     17 |  6189 | `		int nByte = 0;` |
|     17 |  6190 | `		const char *zVal = ph7_value_to_string(pVal,&nByte);` |
|     17 |  6191 | `		SySetPut(&pList->aOfs,(const void *)&nOfs);` |
|     17 |  6192 | `		SyBlobAppend(&pList->sPool,zVal,(sxu32)nByte);` |
|     17 |  6193 | `		SyBlobAppend(&pList->sPool,"",1);` |
|      8 |  6194 | `	}` |
|     19 |  6195 | `	return PH7_OK;` |
|      1 |  6196 | `}` |
|      - |  6197 | `/*` |
|      - |  6198 | ` * Canonicalize the receiver into *pzOut (xmlFree'd by the caller) and answer` |
|      - |  6199 | ` * its byte count, or -1 when php answers false/"" instead. *pRc carries a` |
|      - |  6200 | ` * refusal php raises before anything is written.` |
|      - |  6201 | ` *` |
|      - |  6202 | `` * iXPathPos is the 1-based position of `$xpath` in the CALLING method's`` |
|      - |  6203 | ` * parameter list: 3 on C14N, 4 on C14NFile, and php's messages print it.` |
|      - |  6204 | ` */` |
|    118 |  6205 | `static int DomC14NRun(ph7_context *pCtx,int nArg,ph7_value **apArg,int iXPathPos,` |
|      - |  6206 | `	const char *zFn,xmlChar **pzOut,int *pRc)` |
|      1 |  6207 | `{` |
|      - |  6208 | `	char zGiven[64];` |
|    119 |  6209 | `	ph7_vm *pVm = pCtx->pVm;` |
|    119 |  6210 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    119 |  6211 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    119 |  6212 | `	int iFirst = iXPathPos - 3;   /* index of $exclusive */` |
|    119 |  6213 | `	int bExclusive = nArg > iFirst && ph7_value_to_bool(apArg[iFirst]);` |
|    119 |  6214 | `	int bComments = nArg > iFirst+1 && ph7_value_to_bool(apArg[iFirst+1]);` |
|     76 |  6215 | `	ph7_value *pXPath = (nArg > iFirst+2 && ph7_value_is_array(apArg[iFirst+2]))` |
|     83 |  6216 | `		? apArg[iFirst+2] : 0;` |
|     68 |  6217 | `	ph7_value *pPrefixes = (nArg > iFirst+3 && ph7_value_is_array(apArg[iFirst+3]))` |
|     75 |  6218 | `		? apArg[iFirst+3] : 0;` |
|      - |  6219 | `	DomC14NPrefixes sPrefixes;` |
|    119 |  6220 | `	xmlXPathContextPtr pXCtx = 0;` |
|    119 |  6221 | `	xmlXPathObjectPtr pXObj = 0;` |
|    119 |  6222 | `	xmlNodeSetPtr pSet = 0;` |
|      - |  6223 | `	sxu32 nMark,n;` |
|      - |  6224 | `	int nOut;` |
|    119 |  6225 | `	*pzOut = 0;` |
|    119 |  6226 | `	sPrefixes.apPrefix = 0;` |
|    119 |  6227 | `	SyBlobInit(&sPrefixes.sPool,&pVm->sAllocator);` |
|    119 |  6228 | `	SySetInit(&sPrefixes.aOfs,&pVm->sAllocator,sizeof(sxu32));` |
|    119 |  6229 | `	if( pNode == 0 ){` |
|    ! 0 |  6230 | `		nOut = -1;` |
|    ! 0 |  6231 | `		goto done;` |
|      - |  6232 | `	}` |
|    119 |  6233 | `	if( pNode->doc == 0 ){` |
|      - |  6234 | `		/* php's plain Error, no DOM code: canonicalization asks libxml for the` |
|      - |  6235 | `		 * document's context, and a constructed node has none. */` |
|      5 |  6236 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Node must be associated with a document");` |
|      5 |  6237 | `		nOut = -1;` |
|      5 |  6238 | `		goto done;` |
|      - |  6239 | `	}` |
|    115 |  6240 | `	if( pXPath ){` |
|     17 |  6241 | `		ph7_value *pQuery = ph7_array_fetch(pXPath,"query",(int)sizeof("query")-1);` |
|      - |  6242 | `		ph7_value *pNs;` |
|     17 |  6243 | `		if( pQuery == 0 ){` |
|      7 |  6244 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      2 |  6245 | `				"%s(): Argument #%d ($xpath) must have a \"query\" key",zFn,iXPathPos);` |
|      5 |  6246 | `			nOut = -1;` |
|      5 |  6247 | `			goto done;` |
|      - |  6248 | `		}` |
|     13 |  6249 | `		if( !ph7_value_is_string(pQuery) ){` |
|      4 |  6250 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  6251 | `				"%s(): Argument #%d ($xpath) \"query\" option must be a string, %s given",` |
|      1 |  6252 | `				zFn,iXPathPos,VmValueGivenName(pQuery,zGiven,sizeof(zGiven)));` |
|      3 |  6253 | `			nOut = -1;` |
|      3 |  6254 | `			goto done;` |
|      - |  6255 | `		}` |
|     11 |  6256 | `		pXCtx = xmlXPathNewContext(pNode->doc);` |
|     11 |  6257 | `		if( pXCtx == 0 ){` |
|    ! 0 |  6258 | `			nOut = -1;` |
|    ! 0 |  6259 | `			goto done;` |
|      - |  6260 | `		}` |
|     11 |  6261 | `		pXCtx->node = pNode;` |
|     11 |  6262 | `		pNs = ph7_array_fetch(pXPath,"namespaces",(int)sizeof("namespaces")-1);` |
|     11 |  6263 | `		if( pNs && ph7_value_is_array(pNs) ){` |
|      3 |  6264 | `			ph7_array_walk(pNs,DomC14NRegisterNs,pXCtx);` |
|      1 |  6265 | `		}` |
|     11 |  6266 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     11 |  6267 | `		pXObj = xmlXPathEvalExpression((const xmlChar *)ph7_value_to_string(pQuery,0),pXCtx);` |
|      - |  6268 | `		/* php lets libxml's own complaint out first ("Invalid expression"), THEN` |
|      - |  6269 | `		 * raises its refusal, so the queue is flushed rather than dropped. */` |
|     11 |  6270 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|     11 |  6271 | `		pXCtx->node = 0;` |
|    104 |  6272 | `	}else if( pNode->type != XML_DOCUMENT_NODE ){` |
|     55 |  6273 | `		pXCtx = xmlXPathNewContext(pNode->doc);` |
|     55 |  6274 | `		if( pXCtx == 0 ){` |
|    ! 0 |  6275 | `			nOut = -1;` |
|    ! 0 |  6276 | `			goto done;` |
|      - |  6277 | `		}` |
|     55 |  6278 | `		pXCtx->node = pNode;` |
|     55 |  6279 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     55 |  6280 | `		pXObj = xmlXPathEvalExpression(` |
|     27 |  6281 | `			(const xmlChar *)"(.//. \| .//@* \| .//namespace::*)",pXCtx);` |
|     55 |  6282 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|     55 |  6283 | `		pXCtx->node = 0;` |
|     27 |  6284 | `	}` |
|    109 |  6285 | `	if( pXCtx ){` |
|     65 |  6286 | `		if( pXObj == 0 \|\| pXObj->type != XPATH_NODESET ){` |
|      3 |  6287 | `			*pRc = PH7_VmThrowException(pCtx,"Error","XPath query did not return a nodeset");` |
|      3 |  6288 | `			nOut = -1;` |
|      3 |  6289 | `			goto done;` |
|      - |  6290 | `		}` |
|     63 |  6291 | `		pSet = pXObj->nodesetval;` |
|     31 |  6292 | `	}` |
|      - |  6293 | ``	/* php reads `$nsPrefixes` only AFTER the query has been resolved, so a bad`` |
|      - |  6294 | `	 * query's refusal reaches the caller with no notice in front of it. */` |
|    107 |  6295 | `	if( pPrefixes ){` |
|     17 |  6296 | `		if( bExclusive ){` |
|     15 |  6297 | `			ph7_array_walk(pPrefixes,DomC14NCollectPrefix,&sPrefixes);` |
|     15 |  6298 | `			n = SySetUsed(&sPrefixes.aOfs);` |
|     15 |  6299 | `			if( n > 0 ){` |
|     25 |  6300 | `				sPrefixes.apPrefix = (xmlChar **)SyMemBackendAlloc(&pVm->sAllocator,` |
|     12 |  6301 | `					(sxu32)((n+1)*sizeof(xmlChar *)));` |
|     13 |  6302 | `				if( sPrefixes.apPrefix == 0 ){` |
|    ! 0 |  6303 | `					nOut = -1;` |
|    ! 0 |  6304 | `					goto done;` |
|      - |  6305 | `				}` |
|      - |  6306 | `				/* Offsets, not pointers, until the pool has stopped growing. */` |
|     29 |  6307 | `				for( n = 0 ; n < SySetUsed(&sPrefixes.aOfs) ; ++n ){` |
|     25 |  6308 | `					sPrefixes.apPrefix[n] = (xmlChar *)SyBlobData(&sPrefixes.sPool)` |
|     16 |  6309 | `						+ ((sxu32 *)SySetBasePtr(&sPrefixes.aOfs))[n];` |
|      9 |  6310 | `				}` |
|     13 |  6311 | `				sPrefixes.apPrefix[n] = 0;` |
|      6 |  6312 | `			}` |
|      8 |  6313 | `		}else{` |
|      - |  6314 | `			/* php's E_NOTICE, and the list is then ignored outright. */` |
|      3 |  6315 | `			ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|      - |  6316 | `				"Inclusive namespace prefixes only allowed in exclusive mode.");` |
|      - |  6317 | `		}` |
|      8 |  6318 | `	}` |
|      - |  6319 | `	/* Canonicalized into an output buffer of our own rather than through` |
|      - |  6320 | `	 * xmlC14NDocDumpMemory, which is php's shape and one diagnostic quieter:` |
|      - |  6321 | `	 * that wrapper adds an "Internal error : saving doc to output buffer" of` |
|      - |  6322 | `	 * its own on top of libxml's real complaint, and php -- which drives the` |
|      - |  6323 | `	 * save itself -- never prints it. */` |
|      - |  6324 | `	{` |
|    107 |  6325 | `		xmlBufferPtr pBuf = xmlBufferCreate();` |
|    107 |  6326 | `		xmlOutputBufferPtr pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|    107 |  6327 | `		if( pOut == 0 ){` |
|    ! 0 |  6328 | `			if( pBuf ){` |
|    ! 0 |  6329 | `				xmlBufferFree(pBuf);` |
|    ! 0 |  6330 | `			}` |
|    ! 0 |  6331 | `			nOut = -1;` |
|    ! 0 |  6332 | `			goto done;` |
|      - |  6333 | `		}` |
|    107 |  6334 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    160 |  6335 | `		nOut = xmlC14NDocSaveTo(pNode->doc,pSet,` |
|     53 |  6336 | `			bExclusive ? XML_C14N_EXCLUSIVE_1_0 : XML_C14N_1_0,` |
|     53 |  6337 | `			sPrefixes.apPrefix,bComments,pOut);` |
|    107 |  6338 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    107 |  6339 | `		xmlOutputBufferFlush(pOut);` |
|    107 |  6340 | `		if( nOut >= 0 ){` |
|    101 |  6341 | `			const xmlChar *zBuf = xmlBufferContent(pBuf);` |
|    101 |  6342 | `			nOut = (int)xmlBufferLength(pBuf);` |
|      - |  6343 | `			/* An EMPTY canonicalization is a real answer -- a detached node` |
|      - |  6344 | `			 * is visible from nowhere in the document -- so the bytes are` |
|      - |  6345 | `			 * always allocated, even when there are none. */` |
|    101 |  6346 | `			*pzOut = xmlStrndup(zBuf ? zBuf : (const xmlChar *)"",nOut);` |
|    101 |  6347 | `			if( *pzOut == 0 ){` |
|    ! 0 |  6348 | `				nOut = -1;` |
|    ! 0 |  6349 | `			}` |
|     50 |  6350 | `		}` |
|    107 |  6351 | `		xmlOutputBufferClose(pOut);` |
|    107 |  6352 | `		xmlBufferFree(pBuf);` |
|      - |  6353 | `	}` |
|    107 |  6354 | `	if( nOut < 0 && *pzOut ){` |
|    ! 0 |  6355 | `		xmlFree(*pzOut);` |
|    ! 0 |  6356 | `		*pzOut = 0;` |
|    ! 0 |  6357 | `	}` |
|     53 |  6358 | `done:` |
|    119 |  6359 | `	if( pXObj ){` |
|     63 |  6360 | `		xmlXPathFreeObject(pXObj);` |
|     31 |  6361 | `	}` |
|    119 |  6362 | `	if( pXCtx ){` |
|     65 |  6363 | `		xmlXPathFreeContext(pXCtx);` |
|     32 |  6364 | `	}` |
|    119 |  6365 | `	if( sPrefixes.apPrefix ){` |
|     13 |  6366 | `		SyMemBackendFree(&pVm->sAllocator,(void *)sPrefixes.apPrefix);` |
|      6 |  6367 | `	}` |
|    119 |  6368 | `	SyBlobRelease(&sPrefixes.sPool);` |
|    119 |  6369 | `	SySetRelease(&sPrefixes.aOfs);` |
|    119 |  6370 | `	return nOut;` |
|      1 |  6371 | `}` |
|      - |  6372 | `/*` |
|      - |  6373 | ` * DOMNode::C14N(bool $exclusive = false, bool $withComments = false,` |
|      - |  6374 | ` *               ?array $xpath = null, ?array $nsPrefixes = null): string\|false` |
|      - |  6375 | ` *` |
|      - |  6376 | ` * FALSE when the canonicalization fails, which is the answer a signer has to` |
|      - |  6377 | ` * be able to tell from a document that canonicalizes to nothing: a detached` |
|      - |  6378 | ` * node and a fragment are both the EMPTY STRING (nothing of either is visible` |
|      - |  6379 | ` * from the document, and that is a real answer), while an entity REFERENCE` |
|      - |  6380 | ` * anywhere in the tree -- an ordinary document parsed without` |
|      - |  6381 | `` * `substituteEntities` -- is a refusal libxml states and php reports as false.`` |
|      - |  6382 | ` * Answering "" for both signed the empty string instead of failing.` |
|      - |  6383 | ` */` |
|    100 |  6384 | `DOM_METHOD(vm_builtin_DOMNode_C14N)` |
|      1 |  6385 | `{` |
|    101 |  6386 | `	xmlChar *zOut = 0;` |
|    101 |  6387 | `	int rc = PH7_OK;` |
|    101 |  6388 | `	int nOut = DomC14NRun(pCtx,nArg,apArg,3,"DOMNode::C14N",&zOut,&rc);` |
|    101 |  6389 | `	if( rc != PH7_OK ){` |
|     11 |  6390 | `		return rc;` |
|      - |  6391 | `	}` |
|     91 |  6392 | `	if( nOut < 0 ){` |
|      5 |  6393 | `		ph7_result_bool(pCtx,0);` |
|      5 |  6394 | `		return PH7_OK;` |
|      - |  6395 | `	}` |
|     87 |  6396 | `	ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|     87 |  6397 | `	xmlFree(zOut);` |
|     87 |  6398 | `	return PH7_OK;` |
|     51 |  6399 | `}` |
|      - |  6400 | `/*` |
|      - |  6401 | ` * DOMNode::C14NFile(string $uri, bool $exclusive = false,` |
|      - |  6402 | ` *                   bool $withComments = false, ?array $xpath = null,` |
|      - |  6403 | ` *                   ?array $nsPrefixes = null): int\|false` |
|      - |  6404 | ` *` |
|      - |  6405 | ` * The same canonicalization written to a destination instead of answered, and` |
|      - |  6406 | ` * the byte count rather than the bytes. The destination goes through the stream` |
|      - |  6407 | `` * layer -- php's libxml I/O is wired to php's streams, so `php://stdout` and a`` |
|      - |  6408 | ` * userland wrapper are both valid here -- which is also where php's` |
|      - |  6409 | ` * "Failed to open stream" warning comes from.` |
|      - |  6410 | ` */` |
|     22 |  6411 | `DOM_METHOD(vm_builtin_DOMNode_C14NFile)` |
|      1 |  6412 | `{` |
|     23 |  6413 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  6414 | `	const ph7_io_stream *pStream;` |
|      - |  6415 | `	void *pHandle;` |
|     23 |  6416 | `	xmlChar *zOut = 0;` |
|      - |  6417 | `	const char *zFile;` |
|     23 |  6418 | `	int nFile = 0,nOut,rc = PH7_OK;` |
|     23 |  6419 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|     23 |  6420 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|      3 |  6421 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  6422 | `			"DOMNode::C14NFile(): Argument #1 ($uri) must not contain any null bytes");` |
|      - |  6423 | `	}` |
|     21 |  6424 | `	if( nFile < 1 ){` |
|      3 |  6425 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|      - |  6426 | `	}` |
|     19 |  6427 | `	nOut = DomC14NRun(pCtx,nArg,apArg,4,"DOMNode::C14NFile",&zOut,&rc);` |
|     19 |  6428 | `	if( rc != PH7_OK ){` |
|      3 |  6429 | `		return rc;` |
|      - |  6430 | `	}` |
|     17 |  6431 | `	if( nOut < 0 ){` |
|      3 |  6432 | `		ph7_result_bool(pCtx,0);` |
|      3 |  6433 | `		return PH7_OK;` |
|      - |  6434 | `	}` |
|     15 |  6435 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|     15 |  6436 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|      - |  6437 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|     14 |  6438 | `		ph7_function_name(pCtx)) : 0;` |
|     15 |  6439 | `	if( pHandle == 0 ){` |
|      5 |  6440 | `		xmlFree(zOut);` |
|      5 |  6441 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      5 |  6442 | `		ph7_result_bool(pCtx,0);` |
|      5 |  6443 | `		return PH7_OK;` |
|      - |  6444 | `	}` |
|     11 |  6445 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|    ! 0 |  6446 | `		nOut = -1;` |
|    ! 0 |  6447 | `	}` |
|     11 |  6448 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     11 |  6449 | `	xmlFree(zOut);` |
|     11 |  6450 | `	if( nOut < 0 ){` |
|    ! 0 |  6451 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  6452 | `		return PH7_OK;` |
|      - |  6453 | `	}` |
|     11 |  6454 | `	ph7_result_int(pCtx,nOut);` |
|     11 |  6455 | `	return PH7_OK;` |
|     12 |  6456 | `}` |
|      - |  6457 | `/*` |
|      - |  6458 | ` * DOMNode::__sleep(): array and DOMNode::__wakeup(): void` |
|      - |  6459 | ` *` |
|      - |  6460 | ` * php declares both on DOMNode and both do one thing: refuse. They are the` |
|      - |  6461 | ``  * MECHANISM behind the refusal, not decoration -- `serialize()` finds `__sleep` `` |
|      - |  6462 | `` * and `unserialize()` calls `__wakeup`, which is why a subclass that declares`` |
|      - |  6463 | ` * its own escapes both. Without them, PHL refused serialize() from its own deny` |
|      - |  6464 | ` * handler (same sentence) but UNSERIALIZE went through in silence and handed` |
|      - |  6465 | ` * back a DOM object with no node behind it, which then answered nothing for` |
|      - |  6466 | ` * every property a program read off it.` |
|      - |  6467 | ` */` |
|     30 |  6468 | `DOM_METHOD(vm_builtin_DOMNode_sleep)` |
|      1 |  6469 | `{` |
|     31 |  6470 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     15 |  6471 | `	SXUNUSED(nArg);` |
|     15 |  6472 | `	SXUNUSED(apArg);` |
|     31 |  6473 | `	if( pThis == 0 ){` |
|    ! 0 |  6474 | `		return PH7_OK;` |
|      - |  6475 | `	}` |
|     46 |  6476 | `	return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  6477 | `		"Serialization of '%z' is not allowed, unless serialization methods "` |
|     30 |  6478 | `		"are implemented in a subclass",&pThis->pClass->sDisp);` |
|     16 |  6479 | `}` |
|     16 |  6480 | `DOM_METHOD(vm_builtin_DOMNode_wakeup)` |
|      1 |  6481 | `{` |
|     17 |  6482 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      8 |  6483 | `	SXUNUSED(nArg);` |
|      8 |  6484 | `	SXUNUSED(apArg);` |
|     17 |  6485 | `	if( pThis == 0 ){` |
|    ! 0 |  6486 | `		return PH7_OK;` |
|      - |  6487 | `	}` |
|     25 |  6488 | `	return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  6489 | `		"Unserialization of '%z' is not allowed, unless unserialization methods "` |
|     16 |  6490 | `		"are implemented in a subclass",&pThis->pClass->sDisp);` |
|      9 |  6491 | `}` |
|      - |  6492 |  |
|      - |  6493 | `/* ===== DOMNodeList and DOMNamedNodeMap ===== */` |
|      - |  6494 |  |
|      - |  6495 | `/*` |
|      - |  6496 | ` * A node list is one of three things, and which one it is decides both count()` |
|      - |  6497 | ` * and item(). Two of the three are LIVE views (they re-walk the tree on every` |
|      - |  6498 | ` * question, which is what makes getElementsByTagName track mutations); the third` |
|      - |  6499 | ` * is the document-order snapshot DOMXPath::query froze.` |
|      - |  6500 | ` */` |
|      - |  6501 | `#define DNL_CHILD 0   /* $node->childNodes */` |
|      - |  6502 | `#define DNL_GEBTN 1   /* getElementsByTagName($name) */` |
|      - |  6503 | `#define DNL_SNAP  2   /* DOMXPath::query() */` |
|      - |  6504 | `#define DNL_GEBTNNS 3 /* getElementsByTagNameNS($uri, $localName) */` |
|      - |  6505 | `/* ...and a NAMED map is one of two: an element's attribute list, or one of the` |
|      - |  6506 | ` * two DTD declaration TABLES, which are libxml hash tables rather than node` |
|      - |  6507 | ` * lists -- the reason a parameter entity, which is a child of the DTD like` |
|      - |  6508 | `` * every other declaration, is not in `entities`. */`` |
|      - |  6509 | `#define DNL_ENTS  4   /* $doctype->entities */` |
|      - |  6510 | `#define DNL_NOTS  5   /* $doctype->notations */` |
|      - |  6511 | `#define DNL_KIND  "__kind"` |
|      - |  6512 | `#define DNL_OWNER "__owner"` |
|      - |  6513 | `#define DNL_NAME  "__name"` |
|      - |  6514 | `#define DNL_URI   "__uri"` |
|      - |  6515 | `#define DNL_SNAP_SLOT "__snap"` |
|      - |  6516 |  |
|      - |  6517 | `/* The node a live list is a view OF. */` |
|   1192 |  6518 | `static phl_domnode * DomListOwner(ph7_class_instance *pList)` |
|      4 |  6519 | `{` |
|   1196 |  6520 | `	return DomResOf(PH7_NativeAttrObj(pList,DNL_OWNER));` |
|      4 |  6521 | `}` |
|    176 |  6522 | `static ph7_hashmap * DomListSnap(ph7_class_instance *pList)` |
|      1 |  6523 | `{` |
|    177 |  6524 | `	ph7_value *pVal = PH7_NativeAttr(pList,DNL_SNAP_SLOT);` |
|    177 |  6525 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  6526 | `		return 0;` |
|      - |  6527 | `	}` |
|    177 |  6528 | `	return (ph7_hashmap *)pVal->x.pOther;` |
|     89 |  6529 | `}` |
|    274 |  6530 | `static int DomListCount(ph7_class_instance *pList)` |
|      2 |  6531 | `{` |
|      - |  6532 | `	phl_domnode *pOwner;` |
|    276 |  6533 | `	const char *zName,*zUri = 0;` |
|    276 |  6534 | `	int nName,nUri = -1,iCount = 0;` |
|    276 |  6535 | `	if( pList == 0 ){` |
|    ! 0 |  6536 | `		return 0;` |
|      - |  6537 | `	}` |
|    276 |  6538 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){` |
|     69 |  6539 | `		ph7_hashmap *pMap = DomListSnap(pList);` |
|     69 |  6540 | `		return pMap ? (int)pMap->nEntry : 0;` |
|      - |  6541 | `	}` |
|    208 |  6542 | `	pOwner = DomListOwner(pList);` |
|    208 |  6543 | `	if( pOwner == 0 ){` |
|      3 |  6544 | `		return 0;` |
|      - |  6545 | `	}` |
|    206 |  6546 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){` |
|    136 |  6547 | `		return DomChildCount((xmlNodePtr)pOwner->pNode,0);` |
|      - |  6548 | `	}` |
|     71 |  6549 | `	PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);` |
|     71 |  6550 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_GEBTNNS ){` |
|     49 |  6551 | `		PH7_NativeAttrStr(pList,DNL_URI,&zUri,&nUri);` |
|     24 |  6552 | `	}` |
|     71 |  6553 | `	DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,-1,&iCount);` |
|     71 |  6554 | `	return iCount;` |
|    139 |  6555 | `}` |
|      - |  6556 | `/* The wrapper at one index, or NULL past the end. BORROWED, like every wrap. */` |
|    718 |  6557 | `static ph7_class_instance * DomListItem(ph7_vm *pVm,ph7_class_instance *pList,int iIndex)` |
|      2 |  6558 | `{` |
|      - |  6559 | `	ph7_class_instance *pDoc;` |
|      - |  6560 | `	phl_domnode *pOwner;` |
|    720 |  6561 | `	xmlNodePtr pNode = 0;` |
|    720 |  6562 | `	const char *zName,*zUri = 0;` |
|    720 |  6563 | `	int nName,nUri = -1;` |
|    720 |  6564 | `	if( pList == 0 \|\| iIndex < 0 ){` |
|    ! 0 |  6565 | `		return 0;` |
|      - |  6566 | `	}` |
|    720 |  6567 | `	pDoc = PH7_NativeAttrObj(pList,DOM_DOC);` |
|    720 |  6568 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){` |
|    109 |  6569 | `		ph7_hashmap *pMap = DomListSnap(pList);` |
|    109 |  6570 | `		ph7_hashmap_node *pEntry = 0;` |
|      - |  6571 | `		ph7_value sKey,*pHit;` |
|      - |  6572 | `		phl_domnode *pRes;` |
|    109 |  6573 | `		if( pMap == 0 ){` |
|    ! 0 |  6574 | `			return 0;` |
|      - |  6575 | `		}` |
|    109 |  6576 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)iIndex);` |
|    109 |  6577 | `		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){` |
|     21 |  6578 | `			pEntry = 0;` |
|     10 |  6579 | `		}` |
|    109 |  6580 | `		PH7_MemObjRelease(&sKey);` |
|    109 |  6581 | `		pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;` |
|    109 |  6582 | `		if( pHit && (pHit->iFlags & MEMOBJ_OBJ) ){` |
|      - |  6583 | `			/* A namespace:: axis entry holds the DOMNameSpaceNode ITSELF (a` |
|      - |  6584 | `			 * fresh object per query, one object per list -- php's answer);` |
|      - |  6585 | `			 * the snapshot owns it, so this one is borrowed and referenced` |
|      - |  6586 | `			 * here to answer with the same contract DomWrap's does. */` |
|     39 |  6587 | `			ph7_class_instance *pNs = (ph7_class_instance *)pHit->x.pOther;` |
|     39 |  6588 | `			pNs->iRef++;` |
|     39 |  6589 | `			return pNs;` |
|      - |  6590 | `		}` |
|     71 |  6591 | `		pRes = (pHit && (pHit->iFlags & MEMOBJ_RES)) ? (phl_domnode *)pHit->x.pOther : 0;` |
|     71 |  6592 | `		return pRes ? DomWrap(&(*pVm),pDoc,pRes->pShell,(xmlNodePtr)pRes->pNode) : 0;` |
|      - |  6593 | `	}` |
|    612 |  6594 | `	pOwner = DomListOwner(pList);` |
|    612 |  6595 | `	if( pOwner == 0 ){` |
|     13 |  6596 | `		return 0;` |
|      - |  6597 | `	}` |
|    600 |  6598 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){` |
|    332 |  6599 | `		pNode = DomChildAt((xmlNodePtr)pOwner->pNode,iIndex);` |
|    167 |  6600 | `	}else{` |
|    269 |  6601 | `		PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);` |
|    269 |  6602 | `		if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_GEBTNNS ){` |
|    147 |  6603 | `			PH7_NativeAttrStr(pList,DNL_URI,&zUri,&nUri);` |
|     73 |  6604 | `		}` |
|    269 |  6605 | `		pNode = DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,iIndex,0);` |
|      - |  6606 | `	}` |
|    600 |  6607 | `	return DomWrap(&(*pVm),pDoc,pOwner->pShell,pNode);` |
|    361 |  6608 | `}` |
|      - |  6609 | `/*` |
|      - |  6610 | ` * Build one. pOwnerObj is the node the live view is of (NULL for a snapshot),` |
|      - |  6611 | ` * pSnap the frozen list (NULL otherwise). The caller owns the reference.` |
|      - |  6612 | ` */` |
|    626 |  6613 | `static ph7_class_instance * DomNewCollection(ph7_vm *pVm,const char *zClass,` |
|      - |  6614 | `	ph7_class_instance *pDoc,int iKind,ph7_class_instance *pOwnerObj,` |
|      - |  6615 | `	const char *zName,const char *zUri,ph7_value *pSnap)` |
|      4 |  6616 | `{` |
|    630 |  6617 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    630 |  6618 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|    630 |  6619 | `	if( pObj == 0 ){` |
|    ! 0 |  6620 | `		return 0;` |
|      - |  6621 | `	}` |
|    630 |  6622 | `	PH7_NativeSetAttrInt(&(*pVm),pObj,DNL_KIND,iKind);` |
|    630 |  6623 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|    630 |  6624 | `	if( pOwnerObj ){` |
|    536 |  6625 | `		PH7_NativeSetAttrObj(&(*pVm),pObj,DNL_OWNER,pOwnerObj);` |
|    266 |  6626 | `	}` |
|    630 |  6627 | `	if( zName ){` |
|    147 |  6628 | `		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_NAME,zName,(int)SyStrlen(zName));` |
|     73 |  6629 | `	}` |
|    630 |  6630 | `	if( zUri ){` |
|     45 |  6631 | `		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_URI,zUri,(int)SyStrlen(zUri));` |
|     22 |  6632 | `	}` |
|    630 |  6633 | `	if( pSnap ){` |
|     95 |  6634 | `		ph7_value *pSlot = PH7_NativeAttr(pObj,DNL_SNAP_SLOT);` |
|     95 |  6635 | `		if( pSlot ){` |
|     95 |  6636 | `			PH7_MemObjStore(pSnap,pSlot);` |
|     47 |  6637 | `		}` |
|     47 |  6638 | `	}` |
|    630 |  6639 | `	return pObj;` |
|    317 |  6640 | `}` |
|      - |  6641 | `/* DOMNodeList::count(): int and ::item(int $index): ?DOMNode */` |
|     10 |  6642 | `DOM_METHOD(vm_builtin_DOMNodeList_count)` |
|      1 |  6643 | `{` |
|      5 |  6644 | `	SXUNUSED(nArg);` |
|      5 |  6645 | `	SXUNUSED(apArg);` |
|     11 |  6646 | `	ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));` |
|     11 |  6647 | `	return PH7_OK;` |
|      1 |  6648 | `}` |
|      - |  6649 | `/*` |
|      - |  6650 | ` * The index both collections take, screened before it is narrowed.` |
|      - |  6651 | ` *` |
|      - |  6652 | `` * php's is a `int` position in a list that cannot hold more than INT_MAX`` |
|      - |  6653 | ` * entries, so everything outside [0, INT_MAX] is out of range -- and the two` |
|      - |  6654 | ` * classes then disagree about what to DO with one: the list answers null and` |
|      - |  6655 | ` * the named map raises a ValueError naming the bound.  Narrowing first was a` |
|      - |  6656 | ``  * silent wrong answer either way: `item(4294967296)` and `item(PHP_INT_MIN)` `` |
|      - |  6657 | ` * truncate to 0 and answered the FIRST node of the collection.` |
|      - |  6658 | ` *` |
|      - |  6659 | ` * Answers 1 when the index is usable.` |
|      - |  6660 | ` */` |
|      - |  6661 | `#define DOM_INDEX_MAX 2147483647` |
|    396 |  6662 | `static int DomCollectionIndex(int nArg,ph7_value **apArg,int *piIndex)` |
|      2 |  6663 | `{` |
|    398 |  6664 | `	ph7_int64 iWant = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    398 |  6665 | `	*piIndex = 0;` |
|    398 |  6666 | `	if( iWant < 0 \|\| iWant > DOM_INDEX_MAX ){` |
|     23 |  6667 | `		return 0;` |
|      - |  6668 | `	}` |
|    376 |  6669 | `	*piIndex = (int)iWant;` |
|    376 |  6670 | `	return 1;` |
|    200 |  6671 | `}` |
|    312 |  6672 | `DOM_METHOD(vm_builtin_DOMNodeList_item)` |
|      1 |  6673 | `{` |
|      - |  6674 | `	int iIndex;` |
|    313 |  6675 | `	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){` |
|     13 |  6676 | `		ph7_result_null(pCtx);` |
|     13 |  6677 | `		return PH7_OK;` |
|      - |  6678 | `	}` |
|    301 |  6679 | `	return DomResultOwned(pCtx,DomListItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));` |
|    157 |  6680 | `}` |
|      - |  6681 | ``/* php exposes `length` on both collections as a virtual property; the`` |
|      - |  6682 | ` * property HOOK below turns each recognizer into php's read and has handlers. */` |
|    268 |  6683 | `static int DomListProp(ph7_context *pCtx,const char *zName)` |
|      2 |  6684 | `{` |
|    270 |  6685 | `	if( DomNameIs(zName,"length") ){` |
|    266 |  6686 | `		ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));` |
|    266 |  6687 | `		return 1;` |
|      - |  6688 | `	}` |
|      5 |  6689 | `	return 0;` |
|    136 |  6690 | `}` |
|      - |  6691 | `/*` |
|      - |  6692 | ` * DOMNamedNodeMap: an element's attributes, keyed by name.` |
|      - |  6693 | ` *` |
|      - |  6694 | ` * It shares DOMNodeList's slots (the owner element in $__owner) but walks the` |
|      - |  6695 | ` * attribute list rather than the child list, so it gets its own two readers.` |
|      - |  6696 | ` */` |
|      - |  6697 | `/* Is this map one of the DTD DECLARATION tables rather than an element's` |
|      - |  6698 | ` * attribute list? The two are walked with entirely different machinery. */` |
|    468 |  6699 | `static int DomMapIsTable(ph7_class_instance *pMap)` |
|      3 |  6700 | `{` |
|    471 |  6701 | `	sxi64 iKind = pMap ? PH7_NativeAttrInt(pMap,DNL_KIND) : (sxi64)DNL_CHILD;` |
|    471 |  6702 | `	return iKind == DNL_ENTS \|\| iKind == DNL_NOTS;` |
|      3 |  6703 | `}` |
|      - |  6704 | `/*` |
|      - |  6705 | ` * The table itself, which is NULL for a doctype that declares nothing of that` |
|      - |  6706 | ` * kind: libxml allocates the hash only when the first declaration arrives, so` |
|      - |  6707 | ` * an absent table is an EMPTY map and not an error.` |
|      - |  6708 | ` */` |
|     94 |  6709 | `static xmlHashTablePtr DomMapHash(ph7_class_instance *pMap,phl_domnode *pOwner)` |
|      2 |  6710 | `{` |
|     96 |  6711 | `	xmlDtdPtr pDtd = pOwner ? (xmlDtdPtr)pOwner->pNode : 0;` |
|     94 |  6712 | `	if( !DomMapIsTable(pMap) \|\| pDtd == 0` |
|     96 |  6713 | `	 \|\| (pDtd->type != XML_DTD_NODE && pDtd->type != XML_DOCUMENT_TYPE_NODE) ){` |
|    ! 0 |  6714 | `		return 0;` |
|      - |  6715 | `	}` |
|     96 |  6716 | `	return (xmlHashTablePtr)(PH7_NativeAttrInt(pMap,DNL_KIND) == DNL_NOTS` |
|     47 |  6717 | `		? pDtd->notations : pDtd->entities);` |
|     49 |  6718 | `}` |
|      - |  6719 | `/*` |
|      - |  6720 | ` * A NOTATION declaration answered as a node.` |
|      - |  6721 | ` *` |
|      - |  6722 | `` * libxml's xmlNotation is `{name, PublicID, SystemID}` -- three strings and no`` |
|      - |  6723 | ` * type field -- so it cannot be handed to anything that walks a node.  php` |
|      - |  6724 | ` * builds an entity-shaped stand-in around it (the two structs share their` |
|      - |  6725 | ` * header, which is why the same reader answers both) with NO document and NO` |
|      - |  6726 | `` * parent, and that absence is php-visible: a notation's `ownerDocument` is`` |
|      - |  6727 | `` * null, its `isConnected` false and its `getRootNode()` itself.`` |
|      - |  6728 | ` *` |
|      - |  6729 | ` * php builds a FRESH one per lookup and frees it with the object; PHL builds` |
|      - |  6730 | ` * one per declaration and keeps it on the document's shell, so the wrapper` |
|      - |  6731 | `` * identity every other node has holds here too (recorded: `$map->item(0) ===`` |
|      - |  6732 | `` * $map->item(0)` is true here and false there).`` |
|      - |  6733 | ` */` |
|     34 |  6734 | `static xmlNodePtr DomNotationNode(phl_xmldoc *pShell,xmlNotationPtr pNot)` |
|      2 |  6735 | `{` |
|      - |  6736 | `	xmlEntityPtr *apHave;` |
|      - |  6737 | `	xmlEntityPtr pNode;` |
|      - |  6738 | `	sxu32 n;` |
|     36 |  6739 | `	if( pShell == 0 \|\| pNot == 0 ){` |
|    ! 0 |  6740 | `		return 0;` |
|      - |  6741 | `	}` |
|     36 |  6742 | `	apHave = (xmlEntityPtr *)SySetBasePtr(&pShell->aNotations);` |
|     48 |  6743 | `	for( n = 0 ; n < SySetUsed(&pShell->aNotations) ; ++n ){` |
|     38 |  6744 | `		if( apHave[n]->_private == (void *)pNot ){` |
|     26 |  6745 | `			return (xmlNodePtr)apHave[n];` |
|      - |  6746 | `		}` |
|      7 |  6747 | `	}` |
|     12 |  6748 | `	pNode = (xmlEntityPtr)xmlMalloc(sizeof(xmlEntity));` |
|     12 |  6749 | `	if( pNode == 0 ){` |
|    ! 0 |  6750 | `		return 0;` |
|      - |  6751 | `	}` |
|     12 |  6752 | `	SyZero(pNode,sizeof(xmlEntity));` |
|     12 |  6753 | `	pNode->type = XML_NOTATION_NODE;` |
|     12 |  6754 | `	pNode->name = xmlStrdup(pNot->name);` |
|     12 |  6755 | `	pNode->ExternalID = xmlStrdup(pNot->PublicID);` |
|     12 |  6756 | `	pNode->SystemID = xmlStrdup(pNot->SystemID);` |
|      - |  6757 | `	/* The declaration this stands for, so a second lookup finds it again. */` |
|     12 |  6758 | `	pNode->_private = (void *)pNot;` |
|     12 |  6759 | `	if( SySetPut(&pShell->aNotations,(const void *)&pNode) != SXRET_OK ){` |
|    ! 0 |  6760 | `		if( pNode->name ){` |
|    ! 0 |  6761 | `			xmlFree((xmlChar *)pNode->name);` |
|    ! 0 |  6762 | `		}` |
|    ! 0 |  6763 | `		if( pNode->ExternalID ){` |
|    ! 0 |  6764 | `			xmlFree((xmlChar *)pNode->ExternalID);` |
|    ! 0 |  6765 | `		}` |
|    ! 0 |  6766 | `		if( pNode->SystemID ){` |
|    ! 0 |  6767 | `			xmlFree((xmlChar *)pNode->SystemID);` |
|    ! 0 |  6768 | `		}` |
|    ! 0 |  6769 | `		xmlFree(pNode);` |
|    ! 0 |  6770 | `		return 0;` |
|      - |  6771 | `	}` |
|     12 |  6772 | `	return (xmlNodePtr)pNode;` |
|     19 |  6773 | `}` |
|      - |  6774 | `/* The payload a table map hands back, as a node: an entity declaration IS one,` |
|      - |  6775 | ` * a notation declaration needs its stand-in. */` |
|     82 |  6776 | `static xmlNodePtr DomTablePayload(ph7_class_instance *pMap,phl_domnode *pOwner,void *pPayload)` |
|      2 |  6777 | `{` |
|     84 |  6778 | `	if( pPayload == 0 ){` |
|     15 |  6779 | `		return 0;` |
|      - |  6780 | `	}` |
|     70 |  6781 | `	if( PH7_NativeAttrInt(pMap,DNL_KIND) == DNL_NOTS ){` |
|     36 |  6782 | `		return DomNotationNode(pOwner->pShell,(xmlNotationPtr)pPayload);` |
|      - |  6783 | `	}` |
|     35 |  6784 | `	return (xmlNodePtr)pPayload;` |
|     43 |  6785 | `}` |
|      - |  6786 | `/*` |
|      - |  6787 | ` * php walks these tables with xmlHashScan and takes the n-th thing it is` |
|      - |  6788 | ` * handed, so the ORDER a map answers in is the hash's and not the document's.` |
|      - |  6789 | ` * The same walk, so the same order.` |
|      - |  6790 | ` */` |
|      - |  6791 | `typedef struct DomHashPick DomHashPick;` |
|      - |  6792 | `struct DomHashPick {` |
|      - |  6793 | `	int iWant;              /* index still to be stepped over */` |
|      - |  6794 | `	void *pHit;             /* the payload at index 0 of what is left */` |
|      - |  6795 | `};` |
|     90 |  6796 | `static void DomHashPickOne(void *pPayload,void *pData,const xmlChar *zName)` |
|      2 |  6797 | `{` |
|     92 |  6798 | `	DomHashPick *pPick = (DomHashPick *)pData;` |
|     45 |  6799 | `	SXUNUSED(zName);` |
|     92 |  6800 | `	if( pPick->iWant > 0 ){` |
|     19 |  6801 | `		pPick->iWant--;` |
|     83 |  6802 | `	}else if( pPick->pHit == 0 ){` |
|     34 |  6803 | `		pPick->pHit = pPayload;` |
|     16 |  6804 | `	}` |
|     92 |  6805 | `}` |
|     40 |  6806 | `static void * DomHashAt(xmlHashTablePtr pTab,int iIndex)` |
|      2 |  6807 | `{` |
|      - |  6808 | `	DomHashPick sPick;` |
|     42 |  6809 | `	if( pTab == 0 \|\| iIndex < 0 \|\| iIndex >= xmlHashSize(pTab) ){` |
|      9 |  6810 | `		return 0;` |
|      - |  6811 | `	}` |
|     34 |  6812 | `	sPick.iWant = iIndex;` |
|     34 |  6813 | `	sPick.pHit = 0;` |
|     34 |  6814 | `	xmlHashScan(pTab,DomHashPickOne,&sPick);` |
|     34 |  6815 | `	return sPick.pHit;` |
|     22 |  6816 | `}` |
|    250 |  6817 | `static ph7_class_instance * DomMapItem(ph7_vm *pVm,ph7_class_instance *pMap,int iIndex)` |
|      3 |  6818 | `{` |
|    253 |  6819 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|      - |  6820 | `	xmlNodePtr pNode;` |
|    253 |  6821 | `	if( pOwner == 0 \|\| iIndex < 0 ){` |
|      3 |  6822 | `		return 0;` |
|      - |  6823 | `	}` |
|    251 |  6824 | `	pNode = DomMapIsTable(pMap)` |
|     40 |  6825 | `		? DomTablePayload(pMap,pOwner,DomHashAt(DomMapHash(pMap,pOwner),iIndex))` |
|    228 |  6826 | `		: (xmlNodePtr)DomAttrAt((xmlNodePtr)pOwner->pNode,iIndex);` |
|    251 |  6827 | `	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,pNode);` |
|    128 |  6828 | `}` |
|     28 |  6829 | `static int DomMapCount(ph7_class_instance *pMap)` |
|      1 |  6830 | `{` |
|     29 |  6831 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|     29 |  6832 | `	if( DomMapIsTable(pMap) ){` |
|     13 |  6833 | `		xmlHashTablePtr pTab = DomMapHash(pMap,pOwner);` |
|     13 |  6834 | `		return pTab ? xmlHashSize(pTab) : 0;` |
|      - |  6835 | `	}` |
|     17 |  6836 | `	return pOwner ? DomAttrCount((xmlNodePtr)pOwner->pNode) : 0;` |
|     15 |  6837 | `}` |
|      6 |  6838 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_count)` |
|      1 |  6839 | `{` |
|      3 |  6840 | `	SXUNUSED(nArg);` |
|      3 |  6841 | `	SXUNUSED(apArg);` |
|      7 |  6842 | `	ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));` |
|      7 |  6843 | `	return PH7_OK;` |
|      1 |  6844 | `}` |
|     84 |  6845 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_item)` |
|      2 |  6846 | `{` |
|      - |  6847 | `	int iIndex;` |
|     86 |  6848 | `	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){` |
|      - |  6849 | `		/* The map REFUSES what the list answers null for, and names the bound. */` |
|     11 |  6850 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  6851 | `			"DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and %d",` |
|      - |  6852 | `			DOM_INDEX_MAX);` |
|      - |  6853 | `	}` |
|     76 |  6854 | `	return DomResultOwned(pCtx,DomMapItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));` |
|     44 |  6855 | `}` |
|      - |  6856 | `/*` |
|      - |  6857 | `` * The by-NAME lookup, which `getNamedItem()` and the `$map['href']` subscript`` |
|      - |  6858 | ` * share.` |
|      - |  6859 | ` *` |
|      - |  6860 | ` * The MAP asks libxml's name-only question, where DOMElement's own` |
|      - |  6861 | `` * getAttributeNode resolves the prefix: `getNamedItem('k')` finds the`` |
|      - |  6862 | `` * namespaced `p:k` that `getAttribute('k')` does not. A DECLARATION table is`` |
|      - |  6863 | ` * keyed by that name to begin with, so it is one lookup.` |
|      - |  6864 | ` */` |
|     82 |  6865 | `static ph7_class_instance * DomMapNamed(ph7_vm *pVm,ph7_class_instance *pMap,const char *zName)` |
|      2 |  6866 | `{` |
|     84 |  6867 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|      - |  6868 | `	xmlNodePtr pHit;` |
|     84 |  6869 | `	if( pOwner == 0 ){` |
|    ! 0 |  6870 | `		return 0;` |
|      - |  6871 | `	}` |
|     84 |  6872 | `	pHit = DomMapIsTable(pMap)` |
|     54 |  6873 | `		? DomTablePayload(pMap,pOwner,` |
|     18 |  6874 | `			xmlHashLookup(DomMapHash(pMap,pOwner),(const xmlChar *)zName))` |
|     64 |  6875 | `		: (xmlNodePtr)DomAttrByLocal((xmlNodePtr)pOwner->pNode,zName);` |
|     84 |  6876 | `	if( pHit == 0 ){` |
|     29 |  6877 | `		return 0;` |
|      - |  6878 | `	}` |
|     56 |  6879 | `	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,pHit);` |
|     43 |  6880 | `}` |
|      - |  6881 | `/* DOMNamedNodeMap::getNamedItem(string $qualifiedName): ?DOMAttr */` |
|     42 |  6882 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItem)` |
|      2 |  6883 | `{` |
|     44 |  6884 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     44 |  6885 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     44 |  6886 | `	return DomResultOwned(pCtx,DomMapNamed(pCtx->pVm,pThis,zName));` |
|      2 |  6887 | `}` |
|      - |  6888 | `/* DOMNamedNodeMap::getNamedItemNS(?string $namespace, string $localName): ?DOMNode */` |
|     16 |  6889 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItemNS)` |
|      1 |  6890 | `{` |
|     17 |  6891 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     17 |  6892 | `	phl_domnode *pOwner = pThis ? DomListOwner(pThis) : 0;` |
|     17 |  6893 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     17 |  6894 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|      - |  6895 | `	/* A NULL namespace is the map's ANY here, not the element's "in no` |
|      - |  6896 | ``	 * namespace": `$el->attributes->getNamedItemNS(null,'k')` answers a`` |
|      - |  6897 | ``	 * namespaced `p:k` where `$el->getAttributeNodeNS(null,'k')` answers null.`` |
|      - |  6898 | `	 * An EMPTY namespace is neither -- it matches a URI no document has.` |
|      - |  6899 | `	 * A DECLARATION table has no namespaces at all and php reads right past` |
|      - |  6900 | `	 * the argument there: the URI decides nothing, the name decides` |
|      - |  6901 | `	 * everything. */` |
|     25 |  6902 | `	xmlNodePtr pHit = pOwner == 0 ? 0` |
|     29 |  6903 | `		: DomMapIsTable(pThis)` |
|      9 |  6904 | `			? DomTablePayload(pThis,pOwner,` |
|      3 |  6905 | `				xmlHashLookup(DomMapHash(pThis,pOwner),(const xmlChar *)zLocal))` |
|     18 |  6906 | `		: zUri ? (xmlNodePtr)DomAttrByNs((xmlNodePtr)pOwner->pNode,zUri,zLocal)` |
|      6 |  6907 | `		       : (xmlNodePtr)DomAttrByLocal((xmlNodePtr)pOwner->pNode,zLocal);` |
|     17 |  6908 | `	if( pHit == 0 ){` |
|      7 |  6909 | `		ph7_result_null(pCtx);` |
|      7 |  6910 | `		return PH7_OK;` |
|      - |  6911 | `	}` |
|     16 |  6912 | `	return DomResultOwned(pCtx,DomWrap(pCtx->pVm,PH7_NativeAttrObj(pThis,DOM_DOC),` |
|      5 |  6913 | `		pOwner->pShell,pHit));` |
|      9 |  6914 | `}` |
|     26 |  6915 | `static int DomMapProp(ph7_context *pCtx,const char *zName)` |
|      1 |  6916 | `{` |
|     27 |  6917 | `	if( DomNameIs(zName,"length") ){` |
|     23 |  6918 | `		ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));` |
|     23 |  6919 | `		return 1;` |
|      - |  6920 | `	}` |
|      5 |  6921 | `	return 0;` |
|     14 |  6922 | `}` |
|      - |  6923 | `/*` |
|      - |  6924 | `` * `$list[0]`, `$map['href']` and `isset($list[0])` -- ph7_class::xDim for the`` |
|      - |  6925 | ` * two collections, which are php's read_dimension / has_dimension handlers.` |
|      - |  6926 | ` *` |
|      - |  6927 | ` * php 8.3 gave both classes those handlers WITHOUT declaring ArrayAccess, so` |
|      - |  6928 | `` * `$list instanceof ArrayAccess` is FALSE there and the subscript reads anyway`` |
|      - |  6929 | ` * -- which is how every modern DOM example is written, and which no interface` |
|      - |  6930 | ` * list can state. Only the READ half exists: a store, an append and an unset` |
|      - |  6931 | `` * are all `Cannot use object of type C as array`, which is what the opcode`` |
|      - |  6932 | ` * answers for a class carrying no ArrayAccess.` |
|      - |  6933 | ` *` |
|      - |  6934 | ` * What an offset MEANS is one rule for both classes, and it is not the array` |
|      - |  6935 | `` * one. A STRING that STARTS with a number is an INDEX -- `"1x"` is 1 and`` |
|      - |  6936 | `` * `" 2 "` is 2, silently, php's is_numeric_string with errors allowed -- and`` |
|      - |  6937 | ` * one that does not is a NAME, which the list has no door for at all (so` |
|      - |  6938 | `` * `$list['x']` is null even on a document whose child element is named x).`` |
|      - |  6939 | ` * Every other offset type takes the ordinary int cast, warning exactly where` |
|      - |  6940 | `` * php's `(int)` warns (`$map[new stdClass]` is index 1 and a warning).`` |
|      - |  6941 | ` *` |
|      - |  6942 | ` * Then the two classes DISAGREE about an index outside [0, INT_MAX]: the list` |
|      - |  6943 | `` * answers null and the map raises `item()`'s own ValueError -- worded the way`` |
|      - |  6944 | ` * php words it with no function frame active to name the argument, so the` |
|      - |  6945 | `` * `DOMNamedNodeMap::item(): Argument #1 ($index) ` head is not there. isset()`` |
|      - |  6946 | ` * never refuses: php asks has_dimension, and that one answers a plain false.` |
|      - |  6947 | ` */` |
|      - |  6948 | `/*` |
|      - |  6949 | ` * Classify one offset. Answers 1 for a NAME (pScratch holds the string), 0 for` |
|      - |  6950 | ` * an INDEX in *piIndex. Works on a COPY: the conversion is destructive, the` |
|      - |  6951 | ` * caller's offset must survive it (empty() asks the same offset twice), and` |
|      - |  6952 | `` * php's own `$map[$k]` leaves $k alone. The caller releases pScratch.`` |
|      - |  6953 | ` */` |
|    308 |  6954 | `static int DomDimClassify(ph7_vm *pVm,ph7_value *pOffset,ph7_value *pScratch,sxi64 *piIndex)` |
|      2 |  6955 | `{` |
|    310 |  6956 | `	PH7_MemObjInit(&(*pVm),pScratch);` |
|    310 |  6957 | `	PH7_MemObjStore(pOffset,pScratch);` |
|    310 |  6958 | `	*piIndex = 0;` |
|    310 |  6959 | `	if( (pScratch->iFlags & MEMOBJ_STRING) && !PH7_MemObjStringNumericPrefix(pScratch,0) ){` |
|     69 |  6960 | `		return 1; /* a NAME: the caller reads the bytes out of the copy */` |
|      - |  6961 | `	}` |
|      - |  6962 | `	/* php reads an int out of every other offset type -- null is 0, a bool its` |
|      - |  6963 | `	 * value, a float truncated, an array 0 or 1 by emptiness, an object 1 behind` |
|      - |  6964 | ``	 * `could not be converted to int`. The cast is what hands back the reference`` |
|      - |  6965 | `	 * an object / array copy took (MemObjIntValue unrefs before it overwrites the` |
|      - |  6966 | `	 * type in place), so the release below has only a string blob left to free. */` |
|    242 |  6967 | `	PH7_MemObjWarnIntCast(pScratch);` |
|    242 |  6968 | `	*piIndex = ph7_value_to_int64(pScratch);` |
|    242 |  6969 | `	return 0;` |
|    156 |  6970 | `}` |
|      - |  6971 | `/* Hand the hook's answer back: the node, or nothing at all for a miss (which` |
|      - |  6972 | ` * the caller initialized NULL, and which isset() reads as false). Consumes the` |
|      - |  6973 | ` * reference DomWrap handed the caller. */` |
|    218 |  6974 | `static void DomDimAnswer(ph7_vm *pVm,PH7_NativeDimCtx *pCtx,ph7_class_instance *pHit)` |
|      2 |  6975 | `{` |
|      - |  6976 | `	ph7_value sVal;` |
|    220 |  6977 | `	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){` |
|     27 |  6978 | `		pCtx->pResult->x.iVal = pHit ? 1 : 0;` |
|     27 |  6979 | `		MemObjSetType(pCtx->pResult,MEMOBJ_BOOL);` |
|     27 |  6980 | `		if( pHit ){` |
|     13 |  6981 | `			PH7_ClassInstanceUnref(pHit);` |
|      6 |  6982 | `		}` |
|     50 |  6983 | `		return;` |
|      - |  6984 | `	}` |
|    194 |  6985 | `	if( pHit == 0 ){` |
|     47 |  6986 | `		return;` |
|      - |  6987 | `	}` |
|    148 |  6988 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    148 |  6989 | `	sVal.x.pOther = pHit;` |
|    148 |  6990 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    148 |  6991 | `	PH7_MemObjStore(&sVal,pCtx->pResult);   /* takes its own reference */` |
|    148 |  6992 | `	PH7_ClassInstanceUnref(pHit);           /* ...and ours goes back */` |
|    111 |  6993 | `}` |
|      - |  6994 | ``/* php's refusal for the keyless `$list[]` spelling, which reaches the handler`` |
|      - |  6995 | ` * with no offset at all. */` |
|      4 |  6996 | `static void DomDimNoOffset(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|      1 |  6997 | `{` |
|      5 |  6998 | `	SyString *pName = &pThis->pClass->sName;` |
|      5 |  6999 | `	pCtx->zThrowClass = "Error";` |
|      7 |  7000 | `	SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      4 |  7001 | `		"Cannot access %.*s without offset",(int)pName->nByte,pName->zString);` |
|      5 |  7002 | `}` |
|    192 |  7003 | `static void DomListDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|      2 |  7004 | `{` |
|      - |  7005 | `	ph7_value sKey;` |
|      - |  7006 | `	sxi64 iIndex;` |
|      - |  7007 | `	int bNamed;` |
|    194 |  7008 | `	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){` |
|      - |  7009 | `		/* The WRITE modes only ask for a refusal WORDING, and these two classes` |
|      - |  7010 | ``		 * have none of their own: php answers `Cannot use object of type C as`` |
|      - |  7011 | ``		 * array` for a store, an append and an unset, which is what the caller`` |
|      - |  7012 | `		 * formats when the hook declines. */` |
|     57 |  7013 | `		return;` |
|      - |  7014 | `	}` |
|    168 |  7015 | `	if( pCtx->pOffset == 0 ){` |
|      5 |  7016 | `		DomDimNoOffset(pThis,pCtx);` |
|      5 |  7017 | `		return;` |
|      - |  7018 | `	}` |
|    164 |  7019 | `	bNamed = DomDimClassify(&(*pVm),pCtx->pOffset,&sKey,&iIndex);` |
|    164 |  7020 | `	PH7_MemObjRelease(&sKey);` |
|    164 |  7021 | `	if( bNamed \|\| iIndex < 0 \|\| iIndex > DOM_INDEX_MAX ){` |
|     58 |  7022 | `		return; /* a name the list cannot answer, or an index it answers null to */` |
|      - |  7023 | `	}` |
|    108 |  7024 | `	DomDimAnswer(&(*pVm),pCtx,DomListItem(&(*pVm),pThis,(int)iIndex));` |
|     98 |  7025 | `}` |
|    158 |  7026 | `static void DomMapDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|      2 |  7027 | `{` |
|      - |  7028 | `	ph7_value sKey;` |
|      - |  7029 | `	sxi64 iIndex;` |
|      - |  7030 | `	int bNamed;` |
|    160 |  7031 | `	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){` |
|     50 |  7032 | `		return;   /* see DomListDim: the write side is php's generic sentence */` |
|      - |  7033 | `	}` |
|    148 |  7034 | `	if( pCtx->pOffset == 0 ){` |
|    ! 0 |  7035 | `		DomDimNoOffset(pThis,pCtx);` |
|    ! 0 |  7036 | `		return;` |
|      - |  7037 | `	}` |
|    148 |  7038 | `	bNamed = DomDimClassify(&(*pVm),pCtx->pOffset,&sKey,&iIndex);` |
|    148 |  7039 | `	if( bNamed ){` |
|     41 |  7040 | `		DomDimAnswer(&(*pVm),pCtx,DomMapNamed(&(*pVm),pThis,ph7_value_to_string(&sKey,0)));` |
|     41 |  7041 | `		PH7_MemObjRelease(&sKey);` |
|     41 |  7042 | `		return;` |
|      - |  7043 | `	}` |
|    108 |  7044 | `	PH7_MemObjRelease(&sKey);` |
|    108 |  7045 | `	if( iIndex < 0 \|\| iIndex > DOM_INDEX_MAX ){` |
|      - |  7046 | `		/* The range is screened BEFORE the map is looked at, so a map with no` |
|      - |  7047 | ``		 * owner at all -- `(new DOMNamedNodeMap())[-1]` -- refuses too. */`` |
|     36 |  7048 | `		if( pCtx->iMode == PH7_NATIVE_DIM_READ ){` |
|     28 |  7049 | `			pCtx->zThrowClass = "ValueError";` |
|     28 |  7050 | `			SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - |  7051 | `				"must be between 0 and %d",DOM_INDEX_MAX);` |
|     13 |  7052 | `		}` |
|     36 |  7053 | `		return;` |
|      - |  7054 | `	}` |
|     74 |  7055 | `	DomDimAnswer(&(*pVm),pCtx,DomMapItem(&(*pVm),pThis,(int)iIndex));` |
|     81 |  7056 | `}` |
|      - |  7057 | `/*` |
|      - |  7058 | ` * Both collections are IteratorAggregates, as php's are -- the chunk made` |
|      - |  7059 | ``  * DOMNodeList an `Iterator` with its own cursor (so `$list instanceof Iterator` `` |
|      - |  7060 | ` * was true where php says false) and gave DOMNamedNodeMap no iteration at all,` |
|      - |  7061 | `` * which meant `foreach ($el->attributes as $a)` walked the map's own private`` |
|      - |  7062 | ` * slots instead of the attributes.` |
|      - |  7063 | ` *` |
|      - |  7064 | ` * The cursor lives in the shared InternalIterator (oo_native.c); a vtable states` |
|      - |  7065 | ` * only how to REACH a position. DOMNodeList keys by index, DOMNamedNodeMap by` |
|      - |  7066 | ` * attribute name, which is what php answers for each.` |
|      - |  7067 | ` */` |
|    416 |  7068 | `static void DomIterSettle(ph7_vm *pVm,ph7_class_instance *pIt,int bNamed)` |
|      1 |  7069 | `{` |
|    417 |  7070 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    417 |  7071 | `	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);` |
|      - |  7072 | `	ph7_class_instance *pCur;` |
|    417 |  7073 | `	pCur = bNamed ? DomMapItem(&(*pVm),pSrc,(int)iPos) : DomListItem(&(*pVm),pSrc,(int)iPos);` |
|    417 |  7074 | `	if( pCur == 0 ){` |
|    111 |  7075 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    111 |  7076 | `		return;` |
|      - |  7077 | `	}` |
|    307 |  7078 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pCur);` |
|    307 |  7079 | `	PH7_ClassInstanceUnref(pCur);   /* the slot took its own; pCur lives on it */` |
|    307 |  7080 | `	if( bNamed ){` |
|     77 |  7081 | `		phl_domnode *pNd = DomResOf(pCur);` |
|     77 |  7082 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     77 |  7083 | `		const char *zKey = (pNode && pNode->name) ? (const char *)pNode->name : "";` |
|     77 |  7084 | `		PH7_NativeSetAttrStr(&(*pVm),pIt,PH7_NATIVE_IT_KEY,zKey,(int)SyStrlen(zKey));` |
|     39 |  7085 | `	}else{` |
|    231 |  7086 | `		PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);` |
|      - |  7087 | `	}` |
|    307 |  7088 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    209 |  7089 | `}` |
|    146 |  7090 | `static void DomListRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 |  7091 | `{` |
|    147 |  7092 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    147 |  7093 | `	DomIterSettle(&(*pVm),pIt,0);` |
|    147 |  7094 | `}` |
|    166 |  7095 | `static void DomListNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 |  7096 | `{` |
|    250 |  7097 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    166 |  7098 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    167 |  7099 | `	DomIterSettle(&(*pVm),pIt,0);` |
|    167 |  7100 | `}` |
|     58 |  7101 | `static void DomMapRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 |  7102 | `{` |
|     59 |  7103 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|     59 |  7104 | `	DomIterSettle(&(*pVm),pIt,1);` |
|     59 |  7105 | `}` |
|     46 |  7106 | `static void DomMapNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 |  7107 | `{` |
|     70 |  7108 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|     46 |  7109 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|     47 |  7110 | `	DomIterSettle(&(*pVm),pIt,1);` |
|     47 |  7111 | `}` |
|      - |  7112 | `static const PH7_NativeIterVtab sDomListIterVtab = { DomListRewind, DomListNext, 0, 0 };` |
|      - |  7113 | `static const PH7_NativeIterVtab sDomMapIterVtab  = { DomMapRewind,  DomMapNext, 0, 0 };` |
|      - |  7114 | `/* Both getIterator()s: a fresh InternalIterator per call, as php's are. */` |
|    106 |  7115 | `DOM_METHOD(vm_builtin_Dom_getIterator)` |
|      1 |  7116 | `{` |
|    107 |  7117 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7118 | `	ph7_class_instance *pIt;` |
|     53 |  7119 | `	SXUNUSED(nArg);` |
|     53 |  7120 | `	SXUNUSED(apArg);` |
|    107 |  7121 | `	if( pThis == 0 ){` |
|    ! 0 |  7122 | `		return PH7_OK;` |
|      - |  7123 | `	}` |
|    107 |  7124 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    107 |  7125 | `	if( pIt == 0 ){` |
|    ! 0 |  7126 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  7127 | `	}` |
|    107 |  7128 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    107 |  7129 | `	return PH7_OK;` |
|     54 |  7130 | `}` |
|      - |  7131 |  |
|      - |  7132 | `/* ===== DOMXPath ===== */` |
|      - |  7133 |  |
|      - |  7134 | `/* The prefix => URI table registerNamespace() feeds, replayed onto the fresh` |
|      - |  7135 | ` * evaluation context each query. Hidden slot, materialized by DomXPathSlotMap` |
|      - |  7136 | ` * below with the same three moves the document's identity cache needs. */` |
|      - |  7137 | `#define XP_NSREG "__nsreg"` |
|      - |  7138 | ``/* php declares `document` and `registerNodeNamespaces` VIRTUAL -- its object holds`` |
|      - |  7139 | ` * neither, and both are read_property handlers -- so the two values live in hidden` |
|      - |  7140 | ` * slots and the class declares the php-visible names with no storage at all. */` |
|      - |  7141 | `#define XP_DOC   "__xdoc"` |
|      - |  7142 | `#define XP_NSDEF "__xnsdef"` |
|      - |  7143 | `/*` |
|      - |  7144 | ` * DOMXPath::quote(string $str): string  (static, php 8.4)` |
|      - |  7145 | ` *` |
|      - |  7146 | ` * XPath 1.0 has no escape inside a string literal, so a value is quotable` |
|      - |  7147 | `` * only with the quote character it does not contain: no `'` and it goes in`` |
|      - |  7148 | `` * single quotes, no `"` in double ones, and a value carrying BOTH becomes a`` |
|      - |  7149 | `` * `concat()` of runs, split by the rule stated at the loop below. php's`` |
|      - |  7150 | ` * algorithm exactly, and its output byte for byte.` |
|      - |  7151 | ` */` |
|     38 |  7152 | `DOM_METHOD(vm_builtin_DOMXPath_quote)` |
|      1 |  7153 | `{` |
|     39 |  7154 | `	int nStr = 0;` |
|     39 |  7155 | `	const char *zStr = nArg > 0 ? ph7_value_to_string(apArg[0],&nStr) : "";` |
|     39 |  7156 | `	int bSq = 0,bDq = 0,i;` |
|      - |  7157 | `	SyBlob sOut;` |
|    225 |  7158 | `	for( i = 0 ; i < nStr ; ++i ){` |
|    187 |  7159 | `		if( zStr[i] == '\'' ){` |
|     29 |  7160 | `			bSq = 1;` |
|    173 |  7161 | `		}else if( zStr[i] == '"' ){` |
|     27 |  7162 | `			bDq = 1;` |
|     13 |  7163 | `		}` |
|     94 |  7164 | `	}` |
|     39 |  7165 | `	if( !bSq ){` |
|     17 |  7166 | `		ph7_result_string_format(pCtx,"'%.*s'",nStr,zStr);` |
|     17 |  7167 | `		return PH7_OK;` |
|      - |  7168 | `	}` |
|     23 |  7169 | `	if( !bDq ){` |
|      9 |  7170 | `		ph7_result_string_format(pCtx,"\"%.*s\"",nStr,zStr);` |
|      9 |  7171 | `		return PH7_OK;` |
|      - |  7172 | `	}` |
|     15 |  7173 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     15 |  7174 | `	SyBlobAppend(&sOut,"concat(",sizeof("concat(")-1);` |
|     15 |  7175 | `	i = 0;` |
|     49 |  7176 | `	while( i < nStr ){` |
|      - |  7177 | `		/* Whichever quote kind appears FIRST in what is left decides the run:` |
|      - |  7178 | `		 * the run is wrapped in the OTHER kind and reaches to that first` |
|      - |  7179 | `		 * occurrence, so it swallows every quote of the kind it is not wrapped` |
|      - |  7180 | ``		 * in. `a'b"c'd"e` is four runs that way, and `0"&'<` is two -- the`` |
|      - |  7181 | `		 * first single-quoted, because its first quote character is the double` |
|      - |  7182 | `		 * one. */` |
|     35 |  7183 | `		int iStart = i,j;` |
|     35 |  7184 | `		char cQuote = '"';` |
|     57 |  7185 | `		for( j = i ; j < nStr ; ++j ){` |
|     57 |  7186 | `			if( zStr[j] == '\'' \|\| zStr[j] == '"' ){` |
|     35 |  7187 | `				cQuote = zStr[j] == '"' ? '\'' : '"';` |
|     35 |  7188 | `				break;` |
|      - |  7189 | `			}` |
|     12 |  7190 | `		}` |
|    123 |  7191 | `		while( i < nStr && zStr[i] != cQuote ){` |
|     89 |  7192 | `			i++;` |
|      1 |  7193 | `		}` |
|     35 |  7194 | `		if( iStart > 0 ){` |
|     21 |  7195 | `			SyBlobAppend(&sOut,",",1);` |
|     10 |  7196 | `		}` |
|     35 |  7197 | `		SyBlobAppend(&sOut,&cQuote,1);` |
|     35 |  7198 | `		SyBlobAppend(&sOut,zStr + iStart,(sxu32)(i - iStart));` |
|     35 |  7199 | `		SyBlobAppend(&sOut,&cQuote,1);` |
|      1 |  7200 | `	}` |
|     15 |  7201 | `	SyBlobAppend(&sOut,")",1);` |
|     15 |  7202 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     15 |  7203 | `	SyBlobRelease(&sOut);` |
|     15 |  7204 | `	return PH7_OK;` |
|     20 |  7205 | `}` |
|      - |  7206 |  |
|      - |  7207 | `/* ===== The PHP-function bridge (php:function / php:functionString / own-URI) ===== */` |
|      - |  7208 |  |
|      - |  7209 | ``/* php's reserved URI: `php:function()` is reached through whatever PREFIX the`` |
|      - |  7210 | ` * caller bound to it, so every lookup here is by URI. */` |
|      - |  7211 | `#define XP_PHPNS "http://php.net/xpath"` |
|      - |  7212 | `/* Which callables an evaluation may reach: php's register_phpfunctions. */` |
|      - |  7213 | `#define XP_MODE_NONE   0   /* registerPhpFunctions() never called -- nothing runs */` |
|      - |  7214 | `#define XP_MODE_ALL    1   /* called bare -- any callable name runs */` |
|      - |  7215 | `#define XP_MODE_LIST   2   /* called with a restriction -- the table below decides */` |
|      - |  7216 | `#define XP_FNMODE "__fnmode"` |
|      - |  7217 | `#define XP_FNREG  "__fnreg"    /* restricted: xpath name => the callable to run */` |
|      - |  7218 | `#define XP_NSFN   "__nsfn"     /* own-URI: "<uri>\x01<name>" => callable */` |
|    108 |  7219 | `static ph7_hashmap * DomXPathSlotMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|      1 |  7220 | `{` |
|    109 |  7221 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|    109 |  7222 | `	if( pSlot == 0 ){` |
|    ! 0 |  7223 | `		return 0;` |
|      - |  7224 | `	}` |
|    109 |  7225 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     43 |  7226 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|    ! 0 |  7227 | `			return 0;` |
|      - |  7228 | `		}` |
|     21 |  7229 | `	}` |
|    109 |  7230 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|     55 |  7231 | `}` |
|      - |  7232 | `/* Insert (or replace) one string-keyed entry. */` |
|     26 |  7233 | `static void DomXPathMapPut(ph7_vm *pVm,ph7_hashmap *pMap,const char *zKey,int nKey,ph7_value *pVal)` |
|      1 |  7234 | `{` |
|      - |  7235 | `	ph7_value sKey;` |
|     27 |  7236 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     27 |  7237 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|     27 |  7238 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|     27 |  7239 | `	PH7_MemObjRelease(&sKey);` |
|     27 |  7240 | `}` |
|      - |  7241 | `/* ...and the matching read, or NULL. The value BELONGS to the map. */` |
|     42 |  7242 | `static ph7_value * DomXPathMapGet(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,` |
|      - |  7243 | `	const char *zKey,int nKey)` |
|      1 |  7244 | `{` |
|     43 |  7245 | `	ph7_hashmap *pMap = DomXPathSlotMap(&(*pVm),pThis,zSlot);` |
|     43 |  7246 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  7247 | `	ph7_value sKey,*pHit;` |
|     43 |  7248 | `	if( pMap == 0 ){` |
|    ! 0 |  7249 | `		return 0;` |
|      - |  7250 | `	}` |
|     43 |  7251 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     43 |  7252 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|     43 |  7253 | `	if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){` |
|      9 |  7254 | `		pEntry = 0;` |
|      4 |  7255 | `	}` |
|     43 |  7256 | `	PH7_MemObjRelease(&sKey);` |
|     43 |  7257 | `	pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;` |
|     43 |  7258 | `	return pHit;` |
|     22 |  7259 | `}` |
|      - |  7260 | `/*` |
|      - |  7261 | ` * What one evaluation needs to reach PHP from inside libxml, and what it` |
|      - |  7262 | ` * brings BACK.` |
|      - |  7263 | ` *` |
|      - |  7264 | ` * The bringing back is the whole design problem: a refusal raised from the` |
|      - |  7265 | ` * callback would run the enclosing catch RIGHT THERE, in the middle of` |
|      - |  7266 | ` * libxml's own recursion (the builtin-throw rail), so nothing is raised here.` |
|      - |  7267 | ` * The reason is PARKED -- a code and the name it quotes -- the evaluation is` |
|      - |  7268 | ` * stopped by setting the parser's error field (not xmlXPathErr, which would` |
|      - |  7269 | ` * queue a libxml diagnostic php does not print), and DomXPathEvalRun raises` |
|      - |  7270 | ` * once libxml has unwound. A throw from the CALLBACK ITSELF is the same` |
|      - |  7271 | ` * story one level up: its dispatch status is parked in rcUnwound and returned` |
|      - |  7272 | `` * from the method verbatim, which is what makes `php:function("boom") or`` |
|      - |  7273 | `` * php:function("after")` run neither the `or` arm nor anything past it --`` |
|      - |  7274 | ` * php's answer.` |
|      - |  7275 | ` */` |
|      - |  7276 | `#define XP_FN_OK        0` |
|      - |  7277 | `#define XP_FN_NOREG     1   /* registerPhpFunctions() was never called */` |
|      - |  7278 | `#define XP_FN_NOHANDLER 2   /* restricted, and this name is not in the table */` |
|      - |  7279 | `#define XP_FN_NOTSTR    3   /* the handler name argument is not a string */` |
|      - |  7280 | `#define XP_FN_NONAME    4   /* php:function() with no arguments at all */` |
|      - |  7281 | `#define XP_FN_BADCB     5   /* the name is not callable */` |
|      - |  7282 | `#define XP_FN_NOTNODE   6   /* the callback answered an object that is not a node */` |
|      - |  7283 | `typedef struct DomXPathFnCtx DomXPathFnCtx;` |
|      - |  7284 | `struct DomXPathFnCtx {` |
|      - |  7285 | `	ph7_context *pCtx;            /* the method's own call context */` |
|      - |  7286 | `	ph7_class_instance *pThis;    /* the DOMXPath */` |
|      - |  7287 | `	ph7_class_instance *pDoc;     /* its document object (where wrappers cache) */` |
|      - |  7288 | `	phl_domnode *pDocNd;` |
|      - |  7289 | `	int iErr;                     /* XP_FN_* -- raised after libxml unwinds */` |
|      - |  7290 | `	SyBlob sErrName;              /* the name that refusal quotes */` |
|      - |  7291 | `	sxi32 rcUnwound;              /* a callback that did not return */` |
|      - |  7292 | `};` |
|      - |  7293 | `/* Stop the evaluation without emitting a libxml diagnostic. */` |
|     22 |  7294 | `static void DomXPathFnStop(xmlXPathParserContextPtr pPCtx,DomXPathFnCtx *pFn,int iErr,` |
|      - |  7295 | `	const char *zName,int nName)` |
|      1 |  7296 | `{` |
|     23 |  7297 | `	if( pFn->iErr == XP_FN_OK ){` |
|     23 |  7298 | `		pFn->iErr = iErr;` |
|     23 |  7299 | `		SyBlobReset(&pFn->sErrName);` |
|     23 |  7300 | `		if( zName && nName > 0 ){` |
|     11 |  7301 | `			SyBlobAppend(&pFn->sErrName,zName,(sxu32)nName);` |
|      5 |  7302 | `		}` |
|     11 |  7303 | `	}` |
|     23 |  7304 | `	pPCtx->error = XPATH_EXPR_ERROR;` |
|     23 |  7305 | `}` |
|      - |  7306 | `/*` |
|      - |  7307 | ` * What a callback that did not RETURN leaves behind, which php's two` |
|      - |  7308 | ` * dispatchers do differently and both visibly.` |
|      - |  7309 | ` *` |
|      - |  7310 | ` * The one that looks a callable up in a REGISTERED table -- restricted` |
|      - |  7311 | ` * php:function, and every own-URI name -- returns without pushing, and libxml,` |
|      - |  7312 | ` * finding its value stack one short, queues its own "Stack usage error" before` |
|      - |  7313 | ` * unwinding; that entry is then on the list libxml_get_errors() answers. The` |
|      - |  7314 | ` * UNRESTRICTED php:function path pushes a value first, so its queue stays` |
|      - |  7315 | ` * clean. Either way the exception is the answer, and nothing further of the` |
|      - |  7316 | ` * expression runs.` |
|      - |  7317 | ` */` |
|      2 |  7318 | `static void DomXPathFnUnwound(xmlXPathParserContextPtr pPCtx,int bRegistered)` |
|      1 |  7319 | `{` |
|      3 |  7320 | `	if( !bRegistered ){` |
|      3 |  7321 | `		valuePush(pPCtx,xmlXPathNewCString(""));` |
|      3 |  7322 | `		pPCtx->error = XPATH_EXPR_ERROR;` |
|      1 |  7323 | `	}` |
|      3 |  7324 | `}` |
|      - |  7325 | `/* One XPath argument as php sees it: a nodeset becomes an ARRAY of wrappers` |
|      - |  7326 | ` * (php's own conversion), the three scalars their php types. bAsString is` |
|      - |  7327 | ` * php:functionString's flag, under which a nodeset arrives as its string` |
|      - |  7328 | ` * value instead. */` |
|     58 |  7329 | `static void DomXPathArgToValue(DomXPathFnCtx *pFn,xmlXPathObjectPtr pArg,int bAsString,` |
|      - |  7330 | `	ph7_value *pOut)` |
|      1 |  7331 | `{` |
|     59 |  7332 | `	ph7_vm *pVm = pFn->pCtx->pVm;` |
|     59 |  7333 | `	if( pArg == 0 ){` |
|    ! 0 |  7334 | `		PH7_MemObjInit(pVm,pOut);` |
|    ! 0 |  7335 | `		return;` |
|      - |  7336 | `	}` |
|     59 |  7337 | `	if( pArg->type == XPATH_NODESET && !bAsString ){` |
|      - |  7338 | `		/* The array is built on its OWN reference rather than the method's call` |
|      - |  7339 | `		 * context: a predicate calls this once per node, and a context-owned` |
|      - |  7340 | `		 * one would live until the whole evaluation ended. */` |
|      9 |  7341 | `		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);` |
|      - |  7342 | `		int i;` |
|      9 |  7343 | `		PH7_MemObjInit(pVm,pOut);` |
|      9 |  7344 | `		if( pMap == 0 ){` |
|    ! 0 |  7345 | `			return;` |
|      - |  7346 | `		}` |
|      - |  7347 | `		/* pOut CARRIES the map's only reference, and the caller's release of` |
|      - |  7348 | `		 * it after the call is what frees it. */` |
|      9 |  7349 | `		pOut->x.pOther = pMap;` |
|      9 |  7350 | `		pOut->iFlags = MEMOBJ_HASHMAP;` |
|     21 |  7351 | `		for( i = 0 ; pArg->nodesetval && i < pArg->nodesetval->nodeNr ; ++i ){` |
|     13 |  7352 | `			xmlNodePtr pNode = pArg->nodesetval->nodeTab[i];` |
|      - |  7353 | `			ph7_value sElem;` |
|      - |  7354 | `			ph7_class_instance *pObj;` |
|     13 |  7355 | `			if( pNode == 0 ){` |
|    ! 0 |  7356 | `				continue;` |
|      - |  7357 | `			}` |
|     13 |  7358 | `			if( pNode->type == XML_NAMESPACE_DECL ){` |
|    ! 0 |  7359 | `				xmlNsPtr pNs = (xmlNsPtr)pNode;` |
|    ! 0 |  7360 | `				xmlNodePtr pElem = (xmlNodePtr)pNs->next;` |
|    ! 0 |  7361 | `				xmlNsPtr pOrig = (pElem && pElem->type == XML_ELEMENT_NODE)` |
|    ! 0 |  7362 | `					? xmlSearchNs((xmlDocPtr)pFn->pDocNd->pNode,pElem,pNs->prefix) : 0;` |
|    ! 0 |  7363 | `				if( pOrig == 0 ){` |
|    ! 0 |  7364 | `					continue;` |
|      - |  7365 | `				}` |
|    ! 0 |  7366 | `				pObj = DomNewNsNode(pVm,pFn->pDoc,pFn->pDocNd->pShell,pOrig,pElem);` |
|    ! 0 |  7367 | `				if( pObj == 0 ){` |
|    ! 0 |  7368 | `					continue;` |
|      - |  7369 | `				}` |
|    ! 0 |  7370 | `				PH7_MemObjInit(pVm,&sElem);` |
|    ! 0 |  7371 | `				sElem.x.pOther = pObj;` |
|    ! 0 |  7372 | `				sElem.iFlags = MEMOBJ_OBJ;` |
|    ! 0 |  7373 | `				ph7_array_add_elem(pOut,0,&sElem);   /* takes its own reference */` |
|    ! 0 |  7374 | `				PH7_ClassInstanceUnref(pObj);        /* ...and ours goes back */` |
|    ! 0 |  7375 | `				continue;` |
|      - |  7376 | `			}` |
|     13 |  7377 | `			pObj = DomWrap(pVm,pFn->pDoc,pFn->pDocNd->pShell,pNode);` |
|     13 |  7378 | `			if( pObj == 0 ){` |
|    ! 0 |  7379 | `				continue;` |
|      - |  7380 | `			}` |
|     13 |  7381 | `			PH7_MemObjInit(pVm,&sElem);` |
|     13 |  7382 | `			sElem.x.pOther = pObj;` |
|     13 |  7383 | `			sElem.iFlags = MEMOBJ_OBJ;` |
|     13 |  7384 | `			ph7_array_add_elem(pOut,0,&sElem);   /* takes its own reference */` |
|     13 |  7385 | `			PH7_ClassInstanceUnref(pObj);        /* ...and ours goes back */` |
|      7 |  7386 | `		}` |
|      9 |  7387 | `		return;` |
|      - |  7388 | `	}` |
|     51 |  7389 | `	switch( pArg->type ){` |
|      3 |  7390 | `	case XPATH_BOOLEAN:` |
|      7 |  7391 | `		PH7_MemObjInitFromBool(pVm,pOut,pArg->boolval);` |
|      7 |  7392 | `		break;` |
|      2 |  7393 | `	case XPATH_NUMBER:` |
|      5 |  7394 | `		PH7_MemObjInitFromReal(pVm,pOut,pArg->floatval);` |
|      5 |  7395 | `		break;` |
|     20 |  7396 | `	default: {` |
|     41 |  7397 | `		xmlChar *zStr = xmlXPathCastToString(pArg);` |
|     41 |  7398 | `		PH7_MemObjInitFromString(pVm,pOut,0);` |
|     41 |  7399 | `		if( zStr ){` |
|     41 |  7400 | `			PH7_MemObjStringAppend(pOut,(const char *)zStr,(sxu32)SyStrlen((const char *)zStr));` |
|     41 |  7401 | `			xmlFree(zStr);` |
|     20 |  7402 | `		}` |
|     40 |  7403 | `		break;` |
|      - |  7404 | `	}` |
|      - |  7405 | `	}` |
|     30 |  7406 | `}` |
|      - |  7407 | `/* The callback's ANSWER, pushed back on the XPath stack: a bool stays a` |
|      - |  7408 | ` * boolean, a DOM node becomes a one-node set, and everything else is php's` |
|      - |  7409 | ` * string conversion -- the SAME three for both spellings, since` |
|      - |  7410 | ` * functionString's flag is about the ARGUMENTS. The conversion is the` |
|      - |  7411 | ` * user-visible one (an array draws php's "Array to string conversion" notice` |
|      - |  7412 | ` * and reads "Array"); a non-node object is the TypeError parked above. */` |
|     58 |  7413 | `static void DomXPathPushResult(xmlXPathParserContextPtr pPCtx,DomXPathFnCtx *pFn,` |
|      - |  7414 | `	ph7_value *pRes)` |
|      1 |  7415 | `{` |
|     59 |  7416 | `	if( (pRes->iFlags & MEMOBJ_BOOL) && (pRes->iFlags & MEMOBJ_STRING) == 0 ){` |
|      3 |  7417 | `		valuePush(pPCtx,xmlXPathNewBoolean(pRes->x.iVal != 0));` |
|      3 |  7418 | `		return;` |
|      - |  7419 | `	}` |
|     57 |  7420 | `	if( pRes->iFlags & MEMOBJ_OBJ ){` |
|      7 |  7421 | `		ph7_class_instance *pObj = (ph7_class_instance *)pRes->x.pOther;` |
|      7 |  7422 | `		phl_domnode *pNd = DomResOf(pObj);` |
|      7 |  7423 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      7 |  7424 | `		if( pNode == 0 \|\| pNode->type == XML_NAMESPACE_DECL ){` |
|      5 |  7425 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOTNODE,0,0);` |
|      5 |  7426 | `			return;` |
|      - |  7427 | `		}` |
|      3 |  7428 | `		valuePush(pPCtx,xmlXPathNewNodeSet(pNode));` |
|      3 |  7429 | `		return;` |
|      - |  7430 | `	}` |
|      - |  7431 | `	{` |
|     51 |  7432 | `		int nStr = 0;` |
|      - |  7433 | `		const char *zStr;` |
|      - |  7434 | `		xmlChar *zDup;` |
|     51 |  7435 | `		if( (pRes->iFlags & MEMOBJ_STRING) == 0 ){` |
|     17 |  7436 | `			sxi32 rcStr = PH7_MemObjToStringUV(pRes);` |
|     17 |  7437 | `			if( PH7_CALLBACK_UNWOUND(rcStr) ){` |
|      - |  7438 | `				/* A __toString() that threw: the same rail as the callback's` |
|      - |  7439 | `				 * own throw, one conversion later. */` |
|    ! 0 |  7440 | `				pFn->rcUnwound = rcStr;` |
|    ! 0 |  7441 | `				return;` |
|      - |  7442 | `			}` |
|      8 |  7443 | `		}` |
|     51 |  7444 | `		zStr = ph7_value_to_string(pRes,&nStr);` |
|     51 |  7445 | `		zDup = xmlStrndup((const xmlChar *)zStr,nStr);` |
|     51 |  7446 | `		valuePush(pPCtx,xmlXPathWrapString(zDup));` |
|      - |  7447 | `	}` |
|     30 |  7448 | `}` |
|      - |  7449 | `/*` |
|      - |  7450 | ` * The one C function behind every PHP-backed XPath name. libxml reaches it` |
|      - |  7451 | ` * through the lookup below, with the called name and URI on the context.` |
|      - |  7452 | ` */` |
|     78 |  7453 | `static void DomXPathPhpFn(xmlXPathParserContextPtr pPCtx,int nArgs)` |
|      1 |  7454 | `{` |
|     79 |  7455 | `	xmlXPathContextPtr pXCtx = pPCtx ? pPCtx->context : 0;` |
|     79 |  7456 | `	DomXPathFnCtx *pFn = pXCtx ? (DomXPathFnCtx *)pXCtx->funcLookupData : 0;` |
|     79 |  7457 | `	const xmlChar *zFn = pXCtx ? pXCtx->function : 0;` |
|     79 |  7458 | `	const xmlChar *zUri = pXCtx ? pXCtx->functionURI : 0;` |
|     79 |  7459 | `	int bPhpNs = zUri && xmlStrEqual(zUri,(const xmlChar *)XP_PHPNS);` |
|     79 |  7460 | `	int bAsString = bPhpNs && zFn && xmlStrEqual(zFn,(const xmlChar *)"functionString");` |
|     79 |  7461 | `	int bRegistered = !bPhpNs;   /* a table lookup rather than the name itself */` |
|      - |  7462 | `	xmlXPathObjectPtr *apArg;` |
|     79 |  7463 | `	ph7_value *apVal = 0,sResult,sName;` |
|     79 |  7464 | `	ph7_value *pCallable = 0;` |
|     79 |  7465 | `	int bNameOwned = 0;` |
|      - |  7466 | `	ph7_vm *pVm;` |
|     79 |  7467 | `	int nSkip = bPhpNs ? 1 : 0;   /* php:function's first argument NAMES the callback */` |
|      - |  7468 | `	int i,nCall;` |
|      - |  7469 | `	sxi32 rc;` |
|     79 |  7470 | `	if( pFn == 0 ){` |
|    ! 0 |  7471 | `		return;` |
|      - |  7472 | `	}` |
|     79 |  7473 | `	pVm = pFn->pCtx->pVm;` |
|      - |  7474 | `	/* Take the arguments off the stack FIRST (valuePop answers them last-first),` |
|      - |  7475 | `	 * so every exit below leaves libxml's stack where it found it. */` |
|     79 |  7476 | `	apArg = nArgs > 0` |
|    114 |  7477 | `		? (xmlXPathObjectPtr *)SyMemBackendAlloc(&pVm->sAllocator,` |
|     76 |  7478 | `			sizeof(xmlXPathObjectPtr) * (sxu32)nArgs)` |
|     39 |  7479 | `		: 0;` |
|     79 |  7480 | `	if( nArgs > 0 && apArg == 0 ){` |
|    ! 0 |  7481 | `		pPCtx->error = XPATH_MEMORY_ERROR;` |
|    ! 0 |  7482 | `		return;` |
|      - |  7483 | `	}` |
|    215 |  7484 | `	for( i = nArgs - 1 ; i >= 0 ; --i ){` |
|    137 |  7485 | `		apArg[i] = valuePop(pPCtx);` |
|     69 |  7486 | `	}` |
|     79 |  7487 | `	if( pFn->iErr != XP_FN_OK \|\| pFn->rcUnwound != 0 ){` |
|    ! 0 |  7488 | `		goto done;   /* a previous call already stopped this evaluation */` |
|      - |  7489 | `	}` |
|     79 |  7490 | `	if( bPhpNs ){` |
|     69 |  7491 | `		int nName = 0;` |
|      - |  7492 | `		const char *zName;` |
|     69 |  7493 | `		sxi64 iMode = PH7_NativeAttrInt(pFn->pThis,XP_FNMODE);` |
|     69 |  7494 | `		if( nArgs < 1 ){` |
|      3 |  7495 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NONAME,0,0);` |
|      3 |  7496 | `			goto done;` |
|      - |  7497 | `		}` |
|     67 |  7498 | `		if( apArg[0] == 0 \|\| apArg[0]->type != XPATH_STRING ){` |
|      3 |  7499 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOTSTR,0,0);` |
|      3 |  7500 | `			goto done;` |
|      - |  7501 | `		}` |
|     65 |  7502 | `		zName = apArg[0]->stringval ? (const char *)apArg[0]->stringval : "";` |
|     65 |  7503 | `		nName = (int)SyStrlen(zName);` |
|     65 |  7504 | `		if( iMode == XP_MODE_NONE ){` |
|      5 |  7505 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOREG,0,0);` |
|      5 |  7506 | `			goto done;` |
|      - |  7507 | `		}` |
|     61 |  7508 | `		if( iMode == XP_MODE_LIST ){` |
|     23 |  7509 | `			bRegistered = 1;` |
|     23 |  7510 | `			pCallable = DomXPathMapGet(pVm,pFn->pThis,XP_FNREG,zName,nName);` |
|     23 |  7511 | `			if( pCallable == 0 ){` |
|      9 |  7512 | `				DomXPathFnStop(pPCtx,pFn,XP_FN_NOHANDLER,zName,nName);` |
|      9 |  7513 | `				goto done;` |
|      - |  7514 | `			}` |
|      8 |  7515 | `		}else{` |
|      - |  7516 | `			/* Unrestricted: the NAME ITSELF is the callable, screened here` |
|      - |  7517 | `			 * because no registration screened it. */` |
|     39 |  7518 | `			PH7_MemObjInitFromString(pVm,&sName,0);` |
|     39 |  7519 | `			PH7_MemObjStringAppend(&sName,zName,(sxu32)nName);` |
|     39 |  7520 | `			bNameOwned = 1;` |
|     39 |  7521 | `			if( !PH7_VmIsCallable(pVm,&sName,TRUE) ){` |
|      3 |  7522 | `				DomXPathFnStop(pPCtx,pFn,XP_FN_BADCB,zName,nName);` |
|      3 |  7523 | `				goto done;` |
|      - |  7524 | `			}` |
|     37 |  7525 | `			pCallable = &sName;` |
|      - |  7526 | `		}` |
|     26 |  7527 | `	}else{` |
|      - |  7528 | `		SyBlob sKey;` |
|     11 |  7529 | `		SyBlobInit(&sKey,&pVm->sAllocator);` |
|     11 |  7530 | `		SyBlobAppend(&sKey,(const char *)zUri,zUri ? (sxu32)SyStrlen((const char *)zUri) : 0);` |
|     11 |  7531 | `		SyBlobAppend(&sKey,"\1",1);` |
|     11 |  7532 | `		SyBlobAppend(&sKey,(const char *)zFn,zFn ? (sxu32)SyStrlen((const char *)zFn) : 0);` |
|     16 |  7533 | `		pCallable = DomXPathMapGet(pVm,pFn->pThis,XP_NSFN,` |
|     10 |  7534 | `			(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey));` |
|     11 |  7535 | `		SyBlobRelease(&sKey);` |
|     11 |  7536 | `		if( pCallable == 0 ){` |
|    ! 0 |  7537 | `			goto done;   /* not ours after all: libxml reports the unknown function */` |
|      - |  7538 | `		}` |
|      - |  7539 | `	}` |
|     61 |  7540 | `	nCall = nArgs - nSkip;` |
|     61 |  7541 | `	if( nCall > 0 ){` |
|     67 |  7542 | `		apVal = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|     44 |  7543 | `			sizeof(ph7_value) * (sxu32)nCall);` |
|     45 |  7544 | `		if( apVal == 0 ){` |
|    ! 0 |  7545 | `			pPCtx->error = XPATH_MEMORY_ERROR;` |
|    ! 0 |  7546 | `			goto done;` |
|      - |  7547 | `		}` |
|    103 |  7548 | `		for( i = 0 ; i < nCall ; ++i ){` |
|     59 |  7549 | `			DomXPathArgToValue(pFn,apArg[i + nSkip],bAsString,&apVal[i]);` |
|     30 |  7550 | `		}` |
|     22 |  7551 | `	}` |
|     61 |  7552 | `	PH7_MemObjInit(pVm,&sResult);` |
|      - |  7553 | `	{` |
|     61 |  7554 | `		ph7_value **apPtr = nCall > 0` |
|     66 |  7555 | `			? (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|     44 |  7556 | `				sizeof(ph7_value *) * (sxu32)nCall)` |
|     30 |  7557 | `			: 0;` |
|     61 |  7558 | `		if( nCall > 0 && apPtr == 0 ){` |
|    ! 0 |  7559 | `			pPCtx->error = XPATH_MEMORY_ERROR;` |
|    ! 0 |  7560 | `			PH7_MemObjRelease(&sResult);` |
|    ! 0 |  7561 | `			goto done;` |
|      - |  7562 | `		}` |
|      - |  7563 | `		/* Dispatch off a COPY: pCallable points into a registration map this` |
|      - |  7564 | `		 * very callback can rewrite (a callback calling registerPhpFunctions` |
|      - |  7565 | `		 * on its own DOMXPath), and the map's value would go out from under` |
|      - |  7566 | `		 * the dispatch. */` |
|      - |  7567 | `		ph7_value sCall;` |
|     61 |  7568 | `		PH7_MemObjInit(pVm,&sCall);` |
|     61 |  7569 | `		PH7_MemObjStore(pCallable,&sCall);` |
|    119 |  7570 | `		for( i = 0 ; i < nCall ; ++i ){` |
|     59 |  7571 | `			apPtr[i] = &apVal[i];` |
|     30 |  7572 | `		}` |
|     61 |  7573 | `		rc = PH7_VmCallCallbackByValue(pVm,&sCall,nCall,apPtr,&sResult,0);` |
|     61 |  7574 | `		PH7_MemObjRelease(&sCall);` |
|     61 |  7575 | `		if( apPtr ){` |
|     45 |  7576 | `			SyMemBackendFree(&pVm->sAllocator,apPtr);` |
|     22 |  7577 | `		}` |
|      - |  7578 | `	}` |
|     61 |  7579 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      3 |  7580 | `		pFn->rcUnwound = rc;` |
|      3 |  7581 | `		DomXPathFnUnwound(pPCtx,bRegistered);` |
|      2 |  7582 | `	}else{` |
|     59 |  7583 | `		DomXPathPushResult(pPCtx,pFn,&sResult);` |
|      - |  7584 | `	}` |
|     61 |  7585 | `	PH7_MemObjRelease(&sResult);` |
|     39 |  7586 | `done:` |
|     79 |  7587 | `	if( bNameOwned ){` |
|     39 |  7588 | `		PH7_MemObjRelease(&sName);` |
|     19 |  7589 | `	}` |
|     79 |  7590 | `	if( apVal ){` |
|    103 |  7591 | `		for( i = 0 ; i < nArgs - nSkip ; ++i ){` |
|     59 |  7592 | `			PH7_MemObjRelease(&apVal[i]);` |
|     30 |  7593 | `		}` |
|     45 |  7594 | `		SyMemBackendFree(&pVm->sAllocator,apVal);` |
|     22 |  7595 | `	}` |
|    215 |  7596 | `	for( i = 0 ; i < nArgs ; ++i ){` |
|    137 |  7597 | `		if( apArg[i] ){` |
|    137 |  7598 | `			xmlXPathFreeObject(apArg[i]);` |
|     68 |  7599 | `		}` |
|     69 |  7600 | `	}` |
|     79 |  7601 | `	if( apArg ){` |
|     77 |  7602 | `		SyMemBackendFree(&pVm->sAllocator,apArg);` |
|     38 |  7603 | `	}` |
|     40 |  7604 | `}` |
|      - |  7605 | `/*` |
|      - |  7606 | ` * libxml's function-resolution hook: answer the bridge for php's two reserved` |
|      - |  7607 | ` * names and for any (URI, name) this object registered, and NULL for` |
|      - |  7608 | ` * everything else -- which is what makes libxml fall through to its own table` |
|      - |  7609 | `` * (so `count()` and friends still resolve).`` |
|      - |  7610 | ` */` |
|    143 |  7611 | `static xmlXPathFunction DomXPathFnLookup(void *pUserData,const xmlChar *zName,const xmlChar *zUri)` |
|      1 |  7612 | `{` |
|    144 |  7613 | `	DomXPathFnCtx *pFn = (DomXPathFnCtx *)pUserData;` |
|      - |  7614 | `	SyBlob sKey;` |
|      - |  7615 | `	ph7_value *pHit;` |
|    144 |  7616 | `	if( pFn == 0 \|\| zUri == 0 \|\| zName == 0 ){` |
|     70 |  7617 | `		return 0;` |
|      - |  7618 | `	}` |
|     75 |  7619 | `	if( xmlStrEqual(zUri,(const xmlChar *)XP_PHPNS) ){` |
|     64 |  7620 | `		if( xmlStrEqual(zName,(const xmlChar *)"function")` |
|     38 |  7621 | `		 \|\| xmlStrEqual(zName,(const xmlChar *)"functionString") ){` |
|     65 |  7622 | `			return DomXPathPhpFn;` |
|      - |  7623 | `		}` |
|    ! 0 |  7624 | `		return 0;` |
|      - |  7625 | `	}` |
|     11 |  7626 | `	SyBlobInit(&sKey,&pFn->pCtx->pVm->sAllocator);` |
|     11 |  7627 | `	SyBlobAppend(&sKey,(const char *)zUri,(sxu32)SyStrlen((const char *)zUri));` |
|     11 |  7628 | `	SyBlobAppend(&sKey,"\1",1);` |
|     11 |  7629 | `	SyBlobAppend(&sKey,(const char *)zName,(sxu32)SyStrlen((const char *)zName));` |
|     16 |  7630 | `	pHit = DomXPathMapGet(pFn->pCtx->pVm,pFn->pThis,XP_NSFN,` |
|     10 |  7631 | `		(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey));` |
|     11 |  7632 | `	SyBlobRelease(&sKey);` |
|     11 |  7633 | `	return pHit ? DomXPathPhpFn : 0;` |
|     39 |  7634 | `}` |
|      - |  7635 | `/* The parked refusal, raised once libxml has unwound. */` |
|     22 |  7636 | `static int DomXPathFnRaise(ph7_context *pCtx,DomXPathFnCtx *pFn)` |
|      1 |  7637 | `{` |
|     23 |  7638 | `	const char *zName = (const char *)SyBlobData(&pFn->sErrName);` |
|     23 |  7639 | `	int nName = (int)SyBlobLength(&pFn->sErrName);` |
|     23 |  7640 | `	switch( pFn->iErr ){` |
|      2 |  7641 | `	case XP_FN_NOREG:` |
|      5 |  7642 | `		return PH7_VmThrowException(pCtx,"Error","No callbacks were registered");` |
|      4 |  7643 | `	case XP_FN_NOHANDLER:` |
|     13 |  7644 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      4 |  7645 | `			"No callback handler \"%.*s\" registered",nName,zName);` |
|      1 |  7646 | `	case XP_FN_NOTSTR:` |
|      3 |  7647 | `		return PH7_VmThrowException(pCtx,"TypeError","Handler name must be a string");` |
|      1 |  7648 | `	case XP_FN_NONAME:` |
|      3 |  7649 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  7650 | `			"Function name must be passed as the first argument");` |
|      1 |  7651 | `	case XP_FN_BADCB:` |
|      4 |  7652 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  7653 | `			"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",` |
|      1 |  7654 | `			nName,zName,nName,zName);` |
|      2 |  7655 | `	case XP_FN_NOTNODE:` |
|      5 |  7656 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  7657 | `			"Only objects that are instances of DOM nodes can be converted to an XPath expression");` |
|    ! 0 |  7658 | `	default:` |
|    ! 0 |  7659 | `		break;` |
|      - |  7660 | `	}` |
|    ! 0 |  7661 | `	return PH7_OK;` |
|     12 |  7662 | `}` |
|      - |  7663 | `/*` |
|      - |  7664 | ` * Build the evaluation context for one query()/evaluate() call: a FRESH` |
|      - |  7665 | ` * xmlXPathContext (php keeps a persistent one; replaying the registration` |
|      - |  7666 | ` * table onto a fresh one answers the same), anchored at the explicit context` |
|      - |  7667 | ` * node -- or, with none, at the document ELEMENT, php's own substitution (so` |
|      - |  7668 | ` * query('file') matches a child of the root; an explicitly PASSED document` |
|      - |  7669 | ` * node is NOT substituted and carries no namespaces).` |
|      - |  7670 | ` *` |
|      - |  7671 | ` * bRegNodeNs is php's $registerNodeNS: the context NODE's in-scope` |
|      - |  7672 | ` * declarations go into pXCtx->namespaces, the array xmlXPathNsLookup consults` |
|      - |  7673 | ` * BEFORE the registered table -- which is why a document prefix beats a` |
|      - |  7674 | ` * registerNamespace() one only for that call. The caller frees the returned` |
|      - |  7675 | ` * list with xmlFree AFTER evaluating (the xmlNs entries belong to the tree;` |
|      - |  7676 | ` * only the array is owned).` |
|      - |  7677 | ` */` |
|    228 |  7678 | `static xmlNsPtr * DomXPathCtxOpen(ph7_context *pCtx,phl_domnode *pDocNd,` |
|      - |  7679 | `	phl_domnode *pCtxNd,int bRegNodeNs,xmlXPathContextPtr *ppXCtx)` |
|      1 |  7680 | `{` |
|    229 |  7681 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7682 | `	xmlXPathContextPtr pXCtx;` |
|      - |  7683 | `	ph7_value *pNsReg;` |
|    229 |  7684 | `	xmlNsPtr *aNs = 0;` |
|    229 |  7685 | `	*ppXCtx = 0;` |
|    229 |  7686 | `	pXCtx = xmlXPathNewContext((xmlDocPtr)pDocNd->pNode);` |
|    229 |  7687 | `	if( pXCtx == 0 ){` |
|    ! 0 |  7688 | `		return 0;` |
|      - |  7689 | `	}` |
|    229 |  7690 | `	if( pCtxNd ){` |
|     25 |  7691 | `		pXCtx->node = (xmlNodePtr)pCtxNd->pNode;` |
|     13 |  7692 | `	}else{` |
|    205 |  7693 | `		pXCtx->node = xmlDocGetRootElement((xmlDocPtr)pDocNd->pNode);` |
|      - |  7694 | `	}` |
|    229 |  7695 | `	pNsReg = pThis ? PH7_NativeAttr(pThis,XP_NSREG) : 0;` |
|    229 |  7696 | `	if( pNsReg && (pNsReg->iFlags & MEMOBJ_HASHMAP) ){` |
|     95 |  7697 | `		ph7_array_walk(pNsReg,DomC14NRegisterNs,pXCtx);` |
|     47 |  7698 | `	}` |
|    229 |  7699 | `	if( bRegNodeNs && pXCtx->node ){` |
|    223 |  7700 | `		aNs = xmlGetNsList((xmlDocPtr)pDocNd->pNode,pXCtx->node);` |
|    223 |  7701 | `		if( aNs ){` |
|     45 |  7702 | `			int nNs = 0;` |
|    105 |  7703 | `			while( aNs[nNs] ){` |
|     61 |  7704 | `				nNs++;` |
|      1 |  7705 | `			}` |
|     45 |  7706 | `			pXCtx->namespaces = aNs;` |
|     45 |  7707 | `			pXCtx->nsNr = nNs;` |
|     22 |  7708 | `		}` |
|    111 |  7709 | `	}` |
|    229 |  7710 | `	*ppXCtx = pXCtx;` |
|    229 |  7711 | `	return aNs;` |
|    115 |  7712 | `}` |
|      - |  7713 | `/*` |
|      - |  7714 | ` * Freeze a nodeset result into the document-order snapshot a DNL_SNAP` |
|      - |  7715 | ` * DOMNodeList serves (php's query() is not live), and answer the list. A` |
|      - |  7716 | ` * non-nodeset pObj answers the EMPTY list: php's query() gives that for a` |
|      - |  7717 | `` * scalar-typed expression (`count(//x)`), not false.`` |
|      - |  7718 | ` */` |
|     94 |  7719 | `static int DomXPathResultList(ph7_context *pCtx,ph7_class_instance *pDoc,` |
|      - |  7720 | `	phl_domnode *pDocNd,xmlXPathObjectPtr pObj)` |
|      1 |  7721 | `{` |
|     95 |  7722 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  7723 | `	ph7_class_instance *pList;` |
|     95 |  7724 | `	ph7_value *pSnap = ph7_context_new_array(pCtx);` |
|     95 |  7725 | `	if( pSnap == 0 ){` |
|    ! 0 |  7726 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  7727 | `	}` |
|     95 |  7728 | `	if( pObj && pObj->type == XPATH_NODESET && pObj->nodesetval ){` |
|      - |  7729 | `		int i;` |
|    229 |  7730 | `		for( i = 0 ; i < pObj->nodesetval->nodeNr ; i++ ){` |
|    139 |  7731 | `			xmlNodePtr pNode = pObj->nodesetval->nodeTab[i];` |
|      - |  7732 | `			phl_domnode *pWrap;` |
|      - |  7733 | `			ph7_value *pRes;` |
|    139 |  7734 | `			if( pNode == 0 ){` |
|    ! 0 |  7735 | `				continue;` |
|      - |  7736 | `			}` |
|    139 |  7737 | `			if( pNode->type == XML_NAMESPACE_DECL ){` |
|      - |  7738 | `				/*` |
|      - |  7739 | `				 * A namespace:: axis result. libxml hands the set a COPY that` |
|      - |  7740 | `` 				 * dies with the XPath object (xmlXPathNodeSetDupNs, its `next` `` |
|      - |  7741 | `				 * pointing at the element the axis ran ON), so the snapshot` |
|      - |  7742 | `				 * wraps the ORIGINAL in-scope declaration found back through` |
|      - |  7743 | `				 * that element -- as php answers it: a DOMNameSpaceNode whose` |
|      - |  7744 | `				 * parentNode is the axis element even for a declaration an` |
|      - |  7745 | `				 * ANCESTOR made, fresh per query, stored as the OBJECT itself` |
|      - |  7746 | `				 * (item() twice on one list is one object, php's answer too).` |
|      - |  7747 | `				 */` |
|     43 |  7748 | `				xmlNsPtr pNs = (xmlNsPtr)pNode;` |
|     43 |  7749 | `				xmlNodePtr pElem = (xmlNodePtr)pNs->next;` |
|      - |  7750 | `				xmlNsPtr pOrig;` |
|      - |  7751 | `				ph7_class_instance *pNsObj;` |
|     43 |  7752 | `				if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  7753 | `					continue; /* not derivable: no element behind the copy */` |
|      - |  7754 | `				}` |
|     43 |  7755 | `				pOrig = xmlSearchNs((xmlDocPtr)pDocNd->pNode,pElem,pNs->prefix);` |
|     43 |  7756 | `				if( pOrig == 0 ){` |
|    ! 0 |  7757 | `					continue;` |
|      - |  7758 | `				}` |
|     43 |  7759 | `				pNsObj = DomNewNsNode(pVm,pDoc,pDocNd->pShell,pOrig,pElem);` |
|     43 |  7760 | `				pRes = ph7_context_new_scalar(pCtx);` |
|     43 |  7761 | `				if( pNsObj == 0 \|\| pRes == 0 ){` |
|    ! 0 |  7762 | `					if( pNsObj ){` |
|    ! 0 |  7763 | `						PH7_ClassInstanceUnref(pNsObj);` |
|    ! 0 |  7764 | `					}` |
|    ! 0 |  7765 | `					break;` |
|      - |  7766 | `				}` |
|      - |  7767 | `				/* pRes CARRIES the constructor's reference (no bump here): the` |
|      - |  7768 | `				 * array's insert takes its own, and the call context's release` |
|      - |  7769 | `				 * of pRes at method end consumes ours -- ending at exactly the` |
|      - |  7770 | `				 * array's one. */` |
|     43 |  7771 | `				pRes->x.pOther = pNsObj;` |
|     43 |  7772 | `				pRes->iFlags = MEMOBJ_OBJ;` |
|     43 |  7773 | `				ph7_array_add_elem(pSnap,0,pRes);` |
|     43 |  7774 | `				continue;` |
|      - |  7775 | `			}` |
|     97 |  7776 | `			pWrap = DomNewRes(pVm,pDocNd->pShell,pNode);` |
|     97 |  7777 | `			pRes = ph7_context_new_scalar(pCtx);` |
|     97 |  7778 | `			if( pWrap == 0 \|\| pRes == 0 ){` |
|    ! 0 |  7779 | `				break;` |
|      - |  7780 | `			}` |
|     97 |  7781 | `			ph7_value_resource(pRes,pWrap);` |
|     97 |  7782 | `			ph7_array_add_elem(pSnap,0,pRes);` |
|     49 |  7783 | `		}` |
|     45 |  7784 | `	}` |
|     95 |  7785 | `	pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_SNAP,0,0,0,pSnap);` |
|     95 |  7786 | `	if( pList == 0 ){` |
|    ! 0 |  7787 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  7788 | `	}` |
|     95 |  7789 | `	PH7_NativeResultObject(pCtx,pList);` |
|     95 |  7790 | `	return PH7_OK;` |
|     48 |  7791 | `}` |
|      - |  7792 | `/*` |
|      - |  7793 | ` * The one evaluation body under DOMXPath::query and DOMXPath::evaluate. The` |
|      - |  7794 | ` * two differ only in what they make of the RESULT: query wants a node list` |
|      - |  7795 | ` * (a scalar gets the empty one), evaluate answers the XPath TYPE as php's` |
|      - |  7796 | ` * value -- boolean as bool, number as float, string as string, nodeset as` |
|      - |  7797 | ` * the same snapshot list. An expression that does not evaluate (bad grammar,` |
|      - |  7798 | ` * unknown function, unresolved prefix) answers false from both, with the` |
|      - |  7799 | ` * libxml diagnostics on the shared queue.` |
|      - |  7800 | ` */` |
|    232 |  7801 | `static int DomXPathEvalRun(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - |  7802 | `	const char *zMethod,int bTyped)` |
|      1 |  7803 | `{` |
|    233 |  7804 | `	ph7_vm *pVm = pCtx->pVm;` |
|    233 |  7805 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    233 |  7806 | `	ph7_class_instance *pDoc = pThis ? PH7_NativeAttrObj(pThis,XP_DOC) : 0;` |
|    233 |  7807 | `	phl_domnode *pDocNd = DomResOf(pDoc);` |
|    233 |  7808 | `	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    233 |  7809 | `	phl_domnode *pCtxNd = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;` |
|      - |  7810 | ``	/* php's stub says `= true`, but the live default of the third argument is`` |
|      - |  7811 | `	 * the registerNodeNamespaces PROPERTY (the constructor's second argument` |
|      - |  7812 | `	 * lands there, and a later property write moves the default with it). */` |
|    233 |  7813 | `	int bRegNodeNs = nArg > 2 ? ph7_value_to_bool(apArg[2])` |
|    227 |  7814 | `		: (pThis ? PH7_NativeAttrTruthy(pThis,XP_NSDEF) : 1);` |
|      - |  7815 | `	xmlXPathContextPtr pXCtx;` |
|      - |  7816 | `	xmlXPathObjectPtr pObj;` |
|      - |  7817 | `	xmlNsPtr *aNodeNs;` |
|      - |  7818 | `	DomXPathFnCtx sFn;` |
|      - |  7819 | `	sxu32 nMark;` |
|      - |  7820 | `	sxi32 rc;` |
|    233 |  7821 | `	if( pDocNd == 0 ){` |
|    ! 0 |  7822 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  7823 | `		return PH7_OK;` |
|      - |  7824 | `	}` |
|    232 |  7825 | `	if( pCtxNd && pCtxNd->pNode` |
|     29 |  7826 | `	 && ((xmlNodePtr)pCtxNd->pNode)->doc != (xmlDocPtr)pDocNd->pNode ){` |
|      - |  7827 | `		/* php's plain Error, no DOM code -- a context node of another document` |
|      - |  7828 | `		 * (or of none, a constructed node) cannot anchor this evaluation. */` |
|      5 |  7829 | `		return PH7_VmThrowException(pCtx,"Error","Node from wrong document");` |
|      - |  7830 | `	}` |
|    229 |  7831 | `	aNodeNs = DomXPathCtxOpen(pCtx,pDocNd,pCtxNd,bRegNodeNs,&pXCtx);` |
|    229 |  7832 | `	if( pXCtx == 0 ){` |
|    ! 0 |  7833 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  7834 | `		return PH7_OK;` |
|      - |  7835 | `	}` |
|      - |  7836 | `	/* The PHP-function bridge rides this one evaluation: the record lives on` |
|      - |  7837 | `	 * THIS stack frame, and libxml carries a pointer to it as its lookup data. */` |
|    229 |  7838 | `	sFn.pCtx = pCtx;` |
|    229 |  7839 | `	sFn.pThis = pThis;` |
|    229 |  7840 | `	sFn.pDoc = pDoc;` |
|    229 |  7841 | `	sFn.pDocNd = pDocNd;` |
|    229 |  7842 | `	sFn.iErr = XP_FN_OK;` |
|    229 |  7843 | `	sFn.rcUnwound = 0;` |
|    229 |  7844 | `	SyBlobInit(&sFn.sErrName,&pVm->sAllocator);` |
|    229 |  7845 | `	xmlXPathRegisterFuncLookup(pXCtx,DomXPathFnLookup,&sFn);` |
|    229 |  7846 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    229 |  7847 | `	pObj = xmlXPathEvalExpression((const xmlChar *)zExpr,pXCtx);` |
|    229 |  7848 | `	PH7_LibxmlCaptureEnd(pVm,nMark,zMethod);` |
|    229 |  7849 | `	if( aNodeNs ){` |
|     45 |  7850 | `		pXCtx->namespaces = 0;` |
|     45 |  7851 | `		pXCtx->nsNr = 0;` |
|     45 |  7852 | `		xmlFree(aNodeNs);` |
|     22 |  7853 | `	}` |
|    229 |  7854 | `	if( sFn.rcUnwound != 0 \|\| sFn.iErr != XP_FN_OK ){` |
|      - |  7855 | `		/* A callback did not return, or the bridge parked a refusal it could` |
|      - |  7856 | `		 * not raise from inside libxml's recursion. Either way the evaluation` |
|      - |  7857 | `		 * is over and this is its answer -- raised HERE, where the enclosing` |
|      - |  7858 | `		 * catch runs with libxml already unwound. */` |
|     25 |  7859 | `		sxi32 rcFn = sFn.rcUnwound;` |
|     25 |  7860 | `		if( pObj ){` |
|    ! 0 |  7861 | `			xmlXPathFreeObject(pObj);` |
|    ! 0 |  7862 | `		}` |
|     25 |  7863 | `		xmlXPathFreeContext(pXCtx);` |
|     25 |  7864 | `		if( rcFn == 0 ){` |
|     23 |  7865 | `			rcFn = DomXPathFnRaise(pCtx,&sFn);` |
|     12 |  7866 | `		}else{` |
|      3 |  7867 | `			pCtx->nThrowRc = rcFn;` |
|      - |  7868 | `		}` |
|     25 |  7869 | `		SyBlobRelease(&sFn.sErrName);` |
|     25 |  7870 | `		return rcFn;` |
|      - |  7871 | `	}` |
|    205 |  7872 | `	SyBlobRelease(&sFn.sErrName);` |
|    205 |  7873 | `	if( pObj == 0 ){` |
|     27 |  7874 | `		xmlXPathFreeContext(pXCtx);` |
|     27 |  7875 | `		ph7_result_bool(pCtx,0);` |
|     27 |  7876 | `		return PH7_OK;` |
|      - |  7877 | `	}` |
|    179 |  7878 | `	if( !bTyped ){` |
|     85 |  7879 | `		rc = DomXPathResultList(pCtx,pDoc,pDocNd,pObj);` |
|     43 |  7880 | `	}else{` |
|     95 |  7881 | `		switch( pObj->type ){` |
|      2 |  7882 | `		case XPATH_BOOLEAN:` |
|      5 |  7883 | `			ph7_result_bool(pCtx,pObj->boolval);` |
|      5 |  7884 | `			rc = PH7_OK;` |
|      5 |  7885 | `			break;` |
|     15 |  7886 | `		case XPATH_NUMBER:` |
|     31 |  7887 | `			ph7_result_double(pCtx,pObj->floatval);` |
|     31 |  7888 | `			rc = PH7_OK;` |
|     31 |  7889 | `			break;` |
|     25 |  7890 | `		case XPATH_STRING:` |
|     51 |  7891 | `			ph7_result_string(pCtx,pObj->stringval ? (const char *)pObj->stringval : "",-1);` |
|     51 |  7892 | `			rc = PH7_OK;` |
|     51 |  7893 | `			break;` |
|      5 |  7894 | `		case XPATH_NODESET:` |
|     11 |  7895 | `			rc = DomXPathResultList(pCtx,pDoc,pDocNd,pObj);` |
|     11 |  7896 | `			break;` |
|    ! 0 |  7897 | `		default:` |
|    ! 0 |  7898 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  7899 | `			rc = PH7_OK;` |
|    ! 0 |  7900 | `			break;` |
|      - |  7901 | `		}` |
|      - |  7902 | `	}` |
|    179 |  7903 | `	xmlXPathFreeObject(pObj);` |
|    179 |  7904 | `	xmlXPathFreeContext(pXCtx);` |
|    179 |  7905 | `	return rc;` |
|    117 |  7906 | `}` |
|      - |  7907 | `/*` |
|      - |  7908 | ` * DOMXPath::query(string $expression, ?DOMNode $contextNode = null,` |
|      - |  7909 | ` *                 bool $registerNodeNS = true): DOMNodeList\|false` |
|      - |  7910 | ` */` |
|    100 |  7911 | `DOM_METHOD(vm_builtin_DOMXPath_query)` |
|      1 |  7912 | `{` |
|    101 |  7913 | `	return DomXPathEvalRun(pCtx,nArg,apArg,"DOMXPath::query",0);` |
|      1 |  7914 | `}` |
|      - |  7915 | `/*` |
|      - |  7916 | ` * DOMXPath::evaluate(string $expression, ?DOMNode $contextNode = null,` |
|      - |  7917 | ` *                    bool $registerNodeNS = true): mixed` |
|      - |  7918 | ` */` |
|    132 |  7919 | `DOM_METHOD(vm_builtin_DOMXPath_evaluate)` |
|      1 |  7920 | `{` |
|    133 |  7921 | `	return DomXPathEvalRun(pCtx,nArg,apArg,"DOMXPath::evaluate",1);` |
|      1 |  7922 | `}` |
|      - |  7923 | `/* DOMXPath::__construct(DOMDocument $document, bool $registerNodeNS = true) */` |
|     74 |  7924 | `DOM_METHOD(vm_builtin_DOMXPath_construct)` |
|      1 |  7925 | `{` |
|     75 |  7926 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     75 |  7927 | `	if( pThis && nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|    112 |  7928 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,XP_DOC,` |
|     74 |  7929 | `			(ph7_class_instance *)apArg[0]->x.pOther);` |
|     37 |  7930 | `	}` |
|     75 |  7931 | `	if( pThis && nArg > 1 ){` |
|      7 |  7932 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,XP_NSDEF,` |
|      4 |  7933 | `			ph7_value_to_bool(apArg[1]));` |
|      2 |  7934 | `	}` |
|     75 |  7935 | `	return PH7_OK;` |
|      1 |  7936 | `}` |
|      - |  7937 | `/*` |
|      - |  7938 | ` * DOMXPath::registerNamespace(string $prefix, string $namespace): bool` |
|      - |  7939 | ` *` |
|      - |  7940 | ` * php hands the pair to xmlXPathRegisterNs on its persistent context and` |
|      - |  7941 | ` * answers its status: only the EMPTY prefix refuses (an invalid NCName one is` |
|      - |  7942 | ` * taken, and an empty URI is a registration too -- the prefix then resolves,` |
|      - |  7943 | ` * to a namespace nothing is in). Here the pair goes into the per-object table` |
|      - |  7944 | ` * the next evaluation replays.` |
|      - |  7945 | ` */` |
|     40 |  7946 | `DOM_METHOD(vm_builtin_DOMXPath_registerNamespace)` |
|      1 |  7947 | `{` |
|     41 |  7948 | `	ph7_vm *pVm = pCtx->pVm;` |
|     41 |  7949 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     41 |  7950 | `	int nPfx = 0;` |
|     41 |  7951 | `	const char *zPfx = nArg > 0 ? ph7_value_to_string(apArg[0],&nPfx) : "";` |
|      - |  7952 | `	ph7_hashmap *pMap;` |
|      - |  7953 | `	ph7_value sKey,sVal;` |
|     41 |  7954 | `	if( nPfx < 1 \|\| nArg < 2 ){` |
|      3 |  7955 | `		ph7_result_bool(pCtx,0);` |
|      3 |  7956 | `		return PH7_OK;` |
|      - |  7957 | `	}` |
|     39 |  7958 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_NSREG);` |
|     39 |  7959 | `	if( pMap == 0 ){` |
|    ! 0 |  7960 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  7961 | `		return PH7_OK;` |
|      - |  7962 | `	}` |
|     39 |  7963 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     39 |  7964 | `	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);` |
|     39 |  7965 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|      - |  7966 | `	{` |
|     39 |  7967 | `		int nUri = 0;` |
|     39 |  7968 | `		const char *zUri = ph7_value_to_string(apArg[1],&nUri);` |
|     39 |  7969 | `		PH7_MemObjStringAppend(&sVal,zUri,(sxu32)nUri);` |
|      - |  7970 | `	}` |
|     39 |  7971 | `	PH7_HashmapInsert(pMap,&sKey,&sVal);` |
|     39 |  7972 | `	PH7_MemObjRelease(&sKey);` |
|     39 |  7973 | `	PH7_MemObjRelease(&sVal);` |
|     39 |  7974 | `	ph7_result_bool(pCtx,1);` |
|     39 |  7975 | `	return PH7_OK;` |
|     21 |  7976 | `}` |
|      - |  7977 |  |
|      - |  7978 | `/* One row of the $restrict ARRAY: the value must be callable, and the NAME an` |
|      - |  7979 | ` * expression calls it by is the string key when there is one -- php's alias --` |
|      - |  7980 | ` * and otherwise the value coerced to a string (an array callable therefore` |
|      - |  7981 | ` * registers under "Array", with php's own conversion notice). */` |
|      - |  7982 | `struct DomXPathRestrict {` |
|      - |  7983 | `	ph7_context *pCtx;` |
|      - |  7984 | `	ph7_hashmap *pMap;` |
|      - |  7985 | `	sxi32 rc;` |
|      - |  7986 | `};` |
|     16 |  7987 | `static int DomXPathRestrictRow(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 |  7988 | `{` |
|     17 |  7989 | `	struct DomXPathRestrict *pWalk = (struct DomXPathRestrict *)pUserData;` |
|     17 |  7990 | `	ph7_vm *pVm = pWalk->pCtx->pVm;` |
|      - |  7991 | `	char zBuf[128];` |
|      - |  7992 | `	const char *zWhy;` |
|     17 |  7993 | `	if( pWalk->rc != PH7_OK ){` |
|    ! 0 |  7994 | `		return PH7_OK;` |
|      - |  7995 | `	}` |
|     17 |  7996 | `	zWhy = PH7_VmCallableReason(pVm,pVal,zBuf,(int)sizeof(zBuf));` |
|     17 |  7997 | `	if( zWhy ){` |
|      7 |  7998 | `		pWalk->rc = PH7_VmThrowException(pWalk->pCtx,"TypeError",` |
|      - |  7999 | `			"DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be an array "` |
|      2 |  8000 | `			"with valid callbacks as values, %s",zWhy);` |
|      5 |  8001 | `		return PH7_ABORT;` |
|      - |  8002 | `	}` |
|     15 |  8003 | `	if( pKey && ph7_value_is_string(pKey) ){` |
|      5 |  8004 | `		int nKey = 0;` |
|      5 |  8005 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|      5 |  8006 | `		DomXPathMapPut(pVm,pWalk->pMap,zKey,nKey,pVal);` |
|      3 |  8007 | `	}else{` |
|      - |  8008 | `		/* ph7_value_to_string COERCES in place, which would rewrite the map's` |
|      - |  8009 | `		 * own value; name off a copy. */` |
|      - |  8010 | `		ph7_value sName;` |
|      9 |  8011 | `		int nName = 0;` |
|      - |  8012 | `		const char *zName;` |
|      9 |  8013 | `		PH7_MemObjInit(pVm,&sName);` |
|      9 |  8014 | `		PH7_MemObjStore(pVal,&sName);` |
|      9 |  8015 | `		zName = ph7_value_to_string(&sName,&nName);` |
|      9 |  8016 | `		DomXPathMapPut(pVm,pWalk->pMap,zName,nName,pVal);` |
|      9 |  8017 | `		PH7_MemObjRelease(&sName);` |
|      - |  8018 | `	}` |
|     13 |  8019 | `	return PH7_OK;` |
|      9 |  8020 | `}` |
|      - |  8021 | `/*` |
|      - |  8022 | ` * DOMXPath::registerPhpFunctions(array\|string\|null $restrict = null): void` |
|      - |  8023 | ` *` |
|      - |  8024 | ` * Bare (or null) opens the door to ANY callable name; a string or an array` |
|      - |  8025 | ` * restricts it to the named ones, accumulating across calls -- a later bare` |
|      - |  8026 | ` * call re-opens without forgetting the table, and a later restriction closes` |
|      - |  8027 | ` * it again with everything registered so far still reachable. Each name is` |
|      - |  8028 | ` * screened for callability HERE, so an evaluation never has to.` |
|      - |  8029 | ` */` |
|     32 |  8030 | `DOM_METHOD(vm_builtin_DOMXPath_registerPhpFunctions)` |
|      1 |  8031 | `{` |
|     33 |  8032 | `	ph7_vm *pVm = pCtx->pVm;` |
|     33 |  8033 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8034 | `	ph7_hashmap *pMap;` |
|     33 |  8035 | `	if( pThis == 0 ){` |
|    ! 0 |  8036 | `		return PH7_OK;` |
|      - |  8037 | `	}` |
|     33 |  8038 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|     15 |  8039 | `		PH7_NativeSetAttrInt(pVm,pThis,XP_FNMODE,XP_MODE_ALL);` |
|     15 |  8040 | `		return PH7_OK;` |
|      - |  8041 | `	}` |
|     19 |  8042 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_FNREG);` |
|     19 |  8043 | `	if( pMap == 0 ){` |
|    ! 0 |  8044 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8045 | `	}` |
|      - |  8046 | `	/* The mode moves FIRST, and each row is taken as it is screened: php's` |
|      - |  8047 | `	 * refusal leaves the object restricted with everything registered up to` |
|      - |  8048 | `` 	 * the bad row -- `registerPhpFunctions(['strrev','nope','strtolower'])` `` |
|      - |  8049 | `	 * throws, and afterwards strrev runs while strtolower does not. */` |
|     19 |  8050 | `	PH7_NativeSetAttrInt(pVm,pThis,XP_FNMODE,XP_MODE_LIST);` |
|     19 |  8051 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  8052 | `		struct DomXPathRestrict sWalk;` |
|     13 |  8053 | `		sWalk.pCtx = pCtx;` |
|     13 |  8054 | `		sWalk.pMap = pMap;` |
|     13 |  8055 | `		sWalk.rc = PH7_OK;` |
|     13 |  8056 | `		ph7_array_walk(apArg[0],DomXPathRestrictRow,&sWalk);` |
|     13 |  8057 | `		if( sWalk.rc != PH7_OK ){` |
|      5 |  8058 | `			return sWalk.rc;` |
|      - |  8059 | `		}` |
|      5 |  8060 | `	}else{` |
|      - |  8061 | `		char zBuf[128];` |
|      7 |  8062 | `		const char *zWhy = PH7_VmCallableReason(pVm,apArg[0],zBuf,(int)sizeof(zBuf));` |
|      7 |  8063 | `		int nName = 0;` |
|      - |  8064 | `		const char *zName;` |
|      7 |  8065 | `		if( zWhy ){` |
|      4 |  8066 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  8067 | `				"DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be a callable, %s",` |
|      1 |  8068 | `				zWhy);` |
|      - |  8069 | `		}` |
|      5 |  8070 | `		zName = ph7_value_to_string(apArg[0],&nName);` |
|      5 |  8071 | `		DomXPathMapPut(pVm,pMap,zName,nName,apArg[0]);` |
|      - |  8072 | `	}` |
|     13 |  8073 | `	return PH7_OK;` |
|     17 |  8074 | `}` |
|      - |  8075 | `/* php's callback NAME grammar for registerPhpFunctionNS: an XML NCName, which` |
|      - |  8076 | ` * is what an expression can spell as a function name. */` |
|     18 |  8077 | `static int DomXPathIsCallbackName(const char *zName,int nName)` |
|      1 |  8078 | `{` |
|      - |  8079 | `	int i;` |
|     19 |  8080 | `	if( nName < 1 ){` |
|      3 |  8081 | `		return 0;` |
|      - |  8082 | `	}` |
|     17 |  8083 | `	if( xmlValidateNCName((const xmlChar *)zName,0) != 0 ){` |
|      5 |  8084 | `		return 0;` |
|      - |  8085 | `	}` |
|      - |  8086 | `	/* xmlValidateNCName reads to the NUL, and a name may not carry one. */` |
|     67 |  8087 | `	for( i = 0 ; i < nName ; ++i ){` |
|     55 |  8088 | `		if( zName[i] == 0 ){` |
|    ! 0 |  8089 | `			return 0;` |
|      - |  8090 | `		}` |
|     28 |  8091 | `	}` |
|     13 |  8092 | `	return (int)SyStrlen(zName) == nName;` |
|     10 |  8093 | `}` |
|      - |  8094 | `/*` |
|      - |  8095 | ` * DOMXPath::registerPhpFunctionNS(string $namespaceURI, string $name,` |
|      - |  8096 | ` *                                 callable $callable): void` |
|      - |  8097 | ` *` |
|      - |  8098 | ` * php 8.4's narrow door: one callable under one name in the caller's OWN` |
|      - |  8099 | `` * namespace -- no `php:function("name")` indirection, and independent of`` |
|      - |  8100 | ` * registerPhpFunctions' mode (it neither needs it nor opens it). php's own` |
|      - |  8101 | ` * URI is refused, the name must be an NCName, and the callable is screened` |
|      - |  8102 | ` * here.` |
|      - |  8103 | ` */` |
|     20 |  8104 | `DOM_METHOD(vm_builtin_DOMXPath_registerPhpFunctionNS)` |
|      1 |  8105 | `{` |
|     21 |  8106 | `	ph7_vm *pVm = pCtx->pVm;` |
|     21 |  8107 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     21 |  8108 | `	int nUri = 0,nName = 0;` |
|     21 |  8109 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],&nUri) : "";` |
|     21 |  8110 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],&nName) : "";` |
|      - |  8111 | `	char zBuf[128];` |
|      - |  8112 | `	const char *zWhy;` |
|      - |  8113 | `	ph7_hashmap *pMap;` |
|      - |  8114 | `	SyBlob sKey;` |
|     21 |  8115 | `	if( pThis == 0 \|\| nArg < 3 ){` |
|    ! 0 |  8116 | `		return PH7_OK;` |
|      - |  8117 | `	}` |
|     21 |  8118 | `	if( nUri == (int)sizeof(XP_PHPNS)-1 && SyMemcmp(zUri,XP_PHPNS,(sxu32)nUri) == 0 ){` |
|      3 |  8119 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  8120 | `			"DOMXPath::registerPhpFunctionNS(): Argument #1 ($namespaceURI) must not be "` |
|      - |  8121 | `			"\"%s\" because it is reserved by PHP",XP_PHPNS);` |
|      - |  8122 | `	}` |
|     19 |  8123 | `	if( !DomXPathIsCallbackName(zName,nName) ){` |
|      7 |  8124 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  8125 | `			"DOMXPath::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name");` |
|      - |  8126 | `	}` |
|     13 |  8127 | `	zWhy = PH7_VmCallableReason(pVm,apArg[2],zBuf,(int)sizeof(zBuf));` |
|     13 |  8128 | `	if( zWhy ){` |
|      4 |  8129 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  8130 | `			"DOMXPath::registerPhpFunctionNS(): Argument #3 ($callable) must be a valid callback, %s",` |
|      1 |  8131 | `			zWhy);` |
|      - |  8132 | `	}` |
|     11 |  8133 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_NSFN);` |
|     11 |  8134 | `	if( pMap == 0 ){` |
|    ! 0 |  8135 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8136 | `	}` |
|      - |  8137 | `	/* One key from the pair: a URI cannot carry \x01, so the join is` |
|      - |  8138 | `	 * unambiguous without escaping. */` |
|     11 |  8139 | `	SyBlobInit(&sKey,&pVm->sAllocator);` |
|     11 |  8140 | `	SyBlobAppend(&sKey,zUri,(sxu32)nUri);` |
|     11 |  8141 | `	SyBlobAppend(&sKey,"\1",1);` |
|     11 |  8142 | `	SyBlobAppend(&sKey,zName,(sxu32)nName);` |
|     11 |  8143 | `	DomXPathMapPut(pVm,pMap,(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey),apArg[2]);` |
|     11 |  8144 | `	SyBlobRelease(&sKey);` |
|     11 |  8145 | `	return PH7_OK;` |
|     11 |  8146 | `}` |
|      - |  8147 |  |
|      - |  8148 | `/* ===== Schema validation ===== */` |
|      - |  8149 |  |
|      - |  8150 | `/*` |
|      - |  8151 | ` * The four schema doors -- {XML Schema, RelaxNG} x {a FILE, a STRING} -- and` |
|      - |  8152 | `` * php's `validate()` beside them, all one shape:`` |
|      - |  8153 | ` *` |
|      - |  8154 | ` *   parse the schema (loudly: every libxml complaint reaches the caller's` |
|      - |  8155 | ` *   error handler), and if that fails say "Invalid Schema" / "Invalid RelaxNG"` |
|      - |  8156 | ` *   and answer false; otherwise validate the document and answer whether it` |
|      - |  8157 | ` *   came back clean.` |
|      - |  8158 | ` *` |
|      - |  8159 | ` * Only the pair of libxml families differs, so the switch is four calls wide` |
|      - |  8160 | ` * and the plumbing -- the argument screens, the diagnostic capture, the` |
|      - |  8161 | ` * refusals -- is written once.  The names a caller sees are php's: a filename` |
|      - |  8162 | ` * that is empty or carries a NUL is a ValueError naming the argument, raised` |
|      - |  8163 | ` * before anything is opened.` |
|      - |  8164 | ` */` |
|      - |  8165 | `#define DOM_VAL_SCHEMA 0` |
|      - |  8166 | `#define DOM_VAL_RELAX  1` |
|      - |  8167 |  |
|      - |  8168 | `/*` |
|      - |  8169 | ` * Schema, RelaxNG and DTD-validity diagnostics: onto the shared per-VM queue` |
|      - |  8170 | ` * through PH7_LibxmlQueueError, exactly like the global structured handler.` |
|      - |  8171 | ` *` |
|      - |  8172 | ` * php installs libxml's printf-style pair here instead, which is why its` |
|      - |  8173 | ` * validation diagnostics read as libxml writes them -- "I/O warning : failed` |
|      - |  8174 | ` * to load external entity ...", a parse error over three lines with the` |
|      - |  8175 | ` * offending source and a caret under it -- while every message this engine` |
|      - |  8176 | ` * drains is one structured record with its location appended.  The structured` |
|      - |  8177 | ` * handler is the one this file must keep: it is also what feeds` |
|      - |  8178 | `` * `libxml_get_errors()`, and php's own switches to exactly this shape once`` |
|      - |  8179 | `` * `libxml_use_internal_errors(true)` is on.  The ANSWERS agree; the wording of`` |
|      - |  8180 | ` * a failure does not (the error-format class).` |
|      - |  8181 | ` */` |
|      - |  8182 | `#if LIBXML_VERSION >= 21200` |
|     12 |  8183 | `static void DomSchemaErr(void *pUserData,const xmlError *pErr)` |
|      - |  8184 | `#else` |
|      8 |  8185 | `static void DomSchemaErr(void *pUserData,xmlErrorPtr pErr)` |
|      - |  8186 | `#endif` |
|      1 |  8187 | `{` |
|     21 |  8188 | `	if( pErr == 0 ){` |
|    ! 0 |  8189 | `		return;` |
|      - |  8190 | `	}` |
|     33 |  8191 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|     20 |  8192 | `		pErr->int2,pErr->message,pErr->file);` |
|     13 |  8193 | `}` |
|      - |  8194 | `/* php's own last word when a schema will not parse, under the method's name. */` |
|      8 |  8195 | `static void DomValidateSaySo(ph7_vm *pVm,const char *zFn,const char *zWhat)` |
|      1 |  8196 | `{` |
|      - |  8197 | `	SyBlob sMsg;` |
|      9 |  8198 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      9 |  8199 | `	SyBlobFormat(&sMsg,"%s(): %s",zFn,zWhat);` |
|      9 |  8200 | `	SyBlobNullAppend(&sMsg);` |
|      9 |  8201 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      9 |  8202 | `	SyBlobRelease(&sMsg);` |
|      9 |  8203 | `}` |
|      - |  8204 | `/*` |
|      - |  8205 | ` * The argument every schema door takes: a filename or the schema itself. The` |
|      - |  8206 | ` * two refusals are php's own and answer before any parse.` |
|      - |  8207 | ` */` |
|     36 |  8208 | `static int DomValidateArg(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,` |
|      - |  8209 | `	const char *zFn,const char **pzSrc,int *pnSrc,int *pRc)` |
|      1 |  8210 | `{` |
|     37 |  8211 | `	int nSrc = 0;` |
|     37 |  8212 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|     37 |  8213 | `	const char *zParam = bFile ? "filename" : "source";` |
|     37 |  8214 | `	if( bFile && nSrc != (int)SyStrlen(zSrc) ){` |
|      4 |  8215 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      1 |  8216 | `			"%s(): Argument #1 ($%s) must not contain any null bytes",zFn,zParam);` |
|      3 |  8217 | `		return 0;` |
|      - |  8218 | `	}` |
|     35 |  8219 | `	if( nSrc < 1 ){` |
|     13 |  8220 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      4 |  8221 | `			"%s(): Argument #1 ($%s) must not be empty",zFn,zParam);` |
|      9 |  8222 | `		return 0;` |
|      - |  8223 | `	}` |
|     27 |  8224 | `	*pzSrc = zSrc;` |
|     27 |  8225 | `	*pnSrc = nSrc;` |
|     27 |  8226 | `	return 1;` |
|     19 |  8227 | `}` |
|     36 |  8228 | `static int DomValidateRun(ph7_context *pCtx,int nArg,ph7_value **apArg,int iKind,` |
|      - |  8229 | `	int bFile,const char *zFn)` |
|      1 |  8230 | `{` |
|     37 |  8231 | `	ph7_vm *pVm = pCtx->pVm;` |
|     37 |  8232 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     37 |  8233 | `	int nSrc = 0,rc = PH7_OK,iRc;` |
|     37 |  8234 | `	const char *zSrc = "";` |
|      - |  8235 | `	sxu32 nMark;` |
|      - |  8236 | `	/* php reads the option word from the SCHEMA pair only; RelaxNG's two` |
|      - |  8237 | `	 * declare no second parameter at all. LIBXML_SCHEMA_CREATE is the one bit` |
|      - |  8238 | `	 * it acts on -- "write the schema's default values into the document". */` |
|     31 |  8239 | `	int bCreate = iKind == DOM_VAL_SCHEMA && nArg > 1` |
|     47 |  8240 | `		&& (ph7_value_to_int(apArg[1]) & XML_SCHEMA_VAL_VC_I_CREATE) != 0;` |
|     37 |  8241 | `	if( !DomValidateArg(pCtx,nArg,apArg,bFile,zFn,&zSrc,&nSrc,&rc) ){` |
|     11 |  8242 | `		return rc;` |
|      - |  8243 | `	}` |
|     27 |  8244 | `	if( pDocNd == 0 ){` |
|    ! 0 |  8245 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  8246 | `		return PH7_OK;` |
|      - |  8247 | `	}` |
|     27 |  8248 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     27 |  8249 | `	if( iKind == DOM_VAL_SCHEMA ){` |
|     14 |  8250 | `		xmlSchemaParserCtxtPtr pParser = bFile ? xmlSchemaNewParserCtxt(zSrc)` |
|     11 |  8251 | `		                                       : xmlSchemaNewMemParserCtxt(zSrc,nSrc);` |
|      - |  8252 | `		xmlSchemaPtr pSchema;` |
|      - |  8253 | `		xmlSchemaValidCtxtPtr pValid;` |
|     17 |  8254 | `		if( pParser == 0 ){` |
|    ! 0 |  8255 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    ! 0 |  8256 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  8257 | `			return PH7_OK;` |
|      - |  8258 | `		}` |
|     17 |  8259 | `		xmlSchemaSetParserStructuredErrors(pParser,DomSchemaErr,pVm);` |
|     17 |  8260 | `		pSchema = xmlSchemaParse(pParser);` |
|     17 |  8261 | `		xmlSchemaFreeParserCtxt(pParser);` |
|     17 |  8262 | `		if( pSchema == 0 ){` |
|      5 |  8263 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|      5 |  8264 | `			DomValidateSaySo(pVm,zFn,"Invalid Schema");` |
|      5 |  8265 | `			ph7_result_bool(pCtx,0);` |
|      5 |  8266 | `			return PH7_OK;` |
|      - |  8267 | `		}` |
|     13 |  8268 | `		pValid = xmlSchemaNewValidCtxt(pSchema);` |
|     13 |  8269 | `		if( pValid == 0 ){` |
|    ! 0 |  8270 | `			xmlSchemaFree(pSchema);` |
|    ! 0 |  8271 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    ! 0 |  8272 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  8273 | `			return PH7_OK;` |
|      - |  8274 | `		}` |
|     13 |  8275 | `		if( bCreate ){` |
|      3 |  8276 | `			xmlSchemaSetValidOptions(pValid,XML_SCHEMA_VAL_VC_I_CREATE);` |
|      1 |  8277 | `		}` |
|     13 |  8278 | `		xmlSchemaSetValidStructuredErrors(pValid,DomSchemaErr,pVm);` |
|     13 |  8279 | `		iRc = xmlSchemaValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);` |
|     13 |  8280 | `		xmlSchemaFreeValidCtxt(pValid);` |
|     13 |  8281 | `		xmlSchemaFree(pSchema);` |
|      7 |  8282 | `	}else{` |
|      9 |  8283 | `		xmlRelaxNGParserCtxtPtr pParser = bFile ? xmlRelaxNGNewParserCtxt(zSrc)` |
|      7 |  8284 | `		                                        : xmlRelaxNGNewMemParserCtxt(zSrc,nSrc);` |
|      - |  8285 | `		xmlRelaxNGPtr pSchema;` |
|      - |  8286 | `		xmlRelaxNGValidCtxtPtr pValid;` |
|     11 |  8287 | `		if( pParser == 0 ){` |
|    ! 0 |  8288 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    ! 0 |  8289 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  8290 | `			return PH7_OK;` |
|      - |  8291 | `		}` |
|     11 |  8292 | `		xmlRelaxNGSetParserStructuredErrors(pParser,DomSchemaErr,pVm);` |
|     11 |  8293 | `		pSchema = xmlRelaxNGParse(pParser);` |
|     11 |  8294 | `		xmlRelaxNGFreeParserCtxt(pParser);` |
|     11 |  8295 | `		if( pSchema == 0 ){` |
|      5 |  8296 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|      5 |  8297 | `			DomValidateSaySo(pVm,zFn,"Invalid RelaxNG");` |
|      5 |  8298 | `			ph7_result_bool(pCtx,0);` |
|      5 |  8299 | `			return PH7_OK;` |
|      - |  8300 | `		}` |
|      7 |  8301 | `		pValid = xmlRelaxNGNewValidCtxt(pSchema);` |
|      7 |  8302 | `		if( pValid == 0 ){` |
|    ! 0 |  8303 | `			xmlRelaxNGFree(pSchema);` |
|    ! 0 |  8304 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    ! 0 |  8305 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  8306 | `			return PH7_OK;` |
|      - |  8307 | `		}` |
|      7 |  8308 | `		xmlRelaxNGSetValidStructuredErrors(pValid,DomSchemaErr,pVm);` |
|      7 |  8309 | `		iRc = xmlRelaxNGValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);` |
|      7 |  8310 | `		xmlRelaxNGFreeValidCtxt(pValid);` |
|      7 |  8311 | `		xmlRelaxNGFree(pSchema);` |
|      - |  8312 | `	}` |
|     19 |  8313 | `	PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|     19 |  8314 | `	ph7_result_bool(pCtx,iRc == 0);` |
|     19 |  8315 | `	return PH7_OK;` |
|     19 |  8316 | `}` |
|      - |  8317 | `/* DOMDocument::schemaValidate(string $filename, int $flags = 0): bool */` |
|     14 |  8318 | `DOM_METHOD(vm_builtin_DOMDocument_schemaValidate)` |
|      1 |  8319 | `{` |
|     15 |  8320 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_SCHEMA,TRUE,"DOMDocument::schemaValidate");` |
|      1 |  8321 | `}` |
|      - |  8322 | `/* DOMDocument::schemaValidateSource(string $source, int $flags = 0): bool */` |
|      8 |  8323 | `DOM_METHOD(vm_builtin_DOMDocument_schemaValidateSource)` |
|      1 |  8324 | `{` |
|      9 |  8325 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_SCHEMA,FALSE,"DOMDocument::schemaValidateSource");` |
|      1 |  8326 | `}` |
|      - |  8327 | `/* DOMDocument::relaxNGValidate(string $filename): bool */` |
|      8 |  8328 | `DOM_METHOD(vm_builtin_DOMDocument_relaxNGValidate)` |
|      1 |  8329 | `{` |
|      9 |  8330 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_RELAX,TRUE,"DOMDocument::relaxNGValidate");` |
|      1 |  8331 | `}` |
|      - |  8332 | `/* DOMDocument::relaxNGValidateSource(string $source): bool */` |
|      6 |  8333 | `DOM_METHOD(vm_builtin_DOMDocument_relaxNGValidateSource)` |
|      1 |  8334 | `{` |
|      7 |  8335 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_RELAX,FALSE,"DOMDocument::relaxNGValidateSource");` |
|      1 |  8336 | `}` |
|      - |  8337 | `/*` |
|      - |  8338 | ` * DOMDocument::validate(): bool -- against the document's OWN DTD, which is` |
|      - |  8339 | ` * the one question of the five that takes no argument. libxml's validity` |
|      - |  8340 | ` * complaints ("no DTD found!", "root and DTD name do not match") reach the` |
|      - |  8341 | ` * caller through the same per-VM queue every other diagnostic here does.` |
|      - |  8342 | ` */` |
|      8 |  8343 | `DOM_METHOD(vm_builtin_DOMDocument_validate)` |
|      1 |  8344 | `{` |
|      9 |  8345 | `	ph7_vm *pVm = pCtx->pVm;` |
|      9 |  8346 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|      - |  8347 | `	xmlValidCtxtPtr pValid;` |
|      - |  8348 | `	sxu32 nMark;` |
|      - |  8349 | `	int iRc;` |
|      4 |  8350 | `	SXUNUSED(nArg);` |
|      4 |  8351 | `	SXUNUSED(apArg);` |
|      9 |  8352 | `	if( pDocNd == 0 ){` |
|    ! 0 |  8353 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  8354 | `		return PH7_OK;` |
|      - |  8355 | `	}` |
|      9 |  8356 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      9 |  8357 | `	pValid = xmlNewValidCtxt();` |
|      9 |  8358 | `	if( pValid == 0 ){` |
|    ! 0 |  8359 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::validate");` |
|    ! 0 |  8360 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  8361 | `		return PH7_OK;` |
|      - |  8362 | `	}` |
|      9 |  8363 | `	iRc = xmlValidateDocument(pValid,(xmlDocPtr)pDocNd->pNode);` |
|      9 |  8364 | `	xmlFreeValidCtxt(pValid);` |
|      9 |  8365 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::validate");` |
|      9 |  8366 | `	ph7_result_bool(pCtx,iRc != 0);` |
|      9 |  8367 | `	return PH7_OK;` |
|      5 |  8368 | `}` |
|      - |  8369 | `/*` |
|      - |  8370 | ` * The MARKERS libxml leaves around everything it substituted.` |
|      - |  8371 | ` *` |
|      - |  8372 | ` * An XInclude pass wraps each replacement in an XML_XINCLUDE_START /` |
|      - |  8373 | ` * XML_XINCLUDE_END pair, which are nodes in the tree like any other: they` |
|      - |  8374 | `` * answer from `childNodes`, they shift every index after them, and the first`` |
|      - |  8375 | `` * child of an element whose only content was an `<xi:include>` is one of them`` |
|      - |  8376 | ` * rather than what was included.  php takes them out before answering, so the` |
|      - |  8377 | ` * document a caller gets back is the substituted one and nothing else.  They` |
|      - |  8378 | ` * are parked on the orphan set rather than freed, like every other node this` |
|      - |  8379 | ` * file unlinks.` |
|      - |  8380 | ` */` |
|     14 |  8381 | `static void DomDropXIncludeMarks(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |  8382 | `{` |
|      - |  8383 | `	xmlNodePtr pNext;` |
|     29 |  8384 | `	while( pNode ){` |
|     15 |  8385 | `		pNext = pNode->next;` |
|     15 |  8386 | `		if( pNode->type == XML_XINCLUDE_START \|\| pNode->type == XML_XINCLUDE_END ){` |
|      5 |  8387 | `			xmlUnlinkNode(pNode);` |
|      5 |  8388 | `			DomOrphanAdd(pShell,pNode);` |
|      3 |  8389 | `		}else{` |
|     11 |  8390 | `			DomDropXIncludeMarks(pShell,pNode->children);` |
|      - |  8391 | `		}` |
|     15 |  8392 | `		pNode = pNext;` |
|      1 |  8393 | `	}` |
|     15 |  8394 | `}` |
|      - |  8395 | `/*` |
|      - |  8396 | ` * DOMDocument::xinclude(int $options = 0): int\|false` |
|      - |  8397 | ` *` |
|      - |  8398 | ` * php answers the COUNT of substitutions libxml made, -1 when one of them` |
|      - |  8399 | ` * failed -- and FALSE when there were none at all, which is not an error and` |
|      - |  8400 | ` * is the one answer a caller has to screen for separately.` |
|      - |  8401 | ` */` |
|      6 |  8402 | `DOM_METHOD(vm_builtin_DOMDocument_xinclude)` |
|      1 |  8403 | `{` |
|      7 |  8404 | `	ph7_vm *pVm = pCtx->pVm;` |
|      7 |  8405 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|      7 |  8406 | `	int iOpts = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|      - |  8407 | `	sxu32 nMark;` |
|      - |  8408 | `	int nDone;` |
|      7 |  8409 | `	if( pDocNd == 0 ){` |
|    ! 0 |  8410 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  8411 | `		return PH7_OK;` |
|      - |  8412 | `	}` |
|      7 |  8413 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      7 |  8414 | `	nDone = xmlXIncludeProcessFlags((xmlDocPtr)pDocNd->pNode,iOpts);` |
|      7 |  8415 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::xinclude");` |
|      7 |  8416 | `	if( nDone >= 0 ){` |
|      7 |  8417 | `		DomDropXIncludeMarks(pDocNd->pShell,` |
|      4 |  8418 | `			((xmlDocPtr)pDocNd->pNode)->children);` |
|      2 |  8419 | `	}` |
|      7 |  8420 | `	if( nDone == 0 ){` |
|      3 |  8421 | `		ph7_result_bool(pCtx,0);` |
|      2 |  8422 | `	}else{` |
|      5 |  8423 | `		ph7_result_int(pCtx,nDone);` |
|      - |  8424 | `	}` |
|      7 |  8425 | `	return PH7_OK;` |
|      4 |  8426 | `}` |
|      - |  8427 |  |
|      - |  8428 | `/*` |
|      - |  8429 | ` * DOMDocument::registerNodeClass(string $baseClass, ?string $extendedClass): true` |
|      - |  8430 | ` *` |
|      - |  8431 | ` * php lets a program say which class a node should be WRAPPED in, per` |
|      - |  8432 | `` * document: register `MyElement` against `DOMElement` and every element of`` |
|      - |  8433 | ` * that document -- read from the tree or made by a factory -- comes back a` |
|      - |  8434 | ` * MyElement, so a walk can call the program's own methods on what it finds` |
|      - |  8435 | ` * instead of carrying a parallel table of its own.` |
|      - |  8436 | ` *` |
|      - |  8437 | ` * The lookup is by the class the extension would have used and by nothing` |
|      - |  8438 | `` * else: registering against `DOMNode` or `DOMCharacterData` changes NO`` |
|      - |  8439 | ` * wrapping, because an element is wrapped as a DOMElement and a text node as a` |
|      - |  8440 | ` * DOMText, and neither name is the one registered.` |
|      - |  8441 | ` *` |
|      - |  8442 | ` * The map is the document's, stored in a hidden slot beside its identity cache` |
|      - |  8443 | ` * and carried by a document CLONE the way the parser directives are.` |
|      - |  8444 | ` */` |
|      - |  8445 | `/* The map, materialized on the document the way its identity cache is. */` |
|   3950 |  8446 | `static ph7_hashmap * DomNodeClassMap(ph7_vm *pVm,ph7_class_instance *pDoc,int bMake)` |
|      5 |  8447 | `{` |
|   3955 |  8448 | `	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NCLS) : 0;` |
|   3955 |  8449 | `	if( pSlot == 0 \|\| (!bMake && (pSlot->iFlags & MEMOBJ_HASHMAP) == 0) ){` |
|   3891 |  8450 | `		return 0;` |
|      - |  8451 | `	}` |
|     66 |  8452 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     10 |  8453 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|    ! 0 |  8454 | `			return 0;` |
|      - |  8455 | `		}` |
|      4 |  8456 | `	}` |
|     66 |  8457 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|   1980 |  8458 | `}` |
|      - |  8459 | `/* The class a node of pDoc's tree is wrapped in: the registered one when the` |
|      - |  8460 | ` * document names it, php's own otherwise. */` |
|   3934 |  8461 | `static const char * DomWrapClassName(ph7_vm *pVm,ph7_class_instance *pDoc,int iKind,` |
|      - |  8462 | `	SyBlob *pOut)` |
|      5 |  8463 | `{` |
|   3939 |  8464 | `	const char *zBase = DomClassOfKind(iKind);` |
|   3939 |  8465 | `	ph7_hashmap *pMap = DomNodeClassMap(&(*pVm),pDoc,FALSE);` |
|   3939 |  8466 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  8467 | `	ph7_value sKey,*pHit;` |
|   3939 |  8468 | `	if( pMap == 0 ){` |
|   3891 |  8469 | `		return zBase;` |
|      - |  8470 | `	}` |
|     50 |  8471 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     50 |  8472 | `	PH7_MemObjStringAppend(&sKey,zBase,(sxu32)SyStrlen(zBase));` |
|     50 |  8473 | `	if( PH7_HashmapLookup(pMap,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|     32 |  8474 | `		pHit = HashmapExtractNodeValue(pEntry);` |
|     32 |  8475 | `		if( pHit && (pHit->iFlags & MEMOBJ_STRING) ){` |
|     32 |  8476 | `			SyBlobAppend(pOut,SyBlobData(&pHit->sBlob),SyBlobLength(&pHit->sBlob));` |
|     32 |  8477 | `			SyBlobNullAppend(pOut);` |
|     32 |  8478 | `			zBase = (const char *)SyBlobData(pOut);` |
|     15 |  8479 | `		}` |
|     15 |  8480 | `	}` |
|     50 |  8481 | `	PH7_MemObjRelease(&sKey);` |
|     50 |  8482 | `	return zBase;` |
|   1972 |  8483 | `}` |
|     26 |  8484 | `DOM_METHOD(vm_builtin_DOMDocument_registerNodeClass)` |
|      2 |  8485 | `{` |
|     28 |  8486 | `	ph7_vm *pVm = pCtx->pVm;` |
|     28 |  8487 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     28 |  8488 | `	int nBase = 0,nExt = 0;` |
|     28 |  8489 | `	const char *zBase = nArg > 0 ? ph7_value_to_string(apArg[0],&nBase) : "";` |
|     28 |  8490 | `	const char *zExt = (nArg > 1 && !ph7_value_is_null(apArg[1]))` |
|     38 |  8491 | `		? ph7_value_to_string(apArg[1],&nExt) : 0;` |
|     28 |  8492 | `	ph7_class *pBase,*pExt = 0,*pNode;` |
|      - |  8493 | `	ph7_hashmap *pMap;` |
|      - |  8494 | `	ph7_value sKey,sVal;` |
|     28 |  8495 | `	pBase = PH7_VmExtractClass(pVm,zBase,(sxu32)nBase,FALSE,0);` |
|     28 |  8496 | `	pNode = PH7_VmExtractClass(pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);` |
|     28 |  8497 | `	if( pBase == 0 \|\| pNode == 0 \|\| !PH7_VmInstanceOf(pBase,pNode) ){` |
|      7 |  8498 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  8499 | `			"DOMDocument::registerNodeClass(): Argument #1 ($baseClass) must be a "` |
|      2 |  8500 | `			"class name derived from DOMNode, %.*s given",nBase,zBase);` |
|      - |  8501 | `	}` |
|     24 |  8502 | `	if( zExt ){` |
|     22 |  8503 | `		pExt = PH7_VmExtractClass(pVm,zExt,(sxu32)nExt,FALSE,0);` |
|     22 |  8504 | `		if( pExt == 0 ){` |
|      4 |  8505 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  8506 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be "` |
|      1 |  8507 | `				"a valid class name or null, %.*s given",nExt,zExt);` |
|      - |  8508 | `		}` |
|     20 |  8509 | `		if( !PH7_VmInstanceOf(pExt,pBase) ){` |
|      - |  8510 | `			/* php's plain Error here, not a TypeError: the name IS a class, it` |
|      - |  8511 | `			 * is simply the wrong one. */` |
|      4 |  8512 | `			return PH7_VmThrowException(pCtx,"Error",` |
|      - |  8513 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be "` |
|      - |  8514 | `				"a class name derived from %z or null, %.*s given",` |
|      1 |  8515 | `				&pBase->sDisp,nExt,zExt);` |
|      - |  8516 | `		}` |
|     18 |  8517 | `		if( pExt->iFlags & PH7_CLASS_ABSTRACT ){` |
|      3 |  8518 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  8519 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must "` |
|      - |  8520 | `				"not be an abstract class");` |
|      - |  8521 | `		}` |
|      7 |  8522 | `	}` |
|     18 |  8523 | `	pMap = DomNodeClassMap(pVm,pThis,TRUE);` |
|     18 |  8524 | `	if( pMap == 0 ){` |
|    ! 0 |  8525 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8526 | `	}` |
|      - |  8527 | `	/* Keyed by the base's OWN spelling, which is the one the wrap looks up. */` |
|     18 |  8528 | `	PH7_MemObjInitFromString(pVm,&sKey,&pBase->sName);` |
|     18 |  8529 | `	if( pExt ){` |
|     16 |  8530 | `		PH7_MemObjInitFromString(pVm,&sVal,&pExt->sName);` |
|     16 |  8531 | `		PH7_HashmapInsert(pMap,&sKey,&sVal);` |
|     16 |  8532 | `		PH7_MemObjRelease(&sVal);` |
|      9 |  8533 | `	}else{` |
|      3 |  8534 | `		ph7_hashmap_node *pEntry = 0;` |
|      3 |  8535 | `		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|      3 |  8536 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      1 |  8537 | `		}` |
|      - |  8538 | `	}` |
|     18 |  8539 | `	PH7_MemObjRelease(&sKey);` |
|     18 |  8540 | `	ph7_result_bool(pCtx,1);` |
|     18 |  8541 | `	return PH7_OK;` |
|     15 |  8542 | `}` |
|      - |  8543 |  |
|      - |  8544 | `/* ===== DOMImplementation ===== */` |
|      - |  8545 |  |
|      - |  8546 | `/*` |
|      - |  8547 | ` * php's factory for the two things that cannot be made from a document that` |
|      - |  8548 | ` * does not exist yet: a DOCTYPE, and a document with a namespaced root.` |
|      - |  8549 | ` *` |
|      - |  8550 | `` * It carries no state at all -- `new DOMImplementation` is enough, its three`` |
|      - |  8551 | `` * methods are ordinary instance methods, and `$doc->implementation` answers a`` |
|      - |  8552 | ` * FRESH one on every read.` |
|      - |  8553 | ` */` |
|      - |  8554 |  |
|      - |  8555 | `/* A node that belongs to NO document, wrapped and owned the way a constructed` |
|      - |  8556 | ` * one is: parked on the per-VM limbo shell, its own identity-cache holder. The` |
|      - |  8557 | ` * caller owns the reference. */` |
|     20 |  8558 | `static ph7_class_instance * DomLimboWrap(ph7_vm *pVm,xmlNodePtr pNode)` |
|      1 |  8559 | `{` |
|     21 |  8560 | `	const char *zClass = DomClassOfKind((int)pNode->type);` |
|     21 |  8561 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     21 |  8562 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|     21 |  8563 | `	phl_xmldoc *pShell = pObj ? DomLimboShell(&(*pVm)) : 0;` |
|     21 |  8564 | `	phl_domnode *pRes = pShell ? DomNewRes(&(*pVm),pShell,pNode) : 0;` |
|     21 |  8565 | `	if( pRes == 0 ){` |
|    ! 0 |  8566 | `		if( pObj ){` |
|    ! 0 |  8567 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  8568 | `		}` |
|    ! 0 |  8569 | `		return 0;` |
|      - |  8570 | `	}` |
|     21 |  8571 | `	DomOrphanAdd(pShell,pNode);` |
|     21 |  8572 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|     21 |  8573 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pObj);` |
|     21 |  8574 | `	DomCacheStore(&(*pVm),pObj,pNode,pObj);` |
|     21 |  8575 | `	return pObj;` |
|     11 |  8576 | `}` |
|      - |  8577 | `/*` |
|      - |  8578 | ` * DOMImplementation::hasFeature(string $feature, string $version): bool` |
|      - |  8579 | ` *` |
|      - |  8580 | ` * php's table is two rows wide and the version is compared as a STRING: only` |
|      - |  8581 | ` * "1.0", "2.0" and "" are versions at all, and of those "Core" answers for` |
|      - |  8582 | ``  * "1.0" alone where "XML" answers for every one. So `hasFeature('Core','2.0')` `` |
|      - |  8583 | ``  * is false while `hasFeature('XML','2.0')` is true, and `hasFeature('Core','1')` `` |
|      - |  8584 | ` * -- a version that is not spelled the way the table spells it -- is false.` |
|      - |  8585 | ` */` |
|     22 |  8586 | `DOM_METHOD(vm_builtin_DOMImplementation_hasFeature)` |
|      1 |  8587 | `{` |
|     23 |  8588 | `	const char *zFeature = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     23 |  8589 | `	const char *zVersion = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     31 |  8590 | `	int bKnown = DomNameIs(zVersion,"1.0") \|\| DomNameIs(zVersion,"2.0")` |
|     27 |  8591 | `		\|\| zVersion[0] == 0;` |
|     49 |  8592 | `	ph7_result_bool(pCtx,bKnown` |
|     31 |  8593 | `		&& (DomNameIsCi(zFeature,"XML")` |
|     16 |  8594 | `			\|\| (DomNameIsCi(zFeature,"Core") && DomNameIs(zVersion,"1.0"))));` |
|     23 |  8595 | `	return PH7_OK;` |
|      1 |  8596 | `}` |
|      - |  8597 | `/*` |
|      - |  8598 | ` * DOMImplementation::createDocumentType(string $qualifiedName,` |
|      - |  8599 | ` *     string $publicId = '', string $systemId = ''): DOMDocumentType` |
|      - |  8600 | ` *` |
|      - |  8601 | `` * The name is not checked at ALL beyond being non-empty -- `1bad`, `a b` and`` |
|      - |  8602 | `` * `p:q:r` are each a doctype php builds without a word -- because nothing has`` |
|      - |  8603 | `` * parsed it: the name is the bytes the `<!DOCTYPE ...>` line will carry.`` |
|      - |  8604 | ` */` |
|     22 |  8605 | `DOM_METHOD(vm_builtin_DOMImplementation_createDocumentType)` |
|      1 |  8606 | `{` |
|     23 |  8607 | `	int nName = 0;` |
|     23 |  8608 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|     23 |  8609 | `	const char *zPub = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     23 |  8610 | `	const char *zSys = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|      - |  8611 | `	ph7_class_instance *pObj;` |
|      - |  8612 | `	xmlDtdPtr pDtd;` |
|     23 |  8613 | `	if( nName < 1 ){` |
|      3 |  8614 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  8615 | `			"DOMImplementation::createDocumentType(): Argument #1 ($qualifiedName) "` |
|      - |  8616 | `			"must not be empty");` |
|      - |  8617 | `	}` |
|     41 |  8618 | `	pDtd = xmlNewDtd(0,(const xmlChar *)zName,` |
|     20 |  8619 | `		zPub[0] ? (const xmlChar *)zPub : 0,` |
|     20 |  8620 | `		zSys[0] ? (const xmlChar *)zSys : 0);` |
|     21 |  8621 | `	if( pDtd == 0 ){` |
|    ! 0 |  8622 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8623 | `	}` |
|     21 |  8624 | `	pObj = DomLimboWrap(pCtx->pVm,(xmlNodePtr)pDtd);` |
|     21 |  8625 | `	if( pObj == 0 ){` |
|    ! 0 |  8626 | `		xmlFreeDtd(pDtd);` |
|    ! 0 |  8627 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8628 | `	}` |
|     21 |  8629 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     21 |  8630 | `	return PH7_OK;` |
|     12 |  8631 | `}` |
|      - |  8632 | `/*` |
|      - |  8633 | ` * DOMImplementation::createDocument(?string $namespace = null,` |
|      - |  8634 | ` *     string $qualifiedName = '', ?DOMDocumentType $doctype = null): DOMDocument` |
|      - |  8635 | ` *` |
|      - |  8636 | ` * An empty qualified name is a document with no root at all, which is what` |
|      - |  8637 | ` * makes the three-argument call with only a doctype meaningful.  The name is a` |
|      - |  8638 | `` * QName or nothing (`1bad` and `a:b:c` are the Namespace Error), and the`` |
|      - |  8639 | ` * namespace decides what becomes of its PREFIX: with a URI the element is` |
|      - |  8640 | ` * declared under it, and WITHOUT one the prefix is simply dropped -- php` |
|      - |  8641 | ` * builds the element from the local name and hangs the declaration on it` |
|      - |  8642 | `` * afterwards, so `createDocument('', 'p:root')` is `<root/>`.`` |
|      - |  8643 | ` *` |
|      - |  8644 | ` * A doctype that already belongs to a document is the Wrong Document Error,` |
|      - |  8645 | ` * so the same DOMDocumentType cannot seed two documents.` |
|      - |  8646 | ` */` |
|     26 |  8647 | `DOM_METHOD(vm_builtin_DOMImplementation_createDocument)` |
|      1 |  8648 | `{` |
|     27 |  8649 | `	ph7_vm *pVm = pCtx->pVm;` |
|     27 |  8650 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     27 |  8651 | `	int nName = 0;` |
|     27 |  8652 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],&nName) : "";` |
|     27 |  8653 | `	phl_domnode *pDtdNd = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? DomObjArg(apArg[2]) : 0;` |
|     27 |  8654 | `	xmlDtdPtr pDtd = pDtdNd ? (xmlDtdPtr)pDtdNd->pNode : 0;` |
|      - |  8655 | `	ph7_class *pClass;` |
|      - |  8656 | `	ph7_class_instance *pObj;` |
|      - |  8657 | `	phl_xmldoc *pShell;` |
|      - |  8658 | `	phl_domnode *pRes;` |
|      - |  8659 | `	xmlDocPtr pDoc;` |
|     27 |  8660 | `	xmlNodePtr pRoot = 0;` |
|     27 |  8661 | `	xmlNsPtr pNs = 0;` |
|     27 |  8662 | `	xmlChar *zPrefix = 0,*zLocal = 0;` |
|     27 |  8663 | `	if( pDtd && pDtd->doc ){` |
|      - |  8664 | `		/* php's own screen, and the reason a doctype seeds ONE document. */` |
|      3 |  8665 | `		return DomThrowAlways(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  8666 | `	}` |
|     25 |  8667 | `	if( nName > 0 ){` |
|     19 |  8668 | `		if( xmlValidateQName((const xmlChar *)zName,0) != 0 ){` |
|      9 |  8669 | `			return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  8670 | `		}` |
|     11 |  8671 | `		zLocal = xmlSplitQName2((const xmlChar *)zName,&zPrefix);` |
|     11 |  8672 | `		if( zLocal == 0 ){` |
|      7 |  8673 | `			zLocal = xmlStrdup((const xmlChar *)zName);` |
|      3 |  8674 | `		}` |
|     11 |  8675 | `		if( zUri && zUri[0] ){` |
|      - |  8676 | `			/* php asks libxml for the declaration BEFORE it has a node to hang` |
|      - |  8677 | ``			 * it on, and takes a refusal (the `xml` prefix over its own URI is`` |
|      - |  8678 | `			 * one) as the Namespace Error. */` |
|      5 |  8679 | `			pNs = xmlNewNs(0,zUri,zPrefix);` |
|      5 |  8680 | `			if( pNs == 0 ){` |
|    ! 0 |  8681 | `				if( zLocal ){` |
|    ! 0 |  8682 | `					xmlFree(zLocal);` |
|    ! 0 |  8683 | `				}` |
|    ! 0 |  8684 | `				if( zPrefix ){` |
|    ! 0 |  8685 | `					xmlFree(zPrefix);` |
|    ! 0 |  8686 | `				}` |
|    ! 0 |  8687 | `				return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  8688 | `			}` |
|      2 |  8689 | `		}` |
|      5 |  8690 | `	}` |
|     17 |  8691 | `	pDoc = xmlNewDoc((const xmlChar *)"1.0");` |
|     17 |  8692 | `	pClass = pDoc ? PH7_VmExtractClass(pVm,"DOMDocument",sizeof("DOMDocument")-1,FALSE,0) : 0;` |
|     17 |  8693 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|     17 |  8694 | `	pShell = pObj ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;` |
|     17 |  8695 | `	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|     17 |  8696 | `	if( pRes == 0 ){` |
|    ! 0 |  8697 | `		if( pDoc && pShell == 0 ){` |
|    ! 0 |  8698 | `			xmlFreeDoc(pDoc);` |
|    ! 0 |  8699 | `		}` |
|    ! 0 |  8700 | `		if( pObj ){` |
|    ! 0 |  8701 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  8702 | `		}` |
|    ! 0 |  8703 | `		if( pNs ){` |
|    ! 0 |  8704 | `			xmlFreeNs(pNs);` |
|    ! 0 |  8705 | `		}` |
|    ! 0 |  8706 | `		if( zLocal ){` |
|    ! 0 |  8707 | `			xmlFree(zLocal);` |
|    ! 0 |  8708 | `		}` |
|    ! 0 |  8709 | `		if( zPrefix ){` |
|    ! 0 |  8710 | `			xmlFree(zPrefix);` |
|    ! 0 |  8711 | `		}` |
|    ! 0 |  8712 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8713 | `	}` |
|     17 |  8714 | `	DomSetRes(pVm,pObj,pRes);` |
|     17 |  8715 | `	PH7_NativeSetAttrObj(pVm,pObj,DOM_DOC,pObj);` |
|     17 |  8716 | `	if( pDtd ){` |
|      - |  8717 | `		/* The doctype MOVES: it leaves the limbo shell for this document's` |
|      - |  8718 | `		 * tree, and every wrapper of it -- its own and its declarations' --` |
|      - |  8719 | `		 * re-homes on adoptNode's machinery, which is what makes the doctype` |
|      - |  8720 | ``		 * answer this document as its `ownerDocument` afterwards rather than`` |
|      - |  8721 | `		 * the holder it was its own. */` |
|      5 |  8722 | `		ph7_class_instance *pDtdObj = (ph7_class_instance *)apArg[2]->x.pOther;` |
|      5 |  8723 | `		DomOrphanRemove(pDtdNd->pShell,(xmlNodePtr)pDtd);` |
|      5 |  8724 | `		pDtd->doc = pDoc;` |
|      5 |  8725 | `		pDoc->intSubset = pDtd;` |
|      5 |  8726 | `		DomLinkLast((xmlNodePtr)pDoc,(xmlNodePtr)pDtd);` |
|      5 |  8727 | `		DomAdoptWrappers(pVm,pDtdObj,pObj,pShell,(xmlNodePtr)pDtd);` |
|      2 |  8728 | `	}` |
|     17 |  8729 | `	if( nName > 0 ){` |
|     11 |  8730 | `		pRoot = xmlNewDocNode(pDoc,0,zLocal,0);` |
|     11 |  8731 | `		if( pRoot ){` |
|     11 |  8732 | `			xmlDocSetRootElement(pDoc,pRoot);` |
|     11 |  8733 | `			if( pNs ){` |
|      5 |  8734 | `				pNs->next = pRoot->nsDef;` |
|      5 |  8735 | `				pRoot->nsDef = pNs;` |
|      5 |  8736 | `				xmlSetNs(pRoot,pNs);` |
|      5 |  8737 | `				pNs = 0;` |
|      2 |  8738 | `			}` |
|      5 |  8739 | `		}` |
|      5 |  8740 | `	}` |
|     17 |  8741 | `	if( pNs ){` |
|    ! 0 |  8742 | `		xmlFreeNs(pNs);` |
|    ! 0 |  8743 | `	}` |
|     17 |  8744 | `	if( zLocal ){` |
|     11 |  8745 | `		xmlFree(zLocal);` |
|      5 |  8746 | `	}` |
|     17 |  8747 | `	if( zPrefix ){` |
|      5 |  8748 | `		xmlFree(zPrefix);` |
|      2 |  8749 | `	}` |
|     17 |  8750 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     17 |  8751 | `	return PH7_OK;` |
|     14 |  8752 | `}` |
|      - |  8753 |  |
|      - |  8754 | `/* ===== The shared __get dispatch ===== */` |
|      - |  8755 |  |
|      - |  8756 | `/*` |
|      - |  8757 | ` * DOMNode's virtual properties.` |
|      - |  8758 | ` *` |
|      - |  8759 | ` * php exposes these through property handlers on the class; PHL answers them` |
|      - |  8760 | ` * from __get, as the chunk did. Returns 1 when it recognised the name, so a` |
|      - |  8761 | ` * subclass's own __get can state its extras and then defer here -- which is` |
|      - |  8762 | `` * what `parent::__get($name)` did.`` |
|      - |  8763 | ` */` |
|   3076 |  8764 | `static int DomNodeProp(ph7_context *pCtx,const char *zName)` |
|      4 |  8765 | `{` |
|   3080 |  8766 | `	ph7_vm *pVm = pCtx->pVm;` |
|   3080 |  8767 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   3080 |  8768 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|   3080 |  8769 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   3080 |  8770 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   4596 |  8771 | `	int bIsDoc = pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE);` |
|   3080 |  8772 | `	if( DomNameIs(zName,"nodeName") ){` |
|    617 |  8773 | `		DomNodeName(pCtx,pNode);` |
|   2773 |  8774 | `	}else if( DomNameIs(zName,"nodeValue") ){` |
|     97 |  8775 | `		DomNodeValue(pCtx,pNode);` |
|   2418 |  8776 | `	}else if( DomNameIs(zName,"nodeType") ){` |
|    107 |  8777 | `		ph7_result_int(pCtx,DomNodeTypeOf(pNode));` |
|   2317 |  8778 | `	}else if( DomNameIs(zName,"textContent") ){` |
|     57 |  8779 | `		DomTextContent(pCtx,pNode);` |
|   2236 |  8780 | `	}else if( DomNameIs(zName,"parentNode") ){` |
|    123 |  8781 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);` |
|   2148 |  8782 | `	}else if( DomNameIs(zName,"firstChild") ){` |
|      - |  8783 | `		/* Through the entity-reference resolver: an ADOPTED constructed` |
|      - |  8784 | `		 * reference's raw children are cleared, and php's reader answers the` |
|      - |  8785 | `		 * document's declaration anyway.` |
|      - |  8786 | `		 *` |
|      - |  8787 | `		 * Both ends are gated on php's dom_node_children_valid, which the` |
|      - |  8788 | `		 * DOCTYPE is not: libxml links a DTD's declarations as its children` |
|      - |  8789 | ``		 * and php's `firstChild`/`lastChild`/`hasChildNodes()` answer null,`` |
|      - |  8790 | ``		 * null and false there all the same -- while `childNodes` (which does`` |
|      - |  8791 | `		 * NOT consult it) lists them. */` |
|    594 |  8792 | `		DomResultNodeOf(pCtx,pNd,DomNodeChildFirst(pNode));` |
|   1793 |  8793 | `	}else if( DomNameIs(zName,"lastChild") ){` |
|     87 |  8794 | `		DomResultNodeOf(pCtx,pNd,DomNodeChildLast(pNode));` |
|   1455 |  8795 | `	}else if( DomNameIs(zName,"nextSibling") ){` |
|     69 |  8796 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->next : 0);` |
|   1378 |  8797 | `	}else if( DomNameIs(zName,"previousSibling") ){` |
|     15 |  8798 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->prev : 0);` |
|   1337 |  8799 | `	}else if( DomNameIs(zName,"ownerDocument") ){` |
|      - |  8800 | `		/* A document has no owner document, which is also why DomWrap answers` |
|      - |  8801 | `		 * the document itself rather than a second wrapper for it. The NODE's` |
|      - |  8802 | `		 * document is the source of truth, not the $__doc slot: a constructed` |
|      - |  8803 | `		 * ownerless node's slot points at its own holder, and php answers` |
|      - |  8804 | `		 * null there until an insertion adopts it. */` |
|    135 |  8805 | `		DomResultWrap(pCtx,(bIsDoc \|\| pNode == 0 \|\| pNode->doc == 0) ? 0 : pDoc);` |
|   1263 |  8806 | `	}else if( DomNameIs(zName,"parentElement") ){` |
|      - |  8807 | ``		/* php's `?DOMElement`: the parent when it IS an element, so a root`` |
|      - |  8808 | `		 * element (whose parent is the document) answers null. An ATTRIBUTE` |
|      - |  8809 | `		 * answers its element -- libxml parents an attribute, and php reports` |
|      - |  8810 | ``		 * that parent from both this property and `parentNode`. */`` |
|     49 |  8811 | `		xmlNodePtr pPar = pNode ? pNode->parent : 0;` |
|     49 |  8812 | `		DomResultNodeOf(pCtx,pNd,(pPar && pPar->type == XML_ELEMENT_NODE) ? pPar : 0);` |
|   1173 |  8813 | `	}else if( DomNameIs(zName,"namespaceURI") ){` |
|    329 |  8814 | `		DomNamespaceUri(pCtx,pNode);` |
|    985 |  8815 | `	}else if( DomNameIs(zName,"prefix") ){` |
|    127 |  8816 | `		DomPrefix(pCtx,pNode);` |
|    758 |  8817 | `	}else if( DomNameIs(zName,"localName") ){` |
|    125 |  8818 | `		DomLocalName(pCtx,pNode);` |
|    633 |  8819 | `	}else if( DomNameIs(zName,"isConnected") ){` |
|     77 |  8820 | `		ph7_result_bool(pCtx,DomIsConnected(pNode));` |
|    533 |  8821 | `	}else if( DomNameIs(zName,"baseURI") ){` |
|      - |  8822 | ``		/* php's is libxml's own xmlNodeGetBase(): the nearest `xml:base` on the`` |
|      - |  8823 | `		 * way up, resolved against the DOCUMENT's URI, and that URI itself when` |
|      - |  8824 | `		 * no ancestor declares one. So it answers null exactly when the document` |
|      - |  8825 | ``		 * was never given a URI -- a `new DOMDocument()` that was not loaded --`` |
|      - |  8826 | `		 * and a node created and never appended still answers its document's. */` |
|     39 |  8827 | `		xmlChar *zBase = pNode ? xmlNodeGetBase(pNode->doc,pNode) : 0;` |
|     39 |  8828 | `		if( zBase ){` |
|     23 |  8829 | `			ph7_result_string(pCtx,(const char *)zBase,-1);` |
|     23 |  8830 | `			xmlFree(zBase);` |
|     12 |  8831 | `		}else{` |
|     17 |  8832 | `			ph7_result_null(pCtx);` |
|      1 |  8833 | `		}` |
|    476 |  8834 | `	}else if( DomNameIs(zName,"childNodes") ){` |
|    245 |  8835 | `		ph7_class_instance *pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_CHILD,pThis,0,0,0);` |
|    245 |  8836 | `		if( pList == 0 ){` |
|    ! 0 |  8837 | `			return -1;` |
|      - |  8838 | `		}` |
|    245 |  8839 | `		PH7_NativeResultObject(pCtx,pList);` |
|    335 |  8840 | `	}else if( DomNameIs(zName,"attributes") ){` |
|      - |  8841 | `		/* php: NULL for anything that is not an element. */` |
|    130 |  8842 | `		if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|     11 |  8843 | `			ph7_result_null(pCtx);` |
|      6 |  8844 | `		}else{` |
|    120 |  8845 | `			ph7_class_instance *pMap = DomNewCollection(pVm,"DOMNamedNodeMap",pDoc,DNL_CHILD,pThis,0,0,0);` |
|    120 |  8846 | `			if( pMap == 0 ){` |
|    ! 0 |  8847 | `				return -1;` |
|      - |  8848 | `			}` |
|    120 |  8849 | `			PH7_NativeResultObject(pCtx,pMap);` |
|      - |  8850 | `		}` |
|     66 |  8851 | `	}else{` |
|     85 |  8852 | `		return 0;` |
|      - |  8853 | `	}` |
|   2996 |  8854 | `	return 1;` |
|   1542 |  8855 | `}` |
|      - |  8856 | `/*` |
|      - |  8857 | ` * Is this receiver a node class with NO node behind it?` |
|      - |  8858 | ` *` |
|      - |  8859 | `` * `new DOMNode()`, `new DOMCharacterData()`, `new DOMEntity()`, `new DOMNotation()`,`` |
|      - |  8860 | `` * `new DOMDocumentType()` and `new DOMNameSpaceNode()` all construct in php and none`` |
|      - |  8861 | ` * of them builds a libxml node -- so every property handler on such an object fetches` |
|      - |  8862 | `` * a null pointer and answers php's `DOMException: Invalid State Error`, on a read and`` |
|      - |  8863 | `` * on an `isset()` alike. A name the class does NOT declare never reaches a handler at`` |
|      - |  8864 | ` * all and keeps the ordinary undefined-property warning, which is why the test is` |
|      - |  8865 | ` * against the DECLARATION rather than against the reader.` |
|      - |  8866 | ` */` |
|   6652 |  8867 | `static int DomNodeLess(ph7_context *pCtx,const char *zName)` |
|      5 |  8868 | `{` |
|   6657 |  8869 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8870 | `	phl_domnode *pNd;` |
|   6657 |  8871 | `	if( pThis == 0 \|\| PH7_NativeAttr(pThis,DOM_RES) == 0 ){` |
|    314 |  8872 | `		return 0;   /* not one of the classes that carries a node at all */` |
|      - |  8873 | `	}` |
|   6345 |  8874 | `	pNd = DomResOf(pThis);` |
|   6345 |  8875 | `	if( pNd && pNd->pNode ){` |
|   6253 |  8876 | `		return 0;` |
|      - |  8877 | `	}` |
|     93 |  8878 | `	return PH7_ClassExtractAttribute(pThis->pClass,zName,(sxu32)SyStrlen(zName)) != 0;` |
|   3332 |  8879 | `}` |
|      - |  8880 | `/*` |
|      - |  8881 | ` * The write half. A per-class WRITER answers one of these; the name it does` |
|      - |  8882 | ` * not write is looked up in the class's READER by the property HOOK, which` |
|      - |  8883 | ` * decides between php's two refusals -- a property the table carries is` |
|      - |  8884 | ` * read-only, one it does not is nothing this class answers and goes back on` |
|      - |  8885 | ` * the ordinary path (where PHL's scope policy meets a dynamic property).` |
|      - |  8886 | ` */` |
|      - |  8887 | `#define DOM_SET_UNKNOWN  0   /* not a property of this class */` |
|      - |  8888 | `#define DOM_SET_DONE     1   /* written, or a refusal already raised into *pRc */` |
|      - |  8889 | `/*` |
|      - |  8890 | `` * php's `Cannot modify readonly property C::$p`, worded under the INSTANCE's`` |
|      - |  8891 | ` * class so a userland subclass of DOMElement is reported under its own name.` |
|      - |  8892 | ` *` |
|      - |  8893 | ` * Three writers refuse a name of their own HERE rather than by declining it: the` |
|      - |  8894 | ` * two DTD halves whose read-only properties are DEPRECATED (php's readonly Error` |
|      - |  8895 | ` * carries no deprecation notice, and the reader would have raised one) and the` |
|      - |  8896 | ` * document's read-only four for the same reason.` |
|      - |  8897 | ` */` |
|     98 |  8898 | `static sxi32 DomRefuseWrite(ph7_context *pCtx,const char *zName)` |
|      1 |  8899 | `{` |
|     99 |  8900 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     99 |  8901 | `	if( pThis == 0 ){` |
|    ! 0 |  8902 | `		return PH7_OK;` |
|      - |  8903 | `	}` |
|    148 |  8904 | `	return DomPropThrow(pCtx,"Error",0,"Cannot modify readonly property %z::$%s",` |
|     98 |  8905 | `		&pThis->pClass->sName,zName);` |
|     50 |  8906 | `}` |
|      - |  8907 | `/* A libxml string slot answered as php answers it: the bytes, or null when the` |
|      - |  8908 | `` * document never carried one (`encoding` on a declaration-less document). */`` |
|    209 |  8909 | `static void DomResultXmlStr(ph7_context *pCtx,const xmlChar *zVal)` |
|      1 |  8910 | `{` |
|    210 |  8911 | `	if( zVal ){` |
|    132 |  8912 | `		ph7_result_string(pCtx,(const char *)zVal,-1);` |
|     67 |  8913 | `	}else{` |
|     79 |  8914 | `		ph7_result_null(pCtx);` |
|      - |  8915 | `	}` |
|    210 |  8916 | `}` |
|      - |  8917 | `/*` |
|      - |  8918 | ` * The DOCUMENT's own state block.` |
|      - |  8919 | ` *` |
|      - |  8920 | ` * Nine of php's twenty-two DOMDocument properties are the XML DECLARATION and` |
|      - |  8921 | ` * the document's URI, read straight off libxml's xmlDoc -- and php spells most` |
|      - |  8922 | ` * of them twice, once under the DOM level-3 name and once under the level-1 one` |
|      - |  8923 | `` * it kept for compatibility (`version`/`xmlVersion`, `encoding`/`xmlEncoding`,`` |
|      - |  8924 | `` * `standalone`/`xmlStandalone`).  The pairs are not synonyms in every`` |
|      - |  8925 | ``  * direction: `xmlEncoding` and `actualEncoding` READ the same slot `encoding` `` |
|      - |  8926 | `` * writes and are themselves read-only, which is what makes `$d->xmlEncoding =`` |
|      - |  8927 | `` * 'UTF-8'` php's readonly Error and `$d->encoding = 'UTF-8'` the write that`` |
|      - |  8928 | `` * changes the bytes `saveXML()` emits.`` |
|      - |  8929 | ` *` |
|      - |  8930 | `` * `actualEncoding` and `config` carry php 8.4's #[\Deprecated]: the notice`` |
|      - |  8931 | `` * fires on a READ and on an `isset()` alike (both go through php's property`` |
|      - |  8932 | ` * handler), which is why it is raised HERE rather than in __get -- and NOT on a` |
|      - |  8933 | ` * write, where the readonly refusal comes first and is raised by the writer` |
|      - |  8934 | ` * below without consulting this reader.` |
|      - |  8935 | ` */` |
|    585 |  8936 | `static int DomDocStateProp(ph7_context *pCtx,const char *zName,xmlDocPtr pDoc,int bDepr)` |
|      1 |  8937 | `{` |
|    586 |  8938 | `	int bDeprAe = DomNameIs(zName,"actualEncoding");` |
|    586 |  8939 | `	if( bDeprAe \|\| DomNameIs(zName,"config") ){` |
|      - |  8940 | `		/* ...and NOT on the get_debug_info walk either: php marks the DECLARATION` |
|      - |  8941 | `		 * deprecated and its debug handler reads the C function behind it, so` |
|      - |  8942 | `		 * print_r()/var_dump() of a document raise nothing (bDepr is 0 there). */` |
|     43 |  8943 | `		if( bDepr ){` |
|     46 |  8944 | `			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|     15 |  8945 | `				bDeprAe ? "Property DOMDocument::$actualEncoding is deprecated"` |
|      - |  8946 | `				        : "Property DOMDocument::$config is deprecated");` |
|     15 |  8947 | `		}` |
|      - |  8948 | ``		/* `config` is php's DOM level-3 configuration slot and has never been`` |
|      - |  8949 | `		 * filled in there: the handler answers null and nothing else. */` |
|     43 |  8950 | `		if( bDeprAe ){` |
|     21 |  8951 | `			DomResultXmlStr(pCtx,pDoc ? pDoc->encoding : 0);` |
|     11 |  8952 | `		}else{` |
|     23 |  8953 | `			ph7_result_null(pCtx);` |
|      - |  8954 | `		}` |
|     43 |  8955 | `		return 1;` |
|      - |  8956 | `	}` |
|    544 |  8957 | `	if( DomNameIs(zName,"encoding") \|\| DomNameIs(zName,"xmlEncoding") ){` |
|     60 |  8958 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->encoding : 0);` |
|     60 |  8959 | `		return 1;` |
|      - |  8960 | `	}` |
|    485 |  8961 | `	if( DomNameIs(zName,"version") \|\| DomNameIs(zName,"xmlVersion") ){` |
|     73 |  8962 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->version : 0);` |
|     73 |  8963 | `		return 1;` |
|      - |  8964 | `	}` |
|    413 |  8965 | `	if( DomNameIs(zName,"documentURI") ){` |
|     33 |  8966 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->URL : 0);` |
|     33 |  8967 | `		return 1;` |
|      - |  8968 | `	}` |
|    381 |  8969 | `	if( DomNameIs(zName,"standalone") \|\| DomNameIs(zName,"xmlStandalone") ){` |
|      - |  8970 | `		/* libxml records four states in one int -- no declaration (-1), a` |
|      - |  8971 | ``		 * declaration without the attribute (-2), `no` (0) and `yes` (1) -- and`` |
|      - |  8972 | `		 * php's bool is true for the last one only. */` |
|     71 |  8973 | `		ph7_result_bool(pCtx,pDoc != 0 && pDoc->standalone == 1);` |
|     71 |  8974 | `		return 1;` |
|      - |  8975 | `	}` |
|    311 |  8976 | `	return 0;` |
|    294 |  8977 | `}` |
|      - |  8978 | `/*` |
|      - |  8979 | ` * The three DOMParentNode properties.  php declares them on the three` |
|      - |  8980 | ` * implementers ONLY -- DOMDocument, DOMElement and DOMDocumentFragment -- so` |
|      - |  8981 | `` * `$text->childElementCount` is the Undefined property warning there, which`` |
|      - |  8982 | ` * is why childElementCount cannot live in DomNodeProp (it did, and every node` |
|      - |  8983 | ` * kind answered 0 in silence where php warns and answers null).` |
|      - |  8984 | ` */` |
|   2228 |  8985 | `static int DomParentNodeProp(ph7_context *pCtx,const char *zName)` |
|      4 |  8986 | `{` |
|   2232 |  8987 | `	int bLast = DomNameIs(zName,"lastElementChild");` |
|   2232 |  8988 | `	if( bLast \|\| DomNameIs(zName,"firstElementChild") ){` |
|     37 |  8989 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|     37 |  8990 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     37 |  8991 | `		xmlNodePtr pChild = pNode ? (bLast ? pNode->last : pNode->children) : 0;` |
|     51 |  8992 | `		while( pChild && pChild->type != XML_ELEMENT_NODE ){` |
|     15 |  8993 | `			pChild = bLast ? pChild->prev : pChild->next;` |
|      1 |  8994 | `		}` |
|     37 |  8995 | `		DomResultNodeOf(pCtx,pNd,pChild);` |
|     37 |  8996 | `		return 1;` |
|      - |  8997 | `	}` |
|   2196 |  8998 | `	if( DomNameIs(zName,"childElementCount") ){` |
|     25 |  8999 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|     25 |  9000 | `		ph7_result_int(pCtx,DomChildCount(pNd ? (xmlNodePtr)pNd->pNode : 0,1));` |
|     25 |  9001 | `		return 1;` |
|      - |  9002 | `	}` |
|   2172 |  9003 | `	return 0;` |
|   1118 |  9004 | `}` |
|      - |  9005 | `/*` |
|      - |  9006 | ` * The two DOMChildNode-side properties.  php declares them on DOMElement and` |
|      - |  9007 | ` * DOMCharacterData only -- an attribute, a PI or the document warns Undefined` |
|      - |  9008 | ` * property -- and they skip every node kind that is not an element.` |
|      - |  9009 | ` */` |
|   2184 |  9010 | `static int DomChildNodeProp(ph7_context *pCtx,const char *zName)` |
|      4 |  9011 | `{` |
|   2188 |  9012 | `	int bNext = DomNameIs(zName,"nextElementSibling");` |
|   2188 |  9013 | `	if( bNext \|\| DomNameIs(zName,"previousElementSibling") ){` |
|     27 |  9014 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|     27 |  9015 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     27 |  9016 | `		xmlNodePtr pSib = pNode ? (bNext ? pNode->next : pNode->prev) : 0;` |
|     51 |  9017 | `		while( pSib && pSib->type != XML_ELEMENT_NODE ){` |
|     25 |  9018 | `			pSib = bNext ? pSib->next : pSib->prev;` |
|      1 |  9019 | `		}` |
|     27 |  9020 | `		DomResultNodeOf(pCtx,pNd,pSib);` |
|     27 |  9021 | `		return 1;` |
|      - |  9022 | `	}` |
|   2162 |  9023 | `	return 0;` |
|   1096 |  9024 | `}` |
|      - |  9025 | `/* DOMDocument adds documentElement and the state block above. */` |
|   2405 |  9026 | `static int DomDocPropEx(ph7_context *pCtx,const char *zName,int bDepr)` |
|      5 |  9027 | `{` |
|   2410 |  9028 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   2410 |  9029 | `	if( DomNameIs(zName,"documentElement") ){` |
|   1762 |  9030 | `		DomResultNodeOf(pCtx,pNd,pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0);` |
|   1762 |  9031 | `		return 1;` |
|      - |  9032 | `	}` |
|    649 |  9033 | `	if( DomNameIs(zName,"implementation") ){` |
|      - |  9034 | `		/* A FRESH object on every read, which is php's: the class has no state` |
|      - |  9035 | `		 * and nothing ties one to a document. */` |
|     17 |  9036 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,"DOMImplementation",` |
|      - |  9037 | `			sizeof("DOMImplementation")-1,FALSE,0);` |
|     17 |  9038 | `		ph7_class_instance *pImpl = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|     17 |  9039 | `		if( pImpl ){` |
|     17 |  9040 | `			PH7_NativeResultObject(pCtx,pImpl);` |
|      9 |  9041 | `		}else{` |
|    ! 0 |  9042 | `			ph7_result_null(pCtx);` |
|      - |  9043 | `		}` |
|     17 |  9044 | `		return 1;` |
|      - |  9045 | `	}` |
|    633 |  9046 | `	if( DomNameIs(zName,"doctype") ){` |
|      - |  9047 | `		/* The INTERNAL subset alone, which is php's: a DTD pulled in from the` |
|      - |  9048 | `` 		 * SYSTEM identifier lands in `extSubset` and is not what `doctype` `` |
|      - |  9049 | `		 * answers. Null for a document that declares none. */` |
|     94 |  9050 | `		DomResultNodeOf(pCtx,pNd,` |
|     46 |  9051 | `			pNd ? (xmlNodePtr)xmlGetIntSubset((xmlDocPtr)pNd->pNode) : 0);` |
|     48 |  9052 | `		return 1;` |
|      - |  9053 | `	}` |
|    586 |  9054 | `	if( DomDocStateProp(pCtx,zName,pNd ? (xmlDocPtr)pNd->pNode : 0,bDepr) ){` |
|    276 |  9055 | `		return 1;` |
|      - |  9056 | `	}` |
|      - |  9057 | `	{` |
|      - |  9058 | `		/* The seven directives, out of the one hidden word that holds them. */` |
|      - |  9059 | `		sxu32 i;` |
|   2003 |  9060 | `		for( i = 0 ; i < SX_ARRAYSIZE(aDomDocFlag) ; ++i ){` |
|   1809 |  9061 | `			if( DomNameIs(zName,aDomDocFlag[i].zName) ){` |
|    175 |  9062 | `				ph7_result_bool(pCtx,` |
|    116 |  9063 | `					DomDocFlag(PH7_ContextThis(pCtx),aDomDocFlag[i].iBit));` |
|    117 |  9064 | `				return 1;` |
|      - |  9065 | `			}` |
|    847 |  9066 | `		}` |
|      - |  9067 | `	}` |
|    195 |  9068 | `	if( DomParentNodeProp(pCtx,zName) ){` |
|     29 |  9069 | `		return 1;` |
|      - |  9070 | `	}` |
|    167 |  9071 | `	return DomNodeProp(pCtx,zName);` |
|   1208 |  9072 | `}` |
|   2165 |  9073 | `static int DomDocProp(ph7_context *pCtx,const char *zName)` |
|      5 |  9074 | `{` |
|   2170 |  9075 | `	return DomDocPropEx(pCtx,zName,1);` |
|      5 |  9076 | `}` |
|      - |  9077 | `/* The same reader with php's two #[\Deprecated] notices held back: the` |
|      - |  9078 | ` * get_debug_info walk below reads the handler, not the declaration. */` |
|    240 |  9079 | `static int DomDocPropQuiet(ph7_context *pCtx,const char *zName)` |
|      1 |  9080 | `{` |
|    241 |  9081 | `	return DomDocPropEx(pCtx,zName,0);` |
|      1 |  9082 | `}` |
|      - |  9083 | `/*` |
|      - |  9084 | `` * DOMDocumentType: what the `<!DOCTYPE ...>` line SAYS.`` |
|      - |  9085 | ` *` |
|      - |  9086 | `` * The DTD node has been reachable all along (`$doc->firstChild` on any document`` |
|      - |  9087 | ` * carrying a doctype), so what was missing was not the node but every question` |
|      - |  9088 | ``  * about it: the class, so `instanceof DOMDocumentType` and `get_class()` `` |
|      - |  9089 | ` * answer, and the four identifiers a program reads off one.` |
|      - |  9090 | ` *` |
|      - |  9091 | `` * php's `publicId`/`systemId` here are plain strings that answer "" when the`` |
|      - |  9092 | `` * declaration carries none -- unlike DOMEntity's, which are `?string` and null`` |
|      - |  9093 | ` * for the same absence -- so the two classes cannot share a reader.` |
|      - |  9094 | ` */` |
|    178 |  9095 | `static xmlDtdPtr DomThisDtd(ph7_context *pCtx)` |
|      2 |  9096 | `{` |
|    180 |  9097 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    180 |  9098 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    178 |  9099 | `	if( pNode == 0` |
|    177 |  9100 | `	 \|\| (pNode->type != XML_DTD_NODE && pNode->type != XML_DOCUMENT_TYPE_NODE) ){` |
|      7 |  9101 | `		return 0;` |
|      - |  9102 | `	}` |
|    174 |  9103 | `	return (xmlDtdPtr)pNode;` |
|     91 |  9104 | `}` |
|      - |  9105 | `/*` |
|      - |  9106 | `` * `internalSubset`: the bytes BETWEEN the brackets, rebuilt by dumping each`` |
|      - |  9107 | ` * declaration the subset holds.  php reads them off the DOCUMENT's internal` |
|      - |  9108 | ` * subset rather than the receiver's own children -- so a doctype cloned out of` |
|      - |  9109 | ` * a document that has none answers null -- and answers null, not "", when` |
|      - |  9110 | ` * there is no subset to dump at all.` |
|      - |  9111 | ` */` |
|     18 |  9112 | `static void DomInternalSubset(ph7_context *pCtx,xmlDtdPtr pDtd)` |
|      1 |  9113 | `{` |
|     19 |  9114 | `	xmlDtdPtr pSub = (pDtd && pDtd->doc) ? xmlGetIntSubset(pDtd->doc) : 0;` |
|     19 |  9115 | `	xmlNodePtr pChild = pSub ? pSub->children : 0;` |
|      - |  9116 | `	xmlBufferPtr pBuf;` |
|      - |  9117 | `	xmlOutputBufferPtr pOut;` |
|     19 |  9118 | `	if( pChild == 0 ){` |
|     11 |  9119 | `		ph7_result_null(pCtx);` |
|     11 |  9120 | `		return;` |
|      - |  9121 | `	}` |
|      9 |  9122 | `	pBuf = xmlBufferCreate();` |
|      9 |  9123 | `	pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|      9 |  9124 | `	if( pOut == 0 ){` |
|    ! 0 |  9125 | `		if( pBuf ){` |
|    ! 0 |  9126 | `			xmlBufferFree(pBuf);` |
|    ! 0 |  9127 | `		}` |
|    ! 0 |  9128 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9129 | `		return;` |
|      - |  9130 | `	}` |
|     17 |  9131 | `	for( ; pChild ; pChild = pChild->next ){` |
|      9 |  9132 | `		xmlNodeDumpOutput(pOut,pSub->doc,pChild,0,0,0);` |
|      5 |  9133 | `	}` |
|      9 |  9134 | `	xmlOutputBufferFlush(pOut);` |
|      9 |  9135 | `	ph7_result_string(pCtx,(const char *)xmlBufferContent(pBuf),(int)xmlBufferLength(pBuf));` |
|      9 |  9136 | `	xmlOutputBufferClose(pOut);` |
|      9 |  9137 | `	xmlBufferFree(pBuf);` |
|     10 |  9138 | `}` |
|    178 |  9139 | `static int DomDocTypeProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9140 | `{` |
|    180 |  9141 | `	xmlDtdPtr pDtd = DomThisDtd(pCtx);` |
|    180 |  9142 | `	if( DomNameIs(zName,"name") ){` |
|      - |  9143 | `		/* The name the DOCTYPE declares, which is also its nodeName. */` |
|     33 |  9144 | `		ph7_result_string(pCtx,(pDtd && pDtd->name) ? (const char *)pDtd->name : "",-1);` |
|     33 |  9145 | `		return 1;` |
|      - |  9146 | `	}` |
|    148 |  9147 | `	if( DomNameIs(zName,"publicId") ){` |
|     15 |  9148 | `		ph7_result_string(pCtx,(pDtd && pDtd->ExternalID) ? (const char *)pDtd->ExternalID : "",-1);` |
|     15 |  9149 | `		return 1;` |
|      - |  9150 | `	}` |
|    134 |  9151 | `	if( DomNameIs(zName,"systemId") ){` |
|     15 |  9152 | `		ph7_result_string(pCtx,(pDtd && pDtd->SystemID) ? (const char *)pDtd->SystemID : "",-1);` |
|     15 |  9153 | `		return 1;` |
|      - |  9154 | `	}` |
|    120 |  9155 | `	if( DomNameIs(zName,"internalSubset") ){` |
|     19 |  9156 | `		DomInternalSubset(pCtx,pDtd);` |
|     19 |  9157 | `		return 1;` |
|      - |  9158 | `	}` |
|    102 |  9159 | `	if( DomNameIs(zName,"entities") \|\| DomNameIs(zName,"notations") ){` |
|     54 |  9160 | `		ph7_class_instance *pMap = DomNewCollection(pCtx->pVm,"DOMNamedNodeMap",` |
|     26 |  9161 | `			DomThisDoc(pCtx),DomNameIs(zName,"notations") ? DNL_NOTS : DNL_ENTS,` |
|     13 |  9162 | `			PH7_ContextThis(pCtx),0,0,0);` |
|     28 |  9163 | `		if( pMap ){` |
|     28 |  9164 | `			PH7_NativeResultObject(pCtx,pMap);` |
|     15 |  9165 | `		}else{` |
|    ! 0 |  9166 | `			ph7_result_null(pCtx);` |
|      - |  9167 | `		}` |
|     28 |  9168 | `		return 1;` |
|      - |  9169 | `	}` |
|     75 |  9170 | `	return DomNodeProp(pCtx,zName);` |
|     91 |  9171 | `}` |
|      - |  9172 | `/*` |
|      - |  9173 | `` * DOMEntity: an `<!ENTITY ...>` declaration of the internal subset.`` |
|      - |  9174 | ` *` |
|      - |  9175 | ` * Its three identifiers are the DOM's "for an UNPARSED entity" rule, which php` |
|      - |  9176 | `` * follows to the letter: `publicId`, `systemId` and `notationName` answer null`` |
|      - |  9177 | `` * for every entity that is not `NDATA`-declared, so the external-but-parsed`` |
|      - |  9178 | `` * `<!ENTITY e SYSTEM "e.xml">` reads null from all three while`` |
|      - |  9179 | `` * `<!ENTITY g SYSTEM "g.gif" NDATA gif>` reads its own two and the notation's`` |
|      - |  9180 | `` * name.  (`baseURI`, which DOMNode answers, is where the resolved system`` |
|      - |  9181 | ` * identifier does show for both.)` |
|      - |  9182 | ` *` |
|      - |  9183 | ` * The other three are php 8.4's deprecated block, and like DOMDocument's the` |
|      - |  9184 | `` * notice fires on a READ and on an `isset()` alike -- both go through php's`` |
|      - |  9185 | ` * property handler -- and not on a write, where the readonly refusal comes` |
|      - |  9186 | ` * first and never consults this reader.` |
|      - |  9187 | ` */` |
|     92 |  9188 | `static xmlEntityPtr DomThisEntity(ph7_context *pCtx)` |
|      1 |  9189 | `{` |
|     93 |  9190 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     93 |  9191 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  9192 | `	/* Only a real entity DECLARATION is xmlEntity-shaped. An ELEMENT` |
|      - |  9193 | `	 * declaration wears this class in php and is an xmlElement underneath,` |
|      - |  9194 | `	 * whose fields past the node header are another struct's. */` |
|     93 |  9195 | `	if( pNode == 0 \|\| pNode->type != XML_ENTITY_DECL ){` |
|      7 |  9196 | `		return 0;` |
|      - |  9197 | `	}` |
|     87 |  9198 | `	return (xmlEntityPtr)pNode;` |
|     47 |  9199 | `}` |
|     92 |  9200 | `static int DomEntityPropEx(ph7_context *pCtx,const char *zName,int bDepr)` |
|      1 |  9201 | `{` |
|     93 |  9202 | `	xmlEntityPtr pEnt = DomThisEntity(pCtx);` |
|     93 |  9203 | `	int bUnparsed = pEnt && pEnt->etype == XML_EXTERNAL_GENERAL_UNPARSED_ENTITY;` |
|     93 |  9204 | `	if( DomNameIs(zName,"publicId") ){` |
|      9 |  9205 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->ExternalID : 0);` |
|      9 |  9206 | `		return 1;` |
|      - |  9207 | `	}` |
|     85 |  9208 | `	if( DomNameIs(zName,"systemId") ){` |
|      9 |  9209 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->SystemID : 0);` |
|      9 |  9210 | `		return 1;` |
|      - |  9211 | `	}` |
|     77 |  9212 | `	if( DomNameIs(zName,"notationName") ){` |
|      - |  9213 | ``		/* libxml keeps an unparsed entity's notation name in `content`. */`` |
|     11 |  9214 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->content : 0);` |
|     11 |  9215 | `		return 1;` |
|      - |  9216 | `	}` |
|     67 |  9217 | `	if( DomNameIs(zName,"actualEncoding") ){` |
|      3 |  9218 | `		if( bDepr ){` |
|      3 |  9219 | `			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|      - |  9220 | `				"Property DOMEntity::$actualEncoding is deprecated");` |
|      1 |  9221 | `		}` |
|      3 |  9222 | `		ph7_result_null(pCtx);` |
|      3 |  9223 | `		return 1;` |
|      - |  9224 | `	}` |
|     65 |  9225 | `	if( DomNameIs(zName,"encoding") ){` |
|      5 |  9226 | `		if( bDepr ){` |
|      5 |  9227 | `			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|      - |  9228 | `				"Property DOMEntity::$encoding is deprecated");` |
|      2 |  9229 | `		}` |
|      5 |  9230 | `		ph7_result_null(pCtx);` |
|      5 |  9231 | `		return 1;` |
|      - |  9232 | `	}` |
|     61 |  9233 | `	if( DomNameIs(zName,"version") ){` |
|      - |  9234 | `		/* php has never filled any of the three in: the handler answers NULL` |
|      - |  9235 | `		 * and does nothing else. */` |
|      3 |  9236 | `		if( bDepr ){` |
|      3 |  9237 | `			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|      - |  9238 | `				"Property DOMEntity::$version is deprecated");` |
|      1 |  9239 | `		}` |
|      3 |  9240 | `		ph7_result_null(pCtx);` |
|      3 |  9241 | `		return 1;` |
|      - |  9242 | `	}` |
|     59 |  9243 | `	return DomNodeProp(pCtx,zName);` |
|     47 |  9244 | `}` |
|     92 |  9245 | `static int DomEntityProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9246 | `{` |
|     93 |  9247 | `	return DomEntityPropEx(pCtx,zName,1);` |
|      1 |  9248 | `}` |
|      - |  9249 | `/* ...and the same reader without php's three #[\Deprecated] notices, for the` |
|      - |  9250 | ` * get_debug_info walk (which reads the handler, not the declaration). */` |
|    ! 0 |  9251 | `static int DomEntityPropQuiet(ph7_context *pCtx,const char *zName)` |
|    ! 0 |  9252 | `{` |
|    ! 0 |  9253 | `	return DomEntityPropEx(pCtx,zName,0);` |
|    ! 0 |  9254 | `}` |
|      - |  9255 | `/*` |
|      - |  9256 | `` * php declares `schemaTypeInfo` on both DOMElement and DOMAttr and has never`` |
|      - |  9257 | ` * filled it in: ext/dom answers NULL from a handler that does nothing else.` |
|      - |  9258 | ` * It is a declared property all the same, so a read is NOT the undefined-property` |
|      - |  9259 | ` * warning -- which is the whole difference this row buys.` |
|      - |  9260 | ` */` |
|      2 |  9261 | `static int DomSchemaTypeInfo(ph7_context *pCtx)` |
|      1 |  9262 | `{` |
|      3 |  9263 | `	ph7_result_null(pCtx);` |
|      3 |  9264 | `	return 1;` |
|      1 |  9265 | `}` |
|      - |  9266 | `/* DOMElement adds tagName, and the two attribute-backed names php exposes as` |
|      - |  9267 | `` * properties: `className` IS the class attribute and `id` IS the id one, both`` |
|      - |  9268 | ` * answering "" when the attribute is absent. */` |
|   2014 |  9269 | `static int DomElemProp(ph7_context *pCtx,const char *zName)` |
|      4 |  9270 | `{` |
|      - |  9271 | `	phl_domnode *pNd;` |
|   2018 |  9272 | `	int bClass = DomNameIs(zName,"className");` |
|   2018 |  9273 | `	if( DomNameIs(zName,"tagName") ){` |
|     58 |  9274 | `		pNd = DomThisNode(pCtx);` |
|     58 |  9275 | `		DomNodeName(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|     58 |  9276 | `		return 1;` |
|      - |  9277 | `	}` |
|   1962 |  9278 | `	if( bClass \|\| DomNameIs(zName,"id") ){` |
|      - |  9279 | `		xmlChar *zVal;` |
|      9 |  9280 | `		pNd = DomThisNode(pCtx);` |
|      9 |  9281 | `		zVal = pNd ? xmlGetNoNsProp((xmlNodePtr)pNd->pNode,` |
|      8 |  9282 | `			(const xmlChar *)(bClass ? "class" : "id")) : 0;` |
|      9 |  9283 | `		ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|      9 |  9284 | `		if( zVal ){` |
|      5 |  9285 | `			xmlFree(zVal);` |
|      2 |  9286 | `		}` |
|      9 |  9287 | `		return 1;` |
|      - |  9288 | `	}` |
|   1954 |  9289 | `	if( DomNameIs(zName,"schemaTypeInfo") ){` |
|    ! 0 |  9290 | `		return DomSchemaTypeInfo(pCtx);` |
|      - |  9291 | `	}` |
|   1954 |  9292 | `	if( DomParentNodeProp(pCtx,zName) \|\| DomChildNodeProp(pCtx,zName) ){` |
|     43 |  9293 | `		return 1;` |
|      - |  9294 | `	}` |
|   1912 |  9295 | `	return DomNodeProp(pCtx,zName);` |
|   1011 |  9296 | `}` |
|      - |  9297 | `/* DOMDocumentFragment: the three DOMParentNode properties over DOMNode's. */` |
|     84 |  9298 | `static int DomFragProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9299 | `{` |
|     85 |  9300 | `	if( DomParentNodeProp(pCtx,zName) ){` |
|     11 |  9301 | `		return 1;` |
|      - |  9302 | `	}` |
|     75 |  9303 | `	return DomNodeProp(pCtx,zName);` |
|     43 |  9304 | `}` |
|      - |  9305 | `/* DOMAttr adds name/value/ownerElement/specified/schemaTypeInfo. */` |
|    490 |  9306 | `static int DomAttrProp(ph7_context *pCtx,const char *zName)` |
|      4 |  9307 | `{` |
|    494 |  9308 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    494 |  9309 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    494 |  9310 | `	if( DomNameIs(zName,"name") ){` |
|      9 |  9311 | `		DomNodeName(pCtx,pNode);` |
|      9 |  9312 | `		return 1;` |
|      - |  9313 | `	}` |
|    486 |  9314 | `	if( DomNameIs(zName,"value") ){` |
|     57 |  9315 | `		DomNodeValue(pCtx,pNode);` |
|     57 |  9316 | `		return 1;` |
|      - |  9317 | `	}` |
|    432 |  9318 | `	if( DomNameIs(zName,"ownerElement") ){` |
|     55 |  9319 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);` |
|     55 |  9320 | `		return 1;` |
|      - |  9321 | `	}` |
|      - |  9322 | ``	/* php's `specified` is a DOM level-1 remnant: ext/dom answers TRUE for every`` |
|      - |  9323 | `	 * attribute a program can reach, including one it just created. */` |
|    378 |  9324 | `	if( DomNameIs(zName,"specified") ){` |
|      3 |  9325 | `		ph7_result_bool(pCtx,1);` |
|      3 |  9326 | `		return 1;` |
|      - |  9327 | `	}` |
|    376 |  9328 | `	if( DomNameIs(zName,"schemaTypeInfo") ){` |
|      3 |  9329 | `		return DomSchemaTypeInfo(pCtx);` |
|      - |  9330 | `	}` |
|    374 |  9331 | `	return DomNodeProp(pCtx,zName);` |
|    249 |  9332 | `}` |
|      - |  9333 | `/* DOMCharacterData adds data/length; DOMText adds wholeText on top of those. */` |
|    382 |  9334 | `static int DomCharDataProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9335 | `{` |
|    384 |  9336 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    384 |  9337 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    384 |  9338 | `	if( DomNameIs(zName,"data") ){` |
|    116 |  9339 | `		DomDataValue(pCtx,pNode);` |
|    116 |  9340 | `		return 1;` |
|      - |  9341 | `	}` |
|    270 |  9342 | `	if( DomNameIs(zName,"length") ){` |
|      - |  9343 | `		/* php counts UTF-8 CHARACTERS here (xmlUTF8Strlen over the node's own` |
|      - |  9344 | `		 * content), which is the same unit every offset on this class uses. */` |
|     13 |  9345 | `		ph7_result_int(pCtx,DomCharLength(pNode));` |
|     13 |  9346 | `		return 1;` |
|      - |  9347 | `	}` |
|    258 |  9348 | `	return 0;` |
|    193 |  9349 | `}` |
|    382 |  9350 | `static int DomCharProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9351 | `{` |
|    384 |  9352 | `	if( DomCharDataProp(pCtx,zName) \|\| DomChildNodeProp(pCtx,zName) ){` |
|    134 |  9353 | `		return 1;` |
|      - |  9354 | `	}` |
|    252 |  9355 | `	return DomNodeProp(pCtx,zName);` |
|    193 |  9356 | `}` |
|      - |  9357 | `/*` |
|      - |  9358 | ` * DOMText::wholeText is the whole RUN, not the node: php walks back to the` |
|      - |  9359 | ` * first adjacent text-or-CDATA sibling and forward to the last, concatenating` |
|      - |  9360 | ` * all of them, which is what makes it the answer to "what does this element` |
|      - |  9361 | ` * actually say" after an edit has left the text in pieces. Answering the node's` |
|      - |  9362 | ` * own data (what PHL did) is the same string only when the run is one node` |
|      - |  9363 | ` * long, and silently short otherwise.` |
|      - |  9364 | ` */` |
|     54 |  9365 | `static int DomIsTextRun(xmlNodePtr pNode)` |
|      1 |  9366 | `{` |
|     48 |  9367 | `	return pNode != 0` |
|     74 |  9368 | `		&& (pNode->type == XML_TEXT_NODE \|\| pNode->type == XML_CDATA_SECTION_NODE);` |
|      1 |  9369 | `}` |
|    316 |  9370 | `static int DomTextProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9371 | `{` |
|      - |  9372 | `	phl_domnode *pNd;` |
|      - |  9373 | `	xmlNodePtr pNode,pCur;` |
|      - |  9374 | `	SyBlob sOut;` |
|    318 |  9375 | `	if( DomNameIs(zName,"wholeText") ){` |
|     13 |  9376 | `		pNd = DomThisNode(pCtx);` |
|     13 |  9377 | `		pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     13 |  9378 | `		if( !DomIsTextRun(pNode) ){` |
|    ! 0 |  9379 | `			DomNodeValue(pCtx,pNode);` |
|    ! 0 |  9380 | `			return 1;` |
|      - |  9381 | `		}` |
|     15 |  9382 | `		while( DomIsTextRun(pNode->prev) ){` |
|      3 |  9383 | `			pNode = pNode->prev;` |
|      1 |  9384 | `		}` |
|     13 |  9385 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     29 |  9386 | `		for( pCur = pNode ; DomIsTextRun(pCur) ; pCur = pCur->next ){` |
|     17 |  9387 | `			xmlChar *zPart = xmlNodeGetContent(pCur);` |
|     17 |  9388 | `			if( zPart ){` |
|     17 |  9389 | `				SyBlobAppend(&sOut,(const void *)zPart,(sxu32)SyStrlen((const char *)zPart));` |
|     17 |  9390 | `				xmlFree(zPart);` |
|      8 |  9391 | `			}` |
|      9 |  9392 | `		}` |
|     13 |  9393 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     13 |  9394 | `		SyBlobRelease(&sOut);` |
|     13 |  9395 | `		return 1;` |
|      - |  9396 | `	}` |
|    306 |  9397 | `	return DomCharProp(pCtx,zName);` |
|    160 |  9398 | `}` |
|      - |  9399 | `/*` |
|      - |  9400 | ` * php's typed-property store for the DOM's own string-shaped properties: a` |
|      - |  9401 | ` * scalar coerces, null is accepted only where the declared type is nullable` |
|      - |  9402 | ` * (and means the empty string), and an array or an object is a TypeError` |
|      - |  9403 | ` * naming the class that DECLARES the property rather than the one the write` |
|      - |  9404 | ` * went through. The value is coerced through a COPY -- ph7_value_to_string()` |
|      - |  9405 | ` * converts the object it is handed, and that object is the caller's own` |
|      - |  9406 | `` * `$v` in `$node->nodeValue = $v`.`` |
|      - |  9407 | ` */` |
|    236 |  9408 | `static int DomWriteText(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|      - |  9409 | `	const char *zType,ph7_value *pVal,SyBlob *pOut,int *pRc)` |
|      2 |  9410 | `{` |
|    238 |  9411 | `	int bNullable = zType[0] == '?';` |
|      - |  9412 | `	ph7_value sTmp;` |
|    236 |  9413 | `	if( pVal == 0` |
|    236 |  9414 | `	 \|\| (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0` |
|    224 |  9415 | `	 \|\| ((pVal->iFlags & MEMOBJ_NULL) != 0 && !bNullable) ){` |
|      - |  9416 | `		char zBuf[128];` |
|     35 |  9417 | `		const char *zGiven = "null";` |
|     35 |  9418 | `		if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|     11 |  9419 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|     11 |  9420 | `			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sDisp);` |
|     11 |  9421 | `			zGiven = zBuf;` |
|     30 |  9422 | `		}else if( pVal ){` |
|     25 |  9423 | `			zGiven = ph7_type_name(pVal);` |
|     12 |  9424 | `		}` |
|     52 |  9425 | `		*pRc = DomPropThrow(pCtx,"TypeError",0,` |
|     17 |  9426 | `			"Cannot assign %s to property %s::$%s of type %s",zGiven,zOwner,zProp,zType);` |
|     35 |  9427 | `		return 0;` |
|      - |  9428 | `	}` |
|    204 |  9429 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|    204 |  9430 | `	if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|    192 |  9431 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|    192 |  9432 | `		PH7_MemObjLoad(pVal,&sTmp);` |
|    192 |  9433 | `		PH7_MemObjToString(&sTmp);` |
|    192 |  9434 | `		SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|    192 |  9435 | `		PH7_MemObjRelease(&sTmp);` |
|     95 |  9436 | `	}` |
|    204 |  9437 | `	SyBlobNullAppend(pOut);` |
|    204 |  9438 | `	return 1;` |
|    120 |  9439 | `}` |
|      - |  9440 | `/*` |
|      - |  9441 | `` * The same screen for a `bool` property. php's weak mode takes an int, a float`` |
|      - |  9442 | `` * or a string and answers its truthiness (`"0"` and `""` are false), and refuses`` |
|      - |  9443 | `` * null, an array and an object -- the one difference from the `?string` block`` |
|      - |  9444 | `` * above being that a bool property is NOT nullable, so `= null` is the TypeError`` |
|      - |  9445 | ` * rather than the empty write. Answers 1 when the caller may go on and use` |
|      - |  9446 | ` * ph7_value_to_bool(), 0 when the refusal has been raised into *pRc.` |
|      - |  9447 | ` */` |
|    376 |  9448 | `static int DomWriteBool(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|      - |  9449 | `	ph7_value *pVal,int *pRc)` |
|      1 |  9450 | `{` |
|    377 |  9451 | `	if( pVal == 0 \|\| (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL)) != 0 ){` |
|      - |  9452 | `		char zBuf[128];` |
|     15 |  9453 | `		const char *zGiven = "null";` |
|     15 |  9454 | `		if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  9455 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|    ! 0 |  9456 | `			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sDisp);` |
|    ! 0 |  9457 | `			zGiven = zBuf;` |
|     15 |  9458 | `		}else if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      7 |  9459 | `			zGiven = ph7_type_name(pVal);` |
|      3 |  9460 | `		}` |
|     22 |  9461 | `		*pRc = DomPropThrow(pCtx,"TypeError",0,` |
|      7 |  9462 | `			"Cannot assign %s to property %s::$%s of type bool",zGiven,zOwner,zProp);` |
|     15 |  9463 | `		return 0;` |
|      - |  9464 | `	}` |
|    363 |  9465 | `	return 1;` |
|    189 |  9466 | `}` |
|      - |  9467 | `/* Has this node ever been handed to PHP? The identity cache is the record. */` |
|    158 |  9468 | `static int DomIsWrapped(ph7_vm *pVm,ph7_class_instance *pDoc,xmlNodePtr pNode)` |
|      1 |  9469 | `{` |
|    159 |  9470 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|    159 |  9471 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  9472 | `	ph7_value sKey;` |
|      - |  9473 | `	int bHit;` |
|    159 |  9474 | `	if( pCache == 0 ){` |
|    ! 0 |  9475 | `		return 0;` |
|      - |  9476 | `	}` |
|    159 |  9477 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|    159 |  9478 | `	bHit = PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry != 0;` |
|    159 |  9479 | `	PH7_MemObjRelease(&sKey);` |
|    159 |  9480 | `	return bHit;` |
|     80 |  9481 | `}` |
|      - |  9482 | `/*` |
|      - |  9483 | ` * php FREES the subtree a content write replaces -- except the nodes a PHP` |
|      - |  9484 | ` * variable still holds a wrapper for, which it unlinks and keeps as roots of` |
|      - |  9485 | ` * their own detached fragments. That is observable: after` |
|      - |  9486 | `` * `$el->textContent = 'flat'`, a variable holding a grandchild still reads its`` |
|      - |  9487 | `` * text and answers NULL for `parentNode`. Parking the subtree whole (which is`` |
|      - |  9488 | ` * what this engine must do -- a wrapper's handle is a raw pointer, so nothing` |
|      - |  9489 | ` * here is ever freed before the document is) left every such parent attached,` |
|      - |  9490 | ` * so the same variable answered its old parent's name. Detach exactly the nodes` |
|      - |  9491 | ` * php would have kept: the OUTERMOST wrapped ones, php's own rule, since it` |
|      - |  9492 | ` * stops recursing at a node it is keeping.` |
|      - |  9493 | ` */` |
|    152 |  9494 | `static void DomPartWrapped(ph7_vm *pVm,ph7_class_instance *pDoc,phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |  9495 | `{` |
|    153 |  9496 | `	xmlNodePtr pChild = pNode ? pNode->children : 0;` |
|    155 |  9497 | `	while( pChild ){` |
|      3 |  9498 | `		xmlNodePtr pNext = pChild->next;` |
|      3 |  9499 | `		if( DomIsWrapped(&(*pVm),pDoc,pChild) ){` |
|    ! 0 |  9500 | `			xmlUnlinkNode(pChild);` |
|    ! 0 |  9501 | `			DomOrphanAdd(pShell,pChild);` |
|    ! 0 |  9502 | `		}else{` |
|      3 |  9503 | `			DomPartWrapped(&(*pVm),pDoc,pShell,pChild);` |
|      - |  9504 | `		}` |
|      3 |  9505 | `		pChild = pNext;` |
|      1 |  9506 | `	}` |
|    153 |  9507 | `}` |
|      - |  9508 | `/*` |
|      - |  9509 | ` * Everything a content write has to do before libxml sees it: the node's` |
|      - |  9510 | ` * children go to the document's ORPHAN set rather than being freed, because a` |
|      - |  9511 | ` * PHP variable may still hold a wrapper for one of them and the wrapper's` |
|      - |  9512 | ` * handle is a raw pointer. (php keeps such a node alive through its own` |
|      - |  9513 | ` * wrapper refcount; this engine parks it, exactly as removeChild does.)` |
|      - |  9514 | ` */` |
|    110 |  9515 | `static void DomDropChildren(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      2 |  9516 | `{` |
|    112 |  9517 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|    112 |  9518 | `	xmlNodePtr pChild = pNode ? pNode->children : 0;` |
|    268 |  9519 | `	while( pChild ){` |
|    157 |  9520 | `		xmlNodePtr pNext = pChild->next;` |
|    157 |  9521 | `		if( !DomIsWrapped(pCtx->pVm,pDoc,pChild) ){` |
|    151 |  9522 | `			DomPartWrapped(pCtx->pVm,pDoc,pShell,pChild);` |
|     75 |  9523 | `		}` |
|    157 |  9524 | `		xmlUnlinkNode(pChild);` |
|    157 |  9525 | `		DomOrphanAdd(pShell,pChild);` |
|    157 |  9526 | `		pChild = pNext;` |
|      1 |  9527 | `	}` |
|    112 |  9528 | `}` |
|      - |  9529 | `/*` |
|      - |  9530 | ` * php's two content writes, which are NOT the same write.` |
|      - |  9531 | ` *` |
|      - |  9532 | `` * `nodeValue` is libxml's xmlNodeSetContent, and on an element or an attribute`` |
|      - |  9533 | `` * that PARSES entity references: `$el->nodeValue = 'a&b'` is libxml's`` |
|      - |  9534 | ` * "unterminated entity reference" and leaves the node EMPTY, while` |
|      - |  9535 | `` * `'a&amp;b'` stores the one character. `textContent` sets one raw text child`` |
|      - |  9536 | ` * instead, so the same two strings store what they say. Every other node kind` |
|      - |  9537 | ` * takes its content literally either way.` |
|      - |  9538 | ` */` |
|    100 |  9539 | `static void DomSetContent(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode,` |
|      - |  9540 | `	const char *zText,int bParseEntities)` |
|      2 |  9541 | `{` |
|    102 |  9542 | `	int bTree = pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE;` |
|      - |  9543 | `	xmlNodePtr pText;` |
|    102 |  9544 | `	DomDropChildren(pCtx,pShell,pNode);` |
|    102 |  9545 | `	if( !bTree \|\| (bParseEntities && zText[0]) ){` |
|      - |  9546 | `		/* The parsing write is the one that can FAIL -- an unterminated entity` |
|      - |  9547 | `		 * reference leaves the node empty and libxml says so. Route that through` |
|      - |  9548 | `		 * the per-VM queue like every other libxml diagnostic here, or it prints` |
|      - |  9549 | ``		 * itself on stderr past error_reporting(), past `@`, and past`` |
|      - |  9550 | `		 * libxml_get_errors(). */` |
|      - |  9551 | `		SyBlob sFn;` |
|     76 |  9552 | `		sxu32 nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|     76 |  9553 | `		xmlNodeSetContent(pNode,(const xmlChar *)zText);` |
|      - |  9554 | `		/* php attributes the warning to the CALLER's scope -- a property write` |
|      - |  9555 | `		 * is not a call, so there is no accessor name to print. */` |
|     76 |  9556 | `		SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|     76 |  9557 | `		PH7_VmActiveFuncName(pCtx->pVm,&sFn);` |
|     76 |  9558 | `		PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,(const char *)SyBlobData(&sFn));` |
|     76 |  9559 | `		SyBlobRelease(&sFn);` |
|     76 |  9560 | `		return;` |
|      - |  9561 | `	}` |
|      - |  9562 | `	/* One raw text child -- and php leaves one even for the EMPTY string, which` |
|      - |  9563 | ``	 * is why `$el->nodeValue = ''` serializes as <r></r> rather than <r/>.`` |
|      - |  9564 | `	 * (The entity-parsing write is the exception: a string libxml refuses, like` |
|      - |  9565 | ``	 * `'a&b'`, leaves the element with no children at all.) */`` |
|     27 |  9566 | `	pText = xmlNewDocText(pNode->doc,(const xmlChar *)zText);` |
|     27 |  9567 | `	if( pText ){` |
|     27 |  9568 | `		DomLinkLast(pNode,pText);` |
|     13 |  9569 | `	}` |
|     52 |  9570 | `}` |
|      - |  9571 | `/*` |
|      - |  9572 | ` * The same refusal, raised from a property WRITE.` |
|      - |  9573 | ` *` |
|      - |  9574 | ` * A write is not a call, so php has no accessor name to print in front of the` |
|      - |  9575 | `` * warning and attributes it to the CALLER's scope instead (`f(): Namespace`` |
|      - |  9576 | `` * Error`, `Unknown: ...` at file scope) -- the shape DomSetContent already uses`` |
|      - |  9577 | ` * for the libxml diagnostic a content write can produce. Nothing is answered` |
|      - |  9578 | ` * either way: a property write has no return value.` |
|      - |  9579 | ` */` |
|     10 |  9580 | `static int DomThrowWrite(ph7_context *pCtx,int iCode)` |
|      1 |  9581 | `{` |
|     11 |  9582 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|      - |  9583 | `	SyBlob sFn;` |
|      - |  9584 | `	SyString sName;` |
|      - |  9585 | `	int rc;` |
|     11 |  9586 | `	if( pDoc == 0 \|\| DomDocFlag(pDoc,DOM_F_STRICT_ERR) ){` |
|      7 |  9587 | `		return DomThrowAlways(pCtx,iCode);` |
|      - |  9588 | `	}` |
|      5 |  9589 | `	SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|      5 |  9590 | `	PH7_VmActiveFuncName(pCtx->pVm,&sFn);` |
|      5 |  9591 | `	SyStringInitFromBuf(&sName,SyBlobData(&sFn),SyBlobLength(&sFn));` |
|      5 |  9592 | `	rc = PH7_VmThrowError(pCtx->pVm,&sName,PH7_CTX_WARNING,DomErrText(iCode));` |
|      5 |  9593 | `	SyBlobRelease(&sFn);` |
|      5 |  9594 | `	return rc;` |
|      6 |  9595 | `}` |
|      - |  9596 | `/*` |
|      - |  9597 | ` * The node kinds a value write REACHES.` |
|      - |  9598 | ` *` |
|      - |  9599 | ` * php's two writers do not accept the same list, and everything off it is a` |
|      - |  9600 | ` * silent NO-OP -- the write is accepted and the node is left exactly as it was.` |
|      - |  9601 | ` * PHL had one exclusion, the document, and wrote to everything else, which cost` |
|      - |  9602 | ` * three answers and one crash:` |
|      - |  9603 | ` *` |
|      - |  9604 | ` *   - an ENTITY REFERENCE's children are the entity DECLARATION's, shared by` |
|      - |  9605 | ` *     every reference to it and owned by the DTD; dropping them freed nodes the` |
|      - |  9606 | `` *     document frees again at teardown -- `$ref->nodeValue = 'x'` aborted the`` |
|      - |  9607 | ` *     process on a double free;` |
|      - |  9608 | ` *   - a DOCTYPE's children are the declarations of the internal SUBSET, so` |
|      - |  9609 | ``  *     `$doc->doctype->nodeValue = 'x'` silently emptied `<!DOCTYPE r [ ... ]>` `` |
|      - |  9610 | ` *     of every entity, element and attribute declaration in it;` |
|      - |  9611 | `` *   - a FRAGMENT is emptied by `textContent` and left alone by `nodeValue`,`` |
|      - |  9612 | ` *     which is the one kind where the two writers really do disagree.` |
|      - |  9613 | ` */` |
|     78 |  9614 | `static int DomValueWritable(xmlNodePtr pNode,int bValue)` |
|      1 |  9615 | `{` |
|     79 |  9616 | `	if( pNode == 0 ){` |
|      3 |  9617 | `		return 0;` |
|      - |  9618 | `	}` |
|     77 |  9619 | `	switch( pNode->type ){` |
|     30 |  9620 | `	case XML_ELEMENT_NODE:` |
|      - |  9621 | `	case XML_ATTRIBUTE_NODE:` |
|      - |  9622 | `	case XML_TEXT_NODE:` |
|      - |  9623 | `	case XML_COMMENT_NODE:` |
|      - |  9624 | `	case XML_CDATA_SECTION_NODE:` |
|      - |  9625 | `	case XML_PI_NODE:` |
|     61 |  9626 | `		return 1;` |
|      2 |  9627 | `	case XML_DOCUMENT_FRAG_NODE:` |
|      5 |  9628 | `		return !bValue;` |
|      6 |  9629 | `	default:` |
|     13 |  9630 | `		return 0;` |
|      - |  9631 | `	}` |
|     40 |  9632 | `}` |
|      - |  9633 | `/* DOMNode's three writable properties. */` |
|    182 |  9634 | `static int DomSetNodeProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9635 | `{` |
|    183 |  9636 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    183 |  9637 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    183 |  9638 | `	int bValue = DomNameIs(zName,"nodeValue");` |
|      - |  9639 | `	SyBlob sVal;` |
|    183 |  9640 | `	if( bValue \|\| DomNameIs(zName,"textContent") ){` |
|      - |  9641 | `		/* php's one REFUSAL among the no-ops, and it answers before the type` |
|      - |  9642 | ``		 * check does: `$ref->textContent = []` is the readonly Error where`` |
|      - |  9643 | ``		 * `$ref->nodeValue = []` is the TypeError. Left unwritten here, the`` |
|      - |  9644 | `		 * accessor macro falls through to it. */` |
|     93 |  9645 | `		if( !bValue && pNode && pNode->type == XML_ENTITY_REF_NODE ){` |
|      5 |  9646 | `			return DOM_SET_UNKNOWN;` |
|      - |  9647 | `		}` |
|    132 |  9648 | `		if( DomWriteText(pCtx,"DOMNode",bValue ? "nodeValue" : "textContent",` |
|     89 |  9649 | `			bValue ? "?string" : "string",pVal,&sVal,pRc) == 0 ){` |
|     11 |  9650 | `			return DOM_SET_DONE;` |
|      - |  9651 | `		}` |
|     79 |  9652 | `		if( DomValueWritable(pNode,bValue) ){` |
|     63 |  9653 | `			DomSetContent(pCtx,pNd->pShell,pNode,(const char *)SyBlobData(&sVal),bValue);` |
|     31 |  9654 | `		}` |
|     79 |  9655 | `		SyBlobRelease(&sVal);` |
|     79 |  9656 | `		return DOM_SET_DONE;` |
|      - |  9657 | `	}` |
|     91 |  9658 | `	if( DomNameIs(zName,"prefix") ){` |
|      - |  9659 | `		xmlNsPtr pNs;` |
|      - |  9660 | `		xmlNodePtr pDecl;` |
|     29 |  9661 | `		if( DomWriteText(pCtx,"DOMNode","prefix","string",pVal,&sVal,pRc) == 0 ){` |
|      5 |  9662 | `			return DOM_SET_DONE;` |
|      - |  9663 | `		}` |
|      - |  9664 | `		/* Only a node that HAS a namespace can be re-prefixed; php ignores the` |
|      - |  9665 | `		 * write for anything else, including an element in no namespace. */` |
|     25 |  9666 | `		if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->href ){` |
|     21 |  9667 | `			const char *zPrefix = (const char *)SyBlobData(&sVal);` |
|      - |  9668 | `			/* An attribute's declaration goes on its ELEMENT. */` |
|     21 |  9669 | `			pDecl = pNode->type == XML_ATTRIBUTE_NODE ? pNode->parent : pNode;` |
|     20 |  9670 | `			if( DomNameIs(zPrefix,"xml")` |
|     15 |  9671 | `			 && !DomNameIs((const char *)pNode->ns->href,` |
|      - |  9672 | `				"http://www.w3.org/XML/1998/namespace") ){` |
|      - |  9673 | ``				/* php's reserved-prefix refusal: `xml` may only name ITS namespace. */`` |
|      9 |  9674 | `				SyBlobRelease(&sVal);` |
|      9 |  9675 | `				*pRc = DomThrowWrite(pCtx,DOM_ERR_NAMESPACE);` |
|      9 |  9676 | `				return DOM_SET_DONE;` |
|      - |  9677 | `			}` |
|      - |  9678 | `			/* php looks only at the declarations THIS node carries -- an` |
|      - |  9679 | `			 * ancestor's is not reused, which is why re-prefixing a child grows` |
|      - |  9680 | ``			 * a second `xmlns:q` beside the one its parent already has. */`` |
|     29 |  9681 | `			for( pNs = pDecl ? pDecl->nsDef : 0 ; pNs ; pNs = pNs->next ){` |
|     19 |  9682 | `				const char *zHave = pNs->prefix ? (const char *)pNs->prefix : "";` |
|     18 |  9683 | `				if( DomNameIs(zHave,zPrefix) && pNs->href` |
|      5 |  9684 | `				 && xmlStrEqual(pNs->href,pNode->ns->href) ){` |
|      3 |  9685 | `					break;` |
|      - |  9686 | `				}` |
|      9 |  9687 | `			}` |
|     13 |  9688 | `			if( pNs == 0 ){` |
|      - |  9689 | `				/* None binds this prefix to the node's own URI: php declares one,` |
|      - |  9690 | ``				 * which is how `$el->prefix = ''` grows an `xmlns="..."` on the`` |
|      - |  9691 | `				 * element itself -- and how a prefix already bound HERE to another` |
|      - |  9692 | `				 * URI becomes libxml's refusal and php's Namespace Error. */` |
|     16 |  9693 | `				pNs = pDecl ? xmlNewNs(pDecl,pNode->ns->href,` |
|     10 |  9694 | `					zPrefix[0] ? (const xmlChar *)zPrefix : 0) : 0;` |
|      5 |  9695 | `			}` |
|     13 |  9696 | `			if( pNs == 0 ){` |
|      3 |  9697 | `				SyBlobRelease(&sVal);` |
|      3 |  9698 | `				*pRc = DomThrowWrite(pCtx,DOM_ERR_NAMESPACE);` |
|      3 |  9699 | `				return DOM_SET_DONE;` |
|      - |  9700 | `			}` |
|     11 |  9701 | `			xmlSetNs(pNode,pNs);` |
|      5 |  9702 | `		}` |
|     15 |  9703 | `		SyBlobRelease(&sVal);` |
|     15 |  9704 | `		return DOM_SET_DONE;` |
|      - |  9705 | `	}` |
|     63 |  9706 | `	return DOM_SET_UNKNOWN;` |
|     92 |  9707 | `}` |
|      - |  9708 | `/* DOMElement adds className and id, both of them ATTRIBUTES under the name. */` |
|    108 |  9709 | `static int DomSetElemProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9710 | `{` |
|    109 |  9711 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    109 |  9712 | `	int bClass = DomNameIs(zName,"className");` |
|      - |  9713 | `	SyBlob sVal;` |
|    109 |  9714 | `	if( bClass \|\| DomNameIs(zName,"id") ){` |
|      9 |  9715 | `		if( DomWriteText(pCtx,"DOMElement",bClass ? "className" : "id","string",` |
|      7 |  9716 | `			pVal,&sVal,pRc) == 0 ){` |
|      3 |  9717 | `			return DOM_SET_DONE;` |
|      - |  9718 | `		}` |
|      5 |  9719 | `		if( pNd ){` |
|      7 |  9720 | `			xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)(bClass ? "class" : "id"),` |
|      4 |  9721 | `				(const xmlChar *)SyBlobData(&sVal));` |
|      2 |  9722 | `		}` |
|      5 |  9723 | `		SyBlobRelease(&sVal);` |
|      5 |  9724 | `		return DOM_SET_DONE;` |
|      - |  9725 | `	}` |
|    103 |  9726 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|     55 |  9727 | `}` |
|      - |  9728 | `/* DOMAttr::value and DOMCharacterData::data are the node's own content. The` |
|      - |  9729 | `` * attribute's parses entity references, as its `nodeValue` does -- only`` |
|      - |  9730 | `` * `textContent` takes an attribute's bytes literally; character data has no`` |
|      - |  9731 | ` * parsing write at all, whichever name it is written under. */` |
|     80 |  9732 | `static int DomSetContentProp(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|      - |  9733 | `	const char *zName,ph7_value *pVal,int bParseEntities,int *pRc)` |
|      2 |  9734 | `{` |
|     82 |  9735 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      - |  9736 | `	SyBlob sVal;` |
|     82 |  9737 | `	if( !DomNameIs(zName,zProp) ){` |
|     39 |  9738 | `		return DOM_SET_UNKNOWN;` |
|      - |  9739 | `	}` |
|     44 |  9740 | `	if( DomWriteText(pCtx,zOwner,zProp,"string",pVal,&sVal,pRc) == 0 ){` |
|      5 |  9741 | `		return DOM_SET_DONE;` |
|      - |  9742 | `	}` |
|     40 |  9743 | `	if( pNd ){` |
|     59 |  9744 | `		DomSetContent(pCtx,pNd->pShell,(xmlNodePtr)pNd->pNode,` |
|     38 |  9745 | `			(const char *)SyBlobData(&sVal),bParseEntities);` |
|     19 |  9746 | `	}` |
|     40 |  9747 | `	SyBlobRelease(&sVal);` |
|     40 |  9748 | `	return DOM_SET_DONE;` |
|     42 |  9749 | `}` |
|     40 |  9750 | `static int DomSetAttrProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      2 |  9751 | `{` |
|     42 |  9752 | `	int rc = DomSetContentProp(pCtx,"DOMAttr","value",zName,pVal,TRUE,pRc);` |
|     42 |  9753 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      2 |  9754 | `}` |
|     30 |  9755 | `static int DomSetCharProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9756 | `{` |
|     31 |  9757 | `	int rc = DomSetContentProp(pCtx,"DOMCharacterData","data",zName,pVal,FALSE,pRc);` |
|     31 |  9758 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      1 |  9759 | `}` |
|     10 |  9760 | `static int DomSetPiProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9761 | `{` |
|     11 |  9762 | `	int rc = DomSetContentProp(pCtx,"DOMProcessingInstruction","data",zName,pVal,FALSE,pRc);` |
|     11 |  9763 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      1 |  9764 | `}` |
|      - |  9765 | `/*` |
|      - |  9766 | ` * The DOCUMENT's writable state: the three declaration slots php lets a program` |
|      - |  9767 | `` * change, plus `documentURI`.`` |
|      - |  9768 | ` *` |
|      - |  9769 | `` * php declares them `?string`/`bool`, so a null goes through the string three as`` |
|      - |  9770 | `` * the EMPTY string (`$d->version = null` writes `<?xml version=""?>`) and is a`` |
|      - |  9771 | ` * TypeError on the bool pair; an array or an object is a TypeError on all of` |
|      - |  9772 | ` * them.  The read-only four are refused HERE rather than through the reader,` |
|      - |  9773 | ` * because two of them are deprecated and php's readonly Error comes without the` |
|      - |  9774 | ` * deprecation notice a read would have raised.` |
|      - |  9775 | ` */` |
|    456 |  9776 | `static int DomSetDocProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9777 | `{` |
|    457 |  9778 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    457 |  9779 | `	xmlDocPtr pDoc = pNd ? (xmlDocPtr)pNd->pNode : 0;` |
|    457 |  9780 | `	int bVersion = DomNameIs(zName,"version") \|\| DomNameIs(zName,"xmlVersion");` |
|    457 |  9781 | `	int bUri = DomNameIs(zName,"documentURI");` |
|      - |  9782 | `	SyBlob sVal;` |
|    456 |  9783 | `	if( DomNameIs(zName,"actualEncoding") \|\| DomNameIs(zName,"config")` |
|    454 |  9784 | `	 \|\| DomNameIs(zName,"xmlEncoding") ){` |
|      7 |  9785 | `		*pRc = DomRefuseWrite(pCtx,zName);` |
|      7 |  9786 | `		return DOM_SET_DONE;` |
|      - |  9787 | `	}` |
|    451 |  9788 | `	if( bVersion \|\| bUri \|\| DomNameIs(zName,"encoding") ){` |
|      - |  9789 | `		const char *zNew;` |
|     73 |  9790 | `		if( DomWriteText(pCtx,"DOMDocument",zName,"?string",pVal,&sVal,pRc) == 0 ){` |
|     15 |  9791 | `			return DOM_SET_DONE;` |
|      - |  9792 | `		}` |
|     59 |  9793 | `		zNew = (const char *)SyBlobData(&sVal);` |
|     59 |  9794 | `		if( pDoc == 0 ){` |
|    ! 0 |  9795 | `			SyBlobRelease(&sVal);` |
|    ! 0 |  9796 | `			return DOM_SET_DONE;` |
|      - |  9797 | `		}` |
|     59 |  9798 | `		if( bVersion ){` |
|     27 |  9799 | `			if( pDoc->version ){` |
|     27 |  9800 | `				xmlFree((xmlChar *)pDoc->version);` |
|     13 |  9801 | `			}` |
|     27 |  9802 | `			pDoc->version = xmlStrdup((const xmlChar *)zNew);` |
|     46 |  9803 | `		}else if( bUri ){` |
|     15 |  9804 | `			if( pDoc->URL ){` |
|     15 |  9805 | `				xmlFree((xmlChar *)pDoc->URL);` |
|      7 |  9806 | `			}` |
|     15 |  9807 | `			pDoc->URL = xmlStrdup((const xmlChar *)zNew);` |
|      8 |  9808 | `		}else{` |
|      - |  9809 | `			/* php asks libxml for a converter and refuses the name outright when` |
|      - |  9810 | ``			 * there is none -- so `$d->encoding = 'x'` (and the empty string a`` |
|      - |  9811 | `			 * null coerces to) is a ValueError BEFORE anything is written,` |
|      - |  9812 | `			 * rather than a document that cannot be serialized later. */` |
|      - |  9813 | `			/* A null is refused before libxml is asked anything, as php does: the` |
|      - |  9814 | `			 * empty string it coerces to is a name a current libxml (2.15) answers` |
|      - |  9815 | `			 * WITH a converter, so asking would let the null through. */` |
|     19 |  9816 | `			xmlCharEncodingHandlerPtr pEnc = ph7_value_is_null(pVal) ? 0` |
|     17 |  9817 | `				: xmlFindCharEncodingHandler(zNew);` |
|     19 |  9818 | `			if( pEnc == 0 ){` |
|      8 |  9819 | `				SyBlobRelease(&sVal);` |
|      8 |  9820 | `				*pRc = DomPropThrow(pCtx,"ValueError",0,"Invalid document encoding");` |
|      8 |  9821 | `				return DOM_SET_DONE;` |
|      - |  9822 | `			}` |
|     12 |  9823 | `			xmlCharEncCloseFunc(pEnc);` |
|     12 |  9824 | `			if( pDoc->encoding ){` |
|    ! 0 |  9825 | `				xmlFree((xmlChar *)pDoc->encoding);` |
|    ! 0 |  9826 | `			}` |
|     12 |  9827 | `			pDoc->encoding = xmlStrdup((const xmlChar *)zNew);` |
|      - |  9828 | `		}` |
|     52 |  9829 | `		SyBlobRelease(&sVal);` |
|     52 |  9830 | `		return DOM_SET_DONE;` |
|      - |  9831 | `	}` |
|      - |  9832 | `	{` |
|      - |  9833 | ``		/* The seven directives. php declares each `bool`, so the screen is the`` |
|      - |  9834 | `		 * typed-property one a real slot would have applied, and the value lands` |
|      - |  9835 | `		 * in the hidden word rather than in a property of its own. */` |
|      - |  9836 | `		sxu32 i;` |
|   2015 |  9837 | `		for( i = 0 ; i < SX_ARRAYSIZE(aDomDocFlag) ; ++i ){` |
|   1971 |  9838 | `			if( DomNameIs(zName,aDomDocFlag[i].zName) ){` |
|    335 |  9839 | `				ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  9840 | `				sxi64 iWord;` |
|    335 |  9841 | `				if( DomWriteBool(pCtx,"DOMDocument",zName,pVal,pRc) == 0 ){` |
|      5 |  9842 | `					return DOM_SET_DONE;` |
|      - |  9843 | `				}` |
|    331 |  9844 | `				iWord = pThis ? PH7_NativeAttrInt(pThis,DOM_DFLAGS) : 0;` |
|    331 |  9845 | `				if( ph7_value_to_bool(pVal) ){` |
|    149 |  9846 | `					iWord \|= aDomDocFlag[i].iBit;` |
|     75 |  9847 | `				}else{` |
|    183 |  9848 | `					iWord &= ~(sxi64)aDomDocFlag[i].iBit;` |
|      - |  9849 | `				}` |
|    331 |  9850 | `				if( pThis ){` |
|    331 |  9851 | `					PH7_NativeSetAttrInt(pCtx->pVm,pThis,DOM_DFLAGS,iWord);` |
|    165 |  9852 | `				}` |
|    331 |  9853 | `				return DOM_SET_DONE;` |
|      - |  9854 | `			}` |
|    819 |  9855 | `		}` |
|      - |  9856 | `	}` |
|     45 |  9857 | `	if( DomNameIs(zName,"standalone") \|\| DomNameIs(zName,"xmlStandalone") ){` |
|     35 |  9858 | `		if( DomWriteBool(pCtx,"DOMDocument",zName,pVal,pRc) == 0 ){` |
|      9 |  9859 | `			return DOM_SET_DONE;` |
|      - |  9860 | `		}` |
|     27 |  9861 | `		if( pDoc ){` |
|      - |  9862 | `			/* Either way it becomes a DECLARED answer: writing false is` |
|      - |  9863 | ``			 * `standalone="no"` in the output, not the absent attribute. */`` |
|     27 |  9864 | `			pDoc->standalone = ph7_value_to_bool(pVal) ? 1 : 0;` |
|     13 |  9865 | `		}` |
|     27 |  9866 | `		return DOM_SET_DONE;` |
|      - |  9867 | `	}` |
|     11 |  9868 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|    229 |  9869 | `}` |
|      - |  9870 | ``/* The two collections have nothing writable of their own; `length` is read-only. */`` |
|      6 |  9871 | `static int DomSetNothing(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9872 | `{` |
|      3 |  9873 | `	SXUNUSED(pCtx); SXUNUSED(zName); SXUNUSED(pVal); SXUNUSED(pRc);` |
|      7 |  9874 | `	return DOM_SET_UNKNOWN;` |
|      1 |  9875 | `}` |
|      - |  9876 | `/* DOMProcessingInstruction adds target (its name) and data (its content) --` |
|      - |  9877 | ` * php declares it under DOMNode, not DOMCharacterData, so the character-data` |
|      - |  9878 | ` * methods are deliberately absent from it. */` |
|     80 |  9879 | `static int DomPiProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9880 | `{` |
|     81 |  9881 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     81 |  9882 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     81 |  9883 | `	if( DomNameIs(zName,"target") ){` |
|     15 |  9884 | `		ph7_result_string(pCtx,(pNode && pNode->name) ? (const char *)pNode->name : "",-1);` |
|     15 |  9885 | `		return 1;` |
|      - |  9886 | `	}` |
|     67 |  9887 | `	if( DomNameIs(zName,"data") ){` |
|     13 |  9888 | `		DomDataValue(pCtx,pNode);` |
|     13 |  9889 | `		return 1;` |
|      - |  9890 | `	}` |
|     55 |  9891 | `	return DomNodeProp(pCtx,zName);` |
|     41 |  9892 | `}` |
|      - |  9893 | `/*` |
|      - |  9894 | ` * DOMNameSpaceNode's ten properties. It is not a DOMNode -- php gives it its` |
|      - |  9895 | ` * own class with no parent -- so it shares none of the readers above: what it` |
|      - |  9896 | ` * carries is the DECLARATION (an xmlNs) and the element that makes it.` |
|      - |  9897 | ` */` |
|    194 |  9898 | `static int DomNsNodeProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9899 | `{` |
|    195 |  9900 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    195 |  9901 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    195 |  9902 | `	xmlNsPtr pNs = pNd ? (xmlNsPtr)pNd->pNode : 0;` |
|    195 |  9903 | `	ph7_class_instance *pOwner = pThis ? PH7_NativeAttrObj(pThis,DOM_NS_OWNER) : 0;` |
|    195 |  9904 | `	phl_domnode *pOwnerNd = DomResOf(pOwner);` |
|    195 |  9905 | `	const char *zPrefix = (pNs && pNs->prefix) ? (const char *)pNs->prefix : 0;` |
|    195 |  9906 | `	if( pNs == 0 ){` |
|     13 |  9907 | `		return 0;` |
|      - |  9908 | `	}` |
|    183 |  9909 | `	if( DomNameIs(zName,"nodeName") ){` |
|     31 |  9910 | `		if( zPrefix ){` |
|     27 |  9911 | `			ph7_result_string_format(pCtx,"%s:%s",DOM_XMLNS_NAME,zPrefix);` |
|     14 |  9912 | `		}else{` |
|      5 |  9913 | `			ph7_result_string(pCtx,DOM_XMLNS_NAME,-1);` |
|      - |  9914 | `		}` |
|     31 |  9915 | `		return 1;` |
|      - |  9916 | `	}` |
|    153 |  9917 | `	if( DomNameIs(zName,"nodeValue") ){` |
|      - |  9918 | `		/* php builds its wrapper as a fake node whose text CHILD carries the` |
|      - |  9919 | `		 * URI, and an EMPTY href writes no child at all -- so the xmlns=""` |
|      - |  9920 | `		 * UNDECLARATION answers null here while namespaceURI below answers` |
|      - |  9921 | `		 * the empty string off the href itself. Both doors (the attribute` |
|      - |  9922 | `		 * lookups and the namespace:: axis) share this recognizer. */` |
|     35 |  9923 | `		if( pNs->href && pNs->href[0] ){` |
|     33 |  9924 | `			ph7_result_string(pCtx,(const char *)pNs->href,-1);` |
|     17 |  9925 | `		}else{` |
|      3 |  9926 | `			ph7_result_null(pCtx);` |
|      - |  9927 | `		}` |
|     35 |  9928 | `		return 1;` |
|      - |  9929 | `	}` |
|    119 |  9930 | `	if( DomNameIs(zName,"namespaceURI") ){` |
|     21 |  9931 | `		ph7_result_string(pCtx,pNs->href ? (const char *)pNs->href : "",-1);` |
|     21 |  9932 | `		return 1;` |
|      - |  9933 | `	}` |
|     99 |  9934 | `	if( DomNameIs(zName,"nodeType") ){` |
|     17 |  9935 | `		ph7_result_int(pCtx,XML_NAMESPACE_DECL);` |
|     17 |  9936 | `		return 1;` |
|      - |  9937 | `	}` |
|      - |  9938 | ``	/* php answers the EMPTY prefix for the default declaration, and `xmlns` as`` |
|      - |  9939 | `	 * its local name -- the two halves of the name it is spelled with. */` |
|     83 |  9940 | `	if( DomNameIs(zName,"prefix") ){` |
|     21 |  9941 | `		ph7_result_string(pCtx,zPrefix ? zPrefix : "",-1);` |
|     21 |  9942 | `		return 1;` |
|      - |  9943 | `	}` |
|     63 |  9944 | `	if( DomNameIs(zName,"localName") ){` |
|     19 |  9945 | `		ph7_result_string(pCtx,zPrefix ? zPrefix : DOM_XMLNS_NAME,-1);` |
|     19 |  9946 | `		return 1;` |
|      - |  9947 | `	}` |
|     45 |  9948 | `	if( DomNameIs(zName,"isConnected") ){` |
|      5 |  9949 | `		ph7_result_bool(pCtx,pOwnerNd != 0` |
|      2 |  9950 | `			&& DomIsConnected((xmlNodePtr)pOwnerNd->pNode));` |
|      3 |  9951 | `		return 1;` |
|      - |  9952 | `	}` |
|     43 |  9953 | `	if( DomNameIs(zName,"ownerDocument") ){` |
|     11 |  9954 | `		DomResultWrap(pCtx,DomThisDoc(pCtx));` |
|     11 |  9955 | `		return 1;` |
|      - |  9956 | `	}` |
|     33 |  9957 | `	if( DomNameIs(zName,"parentNode") \|\| DomNameIs(zName,"parentElement") ){` |
|     29 |  9958 | `		DomResultWrap(pCtx,pOwner);` |
|     29 |  9959 | `		return 1;` |
|      - |  9960 | `	}` |
|      5 |  9961 | `	return 0;` |
|     98 |  9962 | `}` |
|      - |  9963 | `/*` |
|      - |  9964 | ` * Every property DOMEntity adds is read-only, and the refusal is raised HERE` |
|      - |  9965 | ` * rather than by falling through to the reader: the reader is where the three` |
|      - |  9966 | ` * deprecated names raise their notice, and php's write never reaches it -- the` |
|      - |  9967 | ` * readonly Error comes first and says nothing about deprecation.` |
|      - |  9968 | ` */` |
|     12 |  9969 | `static int DomSetEntityProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9970 | `{` |
|     12 |  9971 | `	if( DomNameIs(zName,"publicId") \|\| DomNameIs(zName,"systemId")` |
|      9 |  9972 | `	 \|\| DomNameIs(zName,"notationName") \|\| DomNameIs(zName,"actualEncoding")` |
|      6 |  9973 | `	 \|\| DomNameIs(zName,"encoding") \|\| DomNameIs(zName,"version") ){` |
|     13 |  9974 | `		*pRc = DomRefuseWrite(pCtx,zName);` |
|     13 |  9975 | `		return DOM_SET_DONE;` |
|      - |  9976 | `	}` |
|    ! 0 |  9977 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      7 |  9978 | `}` |
|      - |  9979 | `/*` |
|      - |  9980 | `` * DOMNotation: the two identifiers a `<!NOTATION ...>` declares.`` |
|      - |  9981 | ` *` |
|      - |  9982 | ` * Both are plain strings that answer "" for the half that is absent -- a` |
|      - |  9983 | `` * SYSTEM-only notation reads "" from `publicId` -- where DOMEntity's same-named`` |
|      - |  9984 | `` * pair are `?string`. The node under them is the stand-in DomNotationNode`` |
|      - |  9985 | ` * built, which shares the entity's layout, so both identifiers are read from` |
|      - |  9986 | ` * the same two fields.` |
|      - |  9987 | ` */` |
|     80 |  9988 | `static int DomNotationProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9989 | `{` |
|     82 |  9990 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     82 |  9991 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     79 |  9992 | `	xmlEntityPtr pNot = (pNode && pNode->type == XML_NOTATION_NODE)` |
|    114 |  9993 | `		? (xmlEntityPtr)pNode : 0;` |
|     82 |  9994 | `	if( DomNameIs(zName,"publicId") ){` |
|      9 |  9995 | `		ph7_result_string(pCtx,(pNot && pNot->ExternalID) ? (const char *)pNot->ExternalID : "",-1);` |
|      9 |  9996 | `		return 1;` |
|      - |  9997 | `	}` |
|     74 |  9998 | `	if( DomNameIs(zName,"systemId") ){` |
|     10 |  9999 | `		ph7_result_string(pCtx,(pNot && pNot->SystemID) ? (const char *)pNot->SystemID : "",-1);` |
|     10 | 10000 | `		return 1;` |
|      - | 10001 | `	}` |
|     65 | 10002 | `	return DomNodeProp(pCtx,zName);` |
|     42 | 10003 | `}` |
|      4 | 10004 | `static int DomSetNotationProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 | 10005 | `{` |
|      5 | 10006 | `	if( DomNameIs(zName,"publicId") \|\| DomNameIs(zName,"systemId") ){` |
|      5 | 10007 | `		*pRc = DomRefuseWrite(pCtx,zName);` |
|      5 | 10008 | `		return DOM_SET_DONE;` |
|      - | 10009 | `	}` |
|    ! 0 | 10010 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      3 | 10011 | `}` |
|      - | 10012 | `/*` |
|      - | 10013 | ` * DOMXPath's two, out of the hidden slots that hold them: php declares both` |
|      - | 10014 | `` * VIRTUAL, so `document` is read-only because its handler has no writer (and not`` |
|      - | 10015 | `` * because the slot is `readonly`, which is why php's isReadOnly() is false there)`` |
|      - | 10016 | `` * and the write refusal is the reader's, exactly as it is for a node's `nodeName`.`` |
|      - | 10017 | ` */` |
|     24 | 10018 | `static int DomXPathProp(ph7_context *pCtx,const char *zName)` |
|      1 | 10019 | `{` |
|     25 | 10020 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 | 10021 | `	if( DomNameIs(zName,"document") ){` |
|     11 | 10022 | `		DomResultWrap(pCtx,pThis ? PH7_NativeAttrObj(pThis,XP_DOC) : 0);` |
|     11 | 10023 | `		return 1;` |
|      - | 10024 | `	}` |
|     15 | 10025 | `	if( DomNameIs(zName,"registerNodeNamespaces") ){` |
|     13 | 10026 | `		ph7_result_bool(pCtx,pThis ? PH7_NativeAttrTruthy(pThis,XP_NSDEF) : 1);` |
|     13 | 10027 | `		return 1;` |
|      - | 10028 | `	}` |
|      3 | 10029 | `	return 0;` |
|     13 | 10030 | `}` |
|     12 | 10031 | `static int DomSetXPathProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 | 10032 | `{` |
|     13 | 10033 | `	if( DomNameIs(zName,"registerNodeNamespaces") ){` |
|      9 | 10034 | `		ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      9 | 10035 | `		if( DomWriteBool(pCtx,"DOMXPath",zName,pVal,pRc) == 0 ){` |
|      3 | 10036 | `			return DOM_SET_DONE;` |
|      - | 10037 | `		}` |
|      7 | 10038 | `		if( pThis ){` |
|      7 | 10039 | `			PH7_NativeSetAttrBool(pCtx->pVm,pThis,XP_NSDEF,ph7_value_to_bool(pVal));` |
|      3 | 10040 | `		}` |
|      7 | 10041 | `		return DOM_SET_DONE;` |
|      - | 10042 | `	}` |
|      - | 10043 | ``	/* `document` falls through to the shared refusal, which reads the class's own`` |
|      - | 10044 | ``	 * recognizer and words php's `Cannot modify readonly property`. */`` |
|      5 | 10045 | `	return DOM_SET_UNKNOWN;` |
|      7 | 10046 | `}` |
|      - | 10047 | `/*` |
|      - | 10048 | ` * DOMDocumentFragment::appendXML(string $data): bool` |
|      - | 10049 | ` *` |
|      - | 10050 | ` * php parses the chunk as a well-balanced FRAGMENT (no single root required,` |
|      - | 10051 | ` * bare text allowed) and appends what it produced; anything libxml refuses is` |
|      - | 10052 | `` * `false` with nothing appended.`` |
|      - | 10053 | ` */` |
|     44 | 10054 | `DOM_METHOD(vm_builtin_DOMDocumentFragment_appendXML)` |
|      1 | 10055 | `{` |
|     45 | 10056 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     45 | 10057 | `	const char *zXml = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     45 | 10058 | `	xmlNodePtr pFrag,pList = 0;` |
|      - | 10059 | `	sxu32 nMark;` |
|      - | 10060 | `	int rc;` |
|     45 | 10061 | `	if( pNd == 0 ){` |
|    ! 0 | 10062 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 | 10063 | `		return PH7_OK;` |
|      - | 10064 | `	}` |
|     45 | 10065 | `	pFrag = (xmlNodePtr)pNd->pNode;` |
|     45 | 10066 | `	if( DomNodeReadOnly(pFrag) ){` |
|      - | 10067 | ``		/* A CONSTRUCTED fragment -- `new DOMDocumentFragment()` -- refuses`` |
|      - | 10068 | `		 * this door the way every child-list door refuses an ownerless` |
|      - | 10069 | `		 * receiver, where its append() takes the same chunk's nodes. */` |
|      3 | 10070 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|      - | 10071 | `	}` |
|     43 | 10072 | `	nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|     43 | 10073 | `	rc = xmlParseBalancedChunkMemory(pFrag->doc,0,0,0,(const xmlChar *)zXml,&pList);` |
|     43 | 10074 | `	PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,"DOMDocumentFragment::appendXML");` |
|     43 | 10075 | `	if( rc != 0 ){` |
|      3 | 10076 | `		if( pList ){` |
|    ! 0 | 10077 | `			xmlFreeNodeList(pList);` |
|    ! 0 | 10078 | `		}` |
|      3 | 10079 | `		ph7_result_bool(pCtx,0);` |
|      3 | 10080 | `		return PH7_OK;` |
|      - | 10081 | `	}` |
|    101 | 10082 | `	while( pList ){` |
|     61 | 10083 | `		xmlNodePtr pNext = pList->next;` |
|     61 | 10084 | `		pList->next = pList->prev = 0;` |
|     61 | 10085 | `		DomLinkLast(pFrag,pList);` |
|     61 | 10086 | `		pList = pNext;` |
|      1 | 10087 | `	}` |
|     41 | 10088 | `	ph7_result_bool(pCtx,1);` |
|     41 | 10089 | `	return PH7_OK;` |
|     23 | 10090 | `}` |
|      - | 10091 | `/* DOMDocument::getElementsByTagName / DOMElement::getElementsByTagName --` |
|      - | 10092 | ` * php declares it on those two, not on DOMNode, so both specs name it. */` |
|    102 | 10093 | `DOM_METHOD(vm_builtin_Dom_getElementsByTagName)` |
|      1 | 10094 | `{` |
|    103 | 10095 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    103 | 10096 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|      - | 10097 | `	ph7_class_instance *pList;` |
|    103 | 10098 | `	if( pThis == 0 ){` |
|    ! 0 | 10099 | `		return PH7_OK;` |
|      - | 10100 | `	}` |
|    103 | 10101 | `	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTN,pThis,zName,0,0);` |
|    103 | 10102 | `	if( pList == 0 ){` |
|    ! 0 | 10103 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 10104 | `	}` |
|    103 | 10105 | `	PH7_NativeResultObject(pCtx,pList);` |
|    103 | 10106 | `	return PH7_OK;` |
|     52 | 10107 | `}` |
|      - | 10108 | `/*` |
|      - | 10109 | ` * DOMDocument::getElementsByTagNameNS / DOMElement::getElementsByTagNameNS` |
|      - | 10110 | ` * (?string $namespace, string $localName): DOMNodeList` |
|      - | 10111 | ` *` |
|      - | 10112 | ` * The namespace-aware half of the only two lookups the DOM has, and the one` |
|      - | 10113 | ` * every namespaced format is read with -- an XSLT stylesheet, a SOAP envelope, a` |
|      - | 10114 | ` * sitemap. Undefined here, so the URI could be answered for a node already found` |
|      - | 10115 | ` * and never searched FOR.` |
|      - | 10116 | ` *` |
|      - | 10117 | ` * The list is live and the receiver is never in it, exactly as the name-only` |
|      - | 10118 | ` * one; DomGebtnMatch carries php's asymmetric wildcard rules.` |
|      - | 10119 | ` */` |
|     44 | 10120 | `DOM_METHOD(vm_builtin_Dom_getElementsByTagNameNS)` |
|      1 | 10121 | `{` |
|     45 | 10122 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     45 | 10123 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|     45 | 10124 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - | 10125 | `	ph7_class_instance *pList;` |
|     45 | 10126 | `	if( pThis == 0 ){` |
|    ! 0 | 10127 | `		return PH7_OK;` |
|      - | 10128 | `	}` |
|     67 | 10129 | `	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTNNS,pThis,` |
|     22 | 10130 | `		zName,zUri ? zUri : "",0);` |
|     45 | 10131 | `	if( pList == 0 ){` |
|    ! 0 | 10132 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 10133 | `	}` |
|     45 | 10134 | `	PH7_NativeResultObject(pCtx,pList);` |
|     45 | 10135 | `	return PH7_OK;` |
|     23 | 10136 | `}` |
|      - | 10137 |  |
|      - | 10138 | `/*` |
|      - | 10139 | ` * php's get_debug_info for the DOM (ph7_class::xPresent), and the ONLY table any` |
|      - | 10140 | ` * presentation surface of a node class shows.` |
|      - | 10141 | ` *` |
|      - | 10142 | ` * Every property php declares on these classes is VIRTUAL -- a read_property` |
|      - | 10143 | ` * handler over libxml's own state -- so php's get_properties answers a node's REAL` |
|      - | 10144 | `` * table and nothing else: `(array)`, `get_object_vars()`, `foreach`,`` |
|      - | 10145 | ``  * `json_encode()`, `var_export()`, `serialize()` and `get_mangled_object_vars()` `` |
|      - | 10146 | ` * see an EMPTY document, and only a SUBCLASS's own properties ever appear there.` |
|      - | 10147 | ` * That half needs no hook at all (the slot walk already answers it), which is why` |
|      - | 10148 | ` * the non-debug call declines and lets the engine fall back to it.` |
|      - | 10149 | ` *` |
|      - | 10150 | ` * The DEBUG half is php's dom_get_debug_info_helper: the object's real properties` |
|      - | 10151 | ` * first (a subclass's own, under php's mangled keys), then the class's` |
|      - | 10152 | ` * property-handler table walked own-entries-first with the parent chain behind it.` |
|      - | 10153 | ` * Two rules come out of that helper and are reproduced here. An OBJECT value is` |
|      - | 10154 | `` * never recursed into -- php substitutes the literal `(object value omitted)`, so`` |
|      - | 10155 | `` * `print_r($doc)` does not print the whole tree through `documentElement`. And a`` |
|      - | 10156 | ` * handler that FAILS contributes no row at all, which in ext/dom is exactly one` |
|      - | 10157 | `` * case: `ownerDocument` is missing from a node libxml never gave a document (one`` |
|      - | 10158 | `` * built with `new`, and the stand-in a NOTATION is read through).`` |
|      - | 10159 | ` *` |
|      - | 10160 | ` * The values are read through the class's own recognizer rather than through` |
|      - | 10161 | ` * __get: php's walk is the C handler, so it runs no userland code and raises none` |
|      - | 10162 | `` * of the deprecations a php-level read of `actualEncoding`/`config` would.`` |
|      - | 10163 | ` */` |
|      - | 10164 | `#define DOM_NODE_DEBUG \` |
|      - | 10165 | `	"nodeName", "nodeValue", "nodeType", "parentNode", "parentElement", "childNodes", \` |
|      - | 10166 | `	"firstChild", "lastChild", "previousSibling", "nextSibling", "attributes", \` |
|      - | 10167 | `	"isConnected", "ownerDocument", "namespaceURI", "prefix", "localName", \` |
|      - | 10168 | `	"baseURI", "textContent"` |
|      - | 10169 | `#define DOM_CHARDATA_DEBUG \` |
|      - | 10170 | `	"data", "length", "previousElementSibling", "nextElementSibling"` |
|      - | 10171 | `static const char * const azDomNodeDebug[] = { DOM_NODE_DEBUG };` |
|      - | 10172 | `static const char * const azDomDocDebug[] = {` |
|      - | 10173 | `	"doctype", "implementation", "documentElement", "actualEncoding", "encoding",` |
|      - | 10174 | `	"xmlEncoding", "standalone", "xmlStandalone", "version", "xmlVersion",` |
|      - | 10175 | `	"strictErrorChecking", "documentURI", "config", "formatOutput", "validateOnParse",` |
|      - | 10176 | `	"resolveExternals", "preserveWhiteSpace", "recover", "substituteEntities",` |
|      - | 10177 | `	"firstElementChild", "lastElementChild", "childElementCount", DOM_NODE_DEBUG` |
|      - | 10178 | `};` |
|      - | 10179 | `static const char * const azDomElemDebug[] = {` |
|      - | 10180 | `	"tagName", "className", "id", "schemaTypeInfo", "firstElementChild",` |
|      - | 10181 | `	"lastElementChild", "childElementCount", "previousElementSibling",` |
|      - | 10182 | `	"nextElementSibling", DOM_NODE_DEBUG` |
|      - | 10183 | `};` |
|      - | 10184 | `static const char * const azDomAttrDebug[] = {` |
|      - | 10185 | `	"name", "specified", "value", "ownerElement", "schemaTypeInfo", DOM_NODE_DEBUG` |
|      - | 10186 | `};` |
|      - | 10187 | `static const char * const azDomCharDebug[] = { DOM_CHARDATA_DEBUG, DOM_NODE_DEBUG };` |
|      - | 10188 | `static const char * const azDomTextDebug[] = {` |
|      - | 10189 | `	"wholeText", DOM_CHARDATA_DEBUG, DOM_NODE_DEBUG` |
|      - | 10190 | `};` |
|      - | 10191 | `static const char * const azDomPiDebug[] = { "target", "data", DOM_NODE_DEBUG };` |
|      - | 10192 | `static const char * const azDomFragDebug[] = {` |
|      - | 10193 | `	"firstElementChild", "lastElementChild", "childElementCount", DOM_NODE_DEBUG` |
|      - | 10194 | `};` |
|      - | 10195 | `static const char * const azDomDocTypeDebug[] = {` |
|      - | 10196 | `	"name", "entities", "notations", "publicId", "systemId", "internalSubset",` |
|      - | 10197 | `	DOM_NODE_DEBUG` |
|      - | 10198 | `};` |
|      - | 10199 | `static const char * const azDomEntityDebug[] = {` |
|      - | 10200 | `	"publicId", "systemId", "notationName", "actualEncoding", "encoding", "version",` |
|      - | 10201 | `	DOM_NODE_DEBUG` |
|      - | 10202 | `};` |
|      - | 10203 | `static const char * const azDomNotationDebug[] = { "publicId", "systemId", DOM_NODE_DEBUG };` |
|      - | 10204 | `static const char * const azDomListDebug[] = { "length" };` |
|      - | 10205 | `static const char * const azDomNsNodeDebug[] = {` |
|      - | 10206 | `	"nodeName", "nodeValue", "nodeType", "prefix", "localName", "namespaceURI",` |
|      - | 10207 | `	"isConnected", "ownerDocument", "parentNode", "parentElement"` |
|      - | 10208 | `};` |
|      - | 10209 | `static const char * const azDomXPathDebug[] = { "document", "registerNodeNamespaces" };` |
|      - | 10210 | `/*` |
|      - | 10211 | ` * The DECLARATIONS behind those names. php declares every one of them on the class` |
|      - | 10212 | `` * -- Reflection lists them, `property_exists()` answers true, `isVirtual()` is true`` |
|      - | 10213 | `` * and `hasDefaultValue()` false -- and keeps no slot for any: PH7_MOD_VIRTUAL is`` |
|      - | 10214 | ` * that pair of facts. Each row states php's own declared type, and the rows are in` |
|      - | 10215 | ` * php's own declaration order, which is the order Reflection reports and (own class` |
|      - | 10216 | ` * first) the order the debug table above walks.` |
|      - | 10217 | ` */` |
|      - | 10218 | `#define DOM_VPROP(NAME,TYPE) \` |
|      - | 10219 | `	{ NAME, PH7_MOD_PUBLIC\|PH7_MOD_VIRTUAL, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, TYPE }` |
|      - | 10220 | `#define DOM_NODE_VPROPS \` |
|      - | 10221 | `	DOM_VPROP("nodeName","string"), \` |
|      - | 10222 | `	DOM_VPROP("nodeValue","?string"), \` |
|      - | 10223 | `	DOM_VPROP("nodeType","int"), \` |
|      - | 10224 | `	DOM_VPROP("parentNode","?DOMNode"), \` |
|      - | 10225 | `	DOM_VPROP("parentElement","?DOMElement"), \` |
|      - | 10226 | `	DOM_VPROP("childNodes","DOMNodeList"), \` |
|      - | 10227 | `	DOM_VPROP("firstChild","?DOMNode"), \` |
|      - | 10228 | `	DOM_VPROP("lastChild","?DOMNode"), \` |
|      - | 10229 | `	DOM_VPROP("previousSibling","?DOMNode"), \` |
|      - | 10230 | `	DOM_VPROP("nextSibling","?DOMNode"), \` |
|      - | 10231 | `	DOM_VPROP("attributes","?DOMNamedNodeMap"), \` |
|      - | 10232 | `	DOM_VPROP("isConnected","bool"), \` |
|      - | 10233 | `	DOM_VPROP("ownerDocument","?DOMDocument"), \` |
|      - | 10234 | `	DOM_VPROP("namespaceURI","?string"), \` |
|      - | 10235 | `	DOM_VPROP("prefix","string"), \` |
|      - | 10236 | `	DOM_VPROP("localName","?string"), \` |
|      - | 10237 | `	DOM_VPROP("baseURI","?string"), \` |
|      - | 10238 | `	DOM_VPROP("textContent","string")` |
|      - | 10239 | `/* php's DOMParentNode trio, declared on each of the three classes that carry it. */` |
|      - | 10240 | `#define DOM_PARENT_VPROPS \` |
|      - | 10241 | `	DOM_VPROP("firstElementChild","?DOMElement"), \` |
|      - | 10242 | `	DOM_VPROP("lastElementChild","?DOMElement"), \` |
|      - | 10243 | `	DOM_VPROP("childElementCount","int")` |
|      - | 10244 | `/* ...and its DOMChildNode pair. */` |
|      - | 10245 | `#define DOM_CHILD_VPROPS \` |
|      - | 10246 | `	DOM_VPROP("previousElementSibling","?DOMElement"), \` |
|      - | 10247 | `	DOM_VPROP("nextElementSibling","?DOMElement")` |
|      - | 10248 | `/*` |
|      - | 10249 | ` * One row per class that HAS a property-handler table -- php's own` |
|      - | 10250 | `` * `dom_xxx_prop_handlers`, which is what its read_property / has_property /`` |
|      - | 10251 | ` * write_property consult before anything else about the object.` |
|      - | 10252 | ` *` |
|      - | 10253 | ` * xRead is the READ handler and xWrite the write one; xDebug is xRead with php's` |
|      - | 10254 | ` * two #[\Deprecated] notices held back, because get_debug_info reads the C` |
|      - | 10255 | ` * function behind the declaration and not the declaration. bNode says the` |
|      - | 10256 | `` * receiver's $__res is an xmlNode, which is what the `ownerDocument` rule may be`` |
|      - | 10257 | ` * asked about -- a namespace declaration carries an xmlNs instead and has no such` |
|      - | 10258 | ` * field to read.` |
|      - | 10259 | ` */` |
|      - | 10260 | `typedef struct DomPropSpec DomPropSpec;` |
|      - | 10261 | `struct DomPropSpec {` |
|      - | 10262 | `	const char *zClass;` |
|      - | 10263 | `	int (*xRead)(ph7_context *,const char *);` |
|      - | 10264 | `	int (*xWrite)(ph7_context *,const char *,ph7_value *,int *);` |
|      - | 10265 | `	int (*xDebug)(ph7_context *,const char *);` |
|      - | 10266 | `	const char * const *azName;` |
|      - | 10267 | `	sxu32 nName;` |
|      - | 10268 | `	int bNode;` |
|      - | 10269 | `};` |
|      - | 10270 | `static const DomPropSpec aDomProp[] = {` |
|      - | 10271 | `	{ "DOMDocument", DomDocProp, DomSetDocProp, DomDocPropQuiet,` |
|      - | 10272 | `	  azDomDocDebug, SX_ARRAYSIZE(azDomDocDebug), 1 },` |
|      - | 10273 | `	{ "DOMElement", DomElemProp, DomSetElemProp, DomElemProp,` |
|      - | 10274 | `	  azDomElemDebug, SX_ARRAYSIZE(azDomElemDebug), 1 },` |
|      - | 10275 | `	{ "DOMAttr", DomAttrProp, DomSetAttrProp, DomAttrProp,` |
|      - | 10276 | `	  azDomAttrDebug, SX_ARRAYSIZE(azDomAttrDebug), 1 },` |
|      - | 10277 | `	{ "DOMText", DomTextProp, DomSetCharProp, DomTextProp,` |
|      - | 10278 | `	  azDomTextDebug, SX_ARRAYSIZE(azDomTextDebug), 1 },` |
|      - | 10279 | `	{ "DOMCharacterData", DomCharProp, DomSetCharProp, DomCharProp,` |
|      - | 10280 | `	  azDomCharDebug, SX_ARRAYSIZE(azDomCharDebug), 1 },` |
|      - | 10281 | `	{ "DOMProcessingInstruction", DomPiProp, DomSetPiProp, DomPiProp,` |
|      - | 10282 | `	  azDomPiDebug, SX_ARRAYSIZE(azDomPiDebug), 1 },` |
|      - | 10283 | `	/* The fragment writes what DOMNode writes; only its READ set is wider. */` |
|      - | 10284 | `	{ "DOMDocumentFragment", DomFragProp, DomSetNodeProp, DomFragProp,` |
|      - | 10285 | `	  azDomFragDebug, SX_ARRAYSIZE(azDomFragDebug), 1 },` |
|      - | 10286 | `	/* The DTD half's OWN properties are all read-only, so its writer states none` |
|      - | 10287 | `	 * of them and each lands on the hook's readonly Error. DOMNode's three still` |
|      - | 10288 | ``	 * write here -- `nodeValue` and `textContent` are accepted and ignored on a`` |
|      - | 10289 | `	 * doctype, which is not the same answer as refusing them. */` |
|      - | 10290 | `	{ "DOMDocumentType", DomDocTypeProp, DomSetNodeProp, DomDocTypeProp,` |
|      - | 10291 | `	  azDomDocTypeDebug, SX_ARRAYSIZE(azDomDocTypeDebug), 1 },` |
|      - | 10292 | `	{ "DOMEntity", DomEntityProp, DomSetEntityProp, DomEntityPropQuiet,` |
|      - | 10293 | `	  azDomEntityDebug, SX_ARRAYSIZE(azDomEntityDebug), 1 },` |
|      - | 10294 | `	{ "DOMNotation", DomNotationProp, DomSetNotationProp, DomNotationProp,` |
|      - | 10295 | `	  azDomNotationDebug, SX_ARRAYSIZE(azDomNotationDebug), 1 },` |
|      - | 10296 | `	{ "DOMNodeList", DomListProp, DomSetNothing, DomListProp,` |
|      - | 10297 | `	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },` |
|      - | 10298 | `	{ "DOMNamedNodeMap", DomMapProp, DomSetNothing, DomMapProp,` |
|      - | 10299 | `	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },` |
|      - | 10300 | `	{ "DOMNameSpaceNode", DomNsNodeProp, DomSetNothing, DomNsNodeProp,` |
|      - | 10301 | `	  azDomNsNodeDebug, SX_ARRAYSIZE(azDomNsNodeDebug), 0 },` |
|      - | 10302 | `	{ "DOMXPath", DomXPathProp, DomSetXPathProp, DomXPathProp,` |
|      - | 10303 | `	  azDomXPathDebug, SX_ARRAYSIZE(azDomXPathDebug), 0 },` |
|      - | 10304 | `	/* DOMComment, DOMCdataSection and DOMEntityReference name no row of their own:` |
|      - | 10305 | `	 * they declare no property php's table does not already carry, so the base-chain` |
|      - | 10306 | `	 * walk below reaches their parent's -- which is php's answer for them too. */` |
|      - | 10307 | `	{ "DOMNode", DomNodeProp, DomSetNodeProp, DomNodeProp,` |
|      - | 10308 | `	  azDomNodeDebug, SX_ARRAYSIZE(azDomNodeDebug), 1 }` |
|      - | 10309 | `};` |
|      - | 10310 | `/* The nearest ancestor with a handler table -- php's own lookup, which is why a` |
|      - | 10311 | ` * userland subclass of DOMElement shows DOMElement's twenty-seven. */` |
|   8967 | 10312 | `static const DomPropSpec * DomPropSpecOf(ph7_class *pClass)` |
|      5 | 10313 | `{` |
|   9272 | 10314 | `	for( ; pClass ; pClass = pClass->pBase ){` |
|   9272 | 10315 | `		sxu32 nName = SyStringLength(&pClass->sName);` |
|      - | 10316 | `		sxu32 i;` |
|  32102 | 10317 | `		for( i = 0 ; i < SX_ARRAYSIZE(aDomProp) ; ++i ){` |
|  31797 | 10318 | `			if( SyStrlen(aDomProp[i].zClass) == nName` |
|  21269 | 10319 | `			 && SyStrncmp(SyStringData(&pClass->sName),aDomProp[i].zClass,nName) == 0 ){` |
|   8972 | 10320 | `				return &aDomProp[i];` |
|      - | 10321 | `			}` |
|  11420 | 10322 | `		}` |
|    152 | 10323 | `	}` |
|    ! 0 | 10324 | `	return 0;` |
|   4489 | 10325 | `}` |
|      - | 10326 | `/*` |
|      - | 10327 | ` * php's read_property / has_property / write_property for every DOM class, as` |
|      - | 10328 | ` * ph7_class::xProp.` |
|      - | 10329 | ` *` |
|      - | 10330 | `` * ext/dom has no `__get`/`__set`/`__isset` anywhere: each class carries a table`` |
|      - | 10331 | ` * of property handlers and php's object handlers consult it FIRST, so a name the` |
|      - | 10332 | ` * table holds is answered by the handler and only a name it does NOT hold falls` |
|      - | 10333 | ` * through to the standard path -- which is where a subclass's own magic accessor` |
|      - | 10334 | ` * finally gets a say. Routing the surface through the magic trio had the order` |
|      - | 10335 | `` * exactly backwards: a subclass that wrote `__get` without delegating replaced`` |
|      - | 10336 | ` * the whole DOM surface for its instances, and every DOM class carried three` |
|      - | 10337 | ` * methods php does not.` |
|      - | 10338 | ` *` |
|      - | 10339 | ` * The recognizers below ARE the table: one per class, answering 1 for a name it` |
|      - | 10340 | ` * knows. A name none of them knows leaves bAnswered at 0, and the member opcode` |
|      - | 10341 | ` * takes the ordinary path from there -- php's own fall-through, undefined-property` |
|      - | 10342 | ` * warning and all.` |
|      - | 10343 | ` *` |
|      - | 10344 | ` * UNSET is not answered here. php's unset_property for a virtual property has` |
|      - | 10345 | `` * nothing to remove and refuses with `Cannot unset C::$p`, which the opcode`` |
|      - | 10346 | ` * already words off the declaration itself; declining leaves that answer, and` |
|      - | 10347 | ` * leaves a name the class does NOT declare on the __unset path php sends it to.` |
|      - | 10348 | ` *` |
|      - | 10349 | ` * Neither is WRITE, the member opcode's question -- it is asked before the value` |
|      - | 10350 | ` * exists, and a handler that really STORES needs it. The opcode recognizes the` |
|      - | 10351 | ` * name as this table's (PH7_ClassNativePropOwns) and routes the write to STORE` |
|      - | 10352 | ` * below, at the point php's write_property gets its zval.` |
|      - | 10353 | ` */` |
|   8959 | 10354 | `static void DomPropHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|      5 | 10355 | `{` |
|   8964 | 10356 | `	const DomPropSpec *pSpec = pThis ? DomPropSpecOf(pThis->pClass) : 0;` |
|   8964 | 10357 | `	sxu32 nName = SyStringLength(pCtx->pName);` |
|      - | 10358 | `	ph7_context sCtx;` |
|      - | 10359 | `	ph7_value sVal;` |
|      - | 10360 | `	char zName[128];` |
|      - | 10361 | `	int bKnown;` |
|   8959 | 10362 | `	if( pSpec == 0` |
|   8964 | 10363 | `	 \|\| pCtx->iMode == PH7_NATIVE_PROP_UNSET \|\| pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|   1807 | 10364 | `		return;` |
|      - | 10365 | `	}` |
|   8248 | 10366 | `	if( nName >= sizeof(zName) ){` |
|      - | 10367 | `		/* Longer than any name ext/dom declares: the ordinary path owns it. */` |
|    ! 0 | 10368 | `		return;` |
|      - | 10369 | `	}` |
|   8248 | 10370 | `	SyMemcpy(SyStringData(pCtx->pName),zName,nName);` |
|   8248 | 10371 | `	zName[nName] = 0;` |
|   8248 | 10372 | `	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){` |
|      - | 10373 | `		/* php's table membership, which for ext/dom is the DECLARATION: every one` |
|      - | 10374 | `		 * of its ninety names is declared virtual on the class, and nothing else` |
|      - | 10375 | `		 * is the handler's. Answered without running a recognizer -- a write shape` |
|      - | 10376 | `		 * asks this before the value exists, and the deprecated names would raise` |
|      - | 10377 | `		 * their notice on a read the program has not made. */` |
|   1434 | 10378 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,zName,nName);` |
|   1434 | 10379 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|   1426 | 10380 | `			pCtx->bAnswered = 1;` |
|    712 | 10381 | `		}` |
|   1434 | 10382 | `		return;` |
|      - | 10383 | `	}` |
|      - | 10384 | `	/* The recognizers answer by WRITING a result, so the scratch context carries` |
|      - | 10385 | `	 * one slot of its own -- and the refusal channel, which is what stops a` |
|      - | 10386 | `	 * readonly Error or a DOMException from running the enclosing catch in the` |
|      - | 10387 | `	 * middle of this access. */` |
|   6816 | 10388 | `	PH7_MemObjInit(pVm,&sVal);` |
|   6816 | 10389 | `	VmInitCallContext(&sCtx,pVm,0,&sVal,0);` |
|   6816 | 10390 | `	sCtx.pThis = pThis;` |
|   6816 | 10391 | `	sCtx.pCalledClass = pThis->pClass;` |
|   6816 | 10392 | `	sCtx.pPropCtx = pCtx;` |
|   6816 | 10393 | `	if( pCtx->iMode == PH7_NATIVE_PROP_STORE ){` |
|      - | 10394 | `		/* php's write_property, with the value. A name the WRITER does not state` |
|      - | 10395 | `		 * is read-only when the class declares it at all -- and the refusal is` |
|      - | 10396 | `		 * php's own, raised where the store would have landed -- and nothing this` |
|      - | 10397 | `		 * class knows otherwise, which puts the write back on the ordinary path. */` |
|    712 | 10398 | `		int rcW = PH7_OK;` |
|    712 | 10399 | `		if( pSpec->xWrite(&sCtx,zName,pCtx->pResult,&rcW) == DOM_SET_DONE ){` |
|    636 | 10400 | `			pCtx->bAnswered = 1;` |
|    636 | 10401 | `			if( pCtx->zThrowClass == 0 && DomNodeLess(&sCtx,zName) ){` |
|      - | 10402 | `				/* A writable name on an object with no node behind it: php's` |
|      - | 10403 | `				 * handler reaches its DOM_GET_OBJ and answers Invalid State --` |
|      - | 10404 | `				 * AFTER the declared type has had its say, which is why a` |
|      - | 10405 | `				 * TypeError already recorded stands. */` |
|      3 | 10406 | `				DomPropRefuse(&sCtx,"DOMException",DOM_ERR_INVALID_STATE,` |
|      1 | 10407 | `					DomErrText(DOM_ERR_INVALID_STATE));` |
|      3 | 10408 | `			}` |
|    394 | 10409 | `		}else if( pSpec->xRead(&sCtx,zName) ){` |
|     77 | 10410 | `			pCtx->bAnswered = 1;` |
|     77 | 10411 | `			DomRefuseWrite(&sCtx,zName);` |
|     38 | 10412 | `		}` |
|    712 | 10413 | `		VmReleaseCallContext(&sCtx);` |
|    712 | 10414 | `		PH7_MemObjRelease(&sVal);` |
|    712 | 10415 | `		return;` |
|      - | 10416 | `	}` |
|   6106 | 10417 | `	if( DomNodeLess(&sCtx,zName) ){` |
|      - | 10418 | `		/* A name the class DECLARES, on an object libxml gave no node -- every` |
|      - | 10419 | `		 * handler would fetch a null pointer, and php answers Invalid State on a` |
|      - | 10420 | `		 * read and on an isset() alike. */` |
|     37 | 10421 | `		DomPropRefuse(&sCtx,"DOMException",DOM_ERR_INVALID_STATE,` |
|     18 | 10422 | `			DomErrText(DOM_ERR_INVALID_STATE));` |
|     37 | 10423 | `		VmReleaseCallContext(&sCtx);` |
|     37 | 10424 | `		PH7_MemObjRelease(&sVal);` |
|     37 | 10425 | `		return;` |
|      - | 10426 | `	}` |
|   6070 | 10427 | `	bKnown = pSpec->xRead(&sCtx,zName) != 0;` |
|   6070 | 10428 | `	if( bKnown && pCtx->zThrowClass == 0 ){` |
|   5960 | 10429 | `		pCtx->bAnswered = 1;` |
|   5960 | 10430 | `		if( pCtx->iMode == PH7_NATIVE_PROP_READ ){` |
|   5880 | 10431 | `			PH7_MemObjStore(&sVal,pCtx->pResult);` |
|   2943 | 10432 | `		}else{` |
|      - | 10433 | `			/* php's has_property fetches the value and judges it: by NULL-ness for` |
|      - | 10434 | ``			 * isset() (`isset($n->nextSibling)` is false on a last child while`` |
|      - | 10435 | ``			 * `isset($n->nodeName)` is true) and by TRUTH for the two check_empty`` |
|      - | 10436 | `			 * questions -- ext/dom's handler makes no distinction between them. */` |
|    121 | 10437 | `			int bSet = pCtx->iMode == PH7_NATIVE_PROP_ISSET` |
|     76 | 10438 | `				? (sVal.iFlags & MEMOBJ_NULL) == 0` |
|     42 | 10439 | `				: ph7_value_to_bool(&sVal);` |
|     81 | 10440 | `			PH7_MemObjRelease(pCtx->pResult);` |
|     81 | 10441 | `			ph7_value_bool(pCtx->pResult,bSet);` |
|      - | 10442 | `		}` |
|   2978 | 10443 | `	}` |
|   6070 | 10444 | `	VmReleaseCallContext(&sCtx);` |
|   6070 | 10445 | `	PH7_MemObjRelease(&sVal);` |
|   4485 | 10446 | `}` |
|      - | 10447 | ``/* php's `dom_node_owner_document_read` answers FAILURE -- and the debug walk then`` |
|      - | 10448 | `` * writes no row -- for a node libxml gave no document: one built with `new`, and`` |
|      - | 10449 | ` * the entity-shaped stand-in a NOTATION declaration is read through. A DOCUMENT` |
|      - | 10450 | ` * itself answers null and keeps its row. */` |
|    244 | 10451 | `static int DomDebugSkip(ph7_class_instance *pThis,const char *zName,int bNode)` |
|      1 | 10452 | `{` |
|      - | 10453 | `	phl_domnode *pNd;` |
|      - | 10454 | `	xmlNodePtr pNode;` |
|    245 | 10455 | `	if( !bNode \|\| !DomNameIs(zName,"ownerDocument") ){` |
|    239 | 10456 | `		return 0;` |
|      - | 10457 | `	}` |
|      7 | 10458 | `	pNd = DomResOf(pThis);` |
|      7 | 10459 | `	pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      6 | 10460 | `	if( pNode == 0` |
|      7 | 10461 | `	 \|\| pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|      7 | 10462 | `		return 0;` |
|      - | 10463 | `	}` |
|    ! 0 | 10464 | `	return pNode->doc == 0;` |
|    123 | 10465 | `}` |
|     24 | 10466 | `static sxi32 DomPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 10467 | `{` |
|      - | 10468 | `	const DomPropSpec *pSpec;` |
|      - | 10469 | `	ph7_context sCtx;` |
|      - | 10470 | `	ph7_value sVal,sKey;` |
|      - | 10471 | `	sxu32 i;` |
|     25 | 10472 | `	if( !bDebug ){` |
|      - | 10473 | `		/* php's get_properties for a DOM object is the object's own table -- which` |
|      - | 10474 | `		 * for anything but a subclass is empty -- so the engine's slot walk IS the` |
|      - | 10475 | `		 * answer and this hook has nothing to add. */` |
|     17 | 10476 | `		return SXERR_NOTFOUND;` |
|      - | 10477 | `	}` |
|      9 | 10478 | `	pSpec = pThis ? DomPropSpecOf(pThis->pClass) : 0;` |
|      9 | 10479 | `	if( pSpec == 0 \|\| (pOut->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 10480 | `		return SXERR_NOTFOUND;` |
|      - | 10481 | `	}` |
|      9 | 10482 | `	if( pSpec->bNode && PH7_NativeAttr(pThis,DOM_RES) != 0 ){` |
|      7 | 10483 | `		phl_domnode *pNd = DomResOf(pThis);` |
|      7 | 10484 | `		if( pNd == 0 \|\| pNd->pNode == 0 ){` |
|      - | 10485 | ``			/* An object with no node behind it (one built with `new`): every handler`` |
|      - | 10486 | `			 * would refuse, so the walk contributes nothing and the object shows its` |
|      - | 10487 | `			 * real table alone. php's dump THROWS out of the debug handler here` |
|      - | 10488 | `			 * instead, after printing the header. */` |
|    ! 0 | 10489 | `			return SXERR_NOTFOUND;` |
|      - | 10490 | `		}` |
|      3 | 10491 | `	}` |
|      - | 10492 | `	/* A subclass's own properties come FIRST and under php's mangled keys, which` |
|      - | 10493 | ``	 * is what prints `[p:MyDoc:private]` beside the fabricated rows. */`` |
|      9 | 10494 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pOut->x.pOther);` |
|      9 | 10495 | `	PH7_MemObjInit(pVm,&sVal);` |
|      - | 10496 | `	/* One scratch call context for the whole walk: the recognizers answer by` |
|      - | 10497 | `	 * WRITING a result, and rule 54 says a second call on the same context would` |
|      - | 10498 | `	 * append to the first answer -- so the slot is blanked between rows. */` |
|      9 | 10499 | `	VmInitCallContext(&sCtx,pVm,0,&sVal,0);` |
|      9 | 10500 | `	sCtx.pThis = pThis;` |
|      9 | 10501 | `	sCtx.pCalledClass = pThis->pClass;` |
|    253 | 10502 | `	for( i = 0 ; i < pSpec->nName ; ++i ){` |
|    245 | 10503 | `		const char *zName = pSpec->azName[i];` |
|    245 | 10504 | `		if( DomDebugSkip(pThis,zName,pSpec->bNode) ){` |
|    ! 0 | 10505 | `			continue;` |
|      - | 10506 | `		}` |
|    245 | 10507 | `		PH7_MemObjRelease(&sVal);` |
|    245 | 10508 | `		PH7_MemObjInit(pVm,&sVal);` |
|    245 | 10509 | `		if( pSpec->xDebug(&sCtx,zName) == 0 ){` |
|    ! 0 | 10510 | `			continue;` |
|      - | 10511 | `		}` |
|    245 | 10512 | `		if( sVal.iFlags & MEMOBJ_OBJ ){` |
|      - | 10513 | `			/* php prints the literal rather than the object: a document would` |
|      - | 10514 | ``			 * otherwise dump its whole tree through `documentElement`. */`` |
|      - | 10515 | `			ph7_value sOmit;` |
|     15 | 10516 | `			PH7_MemObjInitFromString(pVm,&sOmit,0);` |
|     15 | 10517 | `			PH7_MemObjStringAppend(&sOmit,"(object value omitted)",` |
|      - | 10518 | `				sizeof("(object value omitted)")-1);` |
|     15 | 10519 | `			PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     15 | 10520 | `			PH7_MemObjStringAppend(&sKey,zName,(sxu32)SyStrlen(zName));` |
|     15 | 10521 | `			ph7_array_add_elem(pOut,&sKey,&sOmit);` |
|     15 | 10522 | `			PH7_MemObjRelease(&sKey);` |
|     15 | 10523 | `			PH7_MemObjRelease(&sOmit);` |
|     15 | 10524 | `			continue;` |
|      - | 10525 | `		}` |
|    231 | 10526 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    231 | 10527 | `		PH7_MemObjStringAppend(&sKey,zName,(sxu32)SyStrlen(zName));` |
|    231 | 10528 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    231 | 10529 | `		PH7_MemObjRelease(&sKey);` |
|    116 | 10530 | `	}` |
|      9 | 10531 | `	VmReleaseCallContext(&sCtx);` |
|      9 | 10532 | `	PH7_MemObjRelease(&sVal);` |
|      9 | 10533 | `	return SXRET_OK;` |
|     13 | 10534 | `}` |
|      - | 10535 | `/*` |
|      - | 10536 | ` * Install the DOM library: every class declared from C, no embedded chunk and` |
|      - | 10537 | ` * no globally visible thunk left.  Called from PH7_VmInit inside the` |
|      - | 10538 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml (the capture plumbing must` |
|      - | 10539 | ` * exist) and after the Reflection install (DOMException needs Exception).` |
|      - | 10540 | ` */` |
|   7925 | 10541 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)` |
|      5 | 10542 | `{` |
|      - | 10543 | `	/* php's eighteen DOMNode properties -- all VIRTUAL there, so the object holds` |
|      - | 10544 | `	 * none of them and every one is answered by DomNodeProp -- followed by the two` |
|      - | 10545 | `	 * slots every wrapper really carries and the identity cache, which are PHL's` |
|      - | 10546 | `	 * own storage and hidden from every surface. */` |
|      - | 10547 | `	static const PH7_NativePropDef aNodeProp[] = {` |
|      - | 10548 | `		DOM_NODE_VPROPS,` |
|      - | 10549 | `		{ DOM_RES, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10550 | `		{ DOM_DOC, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10551 | `		/* The identity cache. Only a DOCUMENT'S is a document's; a CONSTRUCTED` |
|      - | 10552 | `		 * ownerless node is its own holder (its $__doc points at itself) and` |
|      - | 10553 | `		 * caches its tree's wrappers HERE until an insertion adopts them into` |
|      - | 10554 | `		 * a real document's cache. Empty and unread on every owned node. */` |
|      - | 10555 | `		{ DOM_NODES, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10556 | `	};` |
|      - | 10557 | ``	/* php's own signatures. Declaring `DOMNode $node` is what makes`` |
|      - | 10558 | ``	 * `$n->appendChild(1)` the TypeError php raises instead of a warning from`` |
|      - | 10559 | `	 * reading ->__res off an int. */` |
|      - | 10560 | `	static const PH7_NativeMethodDef aNodeMethod[] = {` |
|      - | 10561 | `		{ "appendChild",    PH7_MOD_PUBLIC, "DOMNode $node", "", vm_builtin_DOMNode_appendChild },` |
|      - | 10562 | `		{ "insertBefore",   PH7_MOD_PUBLIC, "DOMNode $node, ?DOMNode $child = null", "",` |
|      - | 10563 | `		  vm_builtin_DOMNode_insertBefore },` |
|      - | 10564 | `		{ "removeChild",    PH7_MOD_PUBLIC, "DOMNode $child", "", vm_builtin_DOMNode_removeChild },` |
|      - | 10565 | `		{ "replaceChild",   PH7_MOD_PUBLIC, "DOMNode $node, DOMNode $child", "",` |
|      - | 10566 | `		  vm_builtin_DOMNode_replaceChild },` |
|      - | 10567 | `		{ "hasChildNodes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasChildNodes },` |
|      - | 10568 | `		{ "hasAttributes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasAttributes },` |
|      - | 10569 | `		{ "isSameNode",     PH7_MOD_PUBLIC, "DOMNode $otherNode", "@bool", vm_builtin_DOMNode_isSameNode },` |
|      - | 10570 | `		/* php declares no return type at all on this one, not even a tentative` |
|      - | 10571 | `		 * one, so the row states none either. */` |
|      - | 10572 | `		{ "cloneNode",      PH7_MOD_PUBLIC, "bool $deep = false", "", vm_builtin_DOMNode_cloneNode },` |
|      - | 10573 | `		/* php runs the same walk normalizeDocument() does, from the receiver. */` |
|      - | 10574 | `		{ "normalize",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },` |
|      - | 10575 | `		{ "getNodePath",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_DOMNode_getNodePath },` |
|      - | 10576 | `		{ "isEqualNode",    PH7_MOD_PUBLIC, "?DOMNode $otherNode", "bool",` |
|      - | 10577 | `		  vm_builtin_DOMNode_isEqualNode },` |
|      - | 10578 | `		{ "isSupported",    PH7_MOD_PUBLIC, "string $feature, string $version", "@bool",` |
|      - | 10579 | `		  vm_builtin_DOMNode_isSupported },` |
|      - | 10580 | `		/* php's declared type names DOMNameSpaceNode, a class PHL does not have;` |
|      - | 10581 | `		 * the row states it anyway so Reflection reports php's, and nothing can` |
|      - | 10582 | `		 * be handed one. (php's own zpp rejects a NON-object here with a` |
|      - | 10583 | `		 * "?object" message instead -- recorded, the error-format class.) */` |
|      - | 10584 | `		{ "contains",       PH7_MOD_PUBLIC, "DOMNode\|DOMNameSpaceNode\|null $other", "bool",` |
|      - | 10585 | `		  vm_builtin_DOMNode_contains },` |
|      - | 10586 | `		{ "getRootNode",    PH7_MOD_PUBLIC, "?array $options = null", "DOMNode",` |
|      - | 10587 | `		  vm_builtin_DOMNode_getRootNode },` |
|      - | 10588 | `		{ "compareDocumentPosition", PH7_MOD_PUBLIC, "DOMNode $other", "int",` |
|      - | 10589 | `		  vm_builtin_DOMNode_compareDocumentPosition },` |
|      - | 10590 | `		{ "getLineNo",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNode_getLineNo },` |
|      - | 10591 | `		{ "C14N",           PH7_MOD_PUBLIC,` |
|      - | 10592 | `		  "bool $exclusive = false, bool $withComments = false, ?array $xpath = null, "` |
|      - | 10593 | `		  "?array $nsPrefixes = null", "@string\|false", vm_builtin_DOMNode_C14N },` |
|      - | 10594 | `		{ "C14NFile",       PH7_MOD_PUBLIC,` |
|      - | 10595 | `		  "string $uri, bool $exclusive = false, bool $withComments = false, "` |
|      - | 10596 | `		  "?array $xpath = null, ?array $nsPrefixes = null", "@int\|false",` |
|      - | 10597 | `		  vm_builtin_DOMNode_C14NFile },` |
|      - | 10598 | `		/* The refusal MACHINERY, not decoration: serialize() finds __sleep and` |
|      - | 10599 | `		 * unserialize() calls __wakeup, so a subclass declaring either escapes. */` |
|      - | 10600 | `		{ "__sleep",        PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },` |
|      - | 10601 | `		{ "__wakeup",       PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },` |
|      - | 10602 | `		{ "lookupNamespaceURI", PH7_MOD_PUBLIC, "?string $prefix", "@?string",` |
|      - | 10603 | `		  vm_builtin_DOMNode_lookupNamespaceURI },` |
|      - | 10604 | `		{ "lookupPrefix",   PH7_MOD_PUBLIC, "string $namespace", "@?string",` |
|      - | 10605 | `		  vm_builtin_DOMNode_lookupPrefix },` |
|      - | 10606 | `		{ "isDefaultNamespace", PH7_MOD_PUBLIC, "string $namespace", "@bool",` |
|      - | 10607 | `		  vm_builtin_DOMNode_isDefaultNamespace },` |
|      - | 10608 | `	};` |
|      - | 10609 | `	/* php declares the six on DOMNode; every node class inherits them. */` |
|      - | 10610 | `	static const PH7_NativeConstDef aNodeConst[] = {` |
|      - | 10611 | `		{ "DOCUMENT_POSITION_DISCONNECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10612 | `		  DOM_POS_DISCONNECTED, 0, 0.0 },` |
|      - | 10613 | `		{ "DOCUMENT_POSITION_PRECEDING", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10614 | `		  DOM_POS_PRECEDING, 0, 0.0 },` |
|      - | 10615 | `		{ "DOCUMENT_POSITION_FOLLOWING", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10616 | `		  DOM_POS_FOLLOWING, 0, 0.0 },` |
|      - | 10617 | `		{ "DOCUMENT_POSITION_CONTAINS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10618 | `		  DOM_POS_CONTAINS, 0, 0.0 },` |
|      - | 10619 | `		{ "DOCUMENT_POSITION_CONTAINED_BY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10620 | `		  DOM_POS_CONTAINED_BY, 0, 0.0 },` |
|      - | 10621 | `		{ "DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10622 | `		  DOM_POS_IMPL_SPEC, 0, 0.0 },` |
|      - | 10623 | `	};` |
|      - | 10624 | `	/* php's twenty-two, in its own declaration order. Nine are the XML declaration` |
|      - | 10625 | `	 * and the document's URI read straight off libxml, three are the DOMParentNode` |
|      - | 10626 | `	 * trio, and seven are the directives DOM_DFLAGS holds -- none of them a slot,` |
|      - | 10627 | ``	 * which is what makes php's `(array)$doc` an EMPTY array. */`` |
|      - | 10628 | `	static const PH7_NativePropDef aDocProp[] = {` |
|      - | 10629 | `		DOM_VPROP("doctype","?DOMDocumentType"),` |
|      - | 10630 | `		DOM_VPROP("implementation","DOMImplementation"),` |
|      - | 10631 | `		DOM_VPROP("documentElement","?DOMElement"),` |
|      - | 10632 | `		DOM_VPROP("actualEncoding","?string"),` |
|      - | 10633 | `		DOM_VPROP("encoding","?string"),` |
|      - | 10634 | `		DOM_VPROP("xmlEncoding","?string"),` |
|      - | 10635 | `		DOM_VPROP("standalone","bool"),` |
|      - | 10636 | `		DOM_VPROP("xmlStandalone","bool"),` |
|      - | 10637 | `		DOM_VPROP("version","?string"),` |
|      - | 10638 | `		DOM_VPROP("xmlVersion","?string"),` |
|      - | 10639 | `		DOM_VPROP("strictErrorChecking","bool"),` |
|      - | 10640 | `		DOM_VPROP("documentURI","?string"),` |
|      - | 10641 | `		DOM_VPROP("config","mixed"),` |
|      - | 10642 | `		DOM_VPROP("formatOutput","bool"),` |
|      - | 10643 | `		DOM_VPROP("validateOnParse","bool"),` |
|      - | 10644 | `		DOM_VPROP("resolveExternals","bool"),` |
|      - | 10645 | `		DOM_VPROP("preserveWhiteSpace","bool"),` |
|      - | 10646 | `		DOM_VPROP("recover","bool"),` |
|      - | 10647 | `		DOM_VPROP("substituteEntities","bool"),` |
|      - | 10648 | `		DOM_PARENT_VPROPS,` |
|      - | 10649 | `		/* The seven directives, as the one word that holds them. */` |
|      - | 10650 | `		{ DOM_DFLAGS,           PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN,` |
|      - | 10651 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DOM_F_DEFAULT, 0, 0.0 }, 0 },` |
|      - | 10652 | `		/* The identity cache DomWrap keys by node pointer. */` |
|      - | 10653 | `		{ DOM_NODES,            PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10654 | `		/* ...and the base-class => user-class table registerNodeClass writes. */` |
|      - | 10655 | `		{ DOM_NCLS,             PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10656 | `	};` |
|      - | 10657 | `	static const PH7_NativeMethodDef aDocMethod[] = {` |
|      - | 10658 | `		{ "__construct",          PH7_MOD_PUBLIC, "string $version = '1.0', string $encoding = ''", "",` |
|      - | 10659 | `		  vm_builtin_DOMDocument_construct },` |
|      - | 10660 | `		{ "loadXML",              PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",` |
|      - | 10661 | `		  vm_builtin_DOMDocument_loadXML },` |
|      - | 10662 | `		{ "load",                 PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@bool",` |
|      - | 10663 | `		  vm_builtin_DOMDocument_load },` |
|      - | 10664 | `		{ "save",                 PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@int\|false",` |
|      - | 10665 | `		  vm_builtin_DOMDocument_save },` |
|      - | 10666 | `		{ "loadHTML",             PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",` |
|      - | 10667 | `		  vm_builtin_DOMDocument_loadHTML },` |
|      - | 10668 | `		{ "loadHTMLFile",         PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@bool",` |
|      - | 10669 | `		  vm_builtin_DOMDocument_loadHTMLFile },` |
|      - | 10670 | `		{ "saveHTML",             PH7_MOD_PUBLIC, "?DOMNode $node = null", "@string\|false",` |
|      - | 10671 | `		  vm_builtin_DOMDocument_saveHTML },` |
|      - | 10672 | `		{ "saveHTMLFile",         PH7_MOD_PUBLIC, "string $filename", "@int\|false",` |
|      - | 10673 | `		  vm_builtin_DOMDocument_saveHTMLFile },` |
|      - | 10674 | `		{ "saveXML",              PH7_MOD_PUBLIC, "?DOMNode $node = null, int $options = 0", "@string\|false",` |
|      - | 10675 | `		  vm_builtin_DOMDocument_saveXML },` |
|      - | 10676 | `		{ "createElement",        PH7_MOD_PUBLIC, "string $localName, string $value = ''", "",` |
|      - | 10677 | `		  vm_builtin_DOMDocument_createElement },` |
|      - | 10678 | `		/* php declares no return type at all on this one either -- its answer is` |
|      - | 10679 | ``		 * `DOMElement\|false` and it never wrote that down. */`` |
|      - | 10680 | `		{ "createElementNS",      PH7_MOD_PUBLIC,` |
|      - | 10681 | `		  "?string $namespace, string $qualifiedName, string $value = ''", "",` |
|      - | 10682 | `		  vm_builtin_DOMDocument_createElementNS },` |
|      - | 10683 | ``		/* php declares no return type on this one either: `DOMNode\|false`. */`` |
|      - | 10684 | `		{ "importNode",           PH7_MOD_PUBLIC, "DOMNode $node, bool $deep = false", "",` |
|      - | 10685 | `		  vm_builtin_DOMDocument_importNode },` |
|      - | 10686 | `		{ "adoptNode",            PH7_MOD_PUBLIC, "DOMNode $node", "@DOMNode\|false",` |
|      - | 10687 | `		  vm_builtin_DOMDocument_adoptNode },` |
|      - | 10688 | `		{ "getElementById",       PH7_MOD_PUBLIC, "string $elementId", "@?DOMElement",` |
|      - | 10689 | `		  vm_builtin_DOMDocument_getElementById },` |
|      - | 10690 | `		{ "createAttribute",      PH7_MOD_PUBLIC, "string $localName", "",` |
|      - | 10691 | `		  vm_builtin_DOMDocument_createAttribute },` |
|      - | 10692 | `		{ "createAttributeNS",    PH7_MOD_PUBLIC, "?string $namespace, string $qualifiedName", "",` |
|      - | 10693 | `		  vm_builtin_DOMDocument_createAttributeNS },` |
|      - | 10694 | `		{ "createTextNode",       PH7_MOD_PUBLIC, "string $data", "@DOMText",` |
|      - | 10695 | `		  vm_builtin_DOMDocument_createTextNode },` |
|      - | 10696 | `		{ "createComment",        PH7_MOD_PUBLIC, "string $data", "@DOMComment",` |
|      - | 10697 | `		  vm_builtin_DOMDocument_createComment },` |
|      - | 10698 | `		{ "createCDATASection",   PH7_MOD_PUBLIC, "string $data", "",` |
|      - | 10699 | `		  vm_builtin_DOMDocument_createCDATASection },` |
|      - | 10700 | `		{ "createProcessingInstruction", PH7_MOD_PUBLIC, "string $target, string $data = ''", "",` |
|      - | 10701 | `		  vm_builtin_DOMDocument_createPI },` |
|      - | 10702 | `		{ "createEntityReference", PH7_MOD_PUBLIC, "string $name", "",` |
|      - | 10703 | `		  vm_builtin_DOMDocument_createEntityRef },` |
|      - | 10704 | `		{ "createDocumentFragment", PH7_MOD_PUBLIC, "", "@DOMDocumentFragment",` |
|      - | 10705 | `		  vm_builtin_DOMDocument_createFragment },` |
|      - | 10706 | `		{ "normalizeDocument",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },` |
|      - | 10707 | `		{ "registerNodeClass",    PH7_MOD_PUBLIC, "string $baseClass, ?string $extendedClass",` |
|      - | 10708 | `		  "@true", vm_builtin_DOMDocument_registerNodeClass },` |
|      - | 10709 | `		{ "schemaValidate",       PH7_MOD_PUBLIC, "string $filename, int $flags = 0", "@bool",` |
|      - | 10710 | `		  vm_builtin_DOMDocument_schemaValidate },` |
|      - | 10711 | `		{ "schemaValidateSource", PH7_MOD_PUBLIC, "string $source, int $flags = 0", "@bool",` |
|      - | 10712 | `		  vm_builtin_DOMDocument_schemaValidateSource },` |
|      - | 10713 | `		/* php declares no option word on the RelaxNG pair at all. */` |
|      - | 10714 | `		{ "relaxNGValidate",       PH7_MOD_PUBLIC, "string $filename", "@bool",` |
|      - | 10715 | `		  vm_builtin_DOMDocument_relaxNGValidate },` |
|      - | 10716 | `		{ "relaxNGValidateSource", PH7_MOD_PUBLIC, "string $source", "@bool",` |
|      - | 10717 | `		  vm_builtin_DOMDocument_relaxNGValidateSource },` |
|      - | 10718 | `		{ "validate",             PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 10719 | `		  vm_builtin_DOMDocument_validate },` |
|      - | 10720 | `		{ "xinclude",             PH7_MOD_PUBLIC, "int $options = 0", "@int\|false",` |
|      - | 10721 | `		  vm_builtin_DOMDocument_xinclude },` |
|      - | 10722 | `		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",` |
|      - | 10723 | `		  vm_builtin_Dom_getElementsByTagName },` |
|      - | 10724 | `		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",` |
|      - | 10725 | `		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },` |
|      - | 10726 | `		/* The DOMParentNode three: real (non-tentative) void, one untyped` |
|      - | 10727 | `		 * variadic -- php's own rows, screened inside the body. */` |
|      - | 10728 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|      - | 10729 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|      - | 10730 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|      - | 10731 | `	};` |
|      - | 10732 | `	static const PH7_NativeMethodDef aElemMethod[] = {` |
|      - | 10733 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|      - | 10734 | `		  "string $qualifiedName, ?string $value = null, string $namespace = ''", "",` |
|      - | 10735 | `		  vm_builtin_DOMElement_construct },` |
|      - | 10736 | `		{ "getAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@string",` |
|      - | 10737 | `		  vm_builtin_DOMElement_getAttribute },` |
|      - | 10738 | `		{ "hasAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",` |
|      - | 10739 | `		  vm_builtin_DOMElement_hasAttribute },` |
|      - | 10740 | `		{ "setAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName, string $value", "",` |
|      - | 10741 | `		  vm_builtin_DOMElement_setAttribute },` |
|      - | 10742 | `		{ "removeAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",` |
|      - | 10743 | `		  vm_builtin_DOMElement_removeAttribute },` |
|      - | 10744 | `		{ "getAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@string",` |
|      - | 10745 | `		  vm_builtin_DOMElement_getAttributeNS },` |
|      - | 10746 | `		/* php declares no return type at all on the five that hand out a NODE --` |
|      - | 10747 | `		 * not even a tentative one -- because their answer is a union it never` |
|      - | 10748 | `		 * wrote down. The rows state none either. */` |
|      - | 10749 | `		{ "getAttributeNode",     PH7_MOD_PUBLIC, "string $qualifiedName", "",` |
|      - | 10750 | `		  vm_builtin_DOMElement_getAttributeNode },` |
|      - | 10751 | `		{ "getAttributeNodeNS",   PH7_MOD_PUBLIC, "?string $namespace, string $localName", "",` |
|      - | 10752 | `		  vm_builtin_DOMElement_getAttributeNodeNS },` |
|      - | 10753 | `		{ "setAttributeNode",     PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|      - | 10754 | `		  vm_builtin_DOMElement_setAttributeNode },` |
|      - | 10755 | `		{ "setAttributeNodeNS",   PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|      - | 10756 | `		  vm_builtin_DOMElement_setAttributeNodeNS },` |
|      - | 10757 | `		{ "removeAttributeNode",  PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|      - | 10758 | `		  vm_builtin_DOMElement_removeAttributeNode },` |
|      - | 10759 | `		{ "getAttributeNames",    PH7_MOD_PUBLIC, "", "array",` |
|      - | 10760 | `		  vm_builtin_DOMElement_getAttributeNames },` |
|      - | 10761 | `		{ "hasAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@bool",` |
|      - | 10762 | `		  vm_builtin_DOMElement_hasAttributeNS },` |
|      - | 10763 | `		{ "removeAttributeNS",    PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@void",` |
|      - | 10764 | `		  vm_builtin_DOMElement_removeAttributeNS },` |
|      - | 10765 | `		{ "toggleAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName, ?bool $force = null", "bool",` |
|      - | 10766 | `		  vm_builtin_DOMElement_toggleAttribute },` |
|      - | 10767 | `		{ "setIdAttribute",       PH7_MOD_PUBLIC, "string $qualifiedName, bool $isId", "@void",` |
|      - | 10768 | `		  vm_builtin_DOMElement_setIdAttribute },` |
|      - | 10769 | `		{ "setIdAttributeNS",     PH7_MOD_PUBLIC,` |
|      - | 10770 | `		  "string $namespace, string $qualifiedName, bool $isId", "@void",` |
|      - | 10771 | `		  vm_builtin_DOMElement_setIdAttributeNS },` |
|      - | 10772 | `		{ "setIdAttributeNode",   PH7_MOD_PUBLIC, "DOMAttr $attr, bool $isId", "@void",` |
|      - | 10773 | `		  vm_builtin_DOMElement_setIdAttributeNode },` |
|      - | 10774 | `		{ "setAttributeNS",       PH7_MOD_PUBLIC,` |
|      - | 10775 | `		  "?string $namespace, string $qualifiedName, string $value", "@void",` |
|      - | 10776 | `		  vm_builtin_DOMElement_setAttributeNS },` |
|      - | 10777 | `		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",` |
|      - | 10778 | `		  vm_builtin_Dom_getElementsByTagName },` |
|      - | 10779 | `		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",` |
|      - | 10780 | `		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },` |
|      - | 10781 | `		/* The DOMChildNode four, php's order on this class. */` |
|      - | 10782 | `		{ "remove",          PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf },` |
|      - | 10783 | `		{ "before",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_before },` |
|      - | 10784 | `		{ "after",           PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_after },` |
|      - | 10785 | `		{ "replaceWith",     PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceWith },` |
|      - | 10786 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|      - | 10787 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|      - | 10788 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|      - | 10789 | `		/* The 8.3 pair. php declares the first's return as a plain ?DOMElement` |
|      - | 10790 | `		 * and the second's as a real void. */` |
|      - | 10791 | `		{ "insertAdjacentElement", PH7_MOD_PUBLIC, "string $where, DOMElement $element",` |
|      - | 10792 | `		  "?DOMElement", vm_builtin_DOMElement_insertAdjacentElement },` |
|      - | 10793 | `		{ "insertAdjacentText",    PH7_MOD_PUBLIC, "string $where, string $data", "void",` |
|      - | 10794 | `		  vm_builtin_DOMElement_insertAdjacentText },` |
|      - | 10795 | `	};` |
|      - | 10796 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|      - | 10797 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name, string $value = ''", "",` |
|      - | 10798 | `		  vm_builtin_DOMAttr_construct },` |
|      - | 10799 | `		{ "isId",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMAttr_isId },` |
|      - | 10800 | `	};` |
|      - | 10801 | `	static const PH7_NativeMethodDef aCharMethod[] = {` |
|      - | 10802 | `		/* Every offset and count here is in UTF-8 CHARACTERS, php's unit. */` |
|      - | 10803 | `		{ "appendData",    PH7_MOD_PUBLIC, "string $data", "@true",` |
|      - | 10804 | `		  vm_builtin_DOMCharacterData_appendData },` |
|      - | 10805 | `		{ "substringData", PH7_MOD_PUBLIC, "int $offset, int $count", "",` |
|      - | 10806 | `		  vm_builtin_DOMCharacterData_substringData },` |
|      - | 10807 | `		{ "insertData",    PH7_MOD_PUBLIC, "int $offset, string $data", "@bool",` |
|      - | 10808 | `		  vm_builtin_DOMCharacterData_insertData },` |
|      - | 10809 | `		{ "deleteData",    PH7_MOD_PUBLIC, "int $offset, int $count", "@bool",` |
|      - | 10810 | `		  vm_builtin_DOMCharacterData_deleteData },` |
|      - | 10811 | `		{ "replaceData",   PH7_MOD_PUBLIC, "int $offset, int $count, string $data", "@bool",` |
|      - | 10812 | `		  vm_builtin_DOMCharacterData_replaceData },` |
|      - | 10813 | `		/* The DOMChildNode four, php's order on THIS class -- replaceWith` |
|      - | 10814 | `		 * leads here where DOMElement's list starts at remove. */` |
|      - | 10815 | `		{ "replaceWith", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceWith },` |
|      - | 10816 | `		{ "remove",      PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf },` |
|      - | 10817 | `		{ "before",      PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_before },` |
|      - | 10818 | `		{ "after",       PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_after },` |
|      - | 10819 | `	};` |
|      - | 10820 | `	static const PH7_NativeMethodDef aPiMethod[] = {` |
|      - | 10821 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name, string $value = ''", "",` |
|      - | 10822 | `		  vm_builtin_DOMProcessingInstruction_construct },` |
|      - | 10823 | `	};` |
|      - | 10824 | `	static const PH7_NativeMethodDef aFragMethod[] = {` |
|      - | 10825 | `		{ "__construct", PH7_MOD_PUBLIC, "", "",` |
|      - | 10826 | `		  vm_builtin_DOMDocumentFragment_construct },` |
|      - | 10827 | `		{ "appendXML", PH7_MOD_PUBLIC, "string $data", "@bool",` |
|      - | 10828 | `		  vm_builtin_DOMDocumentFragment_appendXML },` |
|      - | 10829 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|      - | 10830 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|      - | 10831 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|      - | 10832 | `	};` |
|      - | 10833 | `	static const PH7_NativeMethodDef aTextMethod[] = {` |
|      - | 10834 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data = ''", "",` |
|      - | 10835 | `		  vm_builtin_DOMText_construct },` |
|      - | 10836 | `		{ "splitText", PH7_MOD_PUBLIC, "int $offset", "", vm_builtin_DOMText_splitText },` |
|      - | 10837 | `		/* php's 8.x rename and the name it renamed, one body. */` |
|      - | 10838 | `		{ "isWhitespaceInElementContent", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 10839 | `		  vm_builtin_DOMText_isWhitespace },` |
|      - | 10840 | `		{ "isElementContentWhitespace",   PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 10841 | `		  vm_builtin_DOMText_isWhitespace },` |
|      - | 10842 | `	};` |
|      - | 10843 | `	static const PH7_NativeMethodDef aEntRefMethod[] = {` |
|      - | 10844 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|      - | 10845 | `		  vm_builtin_DOMEntityReference_construct },` |
|      - | 10846 | `	};` |
|      - | 10847 | `	/* The DTD half declares no method of its own at all -- php's whole` |
|      - | 10848 | `	 * DOMDocumentType surface is properties over DOMNode's method list. */` |
|      - | 10849 | `	/* php's factory class: three ordinary instance methods and no state. */` |
|      - | 10850 | `	static const PH7_NativeMethodDef aImplMethod[] = {` |
|      - | 10851 | `		{ "createDocumentType", PH7_MOD_PUBLIC,` |
|      - | 10852 | `		  "string $qualifiedName, string $publicId = '', string $systemId = ''", "",` |
|      - | 10853 | `		  vm_builtin_DOMImplementation_createDocumentType },` |
|      - | 10854 | `		{ "createDocument", PH7_MOD_PUBLIC,` |
|      - | 10855 | `		  "?string $namespace = null, string $qualifiedName = '', "` |
|      - | 10856 | `		  "?DOMDocumentType $doctype = null", "@DOMDocument",` |
|      - | 10857 | `		  vm_builtin_DOMImplementation_createDocument },` |
|      - | 10858 | `		{ "hasFeature", PH7_MOD_PUBLIC, "string $feature, string $version", "@bool",` |
|      - | 10859 | `		  vm_builtin_DOMImplementation_hasFeature },` |
|      - | 10860 | `	};` |
|      - | 10861 | `	/* The comment and CDATA constructors -- the only method either class` |
|      - | 10862 | `	 * declares of its own; php's CDATA data is REQUIRED where the other two` |
|      - | 10863 | `	 * default. */` |
|      - | 10864 | `	static const PH7_NativeMethodDef aCommentMethod[] = {` |
|      - | 10865 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data = ''", "",` |
|      - | 10866 | `		  vm_builtin_DOMComment_construct },` |
|      - | 10867 | `	};` |
|      - | 10868 | `	static const PH7_NativeMethodDef aCdataMethod[] = {` |
|      - | 10869 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data", "",` |
|      - | 10870 | `		  vm_builtin_DOMCdataSection_construct },` |
|      - | 10871 | `	};` |
|      - | 10872 | `	/* DOMNodeList and DOMNamedNodeMap share a slot layout: what a live view is OF` |
|      - | 10873 | `	 * ($__owner), the document to wrap results against ($__doc), and -- for the` |
|      - | 10874 | `	 * two node-list kinds -- the tag name or the frozen snapshot. */` |
|      - | 10875 | `	static const PH7_NativePropDef aListProp[] = {` |
|      - | 10876 | `		/* php's one declared property on either class, and virtual there too. */` |
|      - | 10877 | `		DOM_VPROP("length","int"),` |
|      - | 10878 | `		{ DNL_KIND,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 10879 | `		{ DOM_DOC,       PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|      - | 10880 | `		{ DNL_OWNER,     PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|      - | 10881 | `		{ DNL_NAME,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 10882 | `		/* ...and, for the namespace-aware lookup, the URI beside the name. */` |
|      - | 10883 | `		{ DNL_URI,       PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 10884 | `		/* The cached node snapshot, missed by the 2 Aug hidden-slot sweep exactly as` |
|      - | 10885 | `		 * Closure's three were: php presents no property on either class this table` |
|      - | 10886 | ``		 * declares (DOMNodeList, DOMNamedNodeMap), and `__snap` was on var_dump,`` |
|      - | 10887 | `		 * (array), get_object_vars, foreach, json_encode and Reflection. */` |
|      - | 10888 | `		{ DNL_SNAP_SLOT, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|      - | 10889 | `	};` |
|      - | 10890 | `	static const PH7_NativeMethodDef aListMethod[] = {` |
|      - | 10891 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNodeList_count },` |
|      - | 10892 | `		{ "item",        PH7_MOD_PUBLIC, "int $index", "", vm_builtin_DOMNodeList_item },` |
|      - | 10893 | `		{ "getIterator", PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },` |
|      - | 10894 | `	};` |
|      - | 10895 | `	static const PH7_NativeMethodDef aMapMethod[] = {` |
|      - | 10896 | `		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNamedNodeMap_count },` |
|      - | 10897 | `		{ "item",         PH7_MOD_PUBLIC, "int $index", "@?DOMNode", vm_builtin_DOMNamedNodeMap_item },` |
|      - | 10898 | `		{ "getNamedItem", PH7_MOD_PUBLIC, "string $qualifiedName", "@?DOMNode",` |
|      - | 10899 | `		  vm_builtin_DOMNamedNodeMap_getNamedItem },` |
|      - | 10900 | `		{ "getNamedItemNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@?DOMNode",` |
|      - | 10901 | `		  vm_builtin_DOMNamedNodeMap_getNamedItemNS },` |
|      - | 10902 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },` |
|      - | 10903 | `	};` |
|      - | 10904 | `	/* The declaration itself, the document it belongs to, and the element that` |
|      - | 10905 | `	 * MAKES it -- php's parentNode/parentElement. */` |
|      - | 10906 | `	static const PH7_NativePropDef aNsNodeProp[] = {` |
|      - | 10907 | `		/* php's ten, in its order: a namespace declaration is not a DOMNode there,` |
|      - | 10908 | `		 * so the class states its own subset rather than inheriting one. */` |
|      - | 10909 | `		DOM_VPROP("nodeName","string"),` |
|      - | 10910 | `		DOM_VPROP("nodeValue","?string"),` |
|      - | 10911 | `		DOM_VPROP("nodeType","int"),` |
|      - | 10912 | `		DOM_VPROP("prefix","string"),` |
|      - | 10913 | `		DOM_VPROP("localName","?string"),` |
|      - | 10914 | `		DOM_VPROP("namespaceURI","?string"),` |
|      - | 10915 | `		DOM_VPROP("isConnected","bool"),` |
|      - | 10916 | `		DOM_VPROP("ownerDocument","?DOMDocument"),` |
|      - | 10917 | `		DOM_VPROP("parentNode","?DOMNode"),` |
|      - | 10918 | `		DOM_VPROP("parentElement","?DOMElement"),` |
|      - | 10919 | `		{ DOM_RES,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10920 | `		{ DOM_DOC,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10921 | `		{ DOM_NS_OWNER, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10922 | `	};` |
|      - | 10923 | `	static const PH7_NativeMethodDef aNsNodeMethod[] = {` |
|      - | 10924 | `		{ "__sleep",  PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },` |
|      - | 10925 | `		{ "__wakeup", PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },` |
|      - | 10926 | `	};` |
|      - | 10927 | `	static const PH7_NativePropDef aXPathProp[] = {` |
|      - | 10928 | ``		/* php models both as VIRTUAL: `document` is read-only because its handler`` |
|      - | 10929 | ``		 * has no writer -- not because the slot is `readonly`, which is why php's`` |
|      - | 10930 | `		 * isReadOnly() answers false and its modifiers are 513. The values live in` |
|      - | 10931 | `		 * the two hidden slots below. */` |
|      - | 10932 | `		DOM_VPROP("document","DOMDocument"),` |
|      - | 10933 | `		DOM_VPROP("registerNodeNamespaces","bool"),` |
|      - | 10934 | `		{ XP_DOC,    PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10935 | `		{ XP_NSDEF,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },` |
|      - | 10936 | `		{ XP_NSREG,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10937 | `		{ XP_FNMODE, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN,` |
|      - | 10938 | `		  { 0, 0, PH7_NATIVE_VAL_INT, XP_MODE_NONE, 0, 0.0 }, 0 },` |
|      - | 10939 | `		{ XP_FNREG,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10940 | `		{ XP_NSFN,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10941 | `	};` |
|      - | 10942 | `	static const PH7_NativeMethodDef aXPathMethod[] = {` |
|      - | 10943 | `		{ "__construct", PH7_MOD_PUBLIC, "DOMDocument $document, bool $registerNodeNS = true", "",` |
|      - | 10944 | `		  vm_builtin_DOMXPath_construct },` |
|      - | 10945 | `		{ "query",       PH7_MOD_PUBLIC,` |
|      - | 10946 | `		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",` |
|      - | 10947 | `		  vm_builtin_DOMXPath_query },` |
|      - | 10948 | `		{ "evaluate",    PH7_MOD_PUBLIC,` |
|      - | 10949 | `		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",` |
|      - | 10950 | `		  vm_builtin_DOMXPath_evaluate },` |
|      - | 10951 | `		{ "registerNamespace", PH7_MOD_PUBLIC, "string $prefix, string $namespace", "@bool",` |
|      - | 10952 | `		  vm_builtin_DOMXPath_registerNamespace },` |
|      - | 10953 | `		{ "registerPhpFunctions", PH7_MOD_PUBLIC, "array\|string\|null $restrict = null", "@void",` |
|      - | 10954 | `		  vm_builtin_DOMXPath_registerPhpFunctions },` |
|      - | 10955 | `		{ "registerPhpFunctionNS", PH7_MOD_PUBLIC,` |
|      - | 10956 | `		  "string $namespaceURI, string $name, callable $callable", "void",` |
|      - | 10957 | `		  vm_builtin_DOMXPath_registerPhpFunctionNS },` |
|      - | 10958 | `		{ "quote", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $str", "string",` |
|      - | 10959 | `		  vm_builtin_DOMXPath_quote },` |
|      - | 10960 | `	};` |
|      - | 10961 | `	/* Bases before subclasses: PH7_InstallNativeClasses declares the whole table` |
|      - | 10962 | `	 * before touching a method, but PH7_ClassInherit still needs the parent to` |
|      - | 10963 | `	 * exist when the child's row is declared. */` |
|      - | 10964 | `	/* php refuses to serialize a NODE class, and its refusal is the soft kind: the` |
|      - | 10965 | `	 * deny handler sits behind the __serialize()/__sleep() lookup, so a subclass that` |
|      - | 10966 | `	 * declares either one is serialized normally and the sentence says so. DOMXPath's` |
|      - | 10967 | `	 * is the HARD kind — a subclass declaring __serialize() is refused there too — and` |
|      - | 10968 | ``	 * DOMNodeList/DOMNamedNodeMap are not refused at all (`0:{}`), which is what they`` |
|      - | 10969 | `	 * became once serialize() stopped emitting the hidden slot. Restating the flag on` |
|      - | 10970 | `	 * every row is rule 29: a native subclass does not inherit its parent's. */` |
|      - | 10971 | `	/* php's 8.0 insertion interfaces: three untyped-variadic void methods on` |
|      - | 10972 | `	 * the parent side (the child side is DOMChildNode below).  A class row` |
|      - | 10973 | `	 * declares its methods before the implement phase runs, so nothing is` |
|      - | 10974 | `	 * stubbed abstract. */` |
|      - | 10975 | `	static const PH7_NativeMethodDef aParentNodeIf[] = {` |
|      - | 10976 | `		{ "append",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10977 | `		{ "prepend",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10978 | `		{ "replaceChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10979 | `	};` |
|      - | 10980 | `	static const PH7_NativeMethodDef aChildNodeIf[] = {` |
|      - | 10981 | `		{ "remove",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "void", 0 },` |
|      - | 10982 | `		{ "before",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10983 | `		{ "after",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10984 | `		{ "replaceWith", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10985 | `	};` |
|      - | 10986 | `	/* The remaining classes' OWN declarations, each php's list in php's order.` |
|      - | 10987 | `	 * They add no storage: every row is virtual, so these tables exist only to put` |
|      - | 10988 | `	 * the names on the class -- for Reflection, for property_exists(), and for the` |
|      - | 10989 | `	 * debug table above to walk. */` |
|      - | 10990 | `	static const PH7_NativePropDef aElemProp[] = {` |
|      - | 10991 | `		DOM_VPROP("tagName","string"),` |
|      - | 10992 | `		DOM_VPROP("className","string"),` |
|      - | 10993 | `		DOM_VPROP("id","string"),` |
|      - | 10994 | `		DOM_VPROP("schemaTypeInfo","mixed"),` |
|      - | 10995 | `		DOM_PARENT_VPROPS,` |
|      - | 10996 | `		DOM_CHILD_VPROPS` |
|      - | 10997 | `	};` |
|      - | 10998 | `	static const PH7_NativePropDef aAttrProp[] = {` |
|      - | 10999 | `		DOM_VPROP("name","string"),` |
|      - | 11000 | `		DOM_VPROP("specified","bool"),` |
|      - | 11001 | `		DOM_VPROP("value","string"),` |
|      - | 11002 | `		DOM_VPROP("ownerElement","?DOMElement"),` |
|      - | 11003 | `		DOM_VPROP("schemaTypeInfo","mixed")` |
|      - | 11004 | `	};` |
|      - | 11005 | `	static const PH7_NativePropDef aCharProp[] = {` |
|      - | 11006 | `		DOM_VPROP("data","string"),` |
|      - | 11007 | `		DOM_VPROP("length","int"),` |
|      - | 11008 | `		DOM_CHILD_VPROPS` |
|      - | 11009 | `	};` |
|      - | 11010 | `	static const PH7_NativePropDef aTextProp[] = {` |
|      - | 11011 | `		DOM_VPROP("wholeText","string")` |
|      - | 11012 | `	};` |
|      - | 11013 | `	static const PH7_NativePropDef aPiProp[] = {` |
|      - | 11014 | `		DOM_VPROP("target","string"),` |
|      - | 11015 | `		DOM_VPROP("data","string")` |
|      - | 11016 | `	};` |
|      - | 11017 | `	static const PH7_NativePropDef aFragProp[] = { DOM_PARENT_VPROPS };` |
|      - | 11018 | `	static const PH7_NativePropDef aDocTypeProp[] = {` |
|      - | 11019 | `		DOM_VPROP("name","string"),` |
|      - | 11020 | `		DOM_VPROP("entities","DOMNamedNodeMap"),` |
|      - | 11021 | `		DOM_VPROP("notations","DOMNamedNodeMap"),` |
|      - | 11022 | `		DOM_VPROP("publicId","string"),` |
|      - | 11023 | `		DOM_VPROP("systemId","string"),` |
|      - | 11024 | `		DOM_VPROP("internalSubset","?string")` |
|      - | 11025 | `	};` |
|      - | 11026 | `	static const PH7_NativePropDef aEntityProp[] = {` |
|      - | 11027 | `		DOM_VPROP("publicId","?string"),` |
|      - | 11028 | `		DOM_VPROP("systemId","?string"),` |
|      - | 11029 | `		DOM_VPROP("notationName","?string"),` |
|      - | 11030 | `		DOM_VPROP("actualEncoding","?string"),` |
|      - | 11031 | `		DOM_VPROP("encoding","?string"),` |
|      - | 11032 | `		DOM_VPROP("version","?string")` |
|      - | 11033 | `	};` |
|      - | 11034 | ``	/* php's pair here are plain `string`, unlike DOMEntity's same-named `?string`. */`` |
|      - | 11035 | `	static const PH7_NativePropDef aNotationProp[] = {` |
|      - | 11036 | `		DOM_VPROP("publicId","string"),` |
|      - | 11037 | `		DOM_VPROP("systemId","string")` |
|      - | 11038 | `	};` |
|      - | 11039 | ``	/* php's DOMException REDECLARES Exception's `$code` as a PUBLIC untyped slot --`` |
|      - | 11040 | ``	 * which is why `(array)$e` and `get_object_vars($e)` show a plain `code` there`` |
|      - | 11041 | `	 * where every other exception mangles it, and why Reflection reports it as` |
|      - | 11042 | `	 * DOMException's own with modifiers 1. The instance keeps the position the BASE` |
|      - | 11043 | `	 * declared it in, so var_dump still prints it third. */` |
|      - | 11044 | `	static const PH7_NativePropDef aExcProp[] = {` |
|      - | 11045 | `		{ "code", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }` |
|      - | 11046 | `	};` |
|      - | 11047 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 11048 | `		{ "DOMException", "Exception", 0, PH7_CLASS_FINAL, 0, 0, 0, 0,` |
|      - | 11049 | `		  aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },` |
|      - | 11050 | `		{ "DOMParentNode", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 11051 | `		  aParentNodeIf, SX_ARRAYSIZE(aParentNodeIf), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 11052 | `		{ "DOMChildNode", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 11053 | `		  aChildNodeIf, SX_ARRAYSIZE(aChildNodeIf), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 11054 | `		{ "DOMNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11055 | `		  aNodeMethod, SX_ARRAYSIZE(aNodeMethod), aNodeConst, SX_ARRAYSIZE(aNodeConst),` |
|      - | 11056 | `		  aNodeProp, SX_ARRAYSIZE(aNodeProp), 0, 0, DomPresent },` |
|      - | 11057 | `		{ "DOMDocument", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11058 | `		  aDocMethod, SX_ARRAYSIZE(aDocMethod), 0, 0, aDocProp, SX_ARRAYSIZE(aDocProp),` |
|      - | 11059 | `		  DomDocRelease, 0, DomPresent },` |
|      - | 11060 | `		{ "DOMElement", "DOMNode", "DOMParentNode,DOMChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11061 | `		  aElemMethod, SX_ARRAYSIZE(aElemMethod), 0, 0, aElemProp, SX_ARRAYSIZE(aElemProp),` |
|      - | 11062 | `		  0, 0, DomPresent },` |
|      - | 11063 | `		{ "DOMAttr", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11064 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), 0, 0, aAttrProp, SX_ARRAYSIZE(aAttrProp),` |
|      - | 11065 | `		  0, 0, DomPresent },` |
|      - | 11066 | `		{ "DOMCharacterData", "DOMNode", "DOMChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11067 | `		  aCharMethod, SX_ARRAYSIZE(aCharMethod), 0, 0, aCharProp, SX_ARRAYSIZE(aCharProp),` |
|      - | 11068 | `		  0, 0, DomPresent },` |
|      - | 11069 | `		{ "DOMText", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11070 | `		  aTextMethod, SX_ARRAYSIZE(aTextMethod), 0, 0, aTextProp, SX_ARRAYSIZE(aTextProp),` |
|      - | 11071 | `		  0, 0, DomPresent },` |
|      - | 11072 | `		{ "DOMComment", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11073 | `		  aCommentMethod, SX_ARRAYSIZE(aCommentMethod), 0, 0, 0, 0, 0, 0, DomPresent },` |
|      - | 11074 | `		{ "DOMCdataSection", "DOMText", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11075 | `		  aCdataMethod, SX_ARRAYSIZE(aCdataMethod), 0, 0, 0, 0, 0, 0, DomPresent },` |
|      - | 11076 | ``		/* php declares the PI under DOMNode (its `data` is its own property, not`` |
|      - | 11077 | `		 * DOMCharacterData's), the fragment and the entity reference plainly. */` |
|      - | 11078 | `		{ "DOMProcessingInstruction", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11079 | `		  aPiMethod, SX_ARRAYSIZE(aPiMethod), 0, 0, aPiProp, SX_ARRAYSIZE(aPiProp),` |
|      - | 11080 | `		  0, 0, DomPresent },` |
|      - | 11081 | `		{ "DOMDocumentFragment", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11082 | `		  aFragMethod, SX_ARRAYSIZE(aFragMethod), 0, 0, aFragProp, SX_ARRAYSIZE(aFragProp),` |
|      - | 11083 | `		  0, 0, DomPresent },` |
|      - | 11084 | `		{ "DOMEntityReference", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11085 | `		  aEntRefMethod, SX_ARRAYSIZE(aEntRefMethod), 0, 0, 0, 0, 0, 0, DomPresent },` |
|      - | 11086 | `		/* The DTD trio state no method of their own -- every name php declares on` |
|      - | 11087 | `		 * them is a property, and the class's handler answers it. */` |
|      - | 11088 | `		{ "DOMDocumentType", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11089 | `		  0, 0, 0, 0, aDocTypeProp,` |
|      - | 11090 | `		  SX_ARRAYSIZE(aDocTypeProp), 0, 0, DomPresent },` |
|      - | 11091 | `		{ "DOMEntity", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11092 | `		  0, 0, 0, 0, aEntityProp,` |
|      - | 11093 | `		  SX_ARRAYSIZE(aEntityProp), 0, 0, DomPresent },` |
|      - | 11094 | `		{ "DOMImplementation", 0, 0, 0,` |
|      - | 11095 | `		  aImplMethod, SX_ARRAYSIZE(aImplMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 11096 | `		{ "DOMNotation", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11097 | `		  0, 0, 0, 0, aNotationProp,` |
|      - | 11098 | `		  SX_ARRAYSIZE(aNotationProp), 0, 0, DomPresent },` |
|      - | 11099 | `		/* php's own two: IteratorAggregate (NOT Iterator -- the chunk had the` |
|      - | 11100 | `		 * list carry its own cursor) and Countable. */` |
|      - | 11101 | `		{ "DOMNodeList", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,` |
|      - | 11102 | `		  aListMethod, SX_ARRAYSIZE(aListMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),` |
|      - | 11103 | `		  0, &sDomListIterVtab, DomPresent },` |
|      - | 11104 | `		{ "DOMNamedNodeMap", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,` |
|      - | 11105 | `		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),` |
|      - | 11106 | `		  0, &sDomMapIterVtab, DomPresent },` |
|      - | 11107 | `		/* php's own: a class of its OWN, with no parent at all -- a namespace` |
|      - | 11108 | ``		 * declaration is not a DOMNode there, and `$ns instanceof DOMNode` is`` |
|      - | 11109 | `		 * false. Its refusal to serialize is the same soft kind the node` |
|      - | 11110 | `		 * classes carry. */` |
|      - | 11111 | `		{ "DOMNameSpaceNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 11112 | `		  aNsNodeMethod, SX_ARRAYSIZE(aNsNodeMethod), 0, 0,` |
|      - | 11113 | `		  aNsNodeProp, SX_ARRAYSIZE(aNsNodeProp), 0, 0, DomPresent },` |
|      - | 11114 | `		{ "DOMXPath", 0, 0, PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|      - | 11115 | `		  aXPathMethod, SX_ARRAYSIZE(aXPathMethod), 0, 0, aXPathProp, SX_ARRAYSIZE(aXPathProp),` |
|      - | 11116 | `		  0, 0, DomPresent },` |
|      - | 11117 | `	};` |
|   7930 | 11118 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|   7930 | 11119 | `	if( rc == SXRET_OK ){` |
|      - | 11120 | `		/* The clone hook (ph7_class::xClone, php's clone_obj): stated on every` |
|      - | 11121 | `		 * node class -- rule 29, a hook is per-row and never inherited between` |
|      - | 11122 | `		 * native rows -- and assigned HERE because PH7_NativeClassSpec carries` |
|      - | 11123 | `		 * no field for it. The document's copies the whole document; a user` |
|      - | 11124 | `		 * subclass reaches the nearest ancestor's hook through the engine's` |
|      - | 11125 | `		 * chain walk, php's handler inheritance. */` |
|      - | 11126 | `		static const char * const azNodeClone[] = {` |
|      - | 11127 | `			"DOMNode", "DOMElement", "DOMAttr", "DOMCharacterData", "DOMText",` |
|      - | 11128 | `			"DOMComment", "DOMCdataSection", "DOMProcessingInstruction",` |
|      - | 11129 | `			"DOMDocumentFragment", "DOMEntityReference", "DOMDocumentType"` |
|      - | 11130 | `		};` |
|      - | 11131 | `		sxu32 n;` |
|      - | 11132 | `		ph7_class *pClass;` |
|  95105 | 11133 | `		for( n = 0 ; n < SX_ARRAYSIZE(azNodeClone) ; ++n ){` |
| 130707 | 11134 | `			pClass = PH7_VmExtractClass(&(*pVm),azNodeClone[n],` |
|  87175 | 11135 | `				(sxu32)SyStrlen(azNodeClone[n]),FALSE,0);` |
|  87180 | 11136 | `			if( pClass ){` |
|  87180 | 11137 | `				pClass->xClone = DomInstanceClone;` |
|  43527 | 11138 | `			}` |
|  43532 | 11139 | `		}` |
|   7930 | 11140 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMDocument",sizeof("DOMDocument")-1,FALSE,0);` |
|   7930 | 11141 | `		if( pClass ){` |
|   7930 | 11142 | `			pClass->xClone = DomInstanceCloneDoc;` |
|   3957 | 11143 | `		}` |
|      - | 11144 | `		/* And the teardown hook (ph7_class::xRelease), on the same rows for the` |
|      - | 11145 | `		 * same reason: the document's identity cache borrows its wrappers, so` |
|      - | 11146 | `		 * each node class has to take its own entry back out and free the` |
|      - | 11147 | `		 * libxml node it was the last holder of. DOMDocument keeps its own,` |
|      - | 11148 | `		 * which forgets the tree's back-pointer instead; DOMNameSpaceNode is` |
|      - | 11149 | `		 * not a node class here (it is not a DOMNode in php either) and its` |
|      - | 11150 | `		 * handle is the document's, shared. */` |
|  95105 | 11151 | `		for( n = 0 ; n < SX_ARRAYSIZE(azNodeClone) ; ++n ){` |
| 130707 | 11152 | `			pClass = PH7_VmExtractClass(&(*pVm),azNodeClone[n],` |
|  87175 | 11153 | `				(sxu32)SyStrlen(azNodeClone[n]),FALSE,0);` |
|  87180 | 11154 | `			if( pClass ){` |
|  87180 | 11155 | `				pClass->xRelease = DomNodeRelease;` |
|  43527 | 11156 | `			}` |
|  43532 | 11157 | `		}` |
|      - | 11158 | `		/* The dimension handlers (ph7_class::xDim, php's read_dimension /` |
|      - | 11159 | `		 * has_dimension), assigned here for the same reason the clone hook is:` |
|      - | 11160 | `		 * PH7_NativeClassSpec carries no field for them, and php's own two` |
|      - | 11161 | `		 * classes wear them without declaring ArrayAccess. */` |
|   7930 | 11162 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMNodeList",sizeof("DOMNodeList")-1,FALSE,0);` |
|   7930 | 11163 | `		if( pClass ){` |
|   7930 | 11164 | `			pClass->xDim = DomListDim;` |
|   3957 | 11165 | `		}` |
|   7930 | 11166 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMNamedNodeMap",sizeof("DOMNamedNodeMap")-1,FALSE,0);` |
|   7930 | 11167 | `		if( pClass ){` |
|   7930 | 11168 | `			pClass->xDim = DomMapDim;` |
|   3957 | 11169 | `		}` |
|      - | 11170 | `		/* The property handlers (ph7_class::xProp, php's read_property /` |
|      - | 11171 | `		 * has_property / write_property), assigned for the same reason. One per` |
|      - | 11172 | `		 * ROOT: the engine walks the base chain for the hook exactly as php's` |
|      - | 11173 | `		 * handlers are inherited, and the hook then picks the per-class table off` |
|      - | 11174 | `		 * aDomProp[] -- so a subclass of DOMElement reaches DOMElement's. */` |
|      - | 11175 | `		{` |
|      - | 11176 | `			static const char * const azPropRoot[] = {` |
|      - | 11177 | `				"DOMNode", "DOMNodeList", "DOMNamedNodeMap", "DOMNameSpaceNode", "DOMXPath"` |
|      - | 11178 | `			};` |
|  47555 | 11179 | `			for( n = 0 ; n < SX_ARRAYSIZE(azPropRoot) ; ++n ){` |
|  39630 | 11180 | `				PH7_NativeClassInstallPropHook(&(*pVm),azPropRoot[n],DomPropHook);` |
|  19790 | 11181 | `			}` |
|      - | 11182 | `		}` |
|   3957 | 11183 | `	}` |
|   7930 | 11184 | `	return rc;` |
|      5 | 11185 | `}` |
|      - | 11186 |  |
|      - | 11187 | `#else` |
|      - | 11188 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - | 11189 | `typedef int vm_dom_unused;` |
|      - | 11190 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - | 11191 |  |
