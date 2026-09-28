# src/ph7/vm_dom.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5491/5939 lines (92.46%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits |  Line | Source |
| ----: | ----: | :--- |
|     - |     1 | `/**` |
|     - |     2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |     3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |     4 | ` */` |
|     - |     5 | `#ifdef PH7_ENABLE_LIBXML` |
|     - |     6 | `#include "ph7int.h"` |
|     - |     7 | `#include <libxml/parser.h>` |
|     - |     8 | `#include <libxml/tree.h>` |
|     - |     9 | `#include <libxml/c14n.h>` |
|     - |    10 | `#include <libxml/xmlsave.h>` |
|     - |    11 | `#include <libxml/xpath.h>` |
|     - |    12 | `#include <libxml/xpathInternals.h>` |
|     - |    13 | `#include <libxml/xmlschemas.h>` |
|     - |    14 | `#include <libxml/relaxng.h>` |
|     - |    15 | `#include <libxml/valid.h>` |
|     - |    16 | `#include <libxml/xinclude.h>` |
|     - |    17 | `#include <libxml/encoding.h>` |
|     - |    18 | `#include <libxml/HTMLparser.h>` |
|     - |    19 | `#include <libxml/HTMLtree.h>` |
|     - |    20 |  |
|     - |    21 | `/*` |
|     - |    22 | ` * ext/dom on libxml2: the DOM classes, declared and bodied in C.` |
|     - |    23 | ` *` |
|     - |    24 | ` * Architecture (see also vm_libxml.c): DOMNode and its subclasses are native` |
|     - |    25 | ` * classes (oo_native.c) whose methods ARE the C below.  Every instance holds` |
|     - |    26 | ` * two slots -- $__res, a phl_domnode resource {phl_xmldoc*, xmlNodePtr}, and` |
|     - |    27 | ` * $__doc, the owning DOMDocument wrapper.  There is no PHP layer left in the` |
|     - |    28 | ` * node tree: what used to be a prelude class over ~30 global __dom_* thunks is` |
|     - |    29 | ` * one C body per method, so the thunks stopped being globally visible names.` |
|     - |    30 | ` *` |
|     - |    31 | ` * Node identity: php guarantees $doc->documentElement === $doc->` |
|     - |    32 | ` * documentElement.  Every wrap goes through DomWrap(), which keys a` |
|     - |    33 | ` * per-document cache ($doc->__nodes) by the node POINTER, so the same` |
|     - |    34 | ` * underlying node always yields the same object.  The cache owns the` |
|     - |    35 | ` * wrappers, which is why DomWrap hands back a BORROWED instance: it stays` |
|     - |    36 | ` * alive as long as its document does.  (The old shape allocated a fresh` |
|     - |    37 | ` * phl_domnode on every navigation step even when the cache then threw the` |
|     - |    38 | ` * result away; only a genuine cache MISS allocates one now.)` |
|     - |    39 | ` *` |
|     - |    40 | ` * Tree surgery (append/insert/replace/remove) is done with manual pointer` |
|     - |    41 | ` * splicing instead of xmlAddChild: xmlAddChild MERGES adjacent text nodes` |
|     - |    42 | ` * and frees the merged-away node, which would dangle any PHP wrapper (and` |
|     - |    43 | ` * violates DOM semantics, which php follows -- appendChild never merges).` |
|     - |    44 | ` * Unlinked nodes are parked on the owning phl_xmldoc's orphan set so they` |
|     - |    45 | ` * are freed with the document at VM reset/release.` |
|     - |    46 | ` */` |
|     - |    47 |  |
|     - |    48 | `/* One native method body. Its receiver's node is DomThisNode(pCtx); apArg is` |
|     - |    49 | ` * php's own argument list, already screened against the declared signature. */` |
|     - |    50 | `#define DOM_METHOD(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     - |    51 |  |
|     - |    52 | `/* The two slots every wrapper carries, and the document's identity cache. */` |
|     - |    53 | `#define DOM_RES   "__res"` |
|     - |    54 | `#define DOM_DOC   "__doc"` |
|     - |    55 | `#define DOM_NODES "__nodes"` |
|     - |    56 | `/* The base-class => user-class table registerNodeClass writes (defined here` |
|     - |    57 | ` * because the document CLONE, far above it, carries the table across). */` |
|     - |    58 | `#define DOM_NCLS  "__ncls"` |
|     - |    59 |  |
|     - |    60 | `/*` |
|     - |    61 | ` * The DOCUMENT's own directives: php's seven boolean properties, real slots` |
|     - |    62 | `` * here (as `preserveWhiteSpace` and `formatOutput` already were) because their`` |
|     - |    63 | ` * value is the extension's own state and not a question about the tree.  Four` |
|     - |    64 | ` * of them are read by every parse and one by every refusal, and a clone of a` |
|     - |    65 | ` * document carries the whole block across rather than resetting it to the class` |
|     - |    66 | ` * defaults.` |
|     - |    67 | ` */` |
|     - |    68 | `static const char * const azDomDocFlag[] = {` |
|     - |    69 | `	"preserveWhiteSpace", "formatOutput", "validateOnParse",` |
|     - |    70 | `	"resolveExternals", "substituteEntities", "recover", "strictErrorChecking"` |
|     - |    71 | `};` |
|     - |    72 |  |
|     - |    73 | `/*` |
|     - |    74 | ` * php's DOMException carries the DOM level-2 error CODE beside its sentence --` |
|     - |    75 | `` * `catch (DOMException $e) { if ($e->getCode() === DOM_NOT_FOUND_ERR) ... }` is`` |
|     - |    76 | ` * how a caller tells one refusal from another, and the sentence is only a` |
|     - |    77 | ` * sentence.  Every throw below states its code; DOM_PHP_ERR (0) is php's own` |
|     - |    78 | ` * "not a DOM error" and no refusal here uses it.` |
|     - |    79 | ` */` |
|     - |    80 | `#define DOM_ERR_INDEX_SIZE     1` |
|     - |    81 | `#define DOM_ERR_HIERARCHY      3` |
|     - |    82 | `#define DOM_ERR_WRONG_DOC      4` |
|     - |    83 | `#define DOM_ERR_INVALID_CHAR   5` |
|     - |    84 | `#define DOM_ERR_NO_MOD         7` |
|     - |    85 | `#define DOM_ERR_NOT_FOUND      8` |
|     - |    86 | `#define DOM_ERR_NOT_SUPPORTED  9` |
|     - |    87 | `#define DOM_ERR_INVALID_STATE 11` |
|     - |    88 | `#define DOM_ERR_SYNTAX        12` |
|     - |    89 | `#define DOM_ERR_NAMESPACE     14` |
|     - |    90 | `/* The sentence php prints for each -- so a refusal that travels as a code can` |
|     - |    91 | ` * be raised from one place. */` |
|   466 |    92 | `static const char * DomErrText(int iCode)` |
|     1 |    93 | `{` |
|   467 |    94 | `	switch( iCode ){` |
|    33 |    95 | `	case DOM_ERR_INDEX_SIZE:   return "Index Size Error";` |
|    63 |    96 | `	case DOM_ERR_HIERARCHY:    return "Hierarchy Request Error";` |
|    51 |    97 | `	case DOM_ERR_WRONG_DOC:    return "Wrong Document Error";` |
|    89 |    98 | `	case DOM_ERR_INVALID_CHAR: return "Invalid Character Error";` |
|    19 |    99 | `	case DOM_ERR_NO_MOD:       return "No Modification Allowed Error";` |
|    11 |   100 | `	case DOM_ERR_NOT_SUPPORTED: return "Not Supported Error";` |
|     3 |   101 | `	case DOM_ERR_INVALID_STATE: return "Invalid State Error";` |
|     5 |   102 | `	case DOM_ERR_SYNTAX:       return "Syntax Error";` |
|   139 |   103 | `	case DOM_ERR_NAMESPACE:    return "Namespace Error";` |
|    63 |   104 | `	default:                   return "Not Found Error";` |
|     - |   105 | `	}` |
|   234 |   106 | `}` |
|     - |   107 | `/* Forward: the refusal has to ask the receiver's document for its mode --` |
|     - |   108 | ` * and check that what the slot holds IS a document. */` |
|     - |   109 | `static ph7_class_instance * DomThisDoc(ph7_context *pCtx);` |
|     - |   110 | `static phl_domnode * DomResOf(ph7_class_instance *pObj);` |
|     - |   111 | `/*` |
|     - |   112 | ` * A DOM refusal, in whichever of php's TWO modes the document is in.` |
|     - |   113 | ` *` |
|     - |   114 | `` * `$doc->strictErrorChecking` (true by default) decides whether a refusal is an`` |
|     - |   115 | ` * exception or a warning: with it off, php raises the SAME sentence as an` |
|     - |   116 | ` * E_WARNING under the method's own name and the method answers instead of` |
|     - |   117 | ``  * unwinding. The two answers it gives are the two this file needs -- `false` `` |
|     - |   118 | ` * from a method that returns something, and NOTHING from one php declares` |
|     - |   119 | `` * `void` -- so the mode is one call with the answer as its argument.`` |
|     - |   120 | ` *` |
|     - |   121 | ``  * The flag is document state rather than tree state: it survives a `loadXML()` `` |
|     - |   122 | ` * onto the same object, and a clone carries it. It is read off the RECEIVER's` |
|     - |   123 | ` * document -- the argument's own is not consulted even when the refusal is` |
|     - |   124 | `` * about that argument -- with exactly one exception, `adoptNode`, which reads`` |
|     - |   125 | ` * the argument's and is passed it explicitly.` |
|     - |   126 | ` *` |
|     - |   127 | ` * And not every refusal consults it at all: php passes a hardcoded "strict" at` |
|     - |   128 | `` * `setAttribute` and `toggleAttribute`, which throw whatever the flag says.`` |
|     - |   129 | ` * Those call DomThrowAlways.` |
|     - |   130 | ` */` |
|     - |   131 | `#define DOM_REFUSE_FALSE 0   /* the method answers false */` |
|     - |   132 | `#define DOM_REFUSE_VOID  1   /* the method answers nothing (php declares it void) */` |
|   408 |   133 | `static int DomThrowAlways(ph7_context *pCtx,int iCode)` |
|     1 |   134 | `{` |
|   409 |   135 | `	return PH7_VmThrowExceptionCode(pCtx,"DOMException",(sxi32)iCode,"%s",DomErrText(iCode));` |
|     1 |   136 | `}` |
|   432 |   137 | `static int DomThrowFor(ph7_context *pCtx,ph7_class_instance *pDoc,int iCode,int iAnswer)` |
|     1 |   138 | `{` |
|     - |   139 | `	/* Only a DOCUMENT carries the flag: a constructed ownerless node's $__doc` |
|     - |   140 | `	 * slot points at its own holder object, and php is always strict there --` |
|     - |   141 | `	 * there is no document to have said otherwise. */` |
|   433 |   142 | `	phl_domnode *pDocNd = pDoc ? DomResOf(pDoc) : 0;` |
|   433 |   143 | `	xmlNodePtr pDocNode = pDocNd ? (xmlNodePtr)pDocNd->pNode : 0;` |
|   432 |   144 | `	if( pDocNode` |
|   406 |   145 | `	 && (pDocNode->type != XML_DOCUMENT_NODE && pDocNode->type != XML_HTML_DOCUMENT_NODE) ){` |
|    15 |   146 | `		pDoc = 0;` |
|     7 |   147 | `	}` |
|   433 |   148 | `	if( pDoc && !PH7_NativeAttrTruthy(pDoc,"strictErrorChecking") ){` |
|     - |   149 | ``		/* The context prints php's own `Class::method(): ` in front of it. */`` |
|    55 |   150 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,DomErrText(iCode));` |
|    55 |   151 | `		if( iAnswer == DOM_REFUSE_FALSE ){` |
|    37 |   152 | `			ph7_result_bool(pCtx,0);` |
|    18 |   153 | `		}` |
|    55 |   154 | `		return PH7_OK;` |
|     - |   155 | `	}` |
|   379 |   156 | `	return DomThrowAlways(pCtx,iCode);` |
|   217 |   157 | `}` |
|   324 |   158 | `static int DomThrow(ph7_context *pCtx,int iCode)` |
|     1 |   159 | `{` |
|   325 |   160 | `	return DomThrowFor(pCtx,DomThisDoc(pCtx),iCode,DOM_REFUSE_FALSE);` |
|     1 |   161 | `}` |
|    98 |   162 | `static int DomThrowVoid(ph7_context *pCtx,int iCode)` |
|     1 |   163 | `{` |
|    99 |   164 | `	return DomThrowFor(pCtx,DomThisDoc(pCtx),iCode,DOM_REFUSE_VOID);` |
|     1 |   165 | `}` |
|     - |   166 | `/* Property names are byte-exact in php, and every name that reaches here is` |
|     - |   167 | ` * NUL-terminated (ph7_value_to_string null-appends). */` |
| 54828 |   168 | `static int DomNameIs(const char *zName,const char *zWant)` |
|     5 |   169 | `{` |
| 54833 |   170 | `	sxu32 n = (sxu32)SyStrlen(zWant);` |
| 54833 |   171 | `	return SyStrlen(zName) == n && SyStrncmp(zName,zWant,n) == 0;` |
|     5 |   172 | `}` |
|     - |   173 | `/* ...and the ONE name the DOM matches case-insensitively: insertAdjacent*'s` |
|     - |   174 | `` * `$where` word ("BeforeBegin" works), php's zend_string_equals_literal_ci. */`` |
|   130 |   175 | `static int DomNameIsCi(const char *zName,const char *zWant)` |
|     1 |   176 | `{` |
|   131 |   177 | `	sxu32 n = (sxu32)SyStrlen(zWant);` |
|   131 |   178 | `	return SyStrlen(zName) == n && SyStrnicmp(zName,zWant,n) == 0;` |
|     1 |   179 | `}` |
|     - |   180 | `/* The handle behind an instance's $__res, or NULL for anything else. */` |
| 15527 |   181 | `static phl_domnode * DomResOf(ph7_class_instance *pObj)` |
|     5 |   182 | `{` |
| 15532 |   183 | `	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,DOM_RES) : 0;` |
| 15532 |   184 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|    39 |   185 | `		return 0;` |
|     - |   186 | `	}` |
| 15494 |   187 | `	return (phl_domnode *)pVal->x.pOther;` |
|  7769 |   188 | `}` |
|     - |   189 | `/* The receiver of a native method, and the two things every body wants from it. */` |
| 11479 |   190 | `static phl_domnode * DomThisNode(ph7_context *pCtx)` |
|     5 |   191 | `{` |
| 11484 |   192 | `	return DomResOf(PH7_ContextThis(pCtx));` |
|     5 |   193 | `}` |
|  7720 |   194 | `static ph7_class_instance * DomThisDoc(ph7_context *pCtx)` |
|     5 |   195 | `{` |
|  7725 |   196 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  7725 |   197 | `	return pThis ? PH7_NativeAttrObj(pThis,DOM_DOC) : 0;` |
|     5 |   198 | `}` |
|     - |   199 | `/* A fresh handle onto one node of pShell's tree. Freed with the VM allocator. */` |
|  6344 |   200 | `static phl_domnode * DomNewRes(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode)` |
|     5 |   201 | `{` |
|  6349 |   202 | `	phl_domnode *pWrap = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|  6349 |   203 | `	if( pWrap ){` |
|  6349 |   204 | `		pWrap->pShell = pShell;` |
|  6349 |   205 | `		pWrap->pNode = pNode;` |
|  3172 |   206 | `	}` |
|  6349 |   207 | `	return pWrap;` |
|     5 |   208 | `}` |
|     - |   209 | `/* Store a handle in an instance's $__res slot. */` |
|  6280 |   210 | `static void DomSetRes(ph7_vm *pVm,ph7_class_instance *pObj,phl_domnode *pRes)` |
|     5 |   211 | `{` |
|     - |   212 | `	ph7_value sVal;` |
|  6285 |   213 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|  6285 |   214 | `	sVal.x.pOther = pRes;` |
|  6285 |   215 | `	sVal.iFlags = MEMOBJ_RES;` |
|  6285 |   216 | `	PH7_NativeSetProp(&(*pVm),pObj,DOM_RES,sizeof(DOM_RES)-1,&sVal);` |
|  6285 |   217 | `}` |
|     - |   218 | `/*` |
|     - |   219 | ` * The document's identity cache, materialized and separated from any copy that` |
|     - |   220 | ` * shares it. Same three moves a native class always needs to own an array slot` |
|     - |   221 | ` * (WeakMap's WmStore is the other one).` |
|     - |   222 | ` */` |
|  4990 |   223 | `static ph7_hashmap * DomCache(ph7_vm *pVm,ph7_class_instance *pDoc)` |
|     5 |   224 | `{` |
|  4995 |   225 | `	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NODES) : 0;` |
|  4995 |   226 | `	if( pSlot == 0 ){` |
|   ! 0 |   227 | `		return 0;` |
|     - |   228 | `	}` |
|  4995 |   229 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   265 |   230 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |   231 | `			return 0;` |
|     - |   232 | `		}` |
|   132 |   233 | `	}` |
|  4995 |   234 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|  2500 |   235 | `}` |
|     - |   236 | `/* php's class for a node type. Anything else is a plain DOMNode, as before. */` |
|  2738 |   237 | `static const char * DomClassOfKind(int iKind)` |
|     5 |   238 | `{` |
|  2743 |   239 | `	switch( iKind ){` |
|  1881 |   240 | `	case XML_ELEMENT_NODE:       return "DOMElement";` |
|   293 |   241 | `	case XML_ATTRIBUTE_NODE:     return "DOMAttr";` |
|   234 |   242 | `	case XML_TEXT_NODE:          return "DOMText";` |
|    41 |   243 | `	case XML_CDATA_SECTION_NODE: return "DOMCdataSection";` |
|    47 |   244 | `	case XML_COMMENT_NODE:       return "DOMComment";` |
|    45 |   245 | `	case XML_PI_NODE:            return "DOMProcessingInstruction";` |
|    93 |   246 | `	case XML_DOCUMENT_FRAG_NODE: return "DOMDocumentFragment";` |
|    35 |   247 | `	case XML_ENTITY_REF_NODE:    return "DOMEntityReference";` |
|    25 |   248 | `	case XML_DTD_NODE:` |
|    52 |   249 | `	case XML_DOCUMENT_TYPE_NODE: return "DOMDocumentType";` |
|     - |   250 | `	/* php has one class for the whole declaration half of a DTD and hands an` |
|     - |   251 | ``	 * ELEMENT declaration the entity's, which is the class a `$doctype->`` |
|     - |   252 | ``	 * childNodes` walk meets. DOMEntity's own readers ask the node's real`` |
|     - |   253 | `	 * type before touching a field, so an element declaration answers null` |
|     - |   254 | `	 * from each of them rather than reading an xmlElement as an xmlEntity. */` |
|    12 |   255 | `	case XML_ENTITY_DECL:` |
|    25 |   256 | `	case XML_ELEMENT_DECL:       return "DOMEntity";` |
|    12 |   257 | `	case XML_NOTATION_NODE:      return "DOMNotation";` |
|   ! 0 |   258 | `	default:                     return "DOMNode";` |
|     - |   259 | `	}` |
|  1374 |   260 | `}` |
|     - |   261 | `/*` |
|     - |   262 | ` * The nodeType php reports, which is not always libxml's own.` |
|     - |   263 | ` *` |
|     - |   264 | ` * The two numberings were built to agree -- a text node is 3 in both -- but` |
|     - |   265 | ` * libxml parses a DOCTYPE into an XML_DTD_NODE (14) where the DOM's number for` |
|     - |   266 | ` * one is DOCUMENT_TYPE_NODE (10), and php reports the DOM's.  So` |
|     - |   267 | `` * `$n->nodeType === XML_DOCUMENT_TYPE_NODE` -- the way a walk tells the doctype`` |
|     - |   268 | `` * from an element without a `get_class` -- was FALSE here for every document`` |
|     - |   269 | ` * carrying one.` |
|     - |   270 | ` */` |
|   100 |   271 | `static int DomNodeTypeOf(xmlNodePtr pNode)` |
|     1 |   272 | `{` |
|   101 |   273 | `	if( pNode == 0 ){` |
|   ! 0 |   274 | `		return 0;` |
|     - |   275 | `	}` |
|   101 |   276 | `	return pNode->type == XML_DTD_NODE ? (int)XML_DOCUMENT_TYPE_NODE : (int)pNode->type;` |
|    51 |   277 | `}` |
|     - |   278 | `/* Defined with registerNodeClass below, which is the only thing that makes the` |
|     - |   279 | ` * answer anything other than DomClassOfKind's. */` |
|     - |   280 | `static const char * DomWrapClassName(ph7_vm *pVm,ph7_class_instance *pDoc,int iKind,` |
|     - |   281 | `	SyBlob *pOut);` |
|     - |   282 | `/*` |
|     - |   283 | ` * The wrapper object for one node of pDoc's tree -- the same one every time,` |
|     - |   284 | `` * which is what makes `$doc->documentElement === $doc->documentElement` true.`` |
|     - |   285 | ` *` |
|     - |   286 | ` * BORROWED: the cache owns the returned instance. A caller that hands it to PHP` |
|     - |   287 | ` * goes through DomResultWrap (ph7_result_value takes its own reference); a` |
|     - |   288 | ` * caller that stores it uses PH7_NativeSetAttrObj, which does the same. Neither` |
|     - |   289 | ` * unrefs.` |
|     - |   290 | ` */` |
|  4508 |   291 | `static ph7_class_instance * DomWrap(ph7_vm *pVm,ph7_class_instance *pDoc,` |
|     - |   292 | `	phl_xmldoc *pShell,xmlNodePtr pNode)` |
|     5 |   293 | `{` |
|     - |   294 | `	ph7_hashmap *pCache;` |
|  4513 |   295 | `	ph7_hashmap_node *pEntry = 0;` |
|     - |   296 | `	ph7_class_instance *pObj;` |
|     - |   297 | `	ph7_class *pClass;` |
|     - |   298 | `	phl_domnode *pRes;` |
|     - |   299 | `	const char *zClass;` |
|     - |   300 | `	ph7_value sKey,sVal;` |
|     - |   301 | `	SyBlob sName;` |
|  4513 |   302 | `	if( pNode == 0 \|\| pDoc == 0 ){` |
|   141 |   303 | `		return 0;` |
|     - |   304 | `	}` |
|  4373 |   305 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|     - |   306 | `		/* The document is its own wrapper: php answers the SAME DOMDocument. */` |
|    19 |   307 | `		return pDoc;` |
|     - |   308 | `	}` |
|  4355 |   309 | `	pCache = DomCache(&(*pVm),pDoc);` |
|  4355 |   310 | `	if( pCache == 0 ){` |
|   ! 0 |   311 | `		return 0;` |
|     - |   312 | `	}` |
|  4355 |   313 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|  4355 |   314 | `	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|  1637 |   315 | `		ph7_value *pHit = HashmapExtractNodeValue(pEntry);` |
|  1637 |   316 | `		if( pHit && (pHit->iFlags & MEMOBJ_OBJ) ){` |
|  1637 |   317 | `			PH7_MemObjRelease(&sKey);` |
|  1637 |   318 | `			return (ph7_class_instance *)pHit->x.pOther;` |
|     - |   319 | `		}` |
|   ! 0 |   320 | `	}` |
|     - |   321 | `	/* php's class for the kind, unless this document has REGISTERED another` |
|     - |   322 | `	 * one for it (registerNodeClass). The name may live in sName's buffer, so` |
|     - |   323 | `	 * the blob outlives the lookup. */` |
|  2723 |   324 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|  2723 |   325 | `	zClass = DomWrapClassName(&(*pVm),pDoc,(int)pNode->type,&sName);` |
|  2723 |   326 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|  2723 |   327 | `	SyBlobRelease(&sName);` |
|  2723 |   328 | `	pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|  2723 |   329 | `	pRes = pObj ? DomNewRes(&(*pVm),pShell,pNode) : 0;` |
|  2723 |   330 | `	if( pRes == 0 ){` |
|   ! 0 |   331 | `		if( pObj ){` |
|   ! 0 |   332 | `			PH7_ClassInstanceUnref(pObj);` |
|   ! 0 |   333 | `		}` |
|   ! 0 |   334 | `		PH7_MemObjRelease(&sKey);` |
|   ! 0 |   335 | `		return 0;` |
|     - |   336 | `	}` |
|  2723 |   337 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|  2723 |   338 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|  2723 |   339 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|  2723 |   340 | `	sVal.x.pOther = pObj;` |
|  2723 |   341 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|  2723 |   342 | `	PH7_HashmapInsert(pCache,&sKey,&sVal);   /* takes the cache's reference */` |
|  2723 |   343 | `	PH7_MemObjRelease(&sKey);` |
|  2723 |   344 | `	PH7_ClassInstanceUnref(pObj);            /* ...and the cache is now the owner */` |
|  2723 |   345 | `	return pObj;` |
|  2259 |   346 | `}` |
|     - |   347 | `/* Answer a borrowed instance (or NULL) from a native method. */` |
|  4064 |   348 | `static int DomResultWrap(ph7_context *pCtx,ph7_class_instance *pObj)` |
|     5 |   349 | `{` |
|     - |   350 | `	ph7_value sRes;` |
|  4069 |   351 | `	if( pObj == 0 ){` |
|    71 |   352 | `		ph7_result_null(pCtx);` |
|    71 |   353 | `		return PH7_OK;` |
|     - |   354 | `	}` |
|  3999 |   355 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|  3999 |   356 | `	sRes.x.pOther = pObj;` |
|  3999 |   357 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|  3999 |   358 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|  3999 |   359 | `	return PH7_OK;` |
|  2037 |   360 | `}` |
|     - |   361 | `/* The common tail: wrap a node of the RECEIVER's document and answer it. */` |
|  3682 |   362 | `static int DomResultNodeOf(ph7_context *pCtx,phl_domnode *pNd,xmlNodePtr pNode)` |
|     5 |   363 | `{` |
|  3687 |   364 | `	if( pNd == 0 \|\| pNode == 0 ){` |
|   208 |   365 | `		ph7_result_null(pCtx);` |
|   208 |   366 | `		return PH7_OK;` |
|     - |   367 | `	}` |
|  3481 |   368 | `	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNode));` |
|  1846 |   369 | `}` |
|     - |   370 | `/* The phl_domnode behind a DOMNode-typed ARGUMENT (already screened by ZPP). */` |
|  1850 |   371 | `static phl_domnode * DomObjArg(ph7_value *pVal)` |
|     3 |   372 | `{` |
|  1853 |   373 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     5 |   374 | `		return 0;` |
|     - |   375 | `	}` |
|  1849 |   376 | `	return DomResOf((ph7_class_instance *)pVal->x.pOther);` |
|   928 |   377 | `}` |
|     - |   378 | `/* ...and the DOCUMENT object it belongs to, which is where its wrapper is` |
|     - |   379 | `` * cached and what its `ownerDocument` answers. */`` |
|   598 |   380 | `static ph7_class_instance * DomObjArgDoc(ph7_value *pVal)` |
|     3 |   381 | `{` |
|   601 |   382 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |   383 | `		return 0;` |
|     - |   384 | `	}` |
|   601 |   385 | `	return PH7_NativeAttrObj((ph7_class_instance *)pVal->x.pOther,DOM_DOC);` |
|   302 |   386 | `}` |
|     - |   387 | `/* The slot a DOMNameSpaceNode carries beside its own two: the element that` |
|     - |   388 | ` * MAKES the declaration, which is php's parentNode for one. */` |
|     - |   389 | `#define DOM_NS_OWNER "__owner"` |
|     - |   390 |  |
|     - |   391 | `/* The element a DOMNameSpaceNode argument was found on, or NULL for anything` |
|     - |   392 | ` * else -- the slot exists on that class alone. */` |
|    16 |   393 | `static phl_domnode * DomNsNodeOwner(ph7_value *pVal)` |
|     1 |   394 | `{` |
|     - |   395 | `	ph7_class_instance *pObj;` |
|    17 |   396 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |   397 | `		return 0;` |
|     - |   398 | `	}` |
|    17 |   399 | `	pObj = (ph7_class_instance *)pVal->x.pOther;` |
|    17 |   400 | `	return DomResOf(PH7_NativeAttrObj(pObj,DOM_NS_OWNER));` |
|     9 |   401 | `}` |
|     - |   402 | `/* Orphan bookkeeping: nodes not linked into their tree but still owned */` |
|  1354 |   403 | `static void DomOrphanAdd(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|     3 |   404 | `{` |
|  1357 |   405 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|     - |   406 | `	sxu32 n;` |
| 15485 |   407 | `	for( n = 0 ; n < SySetUsed(&pShell->aOrphans) ; ++n ){` |
| 14131 |   408 | `		if( apOrphan[n] == pNode ){` |
|   ! 0 |   409 | `			return;` |
|     - |   410 | `		}` |
|  7067 |   411 | `	}` |
|  1357 |   412 | `	SySetPut(&pShell->aOrphans,(const void *)&pNode);` |
|   680 |   413 | `}` |
|   516 |   414 | `static void DomOrphanRemove(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|     3 |   415 | `{` |
|   519 |   416 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|   519 |   417 | `	sxu32 n,nUsed = SySetUsed(&pShell->aOrphans);` |
|  4353 |   418 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|  4353 |   419 | `		if( apOrphan[n] == pNode ){` |
|   519 |   420 | `			apOrphan[n] = apOrphan[nUsed-1];` |
|   519 |   421 | `			SySetTruncate(&pShell->aOrphans,nUsed-1);` |
|   519 |   422 | `			return;` |
|     - |   423 | `		}` |
|  1918 |   424 | `	}` |
|   261 |   425 | `}` |
|     - |   426 | `/* Detach a node from wherever it is (tree or orphan set) prior to linking */` |
|   598 |   427 | `static void DomDetach(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|     3 |   428 | `{` |
|   601 |   429 | `	if( pNode->parent ){` |
|   171 |   430 | `		xmlUnlinkNode(pNode);` |
|    86 |   431 | `	}else{` |
|   431 |   432 | `		DomOrphanRemove(pShell,pNode);` |
|     - |   433 | `	}` |
|   601 |   434 | `}` |
|     - |   435 | `/* Raw child-list splicing (no text-node merging -- DOM/php semantics) */` |
|   494 |   436 | `static void DomLinkLast(xmlNodePtr pParent,xmlNodePtr pChild)` |
|     2 |   437 | `{` |
|   496 |   438 | `	pChild->parent = pParent;` |
|   496 |   439 | `	pChild->next = 0;` |
|   496 |   440 | `	if( pParent->last ){` |
|   153 |   441 | `		pParent->last->next = pChild;` |
|   153 |   442 | `		pChild->prev = pParent->last;` |
|    77 |   443 | `	}else{` |
|   344 |   444 | `		pParent->children = pChild;` |
|   344 |   445 | `		pChild->prev = 0;` |
|     - |   446 | `	}` |
|   496 |   447 | `	pParent->last = pChild;` |
|   496 |   448 | `}` |
|    78 |   449 | `static void DomLinkBefore(xmlNodePtr pParent,xmlNodePtr pChild,xmlNodePtr pRef)` |
|     1 |   450 | `{` |
|    79 |   451 | `	pChild->parent = pParent;` |
|    79 |   452 | `	pChild->next = pRef;` |
|    79 |   453 | `	pChild->prev = pRef->prev;` |
|    79 |   454 | `	if( pRef->prev ){` |
|    41 |   455 | `		pRef->prev->next = pChild;` |
|    21 |   456 | `	}else{` |
|    39 |   457 | `		pParent->children = pChild;` |
|     - |   458 | `	}` |
|    79 |   459 | `	pRef->prev = pChild;` |
|    79 |   460 | `}` |
|     - |   461 |  |
|     - |   462 | `/* ===== Node introspection: the readers behind __get ===== */` |
|     - |   463 |  |
|     - |   464 | `/* php's nodeName rules */` |
|   648 |   465 | `static void DomNodeName(ph7_context *pCtx,xmlNodePtr pNode)` |
|     3 |   466 | `{` |
|   651 |   467 | `	if( pNode == 0 ){` |
|   ! 0 |   468 | `		ph7_result_string(pCtx,"",0);` |
|   ! 0 |   469 | `		return;` |
|     - |   470 | `	}` |
|   651 |   471 | `	switch( pNode->type ){` |
|    13 |   472 | `	case XML_TEXT_NODE:          ph7_result_string(pCtx,"#text",(int)sizeof("#text")-1); break;` |
|     7 |   473 | `	case XML_CDATA_SECTION_NODE: ph7_result_string(pCtx,"#cdata-section",(int)sizeof("#cdata-section")-1); break;` |
|     5 |   474 | `	case XML_COMMENT_NODE:       ph7_result_string(pCtx,"#comment",(int)sizeof("#comment")-1); break;` |
|     5 |   475 | `	case XML_HTML_DOCUMENT_NODE:` |
|    11 |   476 | `	case XML_DOCUMENT_NODE:      ph7_result_string(pCtx,"#document",(int)sizeof("#document")-1); break;` |
|     5 |   477 | `	case XML_DOCUMENT_FRAG_NODE: ph7_result_string(pCtx,"#document-fragment",(int)sizeof("#document-fragment")-1); break;` |
|   306 |   478 | `	default:` |
|   612 |   479 | `		if( (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE)` |
|   598 |   480 | `			&& pNode->ns && pNode->ns->prefix ){` |
|   229 |   481 | `			ph7_result_string_format(pCtx,"%s:%s",(const char *)pNode->ns->prefix,(const char *)pNode->name);` |
|   115 |   482 | `		}else{` |
|   387 |   483 | `			ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|     - |   484 | `		}` |
|   612 |   485 | `		break;` |
|     - |   486 | `	}` |
|   327 |   487 | `}` |
|     - |   488 | `/*` |
|     - |   489 | ` * The three names a namespaced document reads on every node.` |
|     - |   490 | ` *` |
|     - |   491 | `` * `namespaceURI` and `localName` are php's `?string` -- null for a node that`` |
|     - |   492 | ` * cannot carry a name in a namespace at all (a text node, a comment, a PI, the` |
|     - |   493 | `` * document itself) -- while `prefix` is a plain `string` that answers "" there,`` |
|     - |   494 | ` * which is why one of the three cannot be derived from the other two.` |
|     - |   495 | ` *` |
|     - |   496 | ` * All three are gated on the node KIND before anything is read, which is not` |
|     - |   497 | ` * only php's rule but a memory-safety one: only element and attribute nodes are` |
|     - |   498 | ``  * xmlNode/xmlAttr-shaped, and `->ns` on an xmlDoc aliases its `compression` `` |
|     - |   499 | ` * int, on an xmlDtd its notation table.  Reading it there and dereferencing the` |
|     - |   500 | `` * result is a SIGSEGV out of `$doc->namespaceURI` -- an ordinary property read.`` |
|     - |   501 | ` */` |
|   632 |   502 | `static int DomHasNsSlot(xmlNodePtr pNode)` |
|     1 |   503 | `{` |
|   633 |   504 | `	return pNode != 0` |
|   948 |   505 | `		&& (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE);` |
|     1 |   506 | `}` |
|   370 |   507 | `static void DomNamespaceUri(ph7_context *pCtx,xmlNodePtr pNode)` |
|     1 |   508 | `{` |
|   371 |   509 | `	if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->href ){` |
|   275 |   510 | `		ph7_result_string(pCtx,(const char *)pNode->ns->href,-1);` |
|   138 |   511 | `	}else{` |
|    97 |   512 | `		ph7_result_null(pCtx);` |
|     - |   513 | `	}` |
|   371 |   514 | `}` |
|   120 |   515 | `static void DomPrefix(ph7_context *pCtx,xmlNodePtr pNode)` |
|     1 |   516 | `{` |
|   121 |   517 | `	if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->prefix ){` |
|    67 |   518 | `		ph7_result_string(pCtx,(const char *)pNode->ns->prefix,-1);` |
|    34 |   519 | `	}else{` |
|    55 |   520 | `		ph7_result_string(pCtx,"",0);` |
|     - |   521 | `	}` |
|   121 |   522 | `}` |
|   118 |   523 | `static void DomLocalName(ph7_context *pCtx,xmlNodePtr pNode)` |
|     1 |   524 | `{` |
|   119 |   525 | `	if( DomHasNsSlot(pNode) ){` |
|   107 |   526 | `		ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|    54 |   527 | `	}else{` |
|    13 |   528 | `		ph7_result_null(pCtx);` |
|     - |   529 | `	}` |
|   119 |   530 | `}` |
|     - |   531 | ``/* php's `isConnected`: is the node's root the DOCUMENT? A node built by a`` |
|     - |   532 | `` * create* factory carries the document as its `ownerDocument` from birth, so`` |
|     - |   533 | ` * that property cannot answer this and a program testing it reads true for a` |
|     - |   534 | ` * node it has not appended yet. */` |
|    96 |   535 | `static int DomIsConnected(xmlNodePtr pNode)` |
|     1 |   536 | `{` |
|    97 |   537 | `	xmlNodePtr pRoot = pNode;` |
|    97 |   538 | `	if( pRoot == 0 ){` |
|   ! 0 |   539 | `		return 0;` |
|     - |   540 | `	}` |
|   185 |   541 | `	while( pRoot->parent ){` |
|    89 |   542 | `		pRoot = pRoot->parent;` |
|     1 |   543 | `	}` |
|    97 |   544 | `	return pRoot->type == XML_DOCUMENT_NODE \|\| pRoot->type == XML_HTML_DOCUMENT_NODE;` |
|    49 |   545 | `}` |
|     - |   546 | `/*` |
|     - |   547 | ` * php's nodeValue, which is null for every node kind that has no value of its` |
|     - |   548 | ` * own -- the document, a doctype, a fragment, an entity DECLARATION and an` |
|     - |   549 | `` * entity REFERENCE all answer null, where `textContent` on the same node walks`` |
|     - |   550 | ` * its children and answers a string. (The element case is php's own` |
|     - |   551 | ` * convenience: DOM says an element has no node value.)` |
|     - |   552 | ` */` |
|   266 |   553 | `static void DomNodeValue(ph7_context *pCtx,xmlNodePtr pNode)` |
|     3 |   554 | `{` |
|     - |   555 | `	xmlChar *zContent;` |
|   269 |   556 | `	if( pNode == 0 ){` |
|   ! 0 |   557 | `		ph7_result_null(pCtx);` |
|   ! 0 |   558 | `		return;` |
|     - |   559 | `	}` |
|   269 |   560 | `	switch( pNode->type ){` |
|    83 |   561 | `	case XML_TEXT_NODE:` |
|     - |   562 | `	case XML_COMMENT_NODE:` |
|     - |   563 | `	case XML_CDATA_SECTION_NODE:` |
|     - |   564 | `	case XML_PI_NODE:` |
|     - |   565 | `		/* The CONTENT POINTER itself, not xmlNodeGetContent's copy: a null` |
|     - |   566 | `		 * pointer -- the omitted-argument constructor's state -- reads NULL` |
|     - |   567 | `		 * where an empty string reads "", and newer libxml's` |
|     - |   568 | `		 * xmlNodeGetContent papers over exactly that difference (2.13 answers` |
|     - |   569 | `		 * "" for both, 2.9 answers NULL for the pointer). */` |
|   168 |   570 | `		if( pNode->content == 0 ){` |
|    15 |   571 | `			ph7_result_null(pCtx);` |
|     8 |   572 | `		}else{` |
|   154 |   573 | `			ph7_result_string(pCtx,(const char *)pNode->content,-1);` |
|     - |   574 | `		}` |
|   168 |   575 | `		return;` |
|    37 |   576 | `	case XML_ATTRIBUTE_NODE:` |
|     - |   577 | `	case XML_ELEMENT_NODE:` |
|    76 |   578 | `		break;` |
|    13 |   579 | `	default:` |
|    27 |   580 | `		ph7_result_null(pCtx);` |
|    27 |   581 | `		return;` |
|     - |   582 | `	}` |
|    76 |   583 | `	zContent = xmlNodeGetContent(pNode);` |
|    76 |   584 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|    76 |   585 | `	if( zContent ){` |
|    76 |   586 | `		xmlFree(zContent);` |
|    37 |   587 | `	}` |
|   136 |   588 | `}` |
|     - |   589 | ``/* The `data` property's reading of the same content: php COERCES there, so a`` |
|     - |   590 | ` * NULL content pointer -- the omitted-argument constructors' state -- reads ""` |
|     - |   591 | `` * from `$node->data` and null from `$node->nodeValue`, one node, two answers. */`` |
|   126 |   592 | `static void DomDataValue(ph7_context *pCtx,xmlNodePtr pNode)` |
|     2 |   593 | `{` |
|   128 |   594 | `	DomNodeValue(pCtx,pNode);` |
|   128 |   595 | `	if( pCtx->pRet->iFlags & MEMOBJ_NULL ){` |
|     7 |   596 | `		ph7_result_string(pCtx,"",0);` |
|     3 |   597 | `	}` |
|   128 |   598 | `}` |
|     - |   599 | `/* php's textContent: the same walk, but a document answers its text too */` |
|    50 |   600 | `static void DomTextContent(ph7_context *pCtx,xmlNodePtr pNode)` |
|     1 |   601 | `{` |
|    51 |   602 | `	xmlChar *zContent = pNode ? xmlNodeGetContent(pNode) : 0;` |
|    51 |   603 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|    51 |   604 | `	if( zContent ){` |
|    43 |   605 | `		xmlFree(zContent);` |
|    21 |   606 | `	}` |
|    51 |   607 | `}` |
|     - |   608 | `/* The two child counts childNodes->length and childElementCount read. */` |
|     - |   609 | `static xmlNodePtr DomRefChildren(xmlNodePtr pNode);` |
|   148 |   610 | `static int DomChildCount(xmlNodePtr pNode,int bElementsOnly)` |
|     2 |   611 | `{` |
|   150 |   612 | `	xmlNodePtr pChild = DomRefChildren(pNode);` |
|   150 |   613 | `	int iCount = 0;` |
|   384 |   614 | `	for( ; pChild ; pChild = pChild->next ){` |
|   236 |   615 | `		if( !bElementsOnly \|\| pChild->type == XML_ELEMENT_NODE ){` |
|   214 |   616 | `			iCount++;` |
|   106 |   617 | `		}` |
|   119 |   618 | `	}` |
|   150 |   619 | `	return iCount;` |
|     2 |   620 | `}` |
|   330 |   621 | `static xmlNodePtr DomChildAt(xmlNodePtr pNode,int iWant)` |
|     2 |   622 | `{` |
|   332 |   623 | `	xmlNodePtr pChild = DomRefChildren(pNode);` |
|   886 |   624 | `	for( ; pChild && iWant > 0 ; pChild = pChild->next ){` |
|   556 |   625 | `		iWant--;` |
|   279 |   626 | `	}` |
|   332 |   627 | `	return pChild;` |
|     2 |   628 | `}` |
|     - |   629 |  |
|     - |   630 | `/* ===== Tree surgery: DOMNode's four mutators ===== */` |
|     - |   631 |  |
|     - |   632 | `/*` |
|     - |   633 | ` * Defined with the namespace machinery below, and declared here because the` |
|     - |   634 | ` * surgery runs it: a node LINKED into a tree loses the declarations its new` |
|     - |   635 | ` * scope already makes, and gains the ones its new scope no longer makes.` |
|     - |   636 | ` * (The namespace section cannot move up because it reads the attribute` |
|     - |   637 | ` * walker; DomDropChildren below is with the property-write machinery it was` |
|     - |   638 | ` * built for, and replaceChildren() runs the same wrapper-preserving drop.)` |
|     - |   639 | ` */` |
|     - |   640 | `static void DomNsOnInsertEx(xmlNodePtr pNode,int bDeep);` |
|     - |   641 | `static void DomDropChildren(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode);` |
|     - |   642 | `/* The attribute machinery, defined with the attribute surface below: the four` |
|     - |   643 | ` * mutators reach it because php's appendChild/insertBefore ATTACH an attribute` |
|     - |   644 | ` * argument as a property rather than splicing it among the children. */` |
|     - |   645 | `static xmlAttrPtr DomAttrByLocal(xmlNodePtr pElem,const char *zName);` |
|     - |   646 | `static xmlAttrPtr DomAttrByNs(xmlNodePtr pElem,const xmlChar *zUri,const char *zLocal);` |
|     - |   647 | `static void DomAttrLinkLast(xmlNodePtr pElem,xmlAttrPtr pAttr);` |
|     - |   648 | `static void DomAttrLinkBefore(xmlNodePtr pElem,xmlAttrPtr pAttr,xmlAttrPtr pRef);` |
|     - |   649 | `static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr);` |
|     - |   650 | `static void DomNsAttrArrive(xmlNodePtr pElem,xmlAttrPtr pAttr);` |
|     - |   651 | `/* The adoptNode wrapper machinery, defined with it below: the insertion doors` |
|     - |   652 | ` * run it too, because php ADOPTS a constructed, ownerless argument -- doc,` |
|     - |   653 | ` * identity-cache home and handle shell all move on the first insertion. */` |
|     - |   654 | `static void DomAdoptWrappers(ph7_vm *pVm,ph7_class_instance *pSrcDoc,` |
|     - |   655 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode);` |
|     - |   656 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot);` |
|     - |   657 | `/*` |
|     - |   658 | ` * An entity REFERENCE's children as php answers them. libxml's re-homing` |
|     - |   659 | ` * CLEARS the raw link when a constructed reference is adopted -- and php's` |
|     - |   660 | ` * raw state stays cleared, which replaceChild's childless-false cell measures` |
|     - |   661 | `` * -- but php's READERS still resolve: `$ref->firstChild` answers the NEW`` |
|     - |   662 | ` * document's declaration for the name, or the predefined five, or nothing.` |
|     - |   663 | ` */` |
|  1058 |   664 | `static xmlNodePtr DomRefChildren(xmlNodePtr pNode)` |
|     4 |   665 | `{` |
|  1058 |   666 | `	if( pNode && pNode->type == XML_ENTITY_REF_NODE` |
|   546 |   667 | `	 && pNode->children == 0 && pNode->doc ){` |
|     6 |   668 | `		return (xmlNodePtr)xmlGetDocEntity(pNode->doc,pNode->name);` |
|     - |   669 | `	}` |
|  1057 |   670 | `	return pNode ? pNode->children : 0;` |
|   533 |   671 | `}` |
|     - |   672 | `/*` |
|     - |   673 | ` * Take an OWNERLESS subtree into the receiver's world, php's constructed-node` |
|     - |   674 | ` * adoption: the libxml nodes get the receiver's document (none of their` |
|     - |   675 | ` * strings are dict-interned -- a constructed node's are plain allocations, so` |
|     - |   676 | ` * xmlSetTreeDoc is the whole move), the orphan entry crosses from the limbo` |
|     - |   677 | ` * shell to the receiver's, and every wrapper PHP holds re-homes into the` |
|     - |   678 | ` * receiver's identity cache.  Also the OWNERLESS-to-OWNERLESS merge, where no` |
|     - |   679 | ` * document changes hands but the wrappers still need ONE holder for` |
|     - |   680 | `` * `$a->firstChild === $b` to hold.  The caller has already screened documents:`` |
|     - |   681 | ` * a mismatch here means the argument's is NULL.` |
|     - |   682 | ` */` |
|   462 |   683 | `static void DomAdoptIntoRecv(ph7_context *pCtx,ph7_value *pArgVal,phl_domnode *pArgNd)` |
|     3 |   684 | `{` |
|   465 |   685 | `	ph7_vm *pVm = pCtx->pVm;` |
|   465 |   686 | `	ph7_class_instance *pSrcHolder = DomObjArgDoc(pArgVal);` |
|   465 |   687 | `	ph7_class_instance *pDstHolder = DomThisDoc(pCtx);` |
|   465 |   688 | `	phl_domnode *pRecv = DomThisNode(pCtx);` |
|   465 |   689 | `	xmlNodePtr pNode = (xmlNodePtr)pArgNd->pNode;` |
|   465 |   690 | `	xmlNodePtr pRecvNode = pRecv ? (xmlNodePtr)pRecv->pNode : 0;` |
|   465 |   691 | `	if( pSrcHolder == pDstHolder \|\| pRecvNode == 0 ){` |
|   433 |   692 | `		return;` |
|     - |   693 | `	}` |
|    33 |   694 | `	if( pNode->doc == 0 && pRecvNode->doc ){` |
|    29 |   695 | `		xmlSetTreeDoc(pNode,pRecvNode->doc);` |
|    14 |   696 | `	}` |
|    33 |   697 | `	if( pArgNd->pShell != pRecv->pShell ){` |
|    29 |   698 | `		DomOrphanRemove(pArgNd->pShell,pNode);` |
|    29 |   699 | `		DomOrphanAdd(pRecv->pShell,pNode);` |
|    29 |   700 | `		pArgNd->pShell = pRecv->pShell;` |
|    14 |   701 | `	}` |
|    33 |   702 | `	DomAdoptWrappers(pVm,pSrcHolder,pDstHolder,pRecv->pShell,pNode);` |
|   234 |   703 | `}` |
|     - |   704 |  |
|     - |   705 | `/*` |
|     - |   706 | ` * php's dom_node_children_valid: the node kinds that can never have children.` |
|     - |   707 | ` * A level-2 mutator on such a receiver answers FALSE with nothing said at all` |
|     - |   708 | ` * -- no warning, no exception -- and answers it BEFORE any other screen, so` |
|     - |   709 | `` * `$text->appendChild($nodeFromAnotherDocument)` is false, not Wrong Document.`` |
|     - |   710 | ` */` |
|  1136 |   711 | `static int DomChildrenValid(xmlNodePtr pNode)` |
|     4 |   712 | `{` |
|  1140 |   713 | `	switch( pNode->type ){` |
|    14 |   714 | `	case XML_TEXT_NODE:` |
|     - |   715 | `	case XML_CDATA_SECTION_NODE:` |
|     - |   716 | `	case XML_PI_NODE:` |
|     - |   717 | `	case XML_COMMENT_NODE:` |
|     - |   718 | `	case XML_DOCUMENT_TYPE_NODE:` |
|     - |   719 | `	case XML_DTD_NODE:` |
|     - |   720 | `	case XML_NOTATION_NODE:` |
|    29 |   721 | `		return 0;` |
|   554 |   722 | `	default:` |
|  1112 |   723 | `		return 1;` |
|     - |   724 | `	}` |
|   572 |   725 | `}` |
|     - |   726 | `/*` |
|     - |   727 | `` * The two ends of the child list php's `firstChild`/`lastChild` answer, and`` |
|     - |   728 | `` * what `hasChildNodes()` asks -- all three through the same screen, so the`` |
|     - |   729 | ` * DOCTYPE (whose declarations libxml really does link as children) answers` |
|     - |   730 | ` * null, null and false the way php's do.` |
|     - |   731 | ` */` |
|   586 |   732 | `static xmlNodePtr DomNodeChildFirst(xmlNodePtr pNode)` |
|     3 |   733 | `{` |
|   589 |   734 | `	if( pNode == 0 \|\| !DomChildrenValid(pNode) ){` |
|     7 |   735 | `		return 0;` |
|     - |   736 | `	}` |
|   583 |   737 | `	return DomRefChildren(pNode);` |
|   296 |   738 | `}` |
|    80 |   739 | `static xmlNodePtr DomNodeChildLast(xmlNodePtr pNode)` |
|     1 |   740 | `{` |
|    81 |   741 | `	if( pNode == 0 \|\| !DomChildrenValid(pNode) ){` |
|     3 |   742 | `		return 0;` |
|     - |   743 | `	}` |
|    79 |   744 | `	return pNode->last ? pNode->last : DomRefChildren(pNode);` |
|    41 |   745 | `}` |
|     - |   746 | `/*` |
|     - |   747 | ` * php's dom_node_is_read_only: the DTD-owned kinds -- an entity reference's` |
|     - |   748 | ` * subtree is the entity's, shared by every reference to it -- and, one clause` |
|     - |   749 | `` * later, a node with NO document: a constructed `new DOMText('t')` that was`` |
|     - |   750 | ` * never adopted refuses the level-2 child-list doors with No Modification` |
|     - |   751 | ` * Allowed where the modern variadic family compares documents instead.` |
|     - |   752 | ` */` |
|   610 |   753 | `static int DomNodeReadOnly(xmlNodePtr pNode)` |
|     3 |   754 | `{` |
|   613 |   755 | `	switch( pNode->type ){` |
|     5 |   756 | `	case XML_ENTITY_REF_NODE:` |
|     - |   757 | `	case XML_ENTITY_NODE:` |
|     - |   758 | `	case XML_DOCUMENT_TYPE_NODE:` |
|     - |   759 | `	case XML_NOTATION_NODE:` |
|     - |   760 | `	case XML_DTD_NODE:` |
|     - |   761 | `	case XML_ELEMENT_DECL:` |
|     - |   762 | `	case XML_ATTRIBUTE_DECL:` |
|     - |   763 | `	case XML_ENTITY_DECL:` |
|    11 |   764 | `		return 1;` |
|   300 |   765 | `	default:` |
|   603 |   766 | `		return pNode->doc == 0;` |
|     - |   767 | `	}` |
|   308 |   768 | `}` |
|     - |   769 | `/* The two screens every level-2 mutator opens with, in php's order: an` |
|     - |   770 | ` * invalid-children receiver answers false in silence, then the read-only` |
|     - |   771 | ` * refusal -- the receiver's own, or that of the parent the CHILD would be` |
|     - |   772 | ` * taken from. Returns non-zero when the caller must stop (result already` |
|     - |   773 | ` * set). */` |
|   428 |   774 | `static int DomMutatorScreen(ph7_context *pCtx,xmlNodePtr pParent,xmlNodePtr pChild,int *pRc)` |
|     3 |   775 | `{` |
|   431 |   776 | `	if( !DomChildrenValid(pParent) ){` |
|    19 |   777 | `		ph7_result_bool(pCtx,0);` |
|    19 |   778 | `		*pRc = PH7_OK;` |
|    19 |   779 | `		return 1;` |
|     - |   780 | `	}` |
|   410 |   781 | `	if( DomNodeReadOnly(pParent)` |
|   408 |   782 | `	 \|\| (pChild->parent && DomNodeReadOnly(pChild->parent)) ){` |
|    11 |   783 | `		*pRc = DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|    11 |   784 | `		return 1;` |
|     - |   785 | `	}` |
|   403 |   786 | `	return 0;` |
|   217 |   787 | `}` |
|     - |   788 | `/*` |
|     - |   789 | ` * The attribute HALF of appendChild/insertBefore: php hands an attribute` |
|     - |   790 | ` * argument to xmlAddChild, which attaches it as a PROPERTY -- so` |
|     - |   791 | `` * `$el->appendChild($attr)` is a spelling of setAttributeNode, not a child`` |
|     - |   792 | `` * splice (the chunk spliced it among the children and serialized `<r> k=""`,`` |
|     - |   793 | ` * bytes that are not XML). The receiver must be an ELEMENT: a document, a` |
|     - |   794 | ` * fragment or an attribute answers the Hierarchy refusal. An existing` |
|     - |   795 | ``  * attribute of the same name (libxml's name-only match, so a plain `k` `` |
|     - |   796 | ` * displaces a namespaced one -- the setAttributeNode rule) is displaced` |
|     - |   797 | ` * UNLESS it is the argument itself, and the argument always (re)enters at the` |
|     - |   798 | ` * tail of the property list, which is observable: appending an element's own` |
|     - |   799 | ` * first attribute moves it last.` |
|     - |   800 | ` *` |
|     - |   801 | ` * One deliberate divergence, recorded in §7.4: php FREES the displaced` |
|     - |   802 | ` * attribute, so a wrapper held across the call answers Invalid State from` |
|     - |   803 | ` * every later read ("Couldn't fetch DOMAttr" from a method). PHL parks it` |
|     - |   804 | ` * detached and alive -- the same after-state setAttributeNode leaves.` |
|     - |   805 | ` */` |
|    16 |   806 | `static int DomMutatorAttrAttach(ph7_context *pCtx,phl_domnode *pPar,phl_domnode *pChd,` |
|     - |   807 | `	ph7_value *pArg)` |
|     2 |   808 | `{` |
|    18 |   809 | `	xmlNodePtr pElem = (xmlNodePtr)pPar->pNode;` |
|    18 |   810 | `	xmlAttrPtr pAttr = (xmlAttrPtr)pChd->pNode;` |
|     - |   811 | `	xmlAttrPtr pOld;` |
|    18 |   812 | `	if( pElem->type != XML_ELEMENT_NODE ){` |
|   ! 0 |   813 | `		return DomThrow(pCtx,DOM_ERR_HIERARCHY);` |
|     - |   814 | `	}` |
|    11 |   815 | `	pOld = pAttr->ns ? DomAttrByNs(pElem,pAttr->ns->href,(const char *)pAttr->name)` |
|    15 |   816 | `	                 : DomAttrByLocal(pElem,(const char *)pAttr->name);` |
|    18 |   817 | `	if( pOld && pOld != pAttr ){` |
|    10 |   818 | `		xmlUnlinkNode((xmlNodePtr)pOld);` |
|    10 |   819 | `		DomOrphanAdd(pPar->pShell,(xmlNodePtr)pOld);` |
|     4 |   820 | `	}` |
|    18 |   821 | `	DomAttrDetach(pChd->pShell,pAttr);` |
|    18 |   822 | `	DomAttrLinkLast(pElem,pAttr);` |
|    18 |   823 | `	DomNsAttrArrive(pElem,pAttr);` |
|    18 |   824 | `	ph7_result_value(pCtx,pArg);` |
|    18 |   825 | `	return PH7_OK;` |
|    10 |   826 | `}` |
|     - |   827 |  |
|     - |   828 | `/*` |
|     - |   829 | ` * php's refusal taxonomy for linking pChild under pParent, or NULL when the` |
|     - |   830 | ` * link is allowed. The chunk collapsed all of it into one message per method,` |
|     - |   831 | ` * which cost more than a wording: nothing rejected making a node its own` |
|     - |   832 | `` * DESCENDANT, so `$a->firstChild->appendChild($a)` spliced a CYCLE into the`` |
|     - |   833 | ` * tree and every later walk of it ran away.` |
|     - |   834 | ` */` |
|     - |   835 | `/*` |
|     - |   836 | ` * The refusal is in TWO halves because php's empty-fragment answer sits` |
|     - |   837 | ` * between them: a foreign empty fragment is Wrong Document, an empty fragment` |
|     - |   838 | ` * on an ATTRIBUTE receiver is the "Document Fragment is empty" warning plus` |
|     - |   839 | ` * false -- so the document screen runs before the fragment check and the` |
|     - |   840 | ` * receiver-kind screen after it.` |
|     - |   841 | ` */` |
|   400 |   842 | `static int DomLinkRefusalPre(xmlNodePtr pParent,xmlNodePtr pChild)` |
|     3 |   843 | `{` |
|     - |   844 | `	/* A child with NO document is exempt: it is a constructed node, and the` |
|     - |   845 | `	 * level-2 doors ADOPT it -- where the modern variadic family refuses it` |
|     - |   846 | `	 * with this same code. */` |
|   403 |   847 | `	if( pParent->doc != pChild->doc && pChild->doc != 0 ){` |
|    17 |   848 | `		return DOM_ERR_WRONG_DOC;` |
|     - |   849 | `	}` |
|     - |   850 | `	/* A DOCUMENT is never a child, stated outright: the ancestor walk below` |
|     - |   851 | `	 * only sees it from an ATTACHED receiver, and a detached one --` |
|     - |   852 | ``	 * `$d->createElement('x')->appendChild($d)` -- spliced the document node`` |
|     - |   853 | `	 * into its own orphan's child list, which teardown then freed twice. */` |
|   387 |   854 | `	if( pChild->type == XML_DOCUMENT_NODE \|\| pChild->type == XML_HTML_DOCUMENT_NODE ){` |
|     3 |   855 | `		return DOM_ERR_HIERARCHY;` |
|     - |   856 | `	}` |
|   385 |   857 | `	return 0;` |
|   203 |   858 | `}` |
|     - |   859 | `/* The ancestor-cycle walk. Walking UP from the parent also catches` |
|     - |   860 | `` * pChild == pParent, so `$frag->appendChild($frag)` is Hierarchy even`` |
|     - |   861 | ` * for an EMPTY fragment -- the cycle answers before the empty warning. */` |
|   416 |   862 | `static int DomLinkCycle(xmlNodePtr pParent,xmlNodePtr pChild)` |
|     3 |   863 | `{` |
|     - |   864 | `	xmlNodePtr p;` |
|  1237 |   865 | `	for( p = pParent ; p ; p = p->parent ){` |
|   837 |   866 | `		if( p == pChild ){` |
|    17 |   867 | `			return DOM_ERR_HIERARCHY;` |
|     - |   868 | `		}` |
|   412 |   869 | `	}` |
|   403 |   870 | `	return 0;` |
|   211 |   871 | `}` |
|     - |   872 | `/* An ATTRIBUTE takes text and entity references, nothing else -- not even a` |
|     - |   873 | ` * fragment whose every child is text (though the EMPTY fragment's warning` |
|     - |   874 | ` * answers before this). php's Hierarchy refusal. */` |
|   394 |   875 | `static int DomAttrRecvKind(xmlNodePtr pParent,xmlNodePtr pChild)` |
|     3 |   876 | `{` |
|   394 |   877 | `	if( pParent->type == XML_ATTRIBUTE_NODE` |
|   208 |   878 | `	 && pChild->type != XML_TEXT_NODE && pChild->type != XML_ENTITY_REF_NODE ){` |
|     7 |   879 | `		return DOM_ERR_HIERARCHY;` |
|     - |   880 | `	}` |
|   391 |   881 | `	return 0;` |
|   200 |   882 | `}` |
|     - |   883 | `/*` |
|     - |   884 | ` * A DOCUMENT FRAGMENT is not linked, it is EMPTIED: php moves its children into` |
|     - |   885 | ` * the target and answers the FIRST of them (the fragment itself is never a` |
|     - |   886 | ` * child of anything, which is the whole point of the type -- it is how a` |
|     - |   887 | ` * program builds a run of nodes and inserts it in one call). An EMPTY one is` |
|     - |   888 | `` * php's warning plus `false`, not an exception.`` |
|     - |   889 | ` *` |
|     - |   890 | ` * *ppFirst takes the first node moved, or NULL when the argument was not a` |
|     - |   891 | ` * fragment at all; the caller then links the node itself.` |
|     - |   892 | ` */` |
|   792 |   893 | `static int DomIsFragment(xmlNodePtr pNode)` |
|     3 |   894 | `{` |
|   795 |   895 | `	return pNode && pNode->type == XML_DOCUMENT_FRAG_NODE;` |
|     3 |   896 | `}` |
|     6 |   897 | `static int DomFragEmpty(ph7_context *pCtx)` |
|     1 |   898 | `{` |
|     - |   899 | ``	/* The context already qualifies the message with php's `DOMNode::method(): `. */`` |
|     7 |   900 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Document Fragment is empty");` |
|     7 |   901 | `	ph7_result_bool(pCtx,0);` |
|     7 |   902 | `	return PH7_OK;` |
|     1 |   903 | `}` |
|     - |   904 | `/* Move every child of pFrag into pParent, before pRef or at the end. */` |
|    36 |   905 | `static xmlNodePtr DomFragMove(phl_xmldoc *pShell,xmlNodePtr pParent,xmlNodePtr pFrag,xmlNodePtr pRef)` |
|     1 |   906 | `{` |
|    37 |   907 | `	xmlNodePtr pFirst = pFrag->children;` |
|    37 |   908 | `	xmlNodePtr pChild = pFirst;` |
|    83 |   909 | `	while( pChild ){` |
|    47 |   910 | `		xmlNodePtr pNext = pChild->next;` |
|    47 |   911 | `		DomDetach(pShell,pChild);` |
|    47 |   912 | `		if( pRef ){` |
|     9 |   913 | `			DomLinkBefore(pParent,pChild,pRef);` |
|     5 |   914 | `		}else{` |
|    39 |   915 | `			DomLinkLast(pParent,pChild);` |
|     - |   916 | `		}` |
|     - |   917 | `		/* php reconciles each node it MOVED, not the fragment they came from --` |
|     - |   918 | `		 * and through this path it does so DEEPLY (see DomNsOnInsertEx). */` |
|    47 |   919 | `		DomNsOnInsertEx(pChild,1);` |
|    47 |   920 | `		pChild = pNext;` |
|     1 |   921 | `	}` |
|    37 |   922 | `	pFrag->children = pFrag->last = 0;` |
|    37 |   923 | `	return pFirst;` |
|     1 |   924 | `}` |
|     - |   925 | `/*` |
|     - |   926 | ` * DOMNode::appendChild(DOMNode $node): DOMNode` |
|     - |   927 | ` *` |
|     - |   928 | `` * Note the argument reaches C already screened -- `$n->appendChild(1)` is a`` |
|     - |   929 | ``  * TypeError from the declared `DOMNode $node`, where the chunk read `->__res` `` |
|     - |   930 | ` * off an int and warned.` |
|     - |   931 | ` */` |
|   384 |   932 | `DOM_METHOD(vm_builtin_DOMNode_appendChild)` |
|     3 |   933 | `{` |
|   387 |   934 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|   387 |   935 | `	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     - |   936 | `	int iErr,rc;` |
|   387 |   937 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|   ! 0 |   938 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|     - |   939 | `	}` |
|   387 |   940 | `	if( DomMutatorScreen(pCtx,(xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode,&rc) ){` |
|    27 |   941 | `		return rc;` |
|     - |   942 | `	}` |
|   361 |   943 | `	iErr = DomLinkRefusalPre((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|   361 |   944 | `	if( iErr == 0 ){` |
|   345 |   945 | `		iErr = DomLinkCycle((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|   171 |   946 | `	}` |
|     - |   947 | `	/* The empty-fragment answer sits between the screens: a foreign empty` |
|     - |   948 | `	 * fragment is Wrong Document, appending a fragment to ITSELF is the cycle's` |
|     - |   949 | `	 * Hierarchy, and only an empty one on an attribute receiver reaches the` |
|     - |   950 | `	 * warning plus false. */` |
|   358 |   951 | `	if( iErr == 0 && DomIsFragment((xmlNodePtr)pChd->pNode)` |
|   183 |   952 | `	 && ((xmlNodePtr)pChd->pNode)->children == 0 ){` |
|     5 |   953 | `		return DomFragEmpty(pCtx);` |
|     - |   954 | `	}` |
|   357 |   955 | `	if( iErr == 0 ){` |
|   329 |   956 | `		iErr = DomAttrRecvKind((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|   163 |   957 | `	}` |
|     - |   958 | `	/* An attribute lands on an ELEMENT or nowhere -- and BEFORE the adoption,` |
|     - |   959 | ``	 * which is measurable: `$doc->appendChild(new DOMAttr('k'))` refuses with`` |
|     - |   960 | `	 * the argument still ownerless. */` |
|   354 |   961 | `	if( iErr == 0 && ((xmlNodePtr)pChd->pNode)->type == XML_ATTRIBUTE_NODE` |
|   172 |   962 | `	 && ((xmlNodePtr)pPar->pNode)->type != XML_ELEMENT_NODE ){` |
|     3 |   963 | `		iErr = DOM_ERR_HIERARCHY;` |
|     1 |   964 | `	}` |
|   357 |   965 | `	if( iErr ){` |
|    35 |   966 | `		return DomThrow(pCtx,iErr);` |
|     - |   967 | `	}` |
|     - |   968 | `	/* Every screen passed: a document-less argument is ADOPTED here, php's` |
|     - |   969 | `	 * constructed-node door -- wrappers, orphan entry and (for an owned` |
|     - |   970 | `	 * receiver) the document itself all move before the link. */` |
|   323 |   971 | `	DomAdoptIntoRecv(pCtx,apArg[0],pChd);` |
|   323 |   972 | `	if( ((xmlNodePtr)pChd->pNode)->type == XML_ATTRIBUTE_NODE ){` |
|    16 |   973 | `		return DomMutatorAttrAttach(pCtx,pPar,pChd,apArg[0]);` |
|     - |   974 | `	}` |
|   308 |   975 | `	if( DomIsFragment((xmlNodePtr)pChd->pNode) ){` |
|     - |   976 | `		xmlNodePtr pFirst;` |
|    25 |   977 | `		pFirst = DomFragMove(pChd->pShell,(xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode,0);` |
|    25 |   978 | `		return DomResultNodeOf(pCtx,pPar,pFirst);` |
|     - |   979 | `	}` |
|   284 |   980 | `	DomDetach(pChd->pShell,(xmlNodePtr)pChd->pNode);` |
|   284 |   981 | `	DomLinkLast((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|   284 |   982 | `	DomNsOnInsertEx((xmlNodePtr)pChd->pNode,0);` |
|   284 |   983 | `	ph7_result_value(pCtx,apArg[0]);` |
|   284 |   984 | `	return PH7_OK;` |
|   195 |   985 | `}` |
|     - |   986 | `/* DOMNode::insertBefore(DOMNode $node, ?DOMNode $child = null): DOMNode --` |
|     - |   987 | ` * a reference node that is not a child of the receiver is Not Found. */` |
|    44 |   988 | `DOM_METHOD(vm_builtin_DOMNode_insertBefore)` |
|     2 |   989 | `{` |
|    46 |   990 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    46 |   991 | `	phl_domnode *pNew = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|    46 |   992 | `	phl_domnode *pRef = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;` |
|     - |   993 | `	xmlNodePtr pParent,pChild,pAnchor;` |
|     - |   994 | `	int iErr,rc;` |
|    46 |   995 | `	if( pPar == 0 \|\| pNew == 0 ){` |
|   ! 0 |   996 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |   997 | `	}` |
|    46 |   998 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|    46 |   999 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|    46 |  1000 | `	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;` |
|    46 |  1001 | `	if( DomMutatorScreen(pCtx,pParent,pChild,&rc) ){` |
|     3 |  1002 | `		return rc;` |
|     - |  1003 | `	}` |
|    44 |  1004 | `	iErr = DomLinkRefusalPre(pParent,pChild);` |
|    44 |  1005 | `	if( iErr == 0 ){` |
|    42 |  1006 | `		iErr = DomLinkCycle(pParent,pChild);` |
|    20 |  1007 | `	}` |
|     - |  1008 | `	/* Between the screens, and BEFORE the reference-membership refusal: an` |
|     - |  1009 | `	 * empty fragment answers its warning even against a reference node that` |
|     - |  1010 | `	 * is no child of the receiver. */` |
|    44 |  1011 | `	if( iErr == 0 && DomIsFragment(pChild) && pChild->children == 0 ){` |
|     3 |  1012 | `		return DomFragEmpty(pCtx);` |
|     - |  1013 | `	}` |
|    42 |  1014 | `	if( iErr == 0 ){` |
|    40 |  1015 | `		iErr = DomAttrRecvKind(pParent,pChild);` |
|    19 |  1016 | `	}` |
|     - |  1017 | `	/* An attribute lands on an ELEMENT or nowhere, and php answers that` |
|     - |  1018 | `	 * Hierarchy refusal BEFORE the reference-membership one -- a fragment` |
|     - |  1019 | `	 * receiver with an attribute argument and a foreign reference is` |
|     - |  1020 | `	 * Hierarchy, not Not Found. */` |
|    40 |  1021 | `	if( iErr == 0 && pChild->type == XML_ATTRIBUTE_NODE` |
|    28 |  1022 | `	 && pParent->type != XML_ELEMENT_NODE ){` |
|   ! 0 |  1023 | `		iErr = DOM_ERR_HIERARCHY;` |
|   ! 0 |  1024 | `	}` |
|    42 |  1025 | `	if( iErr == 0 && pAnchor && pAnchor->parent != pParent ){` |
|    11 |  1026 | `		iErr = DOM_ERR_NOT_FOUND;` |
|     5 |  1027 | `	}` |
|    42 |  1028 | `	if( iErr ){` |
|    13 |  1029 | `		return DomThrow(pCtx,iErr);` |
|     - |  1030 | `	}` |
|     - |  1031 | `	/* The constructed-node adoption, before ANY of the insertion tails --` |
|     - |  1032 | `	 * php's order, so even an argument the sibling Error is about to strand` |
|     - |  1033 | `	 * detached comes out of the call owned by this document. */` |
|    30 |  1034 | `	DomAdoptIntoRecv(pCtx,apArg[0],pNew);` |
|    30 |  1035 | `	if( pChild->type == XML_ATTRIBUTE_NODE ){` |
|     - |  1036 | `		/*` |
|     - |  1037 | `		 * The attribute half, with insertBefore's own tails. A NULL reference` |
|     - |  1038 | `		 * is the append spelling and attaches (the same-name displacement` |
|     - |  1039 | `		 * included). A reference that is itself an ATTRIBUTE of the receiver` |
|     - |  1040 | `		 * really does mean "before": the argument enters the property list at` |
|     - |  1041 | `		 * the reference's position. Any other reference runs the DISPLACEMENT` |
|     - |  1042 | `		 * and then fails the sibling link, php's own order, so` |
|     - |  1043 | `` 		 * `$el->insertBefore($attr, $child)` on an element carrying `k="old"` `` |
|     - |  1044 | `		 * LOSES the old attribute, attaches nothing, and raises the plain` |
|     - |  1045 | `		 * Error the self-sibling splice raises.` |
|     - |  1046 | `		 */` |
|    13 |  1047 | `		xmlAttrPtr pAttr = (xmlAttrPtr)pChild;` |
|     - |  1048 | `		xmlAttrPtr pOld;` |
|    13 |  1049 | `		if( pAnchor == 0 ){` |
|     3 |  1050 | `			return DomMutatorAttrAttach(pCtx,pPar,pNew,apArg[0]);` |
|     - |  1051 | `		}` |
|    16 |  1052 | `		pOld = pAttr->ns` |
|   ! 0 |  1053 | `			? DomAttrByNs(pParent,pAttr->ns->href,(const char *)pAttr->name)` |
|    10 |  1054 | `			: DomAttrByLocal(pParent,(const char *)pAttr->name);` |
|    11 |  1055 | `		if( pOld && pOld != pAttr ){` |
|     5 |  1056 | `			xmlUnlinkNode((xmlNodePtr)pOld);` |
|     5 |  1057 | `			DomOrphanAdd(pPar->pShell,(xmlNodePtr)pOld);` |
|     2 |  1058 | `		}` |
|    10 |  1059 | `		if( pAnchor->type != XML_ATTRIBUTE_NODE` |
|     9 |  1060 | `		 \|\| pAnchor == (xmlNodePtr)pAttr \|\| pAnchor == (xmlNodePtr)pOld ){` |
|     - |  1061 | `			/* The argument is UNLINKED before the sibling link fails -- php's` |
|     - |  1062 | ``			 * own order, so `$r->insertBefore($cAttr, $child)` costs the other`` |
|     - |  1063 | `			 * element its attribute and attaches nothing here. The link fails` |
|     - |  1064 | `			 * for a non-attribute reference, for the argument AS its own` |
|     - |  1065 | `			 * reference, and for a reference the displacement just took --` |
|     - |  1066 | `			 * php frees it and the sibling link then refuses. */` |
|     9 |  1067 | `			DomAttrDetach(pNew->pShell,pAttr);` |
|     9 |  1068 | `			DomOrphanAdd(pNew->pShell,(xmlNodePtr)pAttr);` |
|     9 |  1069 | `			return PH7_VmThrowException(pCtx,"Error",` |
|     - |  1070 | `				"Cannot add newnode as the previous sibling of refnode");` |
|     - |  1071 | `		}` |
|     3 |  1072 | `		DomAttrDetach(pNew->pShell,pAttr);` |
|     3 |  1073 | `		DomAttrLinkBefore(pParent,pAttr,(xmlAttrPtr)pAnchor);` |
|     3 |  1074 | `		DomNsAttrArrive(pParent,pAttr);` |
|     3 |  1075 | `		ph7_result_value(pCtx,apArg[0]);` |
|     3 |  1076 | `		return PH7_OK;` |
|     - |  1077 | `	}` |
|    18 |  1078 | `	if( pAnchor && pAnchor->type == XML_ATTRIBUTE_NODE ){` |
|     - |  1079 | `		/*` |
|     - |  1080 | `		 * A non-attribute argument against an ATTRIBUTE reference: php hands` |
|     - |  1081 | `		 * the pair to xmlAddPrevSibling, which UNLINKS the argument and then` |
|     - |  1082 | `		 * splices it into the PROPERTY chain -- state no serializer or` |
|     - |  1083 | `		 * childNodes walk ever shows, whose exact shape is libxml's version's.` |
|     - |  1084 | `		 * The bytes agree when PHL simply DETACHES the argument and answers` |
|     - |  1085 | `		 * it; the one detail php answers differently afterwards is recorded in` |
|     - |  1086 | ``		 * §7.4 (the argument's `parentNode` reads the receiver there).`` |
|     - |  1087 | `		 */` |
|     3 |  1088 | `		DomDetach(pNew->pShell,pChild);` |
|     3 |  1089 | `		DomOrphanAdd(pNew->pShell,pChild);` |
|     3 |  1090 | `		ph7_result_value(pCtx,apArg[0]);` |
|     3 |  1091 | `		return PH7_OK;` |
|     - |  1092 | `	}` |
|    15 |  1093 | `	if( pAnchor == pChild ){` |
|     - |  1094 | `		/*` |
|     - |  1095 | `		 * A node cannot be inserted before ITSELF, and linking it anyway made` |
|     - |  1096 | ``		 * it its own sibling: `$p->insertBefore($x,$x)` spliced a cycle into`` |
|     - |  1097 | `		 * the child list, and the next walk of the tree -- saveXML, a` |
|     - |  1098 | `		 * childNodes count, getNodePath -- never returned.` |
|     - |  1099 | `		 *` |
|     - |  1100 | `		 * php's refusal here is a plain Error with no DOM code, because there` |
|     - |  1101 | `		 * is no DOM error for it, and it comes AFTER the node is detached: the` |
|     - |  1102 | `		 * tree loses the node and the caller is told nothing more.` |
|     - |  1103 | `		 */` |
|     5 |  1104 | `		DomDetach(pNew->pShell,pChild);` |
|     5 |  1105 | `		DomOrphanAdd(pNew->pShell,pChild);` |
|     5 |  1106 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - |  1107 | `			"Cannot add newnode as the previous sibling of refnode");` |
|     - |  1108 | `	}` |
|    11 |  1109 | `	if( DomIsFragment(pChild) ){` |
|     - |  1110 | `		/* An EMPTY fragment answered its warning above, before the reference` |
|     - |  1111 | `		 * screen -- php's order. */` |
|     - |  1112 | `		xmlNodePtr pFirst;` |
|     3 |  1113 | `		pFirst = DomFragMove(pNew->pShell,pParent,pChild,pAnchor);` |
|     3 |  1114 | `		return DomResultNodeOf(pCtx,pPar,pFirst);` |
|     - |  1115 | `	}` |
|     9 |  1116 | `	DomDetach(pNew->pShell,pChild);` |
|     9 |  1117 | `	if( pAnchor ){` |
|     9 |  1118 | `		DomLinkBefore(pParent,pChild,pAnchor);` |
|     5 |  1119 | `	}else{` |
|   ! 0 |  1120 | `		DomLinkLast(pParent,pChild);` |
|     - |  1121 | `	}` |
|     9 |  1122 | `	DomNsOnInsertEx(pChild,0);` |
|     9 |  1123 | `	ph7_result_value(pCtx,apArg[0]);` |
|     9 |  1124 | `	return PH7_OK;` |
|    24 |  1125 | `}` |
|     - |  1126 | `/* DOMNode::removeChild(DOMNode $child): DOMNode */` |
|    36 |  1127 | `DOM_METHOD(vm_builtin_DOMNode_removeChild)` |
|     1 |  1128 | `{` |
|    37 |  1129 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    37 |  1130 | `	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     - |  1131 | `	xmlNodePtr pChild;` |
|    37 |  1132 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|   ! 0 |  1133 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  1134 | `	}` |
|    37 |  1135 | `	pChild = (xmlNodePtr)pChd->pNode;` |
|     - |  1136 | ``	/* php's membership test is `no children at all, or the parent pointer`` |
|     - |  1137 | ``	 * disagrees` -- which lets an ATTACHED ATTRIBUTE through (its libxml`` |
|     - |  1138 | `	 * parent IS the element), so removeChild really does remove an attribute` |
|     - |  1139 | `	 * -- but only from an element that has at least one real child; on a` |
|     - |  1140 | `	 * childless one the same attribute is Not Found. (An entity reference's` |
|     - |  1141 | `	 * child fails the parent test: its parent is the DTD.) */` |
|    36 |  1142 | `	if( ((xmlNodePtr)pPar->pNode)->children == 0` |
|    36 |  1143 | `	 \|\| pChild->parent != (xmlNodePtr)pPar->pNode ){` |
|    17 |  1144 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  1145 | `	}` |
|     - |  1146 | `	/* ...and only THEN the read-only refusal, php's order: an entity` |
|     - |  1147 | `	 * reference's child is Not Found territory never reached, while` |
|     - |  1148 | ``	 * `$ownerless->removeChild($its->child)` is the No Modification`` |
|     - |  1149 | `	 * refusal. */` |
|    21 |  1150 | `	if( DomNodeReadOnly((xmlNodePtr)pPar->pNode) ){` |
|     3 |  1151 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|     - |  1152 | `	}` |
|    19 |  1153 | `	xmlUnlinkNode(pChild);` |
|    19 |  1154 | `	DomOrphanAdd(pChd->pShell,pChild);` |
|    19 |  1155 | `	ph7_result_value(pCtx,apArg[0]);` |
|    19 |  1156 | `	return PH7_OK;` |
|    19 |  1157 | `}` |
|     - |  1158 | `/* DOMNode::replaceChild(DOMNode $node, DOMNode $child): DOMNode -- answers the` |
|     - |  1159 | ` * node it replaced, which is the SECOND argument. */` |
|    44 |  1160 | `DOM_METHOD(vm_builtin_DOMNode_replaceChild)` |
|     1 |  1161 | `{` |
|    45 |  1162 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    45 |  1163 | `	phl_domnode *pNew = nArg > 1 ? DomObjArg(apArg[0]) : 0;` |
|    45 |  1164 | `	phl_domnode *pOld = nArg > 1 ? DomObjArg(apArg[1]) : 0;` |
|     - |  1165 | `	xmlNodePtr pParent,pChild,pVictim;` |
|     - |  1166 | `	int iErr;` |
|    45 |  1167 | `	if( pPar == 0 \|\| pNew == 0 \|\| pOld == 0 ){` |
|   ! 0 |  1168 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  1169 | `	}` |
|    45 |  1170 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|    45 |  1171 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|    45 |  1172 | `	pVictim = (xmlNodePtr)pOld->pNode;` |
|     - |  1173 | `	/*` |
|     - |  1174 | `	 * php's replaceChild, in its own order (dom_node_replace_child) -- and it` |
|     - |  1175 | `	 * disagrees with appendChild's twice. The document screen answers FIRST (a` |
|     - |  1176 | `	 * text receiver or a read-only receiver with a foreign argument is Wrong` |
|     - |  1177 | `	 * Document here, where appendChild answers false and No Modification).` |
|     - |  1178 | `	 * Then the two silent-false answers: the invalid-children receiver and the` |
|     - |  1179 | `	 * CHILDLESS one -- nothing to replace, and php says nothing at all, even` |
|     - |  1180 | `	 * for an attribute or a document argument. Then the shared insertion` |
|     - |  1181 | `	 * validity: read-only, the ancestor cycle, the attribute receiver's` |
|     - |  1182 | `	 * child-kind rule, an attribute argument's element-only rule, the` |
|     - |  1183 | `	 * document-as-child rule. Then a rule of replaceChild's OWN: old and new` |
|     - |  1184 | `	 * must be attributes TOGETHER or not at all -- so an attribute argument` |
|     - |  1185 | `	 * against a foreign ATTRIBUTE victim reads Not Found from the membership` |
|     - |  1186 | `	 * check (both are attributes, the pair passes) while an element argument` |
|     - |  1187 | `	 * against the same victim is Hierarchy. The victim's membership answers` |
|     - |  1188 | `	 * last.` |
|     - |  1189 | `	 */` |
|    45 |  1190 | `	if( pChild->doc != pParent->doc && pChild->doc != 0 ){` |
|     3 |  1191 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|     - |  1192 | `	}` |
|    43 |  1193 | `	if( !DomChildrenValid(pParent) \|\| pParent->children == 0 ){` |
|     7 |  1194 | `		ph7_result_bool(pCtx,0);` |
|     7 |  1195 | `		return PH7_OK;` |
|     - |  1196 | `	}` |
|    36 |  1197 | `	if( DomNodeReadOnly(pParent)` |
|    36 |  1198 | `	 \|\| (pChild->parent && DomNodeReadOnly(pChild->parent)) ){` |
|     3 |  1199 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|     - |  1200 | `	}` |
|    35 |  1201 | `	iErr = DomLinkCycle(pParent,pChild);` |
|    35 |  1202 | `	if( iErr == 0 ){` |
|    31 |  1203 | `		iErr = DomAttrRecvKind(pParent,pChild);` |
|    15 |  1204 | `	}` |
|    34 |  1205 | `	if( iErr == 0 && pChild->type == XML_ATTRIBUTE_NODE` |
|    18 |  1206 | `	 && pParent->type != XML_ELEMENT_NODE ){` |
|   ! 0 |  1207 | `		iErr = DOM_ERR_HIERARCHY;` |
|   ! 0 |  1208 | `	}` |
|    34 |  1209 | `	if( iErr == 0` |
|    32 |  1210 | `	 && (pChild->type == XML_DOCUMENT_NODE \|\| pChild->type == XML_HTML_DOCUMENT_NODE) ){` |
|   ! 0 |  1211 | `		iErr = DOM_ERR_HIERARCHY;` |
|   ! 0 |  1212 | `	}` |
|    34 |  1213 | `	if( iErr == 0` |
|    32 |  1214 | `	 && (pChild->type == XML_ATTRIBUTE_NODE) != (pVictim->type == XML_ATTRIBUTE_NODE) ){` |
|     5 |  1215 | `		iErr = DOM_ERR_HIERARCHY;` |
|     2 |  1216 | `	}` |
|    35 |  1217 | `	if( iErr == 0 && pVictim->parent != pParent ){` |
|     7 |  1218 | `		iErr = DOM_ERR_NOT_FOUND;` |
|     3 |  1219 | `	}` |
|    35 |  1220 | `	if( iErr ){` |
|    17 |  1221 | `		return DomThrow(pCtx,iErr);` |
|     - |  1222 | `	}` |
|     - |  1223 | `	/* The constructed-node adoption, php's "document assignment" step. */` |
|    19 |  1224 | `	DomAdoptIntoRecv(pCtx,apArg[0],pNew);` |
|    19 |  1225 | `	if( DomIsFragment(pChild) ){` |
|     - |  1226 | `		/* No empty-fragment refusal here, unlike the other two: php REMOVES the` |
|     - |  1227 | `		 * old child and inserts nothing, and answers it as any replaceChild` |
|     - |  1228 | `		 * does. */` |
|     5 |  1229 | `		DomFragMove(pNew->pShell,pParent,pChild,pVictim);` |
|     5 |  1230 | `		xmlUnlinkNode(pVictim);` |
|     5 |  1231 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|     5 |  1232 | `		ph7_result_value(pCtx,apArg[1]);` |
|     5 |  1233 | `		return PH7_OK;` |
|     - |  1234 | `	}` |
|    15 |  1235 | `	if( pChild != pVictim && pChild->type == XML_ATTRIBUTE_NODE ){` |
|     - |  1236 | `		/* Both sides are attributes (the XOR above let them through): the swap` |
|     - |  1237 | `		 * happens in the PROPERTY list, at the victim's position, with NO` |
|     - |  1238 | `		 * same-name displacement -- php hands the pair to xmlReplaceNode` |
|     - |  1239 | `		 * as-is, so a duplicate name is the caller's to answer for. */` |
|     5 |  1240 | `		DomAttrDetach(pNew->pShell,(xmlAttrPtr)pChild);` |
|     5 |  1241 | `		DomAttrLinkBefore(pParent,(xmlAttrPtr)pChild,(xmlAttrPtr)pVictim);` |
|     5 |  1242 | `		xmlUnlinkNode(pVictim);` |
|     5 |  1243 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|     5 |  1244 | `		DomNsAttrArrive(pParent,(xmlAttrPtr)pChild);` |
|    13 |  1245 | `	}else if( pChild != pVictim ){` |
|     9 |  1246 | `		DomDetach(pNew->pShell,pChild);` |
|     9 |  1247 | `		DomLinkBefore(pParent,pChild,pVictim);` |
|     9 |  1248 | `		xmlUnlinkNode(pVictim);` |
|     9 |  1249 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|     9 |  1250 | `		DomNsOnInsertEx(pChild,0);` |
|     4 |  1251 | `	}` |
|    15 |  1252 | `	ph7_result_value(pCtx,apArg[1]);` |
|    15 |  1253 | `	return PH7_OK;` |
|    23 |  1254 | `}` |
|     - |  1255 | `/* ===== The 8.3 parent/child-node family (DOMParentNode / DOMChildNode) ===== */` |
|     - |  1256 |  |
|     - |  1257 | `/*` |
|     - |  1258 | ` * The name a TypeError prints for a value that is neither a DOMNode nor a` |
|     - |  1259 | ` * string: the CLASS of an object, php's type keyword for anything else --` |
|     - |  1260 | ` * the same rendering DomWriteText uses for a typed property store.` |
|     - |  1261 | ` */` |
|    24 |  1262 | `static const char * DomGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|     1 |  1263 | `{` |
|    25 |  1264 | `	if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|     5 |  1265 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|     5 |  1266 | `		SyBufferFormat(zBuf,nBuf,"%z",&pObj->pClass->sName);` |
|     5 |  1267 | `		return zBuf;` |
|     - |  1268 | `	}` |
|    21 |  1269 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|     3 |  1270 | `		return "null";` |
|     - |  1271 | `	}` |
|    19 |  1272 | `	return ph7_type_name(pVal);` |
|    13 |  1273 | `}` |
|     - |  1274 | `/*` |
|     - |  1275 | ` * php's variadic screen for the 8.0 insertion methods: every argument must be` |
|     - |  1276 | ` * a DOMNode or a STRING (nothing coerces -- an int is refused where an` |
|     - |  1277 | `` * ordinary `string $data` parameter would take it), the WHOLE list is checked`` |
|     - |  1278 | ` * before anything else runs, and the TypeError names the position with no` |
|     - |  1279 | ` * parameter name, because many values share the one variadic formal.  The` |
|     - |  1280 | ``  * method name it prints is the DECLARING class's (`DOMCharacterData::before()` `` |
|     - |  1281 | ` * for a comment), which is what pCtx->pFunc->sName already carries.` |
|     - |  1282 | ` * Answers 0 when every argument passed, non-zero after raising.` |
|     - |  1283 | ` */` |
|   160 |  1284 | `static int DomNodesScreen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|     1 |  1285 | `{` |
|   161 |  1286 | `	ph7_class *pNodeCls = PH7_VmExtractClass(pCtx->pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);` |
|     - |  1287 | `	int i;` |
|   345 |  1288 | `	for( i = 0 ; i < nArg ; i++ ){` |
|   209 |  1289 | `		ph7_value *pVal = apArg[i];` |
|     - |  1290 | `		char zBuf[128];` |
|   209 |  1291 | `		if( pVal->iFlags & MEMOBJ_OBJ ){` |
|   139 |  1292 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|   139 |  1293 | `			if( pNodeCls && PH7_VmInstanceOf(pObj->pClass,pNodeCls) ){` |
|   160 |  1294 | `				continue;` |
|     1 |  1295 | `			}` |
|    73 |  1296 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|    51 |  1297 | `			continue;` |
|     - |  1298 | `		}` |
|    37 |  1299 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  1300 | `			"%z(): Argument #%d must be of type DOMNode\|string, %s given",` |
|    24 |  1301 | `			&pCtx->pFunc->sName,i+1,DomGivenName(pVal,zBuf,sizeof(zBuf)));` |
|    25 |  1302 | `		return -1;` |
|   ! 0 |  1303 | `	}` |
|   137 |  1304 | `	return 0;` |
|    81 |  1305 | `}` |
|     - |  1306 | `/*` |
|     - |  1307 | ` * php's "convert nodes into a node" (dom_zvals_to_single_node), transcribed` |
|     - |  1308 | ` * with its ONE-argument shortcut: a single node argument is handed through` |
|     - |  1309 | ` * whole -- nothing is unlinked, every check waits for the insertion -- while` |
|     - |  1310 | ` * two or more arguments really are appended one by one into an internal` |
|     - |  1311 | ` * fragment.  The difference is observable twice over.  A refusal DURING that` |
|     - |  1312 | ` * conversion (another document's node, a document, an attribute) leaves every` |
|     - |  1313 | `` * argument already converted DETACHED -- `$b->append($a, $attr)` costs the`` |
|     - |  1314 | ` * tree its $a -- and a refusal at the final insertion (the receiver was in` |
|     - |  1315 | ` * the converted set) leaves ALL of them detached, which is how` |
|     - |  1316 | `` * `$b->append($a, $b)` empties <r> of both children where `$r->append($r)`,`` |
|     - |  1317 | ` * one argument, moves nothing at all.  The CYCLE is checked only against the` |
|     - |  1318 | ` * conversion fragment (i.e. never fails there), NOT against the receiver --` |
|     - |  1319 | ` * that waits for the insertion step.` |
|     - |  1320 | ` *` |
|     - |  1321 | ` * The converted list is built in pList (xmlNodePtr entries, in order).  Every` |
|     - |  1322 | ` * node it takes is detached and parked in the receiver's orphan set, where a` |
|     - |  1323 | ` * failure leaves it alive for whatever PHP variable still wraps it -- php` |
|     - |  1324 | ` * frees the unwrapped ones instead, which no program can see.  A fragment` |
|     - |  1325 | ` * argument is emptied INTO the list (php unpacks it), so it stays empty even` |
|     - |  1326 | ` * when a later argument is refused.  Answers 0, or non-zero after raising.` |
|     - |  1327 | ` */` |
|    58 |  1328 | `static int DomNodesConvert(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pParent,` |
|     - |  1329 | `	int nArg,ph7_value **apArg,SySet *pList,int *pRc)` |
|     1 |  1330 | `{` |
|    59 |  1331 | `	ph7_class_instance *pDstHolder = DomThisDoc(pCtx);` |
|     - |  1332 | `	int i;` |
|   149 |  1333 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     - |  1334 | `		phl_domnode *pNd;` |
|     - |  1335 | `		xmlNodePtr pNode;` |
|     - |  1336 | `		ph7_class_instance *pSrcHolder;` |
|   101 |  1337 | `		if( (apArg[i]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    41 |  1338 | `			int nLen = 0;` |
|    41 |  1339 | `			const char *zText = ph7_value_to_string(apArg[i],&nLen);` |
|    41 |  1340 | `			pNode = xmlNewDocTextLen(pParent->doc,(const xmlChar *)zText,nLen);` |
|    41 |  1341 | `			if( pNode ){` |
|    41 |  1342 | `				DomOrphanAdd(pShell,pNode);` |
|    41 |  1343 | `				SySetPut(pList,(const void *)&pNode);` |
|    20 |  1344 | `			}` |
|    41 |  1345 | `			continue;` |
|     - |  1346 | `		}` |
|    61 |  1347 | `		pNd = DomObjArg(apArg[i]);` |
|    61 |  1348 | `		if( pNd == 0 ){` |
|     - |  1349 | `			/* A DOMNode-classed object with no node behind it. php's refusal` |
|     - |  1350 | `			 * ignores strictErrorChecking. */` |
|     3 |  1351 | `			*pRc = DomThrowAlways(pCtx,DOM_ERR_INVALID_STATE);` |
|     7 |  1352 | `			return -1;` |
|     - |  1353 | `		}` |
|    59 |  1354 | `		pNode = (xmlNodePtr)pNd->pNode;` |
|    59 |  1355 | `		if( pNode->doc != pParent->doc ){` |
|     - |  1356 | `			/* No adoption in the modern family: a constructed node's NULL` |
|     - |  1357 | `			 * document is a mismatch like any other and refuses -- only the` |
|     - |  1358 | `			 * OWNERLESS-to-OWNERLESS pair (both NULL) passes. */` |
|     5 |  1359 | `			*pRc = DomThrowVoid(pCtx,DOM_ERR_WRONG_DOC);` |
|     5 |  1360 | `			return -1;` |
|     - |  1361 | `		}` |
|     - |  1362 | `		/* Same document, possibly different HOLDER: two constructed trees` |
|     - |  1363 | `		 * merging. The wrappers move to the receiver's cache so identity` |
|     - |  1364 | `		 * keeps answering. */` |
|    55 |  1365 | `		pSrcHolder = DomObjArgDoc(apArg[i]);` |
|    55 |  1366 | `		if( pSrcHolder != pDstHolder ){` |
|     9 |  1367 | `			DomAdoptWrappers(pCtx->pVm,pSrcHolder,pDstHolder,pShell,pNode);` |
|     9 |  1368 | `			pNd->pShell = pShell;` |
|     4 |  1369 | `		}` |
|    54 |  1370 | `		if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE` |
|    55 |  1371 | `		 \|\| pNode->type == XML_ATTRIBUTE_NODE ){` |
|     5 |  1372 | `			*pRc = DomThrowVoid(pCtx,DOM_ERR_HIERARCHY);` |
|     5 |  1373 | `			return -1;` |
|     - |  1374 | `		}` |
|    51 |  1375 | `		if( DomIsFragment(pNode) ){` |
|   ! 0 |  1376 | `			xmlNodePtr pChild = pNode->children;` |
|   ! 0 |  1377 | `			while( pChild ){` |
|   ! 0 |  1378 | `				xmlNodePtr pNext = pChild->next;` |
|   ! 0 |  1379 | `				xmlUnlinkNode(pChild);` |
|   ! 0 |  1380 | `				DomOrphanAdd(pShell,pChild);` |
|   ! 0 |  1381 | `				SySetPut(pList,(const void *)&pChild);` |
|   ! 0 |  1382 | `				pChild = pNext;` |
|   ! 0 |  1383 | `			}` |
|   ! 0 |  1384 | `			pNode->children = pNode->last = 0;` |
|   ! 0 |  1385 | `			continue;` |
|     - |  1386 | `		}` |
|    51 |  1387 | `		DomDetach(pNd->pShell,pNode);` |
|    51 |  1388 | `		DomOrphanAdd(pShell,pNode);` |
|    51 |  1389 | `		SySetPut(pList,(const void *)&pNode);` |
|    26 |  1390 | `	}` |
|    49 |  1391 | `	return 0;` |
|    30 |  1392 | `}` |
|    84 |  1393 | `static int DomListHas(SySet *pList,xmlNodePtr pNode)` |
|     1 |  1394 | `{` |
|    85 |  1395 | `	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);` |
|     - |  1396 | `	sxu32 n;` |
|   211 |  1397 | `	for( n = 0 ; n < SySetUsed(pList) ; ++n ){` |
|   133 |  1398 | `		if( apNode[n] == pNode ){` |
|     7 |  1399 | `			return 1;` |
|     - |  1400 | `		}` |
|    64 |  1401 | `	}` |
|    79 |  1402 | `	return 0;` |
|    43 |  1403 | `}` |
|     - |  1404 | `/*` |
|     - |  1405 | ` * php's pre-insertion validity for what conversion produced, against the REAL` |
|     - |  1406 | ` * parent this time.  For a single node argument this is where every check` |
|     - |  1407 | ` * runs -- another document before the kind-or-ancestor Hierarchy refusal, the` |
|     - |  1408 | ` * same order the conversion pass uses -- and for a converted list the only` |
|     - |  1409 | ` * question left is whether the receiver is now INSIDE the set (its ancestor` |
|     - |  1410 | ` * chain passes through a detached argument).  Note what php never checks on` |
|     - |  1411 | ` * this path: a document receiver takes a second root element and bare text` |
|     - |  1412 | ` * without complaint, so the document it writes may not be well-formed XML --` |
|     - |  1413 | ` * measured, and matched.` |
|     - |  1414 | ` */` |
|   146 |  1415 | `static int DomInsertValidity(xmlNodePtr pParent,xmlNodePtr pSingle,SySet *pList)` |
|     1 |  1416 | `{` |
|     - |  1417 | `	xmlNodePtr p;` |
|   147 |  1418 | `	if( pSingle ){` |
|    99 |  1419 | `		if( pSingle->doc != pParent->doc ){` |
|    13 |  1420 | `			return DOM_ERR_WRONG_DOC;` |
|     - |  1421 | `		}` |
|    86 |  1422 | `		if( pSingle->type == XML_DOCUMENT_NODE \|\| pSingle->type == XML_HTML_DOCUMENT_NODE` |
|    85 |  1423 | `		 \|\| pSingle->type == XML_ATTRIBUTE_NODE ){` |
|     7 |  1424 | `			return DOM_ERR_HIERARCHY;` |
|     - |  1425 | `		}` |
|   215 |  1426 | `		for( p = pParent ; p ; p = p->parent ){` |
|   151 |  1427 | `			if( p == pSingle ){` |
|    17 |  1428 | `				return DOM_ERR_HIERARCHY;` |
|     - |  1429 | `			}` |
|    68 |  1430 | `		}` |
|    65 |  1431 | `		return 0;` |
|     - |  1432 | `	}` |
|   121 |  1433 | `	for( p = pParent ; p ; p = p->parent ){` |
|    79 |  1434 | `		if( DomListHas(pList,p) ){` |
|     7 |  1435 | `			return DOM_ERR_HIERARCHY;` |
|     - |  1436 | `		}` |
|    37 |  1437 | `	}` |
|    43 |  1438 | `	return 0;` |
|    74 |  1439 | `}` |
|     - |  1440 | `/*` |
|     - |  1441 | ` * The insertion itself (php's dom_insert_node_list_unchecked): everything in` |
|     - |  1442 | ` * pList goes before pRef -- at the end when NULL -- in order.  A list node` |
|     - |  1443 | ` * came through the conversion fragment, so its namespace reconcile is the` |
|     - |  1444 | ` * DEEP one (dom_reconcile_ns_list); a single node is php's dom_reconcile_ns,` |
|     - |  1445 | ` * the shallow appendChild rule.  A single node inserted before ITSELF slides` |
|     - |  1446 | ` * the reference to its next sibling first (the spec's step 3), which is what` |
|     - |  1447 | `` * makes `$r->prepend($r->firstChild)` a no-op instead of a cycle.`` |
|     - |  1448 | ` */` |
|    42 |  1449 | `static void DomNodesPlace(phl_xmldoc *pShell,xmlNodePtr pParent,xmlNodePtr pRef,SySet *pList)` |
|     1 |  1450 | `{` |
|    43 |  1451 | `	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);` |
|     - |  1452 | `	sxu32 n;` |
|   111 |  1453 | `	for( n = 0 ; n < SySetUsed(pList) ; ++n ){` |
|    69 |  1454 | `		xmlNodePtr pNode = apNode[n];` |
|    69 |  1455 | `		DomDetach(pShell,pNode);` |
|    69 |  1456 | `		if( pRef ){` |
|    23 |  1457 | `			DomLinkBefore(pParent,pNode,pRef);` |
|    12 |  1458 | `		}else{` |
|    47 |  1459 | `			DomLinkLast(pParent,pNode);` |
|     - |  1460 | `		}` |
|    69 |  1461 | `		DomNsOnInsertEx(pNode,1);` |
|    35 |  1462 | `	}` |
|    43 |  1463 | `}` |
|     - |  1464 | `/*` |
|     - |  1465 | ` * DOMParentNode::append / prepend / replaceChildren -- one body, three` |
|     - |  1466 | `` * insertion points.  php declares all three `void`, so a refusal in the`` |
|     - |  1467 | ` * non-strict mode is a warning and NOTHING is answered (DomThrowVoid).` |
|     - |  1468 | ` */` |
|     - |  1469 | `#define DOM_PN_APPEND   0` |
|     - |  1470 | `#define DOM_PN_PREPEND  1` |
|     - |  1471 | `#define DOM_PN_REPLACE  2` |
|   108 |  1472 | `static int DomParentNodeInsert(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)` |
|     1 |  1473 | `{` |
|   109 |  1474 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|   109 |  1475 | `	phl_domnode *pOne = 0;` |
|   109 |  1476 | `	xmlNodePtr pParent,pSingle = 0,pRef = 0;` |
|     - |  1477 | `	SySet sList;` |
|   109 |  1478 | `	int iErr,rc = PH7_OK;` |
|   109 |  1479 | `	if( DomNodesScreen(pCtx,nArg,apArg) \|\| pPar == 0 ){` |
|    21 |  1480 | `		return PH7_OK;` |
|     - |  1481 | `	}` |
|    89 |  1482 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|    89 |  1483 | `	SySetInit(&sList,&pCtx->pVm->sAllocator,sizeof(xmlNodePtr));` |
|    89 |  1484 | `	if( nArg == 1 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     - |  1485 | `		/* The one-argument shortcut: the node itself, unconverted.  A shell` |
|     - |  1486 | `		 * with no node behind it is php's SILENT no-op here (the pre-insert` |
|     - |  1487 | `		 * NULL guard), where the multi-argument conversion raises Invalid` |
|     - |  1488 | `		 * State -- one more face of the shortcut. */` |
|    53 |  1489 | `		pOne = DomObjArg(apArg[0]);` |
|    53 |  1490 | `		if( pOne == 0 ){` |
|     3 |  1491 | `			return PH7_OK;` |
|     - |  1492 | `		}` |
|    51 |  1493 | `		pSingle = (xmlNodePtr)pOne->pNode;` |
|    62 |  1494 | `	}else if( DomNodesConvert(pCtx,pPar->pShell,pParent,nArg,apArg,&sList,&rc) ){` |
|     7 |  1495 | `		SySetRelease(&sList);` |
|     7 |  1496 | `		return rc;` |
|     - |  1497 | `	}` |
|    81 |  1498 | `	iErr = DomInsertValidity(pParent,pSingle,&sList);` |
|    81 |  1499 | `	if( iErr ){` |
|    31 |  1500 | `		SySetRelease(&sList);` |
|    31 |  1501 | `		return DomThrowVoid(pCtx,iErr);` |
|     - |  1502 | `	}` |
|    51 |  1503 | `	if( pOne ){` |
|     - |  1504 | `		/* The single-node shortcut skipped the conversion, so it re-homes its` |
|     - |  1505 | `		 * wrappers here: an ownerless argument merging into an ownerless` |
|     - |  1506 | `		 * receiver (the only mismatch the validity lets through). */` |
|    23 |  1507 | `		DomAdoptIntoRecv(pCtx,apArg[0],pOne);` |
|    11 |  1508 | `	}` |
|    51 |  1509 | `	if( iMode == DOM_PN_REPLACE ){` |
|     - |  1510 | `		/* Every remaining child goes -- through the wrapper-preserving drop a` |
|     - |  1511 | `		 * content write uses, so a PHP variable holding one keeps a live` |
|     - |  1512 | `		 * detached node rather than a dangling pointer.  After the validity` |
|     - |  1513 | `		 * check, as php orders it. */` |
|    11 |  1514 | `		DomDropChildren(pCtx,pPar->pShell,pParent);` |
|    46 |  1515 | `	}else if( iMode == DOM_PN_PREPEND ){` |
|     - |  1516 | `		/* The first child AFTER conversion has emptied the set out of the` |
|     - |  1517 | `		 * tree -- and never a member of the set. */` |
|     7 |  1518 | `		pRef = pParent->children;` |
|     3 |  1519 | `	}` |
|    51 |  1520 | `	if( pSingle ){` |
|    23 |  1521 | `		if( DomIsFragment(pSingle) ){` |
|     - |  1522 | `			/* A single fragment splices -- silently even when EMPTY, unlike` |
|     - |  1523 | `			 * appendChild's warning. */` |
|     5 |  1524 | `			DomFragMove(pOne->pShell,pParent,pSingle,pRef);` |
|     3 |  1525 | `		}else{` |
|    19 |  1526 | `			if( pRef == pSingle ){` |
|   ! 0 |  1527 | `				pRef = pSingle->next;` |
|   ! 0 |  1528 | `			}` |
|    19 |  1529 | `			DomDetach(pOne->pShell,pSingle);` |
|    19 |  1530 | `			if( pRef ){` |
|     3 |  1531 | `				DomLinkBefore(pParent,pSingle,pRef);` |
|     2 |  1532 | `			}else{` |
|    17 |  1533 | `				DomLinkLast(pParent,pSingle);` |
|     - |  1534 | `			}` |
|    19 |  1535 | `			DomNsOnInsertEx(pSingle,0);` |
|     - |  1536 | `		}` |
|    12 |  1537 | `	}else{` |
|    29 |  1538 | `		DomNodesPlace(pPar->pShell,pParent,pRef,&sList);` |
|     - |  1539 | `	}` |
|    51 |  1540 | `	SySetRelease(&sList);` |
|    51 |  1541 | `	return PH7_OK;` |
|    55 |  1542 | `}` |
|    86 |  1543 | `DOM_METHOD(vm_builtin_Dom_append)` |
|     1 |  1544 | `{` |
|    87 |  1545 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_APPEND);` |
|     1 |  1546 | `}` |
|     8 |  1547 | `DOM_METHOD(vm_builtin_Dom_prepend)` |
|     1 |  1548 | `{` |
|     9 |  1549 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_PREPEND);` |
|     1 |  1550 | `}` |
|    14 |  1551 | `DOM_METHOD(vm_builtin_Dom_replaceChildren)` |
|     1 |  1552 | `{` |
|    15 |  1553 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_REPLACE);` |
|     1 |  1554 | `}` |
|     - |  1555 | `/*` |
|     - |  1556 | ` * Is this xmlNodePtr one of the ARGUMENT nodes?  The viable-sibling walks ask` |
|     - |  1557 | ` * it about tree nodes, so only object arguments can match -- php's` |
|     - |  1558 | ` * dom_is_node_in_list does the same walk over the zval list.` |
|     - |  1559 | ` */` |
|    24 |  1560 | `static int DomArgListHasNode(int nArg,ph7_value **apArg,xmlNodePtr pNode)` |
|     1 |  1561 | `{` |
|     - |  1562 | `	int i;` |
|    47 |  1563 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    29 |  1564 | `		phl_domnode *pNd = DomObjArg(apArg[i]);` |
|    29 |  1565 | `		if( pNd && (xmlNodePtr)pNd->pNode == pNode ){` |
|     7 |  1566 | `			return 1;` |
|     - |  1567 | `		}` |
|    12 |  1568 | `	}` |
|    19 |  1569 | `	return 0;` |
|    13 |  1570 | `}` |
|     - |  1571 | `/*` |
|     - |  1572 | ` * DOMChildNode::before / after / replaceWith -- php's WHATWG transcription` |
|     - |  1573 | ` * (dom_parent_node_before/after, dom_child_replace_with), sharing the parent` |
|     - |  1574 | ` * side's conversion machinery.  The order is the measurable part: the TYPE` |
|     - |  1575 | ` * screen runs first even for a node with no parent; a parentless receiver` |
|     - |  1576 | ` * then returns in SILENCE -- around an argument that could never be inserted` |
|     - |  1577 | ` * -- and only then does conversion run, with the same mid-list detachment the` |
|     - |  1578 | ` * parent side has.  The reference sibling ("viable") is the nearest sibling` |
|     - |  1579 | ` * NOT in the argument set, read before anything moves; the insertion point is` |
|     - |  1580 | ` * derived from it after conversion, so a set member that was also the first` |
|     - |  1581 | ` * child no longer counts.` |
|     - |  1582 | ` */` |
|     - |  1583 | `#define DOM_CN_BEFORE   0` |
|     - |  1584 | `#define DOM_CN_AFTER    1` |
|     - |  1585 | `#define DOM_CN_REPLACE  2` |
|    52 |  1586 | `static int DomChildNodeOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)` |
|     1 |  1587 | `{` |
|    53 |  1588 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    53 |  1589 | `	phl_domnode *pOne = 0;` |
|    53 |  1590 | `	xmlNodePtr pThis,pParent,pViable,pRef,pSingle = 0;` |
|     - |  1591 | `	SySet sList;` |
|    53 |  1592 | `	int iErr,rc = PH7_OK;` |
|    53 |  1593 | `	if( DomNodesScreen(pCtx,nArg,apArg) \|\| pNd == 0 ){` |
|     5 |  1594 | `		return PH7_OK;` |
|     - |  1595 | `	}` |
|    49 |  1596 | `	pThis = (xmlNodePtr)pNd->pNode;` |
|    49 |  1597 | `	pParent = pThis->parent;` |
|    49 |  1598 | `	if( pParent == 0 ){` |
|    11 |  1599 | `		return PH7_OK;` |
|     - |  1600 | `	}` |
|    38 |  1601 | `	if( iMode == DOM_CN_REPLACE` |
|    25 |  1602 | `	 && (DomNodeReadOnly(pThis) \|\| DomNodeReadOnly(pParent)) ){` |
|     - |  1603 | `		/* replaceWith carries the read-only refusal (it REMOVES the receiver)` |
|     - |  1604 | `		 * where before/after do not: a text child of a constructed ownerless` |
|     - |  1605 | `		 * element takes before() and refuses replaceWith(). The parentless` |
|     - |  1606 | `		 * silence above still answers first -- a constructed ROOT is a silent` |
|     - |  1607 | `		 * no-op, not this refusal. */` |
|   ! 0 |  1608 | `		return DomThrowVoid(pCtx,DOM_ERR_NO_MOD);` |
|     - |  1609 | `	}` |
|    39 |  1610 | `	if( iMode == DOM_CN_BEFORE ){` |
|    19 |  1611 | `		pViable = pThis->prev;` |
|    21 |  1612 | `		while( pViable && DomArgListHasNode(nArg,apArg,pViable) ){` |
|     3 |  1613 | `			pViable = pViable->prev;` |
|     1 |  1614 | `		}` |
|    10 |  1615 | `	}else{` |
|    21 |  1616 | `		pViable = pThis->next;` |
|    25 |  1617 | `		while( pViable && DomArgListHasNode(nArg,apArg,pViable) ){` |
|     5 |  1618 | `			pViable = pViable->next;` |
|     1 |  1619 | `		}` |
|     - |  1620 | `	}` |
|    39 |  1621 | `	SySetInit(&sList,&pCtx->pVm->sAllocator,sizeof(xmlNodePtr));` |
|    39 |  1622 | `	if( nArg == 1 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){` |
|    17 |  1623 | `		pOne = DomObjArg(apArg[0]);` |
|    17 |  1624 | `		if( pOne == 0 ){` |
|   ! 0 |  1625 | `			return PH7_OK;` |
|     - |  1626 | `		}` |
|    17 |  1627 | `		pSingle = (xmlNodePtr)pOne->pNode;` |
|    31 |  1628 | `	}else if( DomNodesConvert(pCtx,pNd->pShell,pParent,nArg,apArg,&sList,&rc) ){` |
|     5 |  1629 | `		SySetRelease(&sList);` |
|     5 |  1630 | `		return rc;` |
|     - |  1631 | `	}` |
|    35 |  1632 | `	iErr = DomInsertValidity(pParent,pSingle,&sList);` |
|    35 |  1633 | `	if( iErr ){` |
|     5 |  1634 | `		SySetRelease(&sList);` |
|     5 |  1635 | `		return DomThrowVoid(pCtx,iErr);` |
|     - |  1636 | `	}` |
|    31 |  1637 | `	if( pOne ){` |
|     - |  1638 | `		/* The single-node shortcut skipped the conversion's wrapper re-home:` |
|     - |  1639 | `		 * an ownerless argument merging into an ownerless receiver's tree, the` |
|     - |  1640 | `		 * only mismatch the validity lets through. */` |
|    17 |  1641 | `		DomAdoptIntoRecv(pCtx,apArg[0],pOne);` |
|     8 |  1642 | `	}` |
|    31 |  1643 | `	if( iMode == DOM_CN_BEFORE ){` |
|     - |  1644 | `		/* Step 5: the viable previous sibling's NEXT -- the parent's first` |
|     - |  1645 | `		 * child when there is none -- both read after conversion. */` |
|    15 |  1646 | `		pRef = pViable ? pViable->next : pParent->children;` |
|     8 |  1647 | `	}else{` |
|    17 |  1648 | `		pRef = pViable;` |
|     - |  1649 | `	}` |
|    31 |  1650 | `	if( iMode == DOM_CN_REPLACE ){` |
|     - |  1651 | `		/* php unlinks the receiver unless conversion already took it. */` |
|     9 |  1652 | `		if( pThis != pSingle && !DomListHas(&sList,pThis) ){` |
|     7 |  1653 | `			xmlUnlinkNode(pThis);` |
|     7 |  1654 | `			DomOrphanAdd(pNd->pShell,pThis);` |
|     3 |  1655 | `		}` |
|     4 |  1656 | `	}` |
|    31 |  1657 | `	if( pSingle ){` |
|    17 |  1658 | `		if( DomIsFragment(pSingle) ){` |
|     3 |  1659 | `			DomFragMove(pOne->pShell,pParent,pSingle,pRef);` |
|     2 |  1660 | `		}else{` |
|    15 |  1661 | `			if( pRef == pSingle ){` |
|     3 |  1662 | `				pRef = pSingle->next;` |
|     1 |  1663 | `			}` |
|    15 |  1664 | `			DomDetach(pOne->pShell,pSingle);` |
|    15 |  1665 | `			if( pRef ){` |
|    13 |  1666 | `				DomLinkBefore(pParent,pSingle,pRef);` |
|     7 |  1667 | `			}else{` |
|     3 |  1668 | `				DomLinkLast(pParent,pSingle);` |
|     - |  1669 | `			}` |
|    15 |  1670 | `			DomNsOnInsertEx(pSingle,0);` |
|     - |  1671 | `		}` |
|     9 |  1672 | `	}else{` |
|    15 |  1673 | `		DomNodesPlace(pNd->pShell,pParent,pRef,&sList);` |
|     - |  1674 | `	}` |
|    31 |  1675 | `	SySetRelease(&sList);` |
|    31 |  1676 | `	return PH7_OK;` |
|    27 |  1677 | `}` |
|    26 |  1678 | `DOM_METHOD(vm_builtin_Dom_before)` |
|     1 |  1679 | `{` |
|    27 |  1680 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_BEFORE);` |
|     1 |  1681 | `}` |
|    12 |  1682 | `DOM_METHOD(vm_builtin_Dom_after)` |
|     1 |  1683 | `{` |
|    13 |  1684 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_AFTER);` |
|     1 |  1685 | `}` |
|    14 |  1686 | `DOM_METHOD(vm_builtin_Dom_replaceWith)` |
|     1 |  1687 | `{` |
|    15 |  1688 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_REPLACE);` |
|     1 |  1689 | `}` |
|     - |  1690 | `/*` |
|     - |  1691 | ` * DOMChildNode::remove(): void -- and php's asymmetry: where before() on a` |
|     - |  1692 | ` * parentless node is a silent no-op, remove() is the Not Found refusal, in` |
|     - |  1693 | ` * whichever mode the document is in.` |
|     - |  1694 | ` */` |
|    10 |  1695 | `DOM_METHOD(vm_builtin_Dom_removeSelf)` |
|     1 |  1696 | `{` |
|    11 |  1697 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     - |  1698 | `	xmlNodePtr pNode;` |
|     5 |  1699 | `	SXUNUSED(nArg);` |
|     5 |  1700 | `	SXUNUSED(apArg);` |
|    11 |  1701 | `	if( pNd == 0 ){` |
|   ! 0 |  1702 | `		return PH7_OK;` |
|     - |  1703 | `	}` |
|    11 |  1704 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|     - |  1705 | `	/* The read-only refusal answers BEFORE the parentless one: a constructed` |
|     - |  1706 | `	 * ownerless node -- necessarily parentless -- is No Modification Allowed` |
|     - |  1707 | `	 * here, where an owned parentless node is Not Found. */` |
|    11 |  1708 | `	if( DomNodeReadOnly(pNode) \|\| (pNode->parent && DomNodeReadOnly(pNode->parent)) ){` |
|     3 |  1709 | `		return DomThrowVoid(pCtx,DOM_ERR_NO_MOD);` |
|     - |  1710 | `	}` |
|     9 |  1711 | `	if( pNode->parent == 0 ){` |
|     5 |  1712 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  1713 | `	}` |
|     5 |  1714 | `	xmlUnlinkNode(pNode);` |
|     5 |  1715 | `	DomOrphanAdd(pNd->pShell,pNode);` |
|     5 |  1716 | `	return PH7_OK;` |
|     6 |  1717 | `}` |
|     - |  1718 |  |
|     - |  1719 | `/* DOMNode::hasChildNodes(): bool / hasAttributes(): bool / getLineNo(): int */` |
|     4 |  1720 | `DOM_METHOD(vm_builtin_DOMNode_hasChildNodes)` |
|     1 |  1721 | `{` |
|     5 |  1722 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     2 |  1723 | `	SXUNUSED(nArg);` |
|     2 |  1724 | `	SXUNUSED(apArg);` |
|     5 |  1725 | `	ph7_result_bool(pCtx,pNd && DomNodeChildFirst((xmlNodePtr)pNd->pNode) != 0);` |
|     5 |  1726 | `	return PH7_OK;` |
|     1 |  1727 | `}` |
|     - |  1728 | `/* The attribute list of an element (empty for anything else). */` |
|   242 |  1729 | `static xmlAttrPtr DomAttrList(xmlNodePtr pNode)` |
|     2 |  1730 | `{` |
|   244 |  1731 | `	return (pNode && pNode->type == XML_ELEMENT_NODE) ? pNode->properties : 0;` |
|     2 |  1732 | `}` |
|    16 |  1733 | `static int DomAttrCount(xmlNodePtr pNode)` |
|     1 |  1734 | `{` |
|    17 |  1735 | `	xmlAttrPtr pAttr = DomAttrList(pNode);` |
|    17 |  1736 | `	int iCount = 0;` |
|    41 |  1737 | `	for( ; pAttr ; pAttr = pAttr->next ){` |
|    25 |  1738 | `		iCount++;` |
|    13 |  1739 | `	}` |
|    17 |  1740 | `	return iCount;` |
|     1 |  1741 | `}` |
|   208 |  1742 | `static xmlAttrPtr DomAttrAt(xmlNodePtr pNode,int iWant)` |
|     2 |  1743 | `{` |
|   210 |  1744 | `	xmlAttrPtr pAttr = DomAttrList(pNode);` |
|   330 |  1745 | `	for( ; pAttr && iWant > 0 ; pAttr = pAttr->next ){` |
|   122 |  1746 | `		iWant--;` |
|    62 |  1747 | `	}` |
|   210 |  1748 | `	return pAttr;` |
|     2 |  1749 | `}` |
|   ! 0 |  1750 | `DOM_METHOD(vm_builtin_DOMNode_hasAttributes)` |
|   ! 0 |  1751 | `{` |
|   ! 0 |  1752 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   ! 0 |  1753 | `	SXUNUSED(nArg);` |
|   ! 0 |  1754 | `	SXUNUSED(apArg);` |
|   ! 0 |  1755 | `	ph7_result_bool(pCtx,pNd && DomAttrCount((xmlNodePtr)pNd->pNode) > 0);` |
|   ! 0 |  1756 | `	return PH7_OK;` |
|   ! 0 |  1757 | `}` |
|     8 |  1758 | `DOM_METHOD(vm_builtin_DOMNode_getLineNo)` |
|     1 |  1759 | `{` |
|     9 |  1760 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     4 |  1761 | `	SXUNUSED(nArg);` |
|     4 |  1762 | `	SXUNUSED(apArg);` |
|     9 |  1763 | `	ph7_result_int64(pCtx,pNd ? (ph7_int64)xmlGetLineNo((xmlNodePtr)pNd->pNode) : 0);` |
|     9 |  1764 | `	return PH7_OK;` |
|     1 |  1765 | `}` |
|     - |  1766 | `/* DOMNode::isSameNode(DOMNode $otherNode): bool -- pointer identity, which is` |
|     - |  1767 | ` * also the identity the wrapper cache keys on. */` |
|     6 |  1768 | `DOM_METHOD(vm_builtin_DOMNode_isSameNode)` |
|     1 |  1769 | `{` |
|     7 |  1770 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     7 |  1771 | `	phl_domnode *pOther = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     7 |  1772 | `	ph7_result_bool(pCtx,pNd != 0 && pOther != 0 && pNd->pNode == pOther->pNode);` |
|     7 |  1773 | `	return PH7_OK;` |
|     1 |  1774 | `}` |
|     - |  1775 |  |
|     - |  1776 | `/* ===== Position, containment and structural equality ===== */` |
|     - |  1777 |  |
|     - |  1778 | `/* php's DOMNode::DOCUMENT_POSITION_* -- the DOM's own bit values. */` |
|     - |  1779 | `#define DOM_POS_DISCONNECTED 1` |
|     - |  1780 | `#define DOM_POS_PRECEDING    2` |
|     - |  1781 | `#define DOM_POS_FOLLOWING    4` |
|     - |  1782 | `#define DOM_POS_CONTAINS     8` |
|     - |  1783 | `#define DOM_POS_CONTAINED_BY 16` |
|     - |  1784 | `#define DOM_POS_IMPL_SPEC    32` |
|     - |  1785 |  |
|     - |  1786 | `/* The topmost node reachable by parent links -- the DOCUMENT for a node in a` |
|     - |  1787 | ` * tree, and the outermost detached node otherwise. */` |
|    66 |  1788 | `static xmlNodePtr DomRootOf(xmlNodePtr pNode)` |
|     1 |  1789 | `{` |
|   155 |  1790 | `	while( pNode && pNode->parent ){` |
|    89 |  1791 | `		pNode = pNode->parent;` |
|     1 |  1792 | `	}` |
|    67 |  1793 | `	return pNode;` |
|     1 |  1794 | `}` |
|     - |  1795 | `/* Is pAnc a STRICT ancestor of pNode? libxml parents an attribute at its` |
|     - |  1796 | `` * element, which is how php answers true for `$el->contains($el->attr)`. */`` |
|    38 |  1797 | `static int DomIsAncestorOf(xmlNodePtr pAnc,xmlNodePtr pNode)` |
|     1 |  1798 | `{` |
|    39 |  1799 | `	xmlNodePtr p = pNode ? pNode->parent : 0;` |
|    75 |  1800 | `	for( ; p ; p = p->parent ){` |
|    51 |  1801 | `		if( p == pAnc ){` |
|    15 |  1802 | `			return 1;` |
|     - |  1803 | `		}` |
|    19 |  1804 | `	}` |
|    25 |  1805 | `	return 0;` |
|    20 |  1806 | `}` |
|    24 |  1807 | `static int DomDepthOf(xmlNodePtr pNode)` |
|     1 |  1808 | `{` |
|    25 |  1809 | `	int n = 0;` |
|    93 |  1810 | `	for( ; pNode ; pNode = pNode->parent ){` |
|    69 |  1811 | `		n++;` |
|    35 |  1812 | `	}` |
|    25 |  1813 | `	return n;` |
|     1 |  1814 | `}` |
|     - |  1815 | `/*` |
|     - |  1816 | ` * Does pA come before pB in document order? Both are distinct nodes of one` |
|     - |  1817 | ` * tree. Lifting each to the depth of the other either lands on the SAME node --` |
|     - |  1818 | ` * one is an ancestor of the other, and an ancestor comes first in a preorder` |
|     - |  1819 | ` * walk -- or, after stepping up in lockstep, on two distinct children of one` |
|     - |  1820 | ` * parent, whose child-list order is the answer.` |
|     - |  1821 | ` *` |
|     - |  1822 | ` * The ancestor case is reachable even though compareDocumentPosition answers` |
|     - |  1823 | ` * CONTAINS/CONTAINED_BY for it: an ATTRIBUTE folds onto its element first, so` |
|     - |  1824 | `` * `$root->attr` against `$child->attr` arrives here as the element PAIR with`` |
|     - |  1825 | ` * one of them an ancestor of the other.` |
|     - |  1826 | ` */` |
|    12 |  1827 | `static int DomPrecedesInTree(xmlNodePtr pA,xmlNodePtr pB)` |
|     1 |  1828 | `{` |
|    13 |  1829 | `	int nA = DomDepthOf(pA),nB = DomDepthOf(pB);` |
|    13 |  1830 | `	xmlNodePtr pUpA = pA,pUpB = pB,p;` |
|    15 |  1831 | `	while( nA > nB ){ pUpA = pUpA->parent; nA--; }` |
|    15 |  1832 | `	while( nB > nA ){ pUpB = pUpB->parent; nB--; }` |
|    13 |  1833 | `	if( pUpA == pUpB ){` |
|     5 |  1834 | `		return pUpA == pA;   /* pA was not lifted: it is the ancestor */` |
|     - |  1835 | `	}` |
|     9 |  1836 | `	while( pUpA && pUpB && pUpA->parent != pUpB->parent ){` |
|   ! 0 |  1837 | `		pUpA = pUpA->parent;` |
|   ! 0 |  1838 | `		pUpB = pUpB->parent;` |
|   ! 0 |  1839 | `	}` |
|     9 |  1840 | `	for( p = pUpA ? pUpA->prev : 0 ; p ; p = p->prev ){` |
|     5 |  1841 | `		if( p == pUpB ){` |
|     5 |  1842 | `			return 0;   /* pB is an earlier sibling */` |
|     - |  1843 | `		}` |
|   ! 0 |  1844 | `	}` |
|     5 |  1845 | `	return 1;` |
|     7 |  1846 | `}` |
|     - |  1847 | `/*` |
|     - |  1848 | ` * DOMNode::compareDocumentPosition(DOMNode $other): int` |
|     - |  1849 | ` *` |
|     - |  1850 | `` * The DOM's own algorithm, run with `other` as node1 and the receiver as node2.`` |
|     - |  1851 | ` * An ATTRIBUTE is folded onto its element first, which is what makes an` |
|     - |  1852 | `` * attribute answer `CONTAINED_BY\|FOLLOWING` against its own element and`` |
|     - |  1853 | `` * `IMPLEMENTATION_SPECIFIC` plus the attribute-list order against a sibling`` |
|     - |  1854 | ` * attribute -- and what makes it compare as its element against everything else.` |
|     - |  1855 | ` *` |
|     - |  1856 | ` * Two nodes in different trees are DISCONNECTED, and the direction bit there is` |
|     - |  1857 | ` * php's own: the raw node POINTERS, which is the only thing available and which` |
|     - |  1858 | ` * php marks IMPLEMENTATION_SPECIFIC for exactly that reason. The bit is stable` |
|     - |  1859 | ` * and antisymmetric within one process; it is not comparable ACROSS engines,` |
|     - |  1860 | ` * so no test pins it.` |
|     - |  1861 | ` */` |
|    32 |  1862 | `DOM_METHOD(vm_builtin_DOMNode_compareDocumentPosition)` |
|     1 |  1863 | `{` |
|    33 |  1864 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    33 |  1865 | `	phl_domnode *pOtherNd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|    33 |  1866 | `	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    33 |  1867 | `	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;` |
|    33 |  1868 | `	xmlNodePtr pNode1,pNode2,pAttr1 = 0,pAttr2 = 0;` |
|    33 |  1869 | `	if( pThisNode == 0 \|\| pOther == 0 ){` |
|    10 |  1870 | `		ph7_result_int(pCtx,0);` |
|    10 |  1871 | `		return PH7_OK;` |
|     - |  1872 | `	}` |
|    43 |  1873 | `	if( pThisNode == pOther ){` |
|     3 |  1874 | `		ph7_result_int(pCtx,0);` |
|     3 |  1875 | `		return PH7_OK;` |
|     - |  1876 | `	}` |
|    41 |  1877 | `	pNode1 = pOther;` |
|    41 |  1878 | `	pNode2 = pThisNode;` |
|    41 |  1879 | `	if( pNode1->type == XML_ATTRIBUTE_NODE ){` |
|    13 |  1880 | `		pAttr1 = pNode1;` |
|    13 |  1881 | `		pNode1 = pNode1->parent;` |
|     6 |  1882 | `	}` |
|    41 |  1883 | `	if( pNode2->type == XML_ATTRIBUTE_NODE ){` |
|    13 |  1884 | `		pAttr2 = pNode2;` |
|    13 |  1885 | `		pNode2 = pNode2->parent;` |
|    13 |  1886 | `		if( pAttr1 && pNode1 && pNode1 == pNode2 ){` |
|     - |  1887 | `			xmlAttrPtr pAttr;` |
|     5 |  1888 | `			for( pAttr = pNode2->properties ; pAttr ; pAttr = pAttr->next ){` |
|     5 |  1889 | `				if( (xmlNodePtr)pAttr == pAttr1 ){` |
|     3 |  1890 | `					ph7_result_int(pCtx,DOM_POS_IMPL_SPEC\|DOM_POS_PRECEDING);` |
|     3 |  1891 | `					return PH7_OK;` |
|     - |  1892 | `				}` |
|     3 |  1893 | `				if( (xmlNodePtr)pAttr == pAttr2 ){` |
|     3 |  1894 | `					ph7_result_int(pCtx,DOM_POS_IMPL_SPEC\|DOM_POS_FOLLOWING);` |
|     3 |  1895 | `					return PH7_OK;` |
|     - |  1896 | `				}` |
|   ! 0 |  1897 | `			}` |
|   ! 0 |  1898 | `		}` |
|     4 |  1899 | `	}` |
|    37 |  1900 | `	if( pNode1 == 0 \|\| pNode2 == 0 \|\| DomRootOf(pNode1) != DomRootOf(pNode2) ){` |
|    47 |  1901 | `		ph7_result_int(pCtx,DOM_POS_DISCONNECTED\|DOM_POS_IMPL_SPEC` |
|    24 |  1902 | `			\|((sxuptr)pThisNode > (sxuptr)pOther ? DOM_POS_PRECEDING : DOM_POS_FOLLOWING));` |
|    25 |  1903 | `		return PH7_OK;` |
|     - |  1904 | `	}` |
|    36 |  1905 | `	if( (pAttr1 == 0 && DomIsAncestorOf(pNode1,pNode2))` |
|    36 |  1906 | `	 \|\| (pAttr2 != 0 && pNode1 == pNode2) ){` |
|    15 |  1907 | `		ph7_result_int(pCtx,DOM_POS_CONTAINS\|DOM_POS_PRECEDING);` |
|    15 |  1908 | `		return PH7_OK;` |
|     - |  1909 | `	}` |
|    24 |  1910 | `	if( (pAttr2 == 0 && DomIsAncestorOf(pNode2,pNode1))` |
|    23 |  1911 | `	 \|\| (pAttr1 != 0 && pNode1 == pNode2) ){` |
|    13 |  1912 | `		ph7_result_int(pCtx,DOM_POS_CONTAINED_BY\|DOM_POS_FOLLOWING);` |
|    13 |  1913 | `		return PH7_OK;` |
|     - |  1914 | `	}` |
|    13 |  1915 | `	ph7_result_int(pCtx,DomPrecedesInTree(pNode1,pNode2)` |
|     - |  1916 | `		? DOM_POS_PRECEDING : DOM_POS_FOLLOWING);` |
|    13 |  1917 | `	return PH7_OK;` |
|    27 |  1918 | `}` |
|     - |  1919 | `/* DOMNode::contains(DOMNode\|DOMNameSpaceNode\|null $other): bool -- INCLUSIVE` |
|     - |  1920 | ` * descendant, so a node contains itself, and (libxml parenting attributes) an` |
|     - |  1921 | ` * element contains its own attributes. A namespace DECLARATION is asked about` |
|     - |  1922 | ` * through the element that MAKES it: its own pointer is an xmlNs, which is in` |
|     - |  1923 | ` * no tree at all. */` |
|    18 |  1924 | `DOM_METHOD(vm_builtin_DOMNode_contains)` |
|     1 |  1925 | `{` |
|    19 |  1926 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    19 |  1927 | `	ph7_value *pArg = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? apArg[0] : 0;` |
|    19 |  1928 | `	phl_domnode *pOtherNd = pArg ? DomObjArg(pArg) : 0;` |
|    19 |  1929 | `	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    19 |  1930 | `	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;` |
|    19 |  1931 | `	phl_domnode *pOwnerNd = pArg ? DomNsNodeOwner(pArg) : 0;` |
|    19 |  1932 | `	if( pOwnerNd ){` |
|     3 |  1933 | `		pOther = (xmlNodePtr)pOwnerNd->pNode;` |
|     1 |  1934 | `	}` |
|    33 |  1935 | `	ph7_result_bool(pCtx,pThisNode != 0 && pOther != 0` |
|    23 |  1936 | `		&& (pThisNode == pOther \|\| DomIsAncestorOf(pThisNode,pOther)));` |
|    19 |  1937 | `	return PH7_OK;` |
|     1 |  1938 | `}` |
|     - |  1939 | `/* DOMNode::getRootNode(?array $options = null): DOMNode -- php declares the` |
|     - |  1940 | `` * options array (the shadow-DOM `composed` key) and reads nothing from it. */`` |
|    14 |  1941 | `DOM_METHOD(vm_builtin_DOMNode_getRootNode)` |
|     1 |  1942 | `{` |
|    15 |  1943 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     7 |  1944 | `	SXUNUSED(nArg);` |
|     7 |  1945 | `	SXUNUSED(apArg);` |
|    15 |  1946 | `	return DomResultNodeOf(pCtx,pNd,DomRootOf(pNd ? (xmlNodePtr)pNd->pNode : 0));` |
|     1 |  1947 | `}` |
|     - |  1948 | `/*` |
|     - |  1949 | ` * DOMNode::isSupported(string $feature, string $version): bool -- the DOM Level` |
|     - |  1950 | `` * 1 feature test, and php's whole table is two rows: `XML` at 1.0 or 2.0 and`` |
|     - |  1951 | `` * `Core` at 1.0 (never `Core` at 2.0). The feature name folds case, the version`` |
|     - |  1952 | ` * does not.` |
|     - |  1953 | ` */` |
|    20 |  1954 | `DOM_METHOD(vm_builtin_DOMNode_isSupported)` |
|     1 |  1955 | `{` |
|    21 |  1956 | `	const char *zFeature = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|    21 |  1957 | `	const char *zVersion = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    21 |  1958 | `	int bOne = DomNameIs(zVersion,"1.0");` |
|    21 |  1959 | `	int bTwo = DomNameIs(zVersion,"2.0");` |
|    21 |  1960 | `	int bXml = SyStrlen(zFeature) == 3 && SyStrnicmp(zFeature,"XML",3) == 0;` |
|    21 |  1961 | `	int bCore = SyStrlen(zFeature) == 4 && SyStrnicmp(zFeature,"Core",4) == 0;` |
|    21 |  1962 | `	ph7_result_bool(pCtx,(bXml && (bOne \|\| bTwo)) \|\| (bCore && bOne));` |
|    21 |  1963 | `	return PH7_OK;` |
|     1 |  1964 | `}` |
|     - |  1965 | `/*` |
|     - |  1966 | ` * php's structural equality, which is NOT the DOM spec's to the letter.` |
|     - |  1967 | ` *` |
|     - |  1968 | ` * The type has to match, then the per-kind identity: an ELEMENT compares its` |
|     - |  1969 | `` * namespace URI, its PREFIX and its local name (so `p:m` and `q:m` bound to the`` |
|     - |  1970 | ` * one URI are NOT equal) plus its attributes as a SET -- same count, and every` |
|     - |  1971 | ` * attribute matched by namespace, local name and value regardless of order. An` |
|     - |  1972 | ` * ATTRIBUTE compares its namespace URI, its name and its value and NOT its` |
|     - |  1973 | ` * prefix, which is the asymmetry no reading of the spec predicts. A PI compares` |
|     - |  1974 | ` * target and data, character data its content, an entity REFERENCE its name.` |
|     - |  1975 | ` * Then the children, in order and in the same number.` |
|     - |  1976 | ` */` |
|   228 |  1977 | `static int DomStrEqOrBothNull(const xmlChar *zA,const xmlChar *zB)` |
|     1 |  1978 | `{` |
|   229 |  1979 | `	if( zA == 0 \|\| zB == 0 ){` |
|    91 |  1980 | `		return zA == zB;` |
|     - |  1981 | `	}` |
|   139 |  1982 | `	return xmlStrEqual(zA,zB) != 0;` |
|   115 |  1983 | `}` |
|   128 |  1984 | `static void DomNsHrefOf(xmlNodePtr pNode,const xmlChar **pzHref,const xmlChar **pzPrefix)` |
|     1 |  1985 | `{` |
|   129 |  1986 | `	*pzHref = (pNode->ns && pNode->ns->href) ? pNode->ns->href : 0;` |
|   129 |  1987 | `	*pzPrefix = (pNode->ns && pNode->ns->prefix) ? pNode->ns->prefix : 0;` |
|   129 |  1988 | `}` |
|    26 |  1989 | `static int DomAttrValueEq(xmlNodePtr pA,xmlNodePtr pB)` |
|     1 |  1990 | `{` |
|    27 |  1991 | `	xmlChar *zA = xmlNodeGetContent(pA);` |
|    27 |  1992 | `	xmlChar *zB = xmlNodeGetContent(pB);` |
|    27 |  1993 | `	int bEq = DomStrEqOrBothNull(zA,zB);` |
|    27 |  1994 | `	if( zA ){ xmlFree(zA); }` |
|    27 |  1995 | `	if( zB ){ xmlFree(zB); }` |
|    27 |  1996 | `	return bEq;` |
|     1 |  1997 | `}` |
|    28 |  1998 | `static int DomAttrSetEqual(xmlNodePtr pA,xmlNodePtr pB)` |
|     1 |  1999 | `{` |
|     - |  2000 | `	xmlAttrPtr pOne,pTwo;` |
|    29 |  2001 | `	int nA = 0,nB = 0;` |
|    63 |  2002 | `	for( pOne = pA->properties ; pOne ; pOne = pOne->next ){ nA++; }` |
|    57 |  2003 | `	for( pTwo = pB->properties ; pTwo ; pTwo = pTwo->next ){ nB++; }` |
|    29 |  2004 | `	if( nA != nB ){` |
|     5 |  2005 | `		return 0;` |
|     - |  2006 | `	}` |
|    47 |  2007 | `	for( pOne = pA->properties ; pOne ; pOne = pOne->next ){` |
|     - |  2008 | `		const xmlChar *zHrefA,*zPfxA,*zHrefB,*zPfxB;` |
|    27 |  2009 | `		DomNsHrefOf((xmlNodePtr)pOne,&zHrefA,&zPfxA);` |
|    35 |  2010 | `		for( pTwo = pB->properties ; pTwo ; pTwo = pTwo->next ){` |
|    31 |  2011 | `			DomNsHrefOf((xmlNodePtr)pTwo,&zHrefB,&zPfxB);` |
|    30 |  2012 | `			if( DomStrEqOrBothNull(zHrefA,zHrefB)` |
|    29 |  2013 | `			 && DomStrEqOrBothNull(pOne->name,pTwo->name)` |
|    27 |  2014 | `			 && DomAttrValueEq((xmlNodePtr)pOne,(xmlNodePtr)pTwo) ){` |
|    23 |  2015 | `				break;` |
|     - |  2016 | `			}` |
|     5 |  2017 | `		}` |
|    27 |  2018 | `		if( pTwo == 0 ){` |
|     5 |  2019 | `			return 0;` |
|     - |  2020 | `		}` |
|    12 |  2021 | `	}` |
|    21 |  2022 | `	return 1;` |
|    15 |  2023 | `}` |
|    86 |  2024 | `static int DomNodesEqual(xmlNodePtr pA,xmlNodePtr pB)` |
|     1 |  2025 | `{` |
|     - |  2026 | `	xmlNodePtr pKidA,pKidB;` |
|     - |  2027 | `	const xmlChar *zHrefA,*zPfxA,*zHrefB,*zPfxB;` |
|    87 |  2028 | `	if( pA == 0 \|\| pB == 0 ){` |
|   ! 0 |  2029 | `		return pA == pB;` |
|     - |  2030 | `	}` |
|    87 |  2031 | `	if( pA->type != pB->type ){` |
|     3 |  2032 | `		return 0;` |
|     - |  2033 | `	}` |
|    85 |  2034 | `	switch( pA->type ){` |
|    17 |  2035 | `	case XML_ELEMENT_NODE:` |
|    35 |  2036 | `		DomNsHrefOf(pA,&zHrefA,&zPfxA);` |
|    35 |  2037 | `		DomNsHrefOf(pB,&zHrefB,&zPfxB);` |
|    34 |  2038 | `		if( !DomStrEqOrBothNull(zHrefA,zHrefB) \|\| !DomStrEqOrBothNull(zPfxA,zPfxB)` |
|    33 |  2039 | `		 \|\| !DomStrEqOrBothNull(pA->name,pB->name) \|\| !DomAttrSetEqual(pA,pB) ){` |
|    15 |  2040 | `			return 0;` |
|     - |  2041 | `		}` |
|    21 |  2042 | `		break;` |
|     1 |  2043 | `	case XML_ATTRIBUTE_NODE:` |
|     3 |  2044 | `		DomNsHrefOf(pA,&zHrefA,&zPfxA);` |
|     3 |  2045 | `		DomNsHrefOf(pB,&zHrefB,&zPfxB);` |
|     2 |  2046 | `		if( !DomStrEqOrBothNull(zHrefA,zHrefB)` |
|     2 |  2047 | `		 \|\| !DomStrEqOrBothNull(pA->name,pB->name)` |
|     3 |  2048 | `		 \|\| !DomAttrValueEq(pA,pB) ){` |
|   ! 0 |  2049 | `			return 0;` |
|     - |  2050 | `		}` |
|     - |  2051 | `		/* An attribute's value IS its child list; comparing it twice would only` |
|     - |  2052 | `		 * refuse a value split across nodes that reads the same. */` |
|     3 |  2053 | `		return 1;` |
|     4 |  2054 | `	case XML_PI_NODE:` |
|     8 |  2055 | `		if( !DomStrEqOrBothNull(pA->name,pB->name)` |
|     8 |  2056 | `		 \|\| !DomStrEqOrBothNull(pA->content,pB->content) ){` |
|     5 |  2057 | `			return 0;` |
|     - |  2058 | `		}` |
|     5 |  2059 | `		break;` |
|    12 |  2060 | `	case XML_TEXT_NODE:` |
|     - |  2061 | `	case XML_CDATA_SECTION_NODE:` |
|     - |  2062 | `	case XML_COMMENT_NODE:` |
|    25 |  2063 | `		if( !DomStrEqOrBothNull(pA->content,pB->content) ){` |
|     3 |  2064 | `			return 0;` |
|     - |  2065 | `		}` |
|    23 |  2066 | `		break;` |
|     2 |  2067 | `	case XML_ENTITY_REF_NODE:` |
|     - |  2068 | `		/* The reference's NAME is what a program wrote; its children are the` |
|     - |  2069 | `		 * DECLARATION libxml resolved it to, which is not part of the node. */` |
|     5 |  2070 | `		return DomStrEqOrBothNull(pA->name,pB->name);` |
|     6 |  2071 | `	default:` |
|    12 |  2072 | `		break;` |
|     - |  2073 | `	}` |
|    59 |  2074 | `	pKidA = pA->children;` |
|    59 |  2075 | `	pKidB = pB->children;` |
|    91 |  2076 | `	while( pKidA && pKidB ){` |
|    35 |  2077 | `		if( !DomNodesEqual(pKidA,pKidB) ){` |
|     3 |  2078 | `			return 0;` |
|     - |  2079 | `		}` |
|    33 |  2080 | `		pKidA = pKidA->next;` |
|    33 |  2081 | `		pKidB = pKidB->next;` |
|     1 |  2082 | `	}` |
|    57 |  2083 | `	return pKidA == 0 && pKidB == 0;` |
|    44 |  2084 | `}` |
|     - |  2085 | `/* DOMNode::isEqualNode(?DOMNode $otherNode): bool */` |
|    54 |  2086 | `DOM_METHOD(vm_builtin_DOMNode_isEqualNode)` |
|     1 |  2087 | `{` |
|    55 |  2088 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    55 |  2089 | `	phl_domnode *pOtherNd = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|   107 |  2090 | `	ph7_result_bool(pCtx,pNd != 0 && pOtherNd != 0` |
|    53 |  2091 | `		&& DomNodesEqual((xmlNodePtr)pNd->pNode,(xmlNodePtr)pOtherNd->pNode));` |
|    55 |  2092 | `	return PH7_OK;` |
|     1 |  2093 | `}` |
|     - |  2094 |  |
|     - |  2095 | `/* ===== Copying: cloneNode ===== */` |
|     - |  2096 |  |
|     - |  2097 | `/*` |
|     - |  2098 | ` * One node copied the way php copies it.` |
|     - |  2099 | ` *` |
|     - |  2100 | ` * libxml's generic copier has no case for a DTD node and answers NULL there, so` |
|     - |  2101 | `` * `$doc->doctype->cloneNode()` was `false` -- php reaches for xmlCopyDtd`` |
|     - |  2102 | ` * instead, which carries the whole internal subset (its declarations, entities` |
|     - |  2103 | ``  * and notations) across. The copy keeps the SOURCE's document in its `doc` `` |
|     - |  2104 | ` * slot without being linked into it, which is what makes php's cloned doctype` |
|     - |  2105 | `` * still answer an `internalSubset` while its `parentNode` is null.`` |
|     - |  2106 | ` */` |
|    50 |  2107 | `static xmlNodePtr DomCopyNode(xmlNodePtr pNode,xmlDocPtr pDoc,int iExtended)` |
|     1 |  2108 | `{` |
|     - |  2109 | `	xmlNodePtr pCopy;` |
|    51 |  2110 | `	if( pNode->type == XML_DTD_NODE \|\| pNode->type == XML_DOCUMENT_TYPE_NODE ){` |
|     5 |  2111 | `		pCopy = (xmlNodePtr)xmlCopyDtd((xmlDtdPtr)pNode);` |
|     5 |  2112 | `		if( pCopy ){` |
|     5 |  2113 | `			pCopy->doc = pNode->doc;` |
|     2 |  2114 | `		}` |
|     5 |  2115 | `		return pCopy;` |
|     - |  2116 | `	}` |
|    47 |  2117 | `	return xmlDocCopyNode(pNode,pDoc,iExtended);` |
|    26 |  2118 | `}` |
|     - |  2119 |  |
|     - |  2120 | `/*` |
|     - |  2121 | ` * Cloning a DOCUMENT is not cloning a node: php builds a SECOND document --` |
|     - |  2122 | ` * its own tree, its own wrapper, its own identity cache -- so the copy's` |
|     - |  2123 | `` * `documentElement` answers the copy as its `ownerDocument` and appending a`` |
|     - |  2124 | ` * node of the ORIGINAL into it is the Wrong Document Error it would be between` |
|     - |  2125 | ` * any two documents. Everything else is one xmlDocCopyNode into the SAME tree,` |
|     - |  2126 | ` * parked as an orphan like every other node this file creates.` |
|     - |  2127 | ` *` |
|     - |  2128 | ` * The parser directives ride along: php's copy answers the receiver's whole` |
|     - |  2129 | ` * flag block, not the class defaults.` |
|     - |  2130 | ` */` |
|    12 |  2131 | `static int DomCloneDocument(ph7_context *pCtx,phl_domnode *pNd,int bDeep)` |
|     1 |  2132 | `{` |
|    13 |  2133 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 |  2134 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2135 | `	ph7_class *pClass;` |
|     - |  2136 | `	ph7_class_instance *pObj;` |
|     - |  2137 | `	phl_xmldoc *pShell;` |
|     - |  2138 | `	phl_domnode *pRes;` |
|     - |  2139 | `	xmlDocPtr pCopy;` |
|    13 |  2140 | `	sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    13 |  2141 | `	pCopy = xmlCopyDoc((xmlDocPtr)pNd->pNode,bDeep ? 1 : 0);` |
|    13 |  2142 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::cloneNode");` |
|    13 |  2143 | `	if( pCopy == 0 ){` |
|   ! 0 |  2144 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2145 | `		return PH7_OK;` |
|     - |  2146 | `	}` |
|     - |  2147 | `	/* php answers a plain DOMDocument even when the receiver is a subclass of` |
|     - |  2148 | ``	 * one: the copy is built by the extension, not by `new static`. */`` |
|    13 |  2149 | `	pClass = PH7_VmExtractClass(pVm,"DOMDocument",sizeof("DOMDocument")-1,FALSE,0);` |
|    13 |  2150 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    13 |  2151 | `	pShell = pObj ? PH7_LibxmlNewDoc(pVm,pCopy) : 0;` |
|    13 |  2152 | `	pRes = pShell ? DomNewRes(pVm,pShell,pCopy) : 0;` |
|    13 |  2153 | `	if( pRes == 0 ){` |
|   ! 0 |  2154 | `		if( pShell == 0 ){` |
|   ! 0 |  2155 | `			xmlFreeDoc(pCopy);   /* not registered: nothing else will free it */` |
|   ! 0 |  2156 | `		}` |
|   ! 0 |  2157 | `		if( pObj ){` |
|   ! 0 |  2158 | `			PH7_ClassInstanceUnref(pObj);` |
|   ! 0 |  2159 | `		}` |
|   ! 0 |  2160 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  2161 | `	}` |
|    13 |  2162 | `	DomSetRes(pVm,pObj,pRes);` |
|    13 |  2163 | `	PH7_NativeSetAttrObj(pVm,pObj,DOM_DOC,pObj);` |
|    13 |  2164 | `	if( pThis ){` |
|     - |  2165 | `		ph7_value *pFrom,*pTo;` |
|     - |  2166 | `		sxu32 i;` |
|    97 |  2167 | `		for( i = 0 ; i < SX_ARRAYSIZE(azDomDocFlag) ; ++i ){` |
|   127 |  2168 | `			PH7_NativeSetAttrBool(pVm,pObj,azDomDocFlag[i],` |
|    84 |  2169 | `				PH7_NativeAttrTruthy(pThis,azDomDocFlag[i]));` |
|    43 |  2170 | `		}` |
|     - |  2171 | `		/* ...and the registerNodeClass table, which php's copy answers too --` |
|     - |  2172 | `		 * shared copy-on-write, which the map's own writer separates. */` |
|    13 |  2173 | `		pFrom = PH7_NativeAttr(pThis,DOM_NCLS);` |
|    13 |  2174 | `		pTo = PH7_NativeAttr(pObj,DOM_NCLS);` |
|    13 |  2175 | `		if( pFrom && pTo && (pFrom->iFlags & MEMOBJ_HASHMAP) ){` |
|     3 |  2176 | `			PH7_MemObjStore(pFrom,pTo);` |
|     1 |  2177 | `		}` |
|     6 |  2178 | `	}` |
|    13 |  2179 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    13 |  2180 | `	return PH7_OK;` |
|     7 |  2181 | `}` |
|     - |  2182 | `/*` |
|     - |  2183 | ` * DOMNode::cloneNode(bool $deep = false): DOMNode\|false` |
|     - |  2184 | ` *` |
|     - |  2185 | `` * The SHALLOW copy is not libxml's shallow copy: php asks for `extended = 2`,`` |
|     - |  2186 | `` * which carries an element's attributes and its own `xmlns` declarations across`` |
|     - |  2187 | `` * while leaving the children behind -- so `$el->cloneNode()` is a usable`` |
|     - |  2188 | `` * template row, not a bare tag. A deep one is `extended = 1`, and libxml then`` |
|     - |  2189 | ` * reconciles whatever namespace the descendants were using onto the copy.` |
|     - |  2190 | ` */` |
|    44 |  2191 | `DOM_METHOD(vm_builtin_DOMNode_cloneNode)` |
|     1 |  2192 | `{` |
|    45 |  2193 | `	ph7_vm *pVm = pCtx->pVm;` |
|    45 |  2194 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    45 |  2195 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    45 |  2196 | `	int bDeep = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|     - |  2197 | `	xmlNodePtr pCopy;` |
|     - |  2198 | `	sxu32 nMark;` |
|    45 |  2199 | `	if( pNode == 0 ){` |
|   ! 0 |  2200 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  2201 | `		return PH7_OK;` |
|     - |  2202 | `	}` |
|    45 |  2203 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|    13 |  2204 | `		return DomCloneDocument(pCtx,pNd,bDeep);` |
|     - |  2205 | `	}` |
|    33 |  2206 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    33 |  2207 | `	pCopy = DomCopyNode(pNode,pNode->doc,bDeep ? 1 : 2);` |
|    33 |  2208 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::cloneNode");` |
|    33 |  2209 | `	if( pCopy == 0 ){` |
|     5 |  2210 | `		ph7_result_bool(pCtx,0);` |
|     5 |  2211 | `		return PH7_OK;` |
|     - |  2212 | `	}` |
|     - |  2213 | `	/* An ATTRIBUTE copy comes back in NO namespace: libxml resolves an` |
|     - |  2214 | `	 * attribute's prefix against the element it is being copied ONTO, and there` |
|     - |  2215 | ``	 * is no element here. php's clone keeps the namespace, so `p:at="1"` cloned`` |
|     - |  2216 | ``	 * stays `p:at="1"` rather than turning into `at="1"` -- a silent rename of`` |
|     - |  2217 | `	 * the very attribute a namespaced document is keyed on. The copy borrows the` |
|     - |  2218 | `	 * SOURCE's declaration, which is the only thing it can do: an attribute` |
|     - |  2219 | ``	 * carries no `nsDef` of its own, and the declaration outlives it (nothing in`` |
|     - |  2220 | `	 * this file frees a node before its document). */` |
|    29 |  2221 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pCopy->ns == 0 ){` |
|     3 |  2222 | `		pCopy->ns = pNode->ns;` |
|     1 |  2223 | `	}` |
|    29 |  2224 | `	DomOrphanAdd(pNd->pShell,pCopy);` |
|    29 |  2225 | `	return DomResultNodeOf(pCtx,pNd,pCopy);` |
|    23 |  2226 | `}` |
|     - |  2227 | `/* Enter one wrapper into a holder's identity cache, keyed by the node pointer.` |
|     - |  2228 | ` * The cache takes its OWN reference; the caller keeps whatever it holds. */` |
|   252 |  2229 | `static void DomCacheStore(ph7_vm *pVm,ph7_class_instance *pDoc,xmlNodePtr pNode,` |
|     - |  2230 | `	ph7_class_instance *pObj)` |
|     1 |  2231 | `{` |
|   253 |  2232 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|     - |  2233 | `	ph7_value sKey,sVal;` |
|   253 |  2234 | `	if( pCache == 0 ){` |
|   ! 0 |  2235 | `		return;` |
|     - |  2236 | `	}` |
|   253 |  2237 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|   253 |  2238 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|   253 |  2239 | `	sVal.x.pOther = pObj;` |
|   253 |  2240 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|   253 |  2241 | `	PH7_HashmapInsert(pCache,&sKey,&sVal);` |
|   253 |  2242 | `	PH7_MemObjRelease(&sKey);` |
|   127 |  2243 | `}` |
|     - |  2244 | `/* Empty a slot the instance copied from its clone source: the null value. */` |
|    10 |  2245 | `static void DomSetSlotNull(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxu32 nName)` |
|     1 |  2246 | `{` |
|     - |  2247 | `	ph7_value sNull;` |
|    11 |  2248 | `	PH7_MemObjInit(&(*pVm),&sNull);` |
|    11 |  2249 | `	PH7_NativeSetProp(&(*pVm),pObj,zName,nName,&sNull);` |
|    11 |  2250 | `}` |
|     - |  2251 | `/*` |
|     - |  2252 | `` * `clone $node` / `clone $doc` -- ph7_class::xClone for the DOM classes.`` |
|     - |  2253 | ` *` |
|     - |  2254 | ` * php's clone_obj handler copies the NODE, so the clone is a second SUBTREE and` |
|     - |  2255 | ` * not a second object over the same one.  The slot-by-slot copy that runs` |
|     - |  2256 | ` * before this hook duplicated $__res, and stopping there is the XMLWriter clone` |
|     - |  2257 | ` * bug one family later: a write through either object shows through both.` |
|     - |  2258 | ` *` |
|     - |  2259 | `` * php's rules, measured: the copy is always DEEP (`clone $el` carries the whole`` |
|     - |  2260 | ` * subtree where cloneNode() defaults shallow), always DETACHED, and stays in` |
|     - |  2261 | `` * the SAME document -- `$c->ownerDocument === $d` -- while a DOCUMENT is copied`` |
|     - |  2262 | ` * whole into a second document, directives, declaration and URI included, so` |
|     - |  2263 | ` * mutating the copy's tree leaves the original's bytes alone.  A user subclass` |
|     - |  2264 | ` * clones through the inherited hook and keeps its class and its own properties,` |
|     - |  2265 | ` * php's handler inheritance (the ENGINE's chain walk serves that).` |
|     - |  2266 | ` */` |
|    18 |  2267 | `static void DomInstanceClone(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)` |
|     1 |  2268 | `{` |
|    19 |  2269 | `	phl_domnode *pNd = DomResOf(pSrc);` |
|    19 |  2270 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    19 |  2271 | `	ph7_class_instance *pDoc = PH7_NativeAttrObj(pClone,DOM_DOC);` |
|     - |  2272 | `	xmlNodePtr pCopy;` |
|     - |  2273 | `	phl_domnode *pRes;` |
|    19 |  2274 | `	if( pNode == 0 ){` |
|   ! 0 |  2275 | `		return;   /* no node behind the source: the copy has none either */` |
|     - |  2276 | `	}` |
|    19 |  2277 | `	pCopy = DomCopyNode(pNode,pNode->doc,1);` |
|    19 |  2278 | `	pRes = pCopy ? DomNewRes(&(*pVm),pNd->pShell,pCopy) : 0;` |
|    19 |  2279 | `	if( pRes == 0 ){` |
|     - |  2280 | `		/* Never leave the slot-copied handle in place: two objects over one` |
|     - |  2281 | `		 * node is the exact aliasing this hook exists to prevent. */` |
|     3 |  2282 | `		if( pCopy ){` |
|   ! 0 |  2283 | `			xmlFreeNode(pCopy);` |
|   ! 0 |  2284 | `		}` |
|     3 |  2285 | `		DomSetSlotNull(&(*pVm),pClone,DOM_RES,sizeof(DOM_RES)-1);` |
|     3 |  2286 | `		return;` |
|     - |  2287 | `	}` |
|     - |  2288 | `	/* The same namespace borrow cloneNode() does: an attribute copied with no` |
|     - |  2289 | `	 * element to resolve against comes back in NO namespace. */` |
|    17 |  2290 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pCopy->ns == 0 ){` |
|     3 |  2291 | `		pCopy->ns = pNode->ns;` |
|     1 |  2292 | `	}` |
|    17 |  2293 | `	DomOrphanAdd(pNd->pShell,pCopy);` |
|    17 |  2294 | `	DomSetRes(&(*pVm),pClone,pRes);` |
|     - |  2295 | `	/* The clone IS the copy's wrapper: enter it into the identity cache so` |
|     - |  2296 | ``	 * `$c->firstChild->parentNode === $c` holds. ($__doc rode the slot copy.) */`` |
|    17 |  2297 | `	DomCacheStore(&(*pVm),pDoc,pCopy,pClone);` |
|    10 |  2298 | `}` |
|     8 |  2299 | `static void DomInstanceCloneDoc(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)` |
|     1 |  2300 | `{` |
|     9 |  2301 | `	phl_domnode *pNd = DomResOf(pSrc);` |
|     - |  2302 | `	xmlDocPtr pCopy;` |
|     - |  2303 | `	phl_xmldoc *pShell;` |
|     - |  2304 | `	phl_domnode *pRes;` |
|     9 |  2305 | `	if( pNd == 0 \|\| pNd->pNode == 0 ){` |
|   ! 0 |  2306 | `		return;` |
|     - |  2307 | `	}` |
|     9 |  2308 | `	pCopy = xmlCopyDoc((xmlDocPtr)pNd->pNode,1);` |
|     9 |  2309 | `	pShell = pCopy ? PH7_LibxmlNewDoc(&(*pVm),pCopy) : 0;` |
|     9 |  2310 | `	pRes = pShell ? DomNewRes(&(*pVm),pShell,pCopy) : 0;` |
|     9 |  2311 | `	if( pRes == 0 ){` |
|   ! 0 |  2312 | `		if( pCopy && pShell == 0 ){` |
|   ! 0 |  2313 | `			xmlFreeDoc(pCopy);   /* not registered: nothing else will free it */` |
|   ! 0 |  2314 | `		}` |
|   ! 0 |  2315 | `		DomSetSlotNull(&(*pVm),pClone,DOM_RES,sizeof(DOM_RES)-1);` |
|   ! 0 |  2316 | `		return;` |
|     - |  2317 | `	}` |
|     9 |  2318 | `	DomSetRes(&(*pVm),pClone,pRes);` |
|     - |  2319 | `	/* Its own document, its own identity cache: the slot copy pointed both at` |
|     - |  2320 | `	 * the SOURCE's, so the copy's documentElement would have answered the` |
|     - |  2321 | `	 * original document as its owner. (The directive slots the copy carried` |
|     - |  2322 | `	 * across are php's answer and stay.) */` |
|     9 |  2323 | `	PH7_NativeSetAttrObj(&(*pVm),pClone,DOM_DOC,pClone);` |
|     9 |  2324 | `	DomSetSlotNull(&(*pVm),pClone,DOM_NODES,sizeof(DOM_NODES)-1);` |
|     5 |  2325 | `}` |
|     - |  2326 |  |
|     - |  2327 | `/* ===== The node CONSTRUCTORS: php's ownerless nodes ===== */` |
|     - |  2328 |  |
|     - |  2329 | `/*` |
|     - |  2330 | `` * php gives a constructed node NO document at all -- `(new DOMText('t'))->`` |
|     - |  2331 | `` * ownerDocument` is null and the libxml node's doc is NULL -- and adopts it on`` |
|     - |  2332 | ` * the first insertion.  Until then the node has to be OWNED by something that` |
|     - |  2333 | ` * frees it: the limbo shell, one per VM, a phl_xmldoc with no xmlDoc whose` |
|     - |  2334 | ` * orphan set carries every constructed-and-never-adopted node to teardown.` |
|     - |  2335 | ` */` |
|   236 |  2336 | `static phl_xmldoc * DomLimboShell(ph7_vm *pVm)` |
|     1 |  2337 | `{` |
|   237 |  2338 | `	if( pVm->pXmlLimbo == 0 ){` |
|     3 |  2339 | `		pVm->pXmlLimbo = PH7_LibxmlNewDoc(&(*pVm),0);` |
|     1 |  2340 | `	}` |
|   237 |  2341 | `	return (phl_xmldoc *)pVm->pXmlLimbo;` |
|     1 |  2342 | `}` |
|     - |  2343 | `/*` |
|     - |  2344 | ` * The shared constructor tail: park the fresh node on the limbo shell, wire` |
|     - |  2345 | ` * the instance's two slots, and make the instance its OWN holder -- $__doc` |
|     - |  2346 | ` * points at itself and the identity cache lives on it, exactly the document's` |
|     - |  2347 | `` * own arrangement, so `$e->firstChild->parentNode === $e` holds for a tree`` |
|     - |  2348 | ` * that belongs to no document.  (ownerDocument still answers null: the getter` |
|     - |  2349 | ` * reads the NODE's document, not the slot.)  Takes ownership of pNode either` |
|     - |  2350 | `` * way; a re-run constructor -- `$t->__construct('b')`, which php allows --`` |
|     - |  2351 | ` * simply re-points the slots and leaves the old node parked.` |
|     - |  2352 | ` */` |
|   216 |  2353 | `static int DomCtorInstall(ph7_context *pCtx,xmlNodePtr pNode)` |
|     1 |  2354 | `{` |
|   217 |  2355 | `	ph7_vm *pVm = pCtx->pVm;` |
|   217 |  2356 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  2357 | `	phl_xmldoc *pShell;` |
|     - |  2358 | `	phl_domnode *pRes;` |
|   217 |  2359 | `	if( pNode == 0 ){` |
|   ! 0 |  2360 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  2361 | `	}` |
|   217 |  2362 | `	pShell = pThis ? DomLimboShell(pVm) : 0;` |
|   217 |  2363 | `	pRes = pShell ? DomNewRes(pVm,pShell,pNode) : 0;` |
|   217 |  2364 | `	if( pRes == 0 ){` |
|   ! 0 |  2365 | `		xmlFreeNode(pNode);` |
|   ! 0 |  2366 | `		return pThis ? PH7_ContextMemoryError(pCtx) : PH7_OK;` |
|     - |  2367 | `	}` |
|   217 |  2368 | `	DomOrphanAdd(pShell,pNode);` |
|   217 |  2369 | `	DomSetRes(pVm,pThis,pRes);` |
|   217 |  2370 | `	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);` |
|   217 |  2371 | `	DomCacheStore(pVm,pThis,pNode,pThis);` |
|   217 |  2372 | `	return PH7_OK;` |
|   109 |  2373 | `}` |
|     - |  2374 | `/* The content of the character-data three: php passes NULL for an OMITTED` |
|     - |  2375 | ` * argument and the string -- even the empty one -- for a given one, which is` |
|     - |  2376 | `` * why `new DOMText()` has a NULL nodeValue where `new DOMText('')` reads "". */`` |
|    66 |  2377 | `DOM_METHOD(vm_builtin_DOMText_construct)` |
|     1 |  2378 | `{` |
|    67 |  2379 | `	int nData = 0;` |
|    67 |  2380 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : 0;` |
|    67 |  2381 | `	xmlNodePtr pNode = zData` |
|    56 |  2382 | `		? xmlNewDocTextLen(0,(const xmlChar *)zData,nData)` |
|    38 |  2383 | `		: xmlNewDocText(0,0);` |
|    67 |  2384 | `	return DomCtorInstall(pCtx,pNode);` |
|     1 |  2385 | `}` |
|    16 |  2386 | `DOM_METHOD(vm_builtin_DOMComment_construct)` |
|     1 |  2387 | `{` |
|    17 |  2388 | `	int nData = 0;` |
|    17 |  2389 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : 0;` |
|     - |  2390 | `	xmlNodePtr pNode;` |
|    17 |  2391 | `	if( zData ){` |
|     - |  2392 | `		/* libxml has no length-taking comment constructor and` |
|     - |  2393 | `		 * xmlNewDocComment measures with strlen, so a NUL-carrying PHP string` |
|     - |  2394 | `		 * goes through a bounded copy. */` |
|    13 |  2395 | `		xmlChar *zCopy = xmlStrndup((const xmlChar *)zData,nData);` |
|    13 |  2396 | `		pNode = zCopy ? xmlNewDocComment(0,zCopy) : 0;` |
|    13 |  2397 | `		if( zCopy ){` |
|    13 |  2398 | `			xmlFree(zCopy);` |
|     6 |  2399 | `		}` |
|     7 |  2400 | `	}else{` |
|     5 |  2401 | `		pNode = xmlNewDocComment(0,0);` |
|     - |  2402 | `	}` |
|    17 |  2403 | `	return DomCtorInstall(pCtx,pNode);` |
|     1 |  2404 | `}` |
|    10 |  2405 | `DOM_METHOD(vm_builtin_DOMCdataSection_construct)` |
|     1 |  2406 | `{` |
|    11 |  2407 | `	int nData = 0;` |
|    11 |  2408 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : "";` |
|    16 |  2409 | `	return DomCtorInstall(pCtx,` |
|     5 |  2410 | `		xmlNewCDataBlock(0,(const xmlChar *)zData,nData));` |
|     1 |  2411 | `}` |
|     - |  2412 | `/*` |
|     - |  2413 | ` * DOMElement::__construct(string $qualifiedName, ?string $value = null,` |
|     - |  2414 | ` *                         string $namespace = '')` |
|     - |  2415 | ` *` |
|     - |  2416 | ` * The constructor's name grammar is its OWN, not createElementNS's, each cell` |
|     - |  2417 | `` * measured: the whole name must be an XML Name first (so `1:a` is Invalid`` |
|     - |  2418 | ` * Character where createElementNS answers Namespace), a prefix without a` |
|     - |  2419 | ` * namespace is the Namespace refusal, and WITH one the name must be a QName` |
|     - |  2420 | `` * whose prefix is neither `xml` nor `xmlns` -- php refuses `xml:a` here even`` |
|     - |  2421 | ` * against the xml namespace's own URI, where createElementNS allows it. A` |
|     - |  2422 | `` * plain `xmlns` passes as an ordinary name and binds the DEFAULT namespace.`` |
|     - |  2423 | ` *` |
|     - |  2424 | ``  * The $value rides libxml's entity parser, createElement's own quirk: `&amp;` `` |
|     - |  2425 | `` * becomes `&`, and an unterminated reference warns (under this constructor's`` |
|     - |  2426 | ` * name) and drops the whole value. An attribute's value -- the constructor` |
|     - |  2427 | `` * below -- is LITERAL instead: `&amp;` stays five characters.`` |
|     - |  2428 | ` */` |
|    96 |  2429 | `DOM_METHOD(vm_builtin_DOMElement_construct)` |
|     1 |  2430 | `{` |
|    97 |  2431 | `	ph7_vm *pVm = pCtx->pVm;` |
|    97 |  2432 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    97 |  2433 | `	int nVal = 0;` |
|    68 |  2434 | `	const char *zVal = (nArg > 1 && !ph7_value_is_null(apArg[1]))` |
|    73 |  2435 | `		? ph7_value_to_string(apArg[1],&nVal) : 0;` |
|    97 |  2436 | `	const char *zUri = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|    97 |  2437 | `	int bHasUri = zUri[0] != 0;` |
|    97 |  2438 | `	xmlChar *zPrefix = 0;` |
|     - |  2439 | `	xmlChar *zLocal;` |
|     - |  2440 | `	xmlNodePtr pNode;` |
|     - |  2441 | `	sxu32 nMark;` |
|    97 |  2442 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|    13 |  2443 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  2444 | `	}` |
|     - |  2445 | `	/* The split is BY HAND, at the first colon, with a leading colon meaning` |
|     - |  2446 | `	 * no prefix at all: libxml's xmlSplitQName2 changed its answer for a name` |
|     - |  2447 | `	 * that ENDS in the colon between 2.9 and 2.13 (the Windows gate caught` |
|     - |  2448 | ``	 * `new DOMElement('a:')` constructing there), and the grammar must answer`` |
|     - |  2449 | `	 * the same on every platform. */` |
|     - |  2450 | `	{` |
|    85 |  2451 | `		const xmlChar *zColon = xmlStrchr((const xmlChar *)zName,':');` |
|    85 |  2452 | `		if( zColon && zColon != (const xmlChar *)zName ){` |
|    43 |  2453 | `			zPrefix = xmlStrndup((const xmlChar *)zName,` |
|    28 |  2454 | `				(int)(zColon - (const xmlChar *)zName));` |
|    29 |  2455 | `			zLocal = xmlStrdup(zColon + 1);` |
|    15 |  2456 | `		}else{` |
|    57 |  2457 | `			zLocal = 0;` |
|     - |  2458 | `		}` |
|     - |  2459 | `	}` |
|    85 |  2460 | `	if( !bHasUri ){` |
|    61 |  2461 | `		if( zPrefix ){` |
|     - |  2462 | `			/* A prefix names a namespace, and none came. */` |
|    13 |  2463 | `			xmlFree(zPrefix);` |
|    13 |  2464 | `			xmlFree(zLocal);` |
|    13 |  2465 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  2466 | `		}` |
|    49 |  2467 | `		if( zLocal ){` |
|   ! 0 |  2468 | `			xmlFree(zLocal);` |
|   ! 0 |  2469 | `		}` |
|    49 |  2470 | ``		zLocal = 0;   /* the whole name, `:a` included */`` |
|    25 |  2471 | `	}else{` |
|    24 |  2472 | `		if( xmlValidateQName((const xmlChar *)zName,0) != 0` |
|    21 |  2473 | `		 \|\| (zPrefix && (xmlStrEqual(zPrefix,(const xmlChar *)"xml")` |
|     9 |  2474 | `		              \|\| xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){` |
|    13 |  2475 | `			if( zPrefix ){` |
|    11 |  2476 | `				xmlFree(zPrefix);` |
|     5 |  2477 | `			}` |
|    13 |  2478 | `			if( zLocal ){` |
|    11 |  2479 | `				xmlFree(zLocal);` |
|     5 |  2480 | `			}` |
|    13 |  2481 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  2482 | `		}` |
|     - |  2483 | `	}` |
|    61 |  2484 | `	pNode = xmlNewNode(0,zLocal ? zLocal : (const xmlChar *)zName);` |
|    61 |  2485 | `	if( zLocal ){` |
|     7 |  2486 | `		xmlFree(zLocal);` |
|     3 |  2487 | `	}` |
|    61 |  2488 | `	if( pNode == 0 ){` |
|   ! 0 |  2489 | `		if( zPrefix ){` |
|   ! 0 |  2490 | `			xmlFree(zPrefix);` |
|   ! 0 |  2491 | `		}` |
|   ! 0 |  2492 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  2493 | `	}` |
|    61 |  2494 | `	if( bHasUri ){` |
|     - |  2495 | `		/* On the node's OWN nsDef, so the declaration serializes here once an` |
|     - |  2496 | `		 * insertion adopts the element and lookupNamespaceURI answers it` |
|     - |  2497 | `		 * meanwhile; the reconcile strips it wherever an ancestor already` |
|     - |  2498 | `		 * declares the binding. */` |
|    13 |  2499 | `		xmlNsPtr pNs = xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);` |
|    13 |  2500 | `		if( pNs ){` |
|    13 |  2501 | `			xmlSetNs(pNode,pNs);` |
|     6 |  2502 | `		}` |
|     6 |  2503 | `	}` |
|    61 |  2504 | `	if( zPrefix ){` |
|     7 |  2505 | `		xmlFree(zPrefix);` |
|     3 |  2506 | `	}` |
|    61 |  2507 | `	if( zVal && nVal > 0 ){` |
|     - |  2508 | `` 		/* The EMPTY value is skipped whole -- php's `new DOMElement('a','')` `` |
|     - |  2509 | `		 * has no text child at all, where libxml's setter would leave one. */` |
|    11 |  2510 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    11 |  2511 | `		xmlNodeSetContentLen(pNode,(const xmlChar *)zVal,nVal);` |
|    11 |  2512 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMElement::__construct");` |
|     5 |  2513 | `	}` |
|    61 |  2514 | `	return DomCtorInstall(pCtx,pNode);` |
|    49 |  2515 | `}` |
|     - |  2516 | `/*` |
|     - |  2517 | ` * The last three: a FRAGMENT takes nothing at all; a PROCESSING INSTRUCTION` |
|     - |  2518 | `` * validates its target as a plain XML Name (`xml`, `XML` and `p:a` all pass --`` |
|     - |  2519 | ` * php never asks whether the target is reserved) and stores its data` |
|     - |  2520 | ` * literally, NULL when omitted like the character-data three; an ENTITY` |
|     - |  2521 | ` * REFERENCE validates its name and takes libxml's answer for the content: a` |
|     - |  2522 | `` * PREDEFINED name (`amp`) arrives with the shared entity declaration as its`` |
|     - |  2523 | ` * child -- a STATIC libxml global, wrapped but never owned, which is why the` |
|     - |  2524 | ` * constructor parks only the reference node itself on the limbo shell.` |
|     - |  2525 | ` */` |
|    12 |  2526 | `DOM_METHOD(vm_builtin_DOMDocumentFragment_construct)` |
|     1 |  2527 | `{` |
|     6 |  2528 | `	SXUNUSED(nArg);` |
|     6 |  2529 | `	SXUNUSED(apArg);` |
|    13 |  2530 | `	return DomCtorInstall(pCtx,xmlNewDocFragment(0));` |
|     1 |  2531 | `}` |
|    22 |  2532 | `DOM_METHOD(vm_builtin_DOMProcessingInstruction_construct)` |
|     1 |  2533 | `{` |
|    23 |  2534 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    23 |  2535 | `	const char *zData = nArg > 1 ? ph7_value_to_string(apArg[1],0) : 0;` |
|    23 |  2536 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     7 |  2537 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  2538 | `	}` |
|    25 |  2539 | `	return DomCtorInstall(pCtx,` |
|     8 |  2540 | `		xmlNewPI((const xmlChar *)zName,(const xmlChar *)zData));` |
|    12 |  2541 | `}` |
|    24 |  2542 | `DOM_METHOD(vm_builtin_DOMEntityReference_construct)` |
|     1 |  2543 | `{` |
|    25 |  2544 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    25 |  2545 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     7 |  2546 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  2547 | `	}` |
|    19 |  2548 | `	return DomCtorInstall(pCtx,xmlNewReference(0,(const xmlChar *)zName));` |
|    13 |  2549 | `}` |
|     - |  2550 | `/*` |
|     - |  2551 | ` * DOMAttr::__construct(string $name, string $value = '')` |
|     - |  2552 | ` *` |
|     - |  2553 | `` * The name is a plain XML Name -- NO QName split at all, so `p:a` and even`` |
|     - |  2554 | `` * `xmlns:x` pass whole and carry no namespace (`prefix` reads "" and`` |
|     - |  2555 | `` * `localName` the full spelling). The value is LITERAL: php builds the text`` |
|     - |  2556 | ` * child directly rather than through the entity parser, which is what keeps` |
|     - |  2557 | `` * `&amp;` five characters where the element constructor's value collapses it.`` |
|     - |  2558 | ` */` |
|    24 |  2559 | `DOM_METHOD(vm_builtin_DOMAttr_construct)` |
|     1 |  2560 | `{` |
|    25 |  2561 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    25 |  2562 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    25 |  2563 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     7 |  2564 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  2565 | `	}` |
|    28 |  2566 | `	return DomCtorInstall(pCtx,` |
|    18 |  2567 | `		(xmlNodePtr)xmlNewProp(0,(const xmlChar *)zName,(const xmlChar *)zVal));` |
|    13 |  2568 | `}` |
|     - |  2569 |  |
|     - |  2570 | `/* ===== Namespaces ===== */` |
|     - |  2571 |  |
|     - |  2572 | `/*` |
|     - |  2573 | ` * Where a namespace lookup starts. php resolves a DOCUMENT to its root element` |
|     - |  2574 | `` * first -- so `$doc->lookupPrefix($uri)` answers what the document element`` |
|     - |  2575 | ` * would, and an empty document answers nothing at all -- and starts from the` |
|     - |  2576 | ` * node itself for everything else, because libxml's own search walks up the` |
|     - |  2577 | ` * parent chain (which is how a text node or a PI reaches its element's` |
|     - |  2578 | ` * declarations, and how a detached one reaches none).` |
|     - |  2579 | ` */` |
|   130 |  2580 | `static xmlNodePtr DomNsAnchor(xmlNodePtr pNode)` |
|     1 |  2581 | `{` |
|   131 |  2582 | `	if( pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE) ){` |
|    29 |  2583 | `		return (xmlNodePtr)xmlDocGetRootElement((xmlDocPtr)pNode);` |
|     - |  2584 | `	}` |
|   103 |  2585 | `	return pNode;` |
|    66 |  2586 | `}` |
|     - |  2587 | ``/* A `?string` argument: its bytes, or NULL for a null one. */`` |
|   508 |  2588 | `static const char * DomArgStrOrNull(int nArg,ph7_value **apArg,int iArg)` |
|     1 |  2589 | `{` |
|   509 |  2590 | `	if( iArg >= nArg \|\| ph7_value_is_null(apArg[iArg]) ){` |
|    93 |  2591 | `		return 0;` |
|     - |  2592 | `	}` |
|   417 |  2593 | `	return ph7_value_to_string(apArg[iArg],0);` |
|   255 |  2594 | `}` |
|     - |  2595 | `/* DOMNode::lookupNamespaceURI(?string $prefix): ?string */` |
|    62 |  2596 | `DOM_METHOD(vm_builtin_DOMNode_lookupNamespaceURI)` |
|     1 |  2597 | `{` |
|    63 |  2598 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    63 |  2599 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|    63 |  2600 | `	const char *zPrefix = DomArgStrOrNull(nArg,apArg,0);` |
|    63 |  2601 | `	xmlNsPtr pNs = pNode ? xmlSearchNs(pNode->doc,pNode,(const xmlChar *)zPrefix) : 0;` |
|    63 |  2602 | `	if( pNs && pNs->href ){` |
|    35 |  2603 | `		ph7_result_string(pCtx,(const char *)pNs->href,-1);` |
|    18 |  2604 | `	}else{` |
|    29 |  2605 | `		ph7_result_null(pCtx);` |
|     - |  2606 | `	}` |
|    63 |  2607 | `	return PH7_OK;` |
|     1 |  2608 | `}` |
|     - |  2609 | `/* DOMNode::lookupPrefix(string $namespace): ?string -- the DEFAULT namespace has` |
|     - |  2610 | `` * no prefix, so a document whose only declaration is `xmlns="..."` answers null`` |
|     - |  2611 | ` * for the very URI lookupNamespaceURI(null) hands back. */` |
|    36 |  2612 | `DOM_METHOD(vm_builtin_DOMNode_lookupPrefix)` |
|     1 |  2613 | `{` |
|    37 |  2614 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    37 |  2615 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|    37 |  2616 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    37 |  2617 | `	xmlNsPtr pNs = (pNode && zUri[0]) ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;` |
|    37 |  2618 | `	if( pNs && pNs->prefix ){` |
|    15 |  2619 | `		ph7_result_string(pCtx,(const char *)pNs->prefix,-1);` |
|     8 |  2620 | `	}else{` |
|    23 |  2621 | `		ph7_result_null(pCtx);` |
|     - |  2622 | `	}` |
|    37 |  2623 | `	return PH7_OK;` |
|     1 |  2624 | `}` |
|     - |  2625 | `/* DOMNode::isDefaultNamespace(string $namespace): bool -- php tests the URI` |
|     - |  2626 | ` * against the default declaration in scope, and answers FALSE for the empty` |
|     - |  2627 | ` * string rather than "this node is in no namespace". */` |
|    32 |  2628 | `DOM_METHOD(vm_builtin_DOMNode_isDefaultNamespace)` |
|     1 |  2629 | `{` |
|    33 |  2630 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    33 |  2631 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|    33 |  2632 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    33 |  2633 | `	xmlNsPtr pNs = (pNode && zUri[0]) ? xmlSearchNs(pNode->doc,pNode,0) : 0;` |
|    45 |  2634 | `	ph7_result_bool(pCtx,pNs != 0 && pNs->href != 0` |
|    22 |  2635 | `		&& xmlStrEqual(pNs->href,(const xmlChar *)zUri));` |
|    33 |  2636 | `	return PH7_OK;` |
|     1 |  2637 | `}` |
|     - |  2638 |  |
|     - |  2639 | `/* ===== Element attributes ===== */` |
|     - |  2640 |  |
|     - |  2641 | `/* Document-order successor within pRoot's subtree (pRoot excluded) */` |
|  1576 |  2642 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot)` |
|     1 |  2643 | `{` |
|  1577 |  2644 | `	if( pCur->children ){` |
|   660 |  2645 | `		return pCur->children;` |
|     - |  2646 | `	}` |
|  1453 |  2647 | `	while( pCur && pCur != pRoot ){` |
|  1133 |  2648 | `		if( pCur->next ){` |
|   598 |  2649 | `			return pCur->next;` |
|     - |  2650 | `		}` |
|   536 |  2651 | `		pCur = pCur->parent;` |
|     1 |  2652 | `	}` |
|   321 |  2653 | `	return 0;` |
|   790 |  2654 | `}` |
|     - |  2655 | `/*` |
|     - |  2656 | ` * ===== Qualified names and the namespaces they need =====` |
|     - |  2657 | ` *` |
|     - |  2658 | ` * The grammar php screens a created name against, and the rule by which it` |
|     - |  2659 | ` * finds or declares the namespace behind it.  Both were missing here, and what` |
|     - |  2660 | `` * stood in for them wrote documents that are not XML: `setAttributeNS('urn:b',`` |
|     - |  2661 | ``  * '1:x', 'v')` emitted `xmlns:1="urn:b" 1:x="v"`, `('urn:b','a:b:c','v')` `` |
|     - |  2662 | ` * emitted an attribute with two colons in its name, and a prefixed name with a` |
|     - |  2663 | `` * NULL namespace emitted `xmlns:q=""`.  php refuses all three with a Namespace`` |
|     - |  2664 | ` * Error before the element is touched.` |
|     - |  2665 | ` */` |
|     - |  2666 | `#define DOM_XML_NS_URI   "http://www.w3.org/XML/1998/namespace"` |
|     - |  2667 | `#define DOM_XMLNS_NS_URI "http://www.w3.org/2000/xmlns/"` |
|     - |  2668 |  |
|     - |  2669 | `typedef struct dom_qname dom_qname;` |
|     - |  2670 | `struct dom_qname {` |
|     - |  2671 | `	xmlChar *zPrefix;   /* NULL when the name carries none */` |
|     - |  2672 | `	xmlChar *zLocal;    /* always allocated */` |
|     - |  2673 | `};` |
|   240 |  2674 | `static void DomQNameRelease(dom_qname *pQ)` |
|     1 |  2675 | `{` |
|   241 |  2676 | `	if( pQ->zPrefix ){` |
|   148 |  2677 | `		xmlFree(pQ->zPrefix);` |
|    73 |  2678 | `	}` |
|   241 |  2679 | `	if( pQ->zLocal ){` |
|   241 |  2680 | `		xmlFree(pQ->zLocal);` |
|   120 |  2681 | `	}` |
|   241 |  2682 | `	pQ->zPrefix = pQ->zLocal = 0;` |
|   241 |  2683 | `}` |
|   272 |  2684 | `static int DomUriIs(const char *zUri,const char *zWant)` |
|     1 |  2685 | `{` |
|   273 |  2686 | `	return zUri != 0 && DomNameIs(zUri,zWant);` |
|     1 |  2687 | `}` |
|     - |  2688 | `/*` |
|     - |  2689 | `` * php's `dom_check_qname`: the name has to be a QName, and a prefix demands a`` |
|     - |  2690 | ` * namespace.  Three callers ask three different questions of the same name, so` |
|     - |  2691 | ` * iMode says which:` |
|     - |  2692 | ` *` |
|     - |  2693 | ` *   DOM_QN_SET   setAttributeNS -- the loosest. A prefixed name is judged as two` |
|     - |  2694 | ` *                NCNames and every failure is the Namespace Error; an unprefixed` |
|     - |  2695 | ` *                one is a plain Name, where a character libxml will not take is` |
|     - |  2696 | `` *                the Invalid Character Error. The unprefixed `xmlns` is how a`` |
|     - |  2697 | ` *                program writes a namespace DECLARATION, so nothing about the` |
|     - |  2698 | ` *                xmlns namespace is checked here.` |
|     - |  2699 | ` *   DOM_QN_ATTR  createAttributeNS -- a QName and nothing else, plus the DOM` |
|     - |  2700 | ` *                spec's pairing (the xmlns namespace may only be spelled by an` |
|     - |  2701 | ` *                xmlns name and an xmlns name may name nothing else) and the` |
|     - |  2702 | `` *                `xml` prefix's own URI.`` |
|     - |  2703 | ` *   DOM_QN_ELEM  createElementNS -- a QName when a namespace came with it, and` |
|     - |  2704 | `` *                the SET side's split when none did (so `createElementNS(null,`` |
|     - |  2705 | `` *                'x y')` is the Invalid Character Error where the attribute`` |
|     - |  2706 | `` *                factory says Namespace Error, and `:x` is an element named`` |
|     - |  2707 | `` *                `:x` there and a refusal here). No reserved rule at all: php`` |
|     - |  2708 | ` *                checks those where it RESOLVES the namespace, which is after` |
|     - |  2709 | ` *                any binding the document already has, so` |
|     - |  2710 | `` *                `createElementNS($XML_NS, 'xmlns:x')` is an `xml:x` element`` |
|     - |  2711 | ` *                rather than a refusal.` |
|     - |  2712 | ` *` |
|     - |  2713 | ` * Answers 0, or the DOM error code to raise.` |
|     - |  2714 | ` */` |
|     - |  2715 | `#define DOM_QN_SET  0` |
|     - |  2716 | `#define DOM_QN_ATTR 1` |
|     - |  2717 | `#define DOM_QN_ELEM 2` |
|   268 |  2718 | `static int DomQNameParse(const char *zQname,const char *zUri,int iMode,dom_qname *pOut)` |
|     1 |  2719 | `{` |
|   269 |  2720 | `	int bHasUri = zUri != 0 && zUri[0] != 0;` |
|     - |  2721 | `	int bXmlnsName;` |
|   269 |  2722 | `	pOut->zPrefix = pOut->zLocal = 0;` |
|   269 |  2723 | `	if( zQname == 0 \|\| zQname[0] == 0 ){` |
|   ! 0 |  2724 | `		return DOM_ERR_NAMESPACE;` |
|     - |  2725 | `	}` |
|   269 |  2726 | `	if( iMode == DOM_QN_ATTR \|\| (iMode == DOM_QN_ELEM && bHasUri) ){` |
|     - |  2727 | `		/* A created name that names a namespace has to be a QName, and every` |
|     - |  2728 | `		 * failure there is the Namespace Error. */` |
|   157 |  2729 | `		if( xmlValidateQName((const xmlChar *)zQname,0) != 0 ){` |
|    29 |  2730 | `			return DOM_ERR_NAMESPACE;` |
|     - |  2731 | `		}` |
|    64 |  2732 | `	}` |
|   241 |  2733 | `	pOut->zLocal = xmlSplitQName2((const xmlChar *)zQname,&pOut->zPrefix);` |
|   241 |  2734 | `	if( pOut->zLocal == 0 ){` |
|     - |  2735 | `		/* No prefix -- or a name that BEGINS with the colon, which libxml hands` |
|     - |  2736 | ``		 * back whole and php then writes literally (`:x`) as long as no`` |
|     - |  2737 | `		 * namespace came with it. */` |
|    94 |  2738 | `		pOut->zLocal = xmlStrdup((const xmlChar *)zQname);` |
|    94 |  2739 | `		if( pOut->zLocal == 0 ){` |
|   ! 0 |  2740 | `			return DOM_ERR_NAMESPACE;` |
|     - |  2741 | `		}` |
|    47 |  2742 | `	}` |
|   241 |  2743 | `	if( iMode == DOM_QN_SET \|\| (iMode == DOM_QN_ELEM && !bHasUri) ){` |
|     - |  2744 | `		/* The SET side separates the two failures php separates. A name with a` |
|     - |  2745 | `		 * PREFIX is judged as two NCNames and every failure there is the` |
|     - |  2746 | `		 * Namespace Error; an unprefixed one is judged as a plain Name, and a` |
|     - |  2747 | `		 * name libxml will not take at all is the Invalid Character Error. A` |
|     - |  2748 | `		 * namespace then demands that the local part be an NCName too, which is` |
|     - |  2749 | ``		 * what refuses `:x` once a URI comes with it. */`` |
|   113 |  2750 | `		if( pOut->zPrefix ){` |
|    63 |  2751 | `			if( xmlValidateNCName(pOut->zPrefix,0) != 0` |
|    61 |  2752 | `			 \|\| xmlValidateNCName(pOut->zLocal,0) != 0 ){` |
|    22 |  2753 | `				DomQNameRelease(pOut);` |
|    22 |  2754 | `				return DOM_ERR_NAMESPACE;` |
|     1 |  2755 | `			}` |
|    71 |  2756 | `		}else if( xmlValidateName((const xmlChar *)zQname,0) != 0 ){` |
|    15 |  2757 | `			DomQNameRelease(pOut);` |
|    15 |  2758 | `			return DOM_ERR_INVALID_CHAR;` |
|     - |  2759 | `		}` |
|    78 |  2760 | `		if( bHasUri && xmlValidateNCName(pOut->zLocal,0) != 0 ){` |
|     4 |  2761 | `			DomQNameRelease(pOut);` |
|     4 |  2762 | `			return DOM_ERR_NAMESPACE;` |
|     - |  2763 | `		}` |
|    37 |  2764 | `	}` |
|   203 |  2765 | `	bXmlnsName = pOut->zPrefix == 0 && xmlStrEqual(pOut->zLocal,(const xmlChar *)"xmlns");` |
|   203 |  2766 | `	if( pOut->zPrefix && !bHasUri ){` |
|     - |  2767 | `		/* A prefix names a namespace, so there has to be one. (Whether the` |
|     - |  2768 | `		 * prefix may be USED is the resolution's question, not the grammar's:` |
|     - |  2769 | `		 * php reuses a binding the document already has whatever prefix was` |
|     - |  2770 | `		 * asked for, and only refuses when it would have to declare one.) */` |
|    21 |  2771 | `		DomQNameRelease(pOut);` |
|    21 |  2772 | `		return DOM_ERR_NAMESPACE;` |
|     - |  2773 | `	}` |
|   182 |  2774 | `	if( iMode == DOM_QN_ATTR && pOut->zPrefix` |
|    24 |  2775 | `	 && xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xml")` |
|    11 |  2776 | `	 && !DomUriIs(zUri,DOM_XML_NS_URI) ){` |
|   ! 0 |  2777 | `		DomQNameRelease(pOut);` |
|   ! 0 |  2778 | `		return DOM_ERR_NAMESPACE;` |
|     - |  2779 | `	}` |
|   183 |  2780 | `	if( iMode == DOM_QN_ATTR ){` |
|     - |  2781 | `		/* The DOM spec's pairing, which php applies to a created ATTRIBUTE: the` |
|     - |  2782 | `		 * xmlns namespace may only be spelled by an xmlns name, and an xmlns` |
|     - |  2783 | `		 * name may name nothing else. */` |
|    55 |  2784 | `		int bXmlnsPrefix = pOut->zPrefix != 0` |
|    30 |  2785 | `			&& xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xmlns");` |
|    31 |  2786 | `		if( (bXmlnsName \|\| bXmlnsPrefix) != DomUriIs(zUri,DOM_XMLNS_NS_URI) ){` |
|     5 |  2787 | `			DomQNameRelease(pOut);` |
|     5 |  2788 | `			return DOM_ERR_NAMESPACE;` |
|     - |  2789 | `		}` |
|    13 |  2790 | `	}` |
|   179 |  2791 | `	return 0;` |
|   135 |  2792 | `}` |
|     - |  2793 | `/*` |
|     - |  2794 | ` * The namespace a node in zUri should carry, declared on pAnchor when the` |
|     - |  2795 | ` * document has none.  php REUSES a binding it can find by URI as long as that` |
|     - |  2796 | ` * binding has a prefix, takes the caller's prefix when it has to declare and` |
|     - |  2797 | `` * the prefix is free, and otherwise generates `default`, `default1`, ... --`` |
|     - |  2798 | ` * which is why asking for a prefix another URI already owns quietly answers` |
|     - |  2799 | `` * `default:x` rather than refusing.`` |
|     - |  2800 | ` *` |
|     - |  2801 | `` * bNeedPrefix is the CREATE side (`createAttributeNS`), where an unprefixed`` |
|     - |  2802 | ` * name still gets a generated prefix; the SET side may declare the DEFAULT` |
|     - |  2803 | ` * namespace instead.` |
|     - |  2804 | ` */` |
|     2 |  2805 | `static xmlNsPtr DomFindPrefixedNs(xmlNodePtr pNode,const char *zUri)` |
|     1 |  2806 | `{` |
|     - |  2807 | `	xmlNodePtr p;` |
|     5 |  2808 | `	for( p = pNode ; p ; p = p->parent ){` |
|     - |  2809 | `		xmlNsPtr pNs;` |
|     5 |  2810 | `		if( p->type != XML_ELEMENT_NODE ){` |
|   ! 0 |  2811 | `			continue;` |
|     - |  2812 | `		}` |
|     7 |  2813 | `		for( pNs = p->nsDef ; pNs ; pNs = pNs->next ){` |
|     4 |  2814 | `			if( pNs->prefix == 0 \|\| pNs->href == 0` |
|     3 |  2815 | `			 \|\| !xmlStrEqual(pNs->href,(const xmlChar *)zUri) ){` |
|     3 |  2816 | `				continue;` |
|     - |  2817 | `			}` |
|     - |  2818 | `			/* ...and only if a nearer declaration has not taken the prefix. */` |
|     3 |  2819 | `			if( xmlSearchNs(pNode->doc,pNode,pNs->prefix) == pNs ){` |
|     3 |  2820 | `				return pNs;` |
|     - |  2821 | `			}` |
|   ! 0 |  2822 | `		}` |
|     2 |  2823 | `	}` |
|   ! 0 |  2824 | `	return 0;` |
|     2 |  2825 | `}` |
|     - |  2826 | `/*` |
|     - |  2827 | ` * Declare a binding of zUri on pAnchor under a prefix nothing there has taken:` |
|     - |  2828 | `` * zBase, then zBase1, zBase2...  php starts from `default` for a namespace`` |
|     - |  2829 | ` * with no prefix of its own and from the prefix ITSELF when it is re-spelling` |
|     - |  2830 | `` * one an inner declaration has shadowed (which is where `p1` comes from).`` |
|     - |  2831 | ` *` |
|     - |  2832 | ` * bScope is what "nothing there has taken" means. xmlNewNs only refuses a second` |
|     - |  2833 | ` * declaration on the SAME element, which is the whole test for a re-spelling` |
|     - |  2834 | ` * (the shadowing declaration is the one being written). An attribute ARRIVING` |
|     - |  2835 | ` * needs the stronger one -- a prefix bound anywhere in scope is taken, or the` |
|     - |  2836 | ` * declaration written here would shadow it and re-point every node under it.` |
|     - |  2837 | ` */` |
|    28 |  2838 | `static xmlNsPtr DomNsGenerateEx(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase,` |
|     - |  2839 | `	int bScope)` |
|     1 |  2840 | `{` |
|    29 |  2841 | `	xmlNsPtr pNs = 0;` |
|     - |  2842 | `	int i;` |
|    35 |  2843 | `	for( i = 0 ; i < 1000 ; i++ ){` |
|     - |  2844 | `		char zGen[256];` |
|    35 |  2845 | `		const char *zB = zBase ? (const char *)zBase : "default";` |
|    35 |  2846 | `		if( SyStrlen(zB) > sizeof(zGen)-16 ){` |
|   ! 0 |  2847 | `			zB = "default";` |
|   ! 0 |  2848 | `		}` |
|    35 |  2849 | `		if( i == 0 ){` |
|    29 |  2850 | `			SyBufferFormat(zGen,sizeof(zGen),"%s",zB);` |
|    15 |  2851 | `		}else{` |
|     7 |  2852 | `			SyBufferFormat(zGen,sizeof(zGen),"%s%d",zB,i);` |
|     - |  2853 | `		}` |
|    35 |  2854 | `		if( bScope && xmlSearchNs(pAnchor->doc,pAnchor,(const xmlChar *)zGen) != 0 ){` |
|     - |  2855 | `			/* Taken -- by a declaration IN SCOPE, which xmlNewNs does not see:` |
|     - |  2856 | `			 * it only refuses a second one on the same element. */` |
|     5 |  2857 | `			continue;` |
|     - |  2858 | `		}` |
|    31 |  2859 | `		pNs = xmlNewNs(pAnchor,(const xmlChar *)zUri,(const xmlChar *)zGen);` |
|    31 |  2860 | `		if( pNs ){` |
|    29 |  2861 | `			return pNs;` |
|     - |  2862 | `		}` |
|     2 |  2863 | `	}` |
|   ! 0 |  2864 | `	return 0;` |
|    15 |  2865 | `}` |
|    14 |  2866 | `static xmlNsPtr DomNsGenerate(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase)` |
|     1 |  2867 | `{` |
|    15 |  2868 | `	return DomNsGenerateEx(pAnchor,zUri,zBase,0);` |
|     1 |  2869 | `}` |
|     - |  2870 | `/* A binding of this URI an ATTRIBUTE can use: one that carries a prefix. */` |
|    44 |  2871 | `static xmlNsPtr DomNsReuse(xmlNodePtr pAnchor,const char *zUri)` |
|     1 |  2872 | `{` |
|    45 |  2873 | `	xmlNsPtr pNs = xmlSearchNsByHref(pAnchor->doc,pAnchor,(const xmlChar *)zUri);` |
|    45 |  2874 | `	if( pNs && pNs->prefix ){` |
|    19 |  2875 | ``		return pNs;   /* including libxml's implicit `xml` binding */`` |
|     - |  2876 | `	}` |
|     - |  2877 | `	/* Bound, but only WITHOUT a prefix, which does not serve an attribute: a` |
|     - |  2878 | `	 * prefixed binding of the same URI further out still does. */` |
|    27 |  2879 | `	return pNs ? DomFindPrefixedNs(pAnchor,zUri) : 0;` |
|    23 |  2880 | `}` |
|    62 |  2881 | `static xmlNsPtr DomNsResolve(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zPrefix,int bNeedPrefix)` |
|     1 |  2882 | `{` |
|     - |  2883 | `	xmlNsPtr pNs;` |
|    63 |  2884 | `	if( !bNeedPrefix ){` |
|    33 |  2885 | `		pNs = DomNsReuse(pAnchor,zUri);` |
|    33 |  2886 | `		if( pNs ){` |
|    15 |  2887 | `			return pNs;` |
|     - |  2888 | `		}` |
|     9 |  2889 | `	}` |
|     - |  2890 | `	/* The CREATE side asks libxml's own question and no more: a document that` |
|     - |  2891 | `	 * binds this URI to the default namespace AND to a prefix answers the` |
|     - |  2892 | `	 * default one there, and php then declares its own rather than looking for` |
|     - |  2893 | `	 * the prefixed binding the SET side would have found. */` |
|    49 |  2894 | `	pNs = xmlSearchNsByHref(pAnchor->doc,pAnchor,(const xmlChar *)zUri);` |
|    49 |  2895 | `	if( bNeedPrefix ){` |
|    31 |  2896 | `		if( pNs && pNs->prefix ){` |
|    11 |  2897 | `			return pNs;` |
|     - |  2898 | `		}` |
|    21 |  2899 | `		pNs = 0;   /* a prefix-less binding is no use to an attribute */` |
|    10 |  2900 | `	}` |
|     - |  2901 | `	/* A prefix-less binding stops php from declaring another one under the` |
|     - |  2902 | `	 * caller's prefix -- what happens then is a generated one. */` |
|    39 |  2903 | `	if( pNs == 0 && (zPrefix != 0 \|\| !bNeedPrefix) ){` |
|    34 |  2904 | `		if( !bNeedPrefix && zPrefix` |
|    16 |  2905 | `		 && (xmlStrEqual(zPrefix,(const xmlChar *)"xml")` |
|    11 |  2906 | `		  \|\| xmlStrEqual(zPrefix,(const xmlChar *)"xmlns")) ){` |
|     - |  2907 | `			/* A RESERVED prefix cannot be declared, and php does not paper over` |
|     - |  2908 | `			 * that with a generated one: it refuses. (Nothing is refused when` |
|     - |  2909 | `			 * the URI already had a binding -- the prefix is never consulted` |
|     - |  2910 | ``			 * then, which is why `setAttributeNS($uri,'xml:id',..)` succeeds on`` |
|     - |  2911 | `			 * a document that binds $uri and fails on one that does not.) */` |
|     5 |  2912 | `			return 0;` |
|     - |  2913 | `		}` |
|    31 |  2914 | `		pNs = xmlNewNs(pAnchor,(const xmlChar *)zUri,zPrefix);` |
|    31 |  2915 | `		if( pNs ){` |
|    27 |  2916 | `			return pNs;` |
|     - |  2917 | `		}` |
|     2 |  2918 | `	}` |
|     9 |  2919 | `	return DomNsGenerate(pAnchor,zUri,0);` |
|    32 |  2920 | `}` |
|     - |  2921 | `/*` |
|     - |  2922 | ` * The namespace a node CREATED in zUri carries, which is a different rule from` |
|     - |  2923 | ` * either side above and php's smallest one: a binding already in scope is used` |
|     - |  2924 | ` * whatever prefix was asked for -- for a fresh node that means only libxml's own` |
|     - |  2925 | `` * `xml` declaration, which is why every `createElementNS($XML_NS, ...)` comes`` |
|     - |  2926 | `` * back spelled `xml:` -- and otherwise the node declares zUri on ITSELF under`` |
|     - |  2927 | ` * the caller's prefix, with no generated prefix and no fallback: the three` |
|     - |  2928 | `` * reserved-name rules php checks here (`dom_get_ns`) are a refusal, not a`` |
|     - |  2929 | ` * rename. NULL means Namespace Error.` |
|     - |  2930 | ` */` |
|    92 |  2931 | `static xmlNsPtr DomNsForCreate(xmlNodePtr pNode,const char *zUri,const xmlChar *zPrefix)` |
|     1 |  2932 | `{` |
|    93 |  2933 | `	xmlNsPtr pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);` |
|    93 |  2934 | `	if( pNs ){` |
|    17 |  2935 | `		return pNs;` |
|     - |  2936 | `	}` |
|    76 |  2937 | `	if( zPrefix != 0` |
|    64 |  2938 | `	 && ((xmlStrEqual(zPrefix,(const xmlChar *)"xml") && !DomUriIs(zUri,DOM_XML_NS_URI))` |
|    46 |  2939 | `	  \|\| (xmlStrEqual(zPrefix,(const xmlChar *)"xmlns") && !DomUriIs(zUri,DOM_XMLNS_NS_URI))` |
|    43 |  2940 | `	  \|\| (DomUriIs(zUri,DOM_XMLNS_NS_URI)` |
|    27 |  2941 | `	   && !xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){` |
|    15 |  2942 | `		return 0;` |
|     - |  2943 | `	}` |
|    69 |  2944 | `	return xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);` |
|    50 |  2945 | `}` |
|     - |  2946 | `/*` |
|     - |  2947 | ` * A declaration that lands on pElem takes the SPELLING away from every node` |
|     - |  2948 | ` * under it that reached its namespace through a declaration this one now` |
|     - |  2949 | ` * shadows -- the node still points at a binding nothing can name from there, so` |
|     - |  2950 | ` * a re-parse of the serialized document reads it in the wrong namespace (or in` |
|     - |  2951 | ` * none).  php re-points those nodes at a binding of their OWN URI: one still in` |
|     - |  2952 | ` * scope when there is one, and otherwise a fresh declaration on pElem under` |
|     - |  2953 | `` * their own prefix numbered up (`p` -> `p1`), or `default` when they had none.`` |
|     - |  2954 | ` *` |
|     - |  2955 | ` * Only called when a write actually declared something, which is what keeps it` |
|     - |  2956 | ` * off the ordinary path.` |
|     - |  2957 | ` */` |
|    76 |  2958 | `static void DomNsRespell(xmlNodePtr pNode,int bAttr)` |
|     1 |  2959 | `{` |
|    77 |  2960 | `	xmlNsPtr pNs = pNode->ns,pAlt;` |
|     - |  2961 | `	/* php declares what it needs on the node that NEEDS it -- the element` |
|     - |  2962 | `	 * itself, or the element an attribute belongs to. */` |
|    77 |  2963 | `	xmlNodePtr pSite = bAttr ? pNode->parent : pNode;` |
|    77 |  2964 | `	if( pNs == 0 \|\| pNs->href == 0 \|\| pSite == 0 ){` |
|    33 |  2965 | `		return;` |
|     - |  2966 | `	}` |
|    45 |  2967 | `	if( xmlSearchNs(pNode->doc,pNode,pNs->prefix) == pNs ){` |
|    29 |  2968 | `		return;   /* the prefix still names this very binding */` |
|     - |  2969 | `	}` |
|     - |  2970 | `	/* An attribute needs a PREFIXED binding; an element is happy with the` |
|     - |  2971 | `	 * default one. */` |
|    13 |  2972 | `	pAlt = bAttr ? DomNsReuse(pNode,(const char *)pNs->href)` |
|    12 |  2973 | `	             : xmlSearchNsByHref(pNode->doc,pNode,pNs->href);` |
|    17 |  2974 | `	if( pAlt == 0 ){` |
|     - |  2975 | `		/* Its own prefix first -- a declaration that was REMOVED leaves that` |
|     - |  2976 | `		 * prefix free again, and php re-declares it unchanged there. */` |
|    11 |  2977 | `		pAlt = xmlNewNs(pSite,pNs->href,pNs->prefix);` |
|     5 |  2978 | `	}` |
|    17 |  2979 | `	if( pAlt == 0 ){` |
|     7 |  2980 | `		pAlt = DomNsGenerate(pSite,(const char *)pNs->href,pNs->prefix);` |
|     3 |  2981 | `	}` |
|    17 |  2982 | `	if( pAlt ){` |
|    17 |  2983 | `		pNode->ns = pAlt;` |
|     8 |  2984 | `	}` |
|    39 |  2985 | `}` |
|    78 |  2986 | `static int DomNsDefCount(xmlNodePtr pElem)` |
|     1 |  2987 | `{` |
|     - |  2988 | `	xmlNsPtr pNs;` |
|    79 |  2989 | `	int n = 0;` |
|    99 |  2990 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|    21 |  2991 | `		n++;` |
|    11 |  2992 | `	}` |
|    79 |  2993 | `	return n;` |
|     1 |  2994 | `}` |
|    34 |  2995 | `static void DomNsReconcile(xmlNodePtr pElem)` |
|     1 |  2996 | `{` |
|    35 |  2997 | `	xmlNodePtr pCur = pElem;` |
|    77 |  2998 | `	while( pCur ){` |
|     - |  2999 | `		xmlAttrPtr pAttr;` |
|    43 |  3000 | `		if( pCur->type == XML_ELEMENT_NODE ){` |
|    43 |  3001 | `			DomNsRespell(pCur,0);` |
|    77 |  3002 | `			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){` |
|    35 |  3003 | `				if( pAttr->type == XML_ATTRIBUTE_NODE ){` |
|    35 |  3004 | `					DomNsRespell((xmlNodePtr)pAttr,1);` |
|    17 |  3005 | `				}` |
|    18 |  3006 | `			}` |
|    21 |  3007 | `		}` |
|    43 |  3008 | `		pCur = DomWalkNext(pCur,pElem);` |
|     1 |  3009 | `	}` |
|    35 |  3010 | `}` |
|     - |  3011 | `/*` |
|     - |  3012 | ` * A declaration is freed with the element that carries it, so one REMOVED from` |
|     - |  3013 | ` * an element cannot simply be dropped: a node further down may still point at` |
|     - |  3014 | ` * it. It goes where libxml's own document teardown will free it and nothing` |
|     - |  3015 | ``  * resolves through it -- `doc->oldNs`, which is what php's `dom_set_old_ns` `` |
|     - |  3016 | ` * writes to.` |
|     - |  3017 | ` */` |
|    32 |  3018 | `static void DomNsPark(xmlNodePtr pOwner,xmlNsPtr pNs)` |
|     1 |  3019 | `{` |
|    33 |  3020 | `	xmlDocPtr pDoc = pOwner->doc;` |
|     - |  3021 | `	xmlNsPtr pTail;` |
|    33 |  3022 | `	pNs->next = 0;` |
|     - |  3023 | ``	/* The list's HEAD must stay libxml's own `xml` declaration, because`` |
|     - |  3024 | ``	 * xmlSearchNs answers doc->oldNs DIRECTLY for the `xml` prefix. Asking for`` |
|     - |  3025 | `	 * it is what builds it. */` |
|    33 |  3026 | `	xmlSearchNs(pDoc,pOwner,(const xmlChar *)"xml");` |
|    33 |  3027 | `	if( pDoc->oldNs == 0 ){` |
|   ! 0 |  3028 | `		pDoc->oldNs = pNs;` |
|   ! 0 |  3029 | `		return;` |
|     - |  3030 | `	}` |
|    37 |  3031 | `	for( pTail = pDoc->oldNs ; pTail->next ; pTail = pTail->next ){}` |
|    33 |  3032 | `	pTail->next = pNs;` |
|    17 |  3033 | `}` |
|     - |  3034 | `/*` |
|     - |  3035 | `` * php's `dom_reconcile_ns`, which every mutator runs on the node it LINKED.`` |
|     - |  3036 | ` * Without it a move wrote documents that are not XML in both directions:` |
|     - |  3037 | ` * appending a node whose namespace was declared on the ancestor it just left` |
|     - |  3038 | `` * emitted `<p:b k="1"/>` with the prefix bound nowhere, and appending one that`` |
|     - |  3039 | `` * carries its own declaration (`createElementNS`, or a chunk `appendXML` built)`` |
|     - |  3040 | ` * emitted a second copy of a declaration the new parent already makes.` |
|     - |  3041 | ` *` |
|     - |  3042 | ` * The strip is php's own test: same URI, and either the node's declaration` |
|     - |  3043 | ` * carries NO prefix -- then any binding of that URI in scope replaces it, even` |
|     - |  3044 | `` * a prefixed one, which is how an appended `createElementNS($uri,'y')` comes`` |
|     - |  3045 | `` * out spelled `p:y` -- or the in-scope binding spells it the same way.`` |
|     - |  3046 | ` *` |
|     - |  3047 | `` * The re-pointing after it is libxml's own `xmlReconciliateNs`, called here`` |
|     - |  3048 | ` * rather than paraphrased: it re-points EVERY node of the subtree at the first` |
|     - |  3049 | ` * in-scope binding of its URI found from the inserted node -- so a URI two` |
|     - |  3050 | ` * prefixes bind is respelled to the first of them, which the setAttributeNS` |
|     - |  3051 | ` * respeller (DomNsReconcile, which only touches a node whose spelling BROKE)` |
|     - |  3052 | ` * does not do -- and declares one on the inserted node for a URI nothing in` |
|     - |  3053 | ` * scope binds any more (including one a DESCENDANT declares, since the search` |
|     - |  3054 | ` * only ever looks up).` |
|     - |  3055 | ` */` |
|   300 |  3056 | `static void DomNsStrip(xmlNodePtr pNode,xmlNodePtr pScopeAt)` |
|     1 |  3057 | `{` |
|   301 |  3058 | `	xmlNsPtr pCur = pNode->nsDef,pPrev = 0;` |
|   301 |  3059 | `	if( pNode->doc == 0 ){` |
|     - |  3060 | `		/* Nothing would own a removed declaration, and a node under it may` |
|     - |  3061 | `		 * still point at one: leave the element's list alone. */` |
|    15 |  3062 | `		return;` |
|     - |  3063 | `	}` |
|   407 |  3064 | `	while( pCur ){` |
|   121 |  3065 | `		xmlNsPtr pNext = pCur->next;` |
|   181 |  3066 | `		xmlNsPtr pScope = pCur->href` |
|   120 |  3067 | `			? xmlSearchNsByHref(pNode->doc,pScopeAt,pCur->href) : 0;` |
|   120 |  3068 | `		if( pScope != 0` |
|    96 |  3069 | `		 && (pCur->prefix == 0` |
|    38 |  3070 | `		  \|\| (pScope->prefix != 0 && xmlStrEqual(pScope->prefix,pCur->prefix))) ){` |
|    31 |  3071 | `			if( pPrev ){` |
|   ! 0 |  3072 | `				pPrev->next = pNext;` |
|   ! 0 |  3073 | `			}else{` |
|    31 |  3074 | `				pNode->nsDef = pNext;` |
|     - |  3075 | `			}` |
|    31 |  3076 | `			DomNsPark(pNode,pCur);` |
|    16 |  3077 | `		}else{` |
|    91 |  3078 | `			pPrev = pCur;` |
|     - |  3079 | `		}` |
|   121 |  3080 | `		pCur = pNext;` |
|     1 |  3081 | `	}` |
|   151 |  3082 | `}` |
|     - |  3083 | `/*` |
|     - |  3084 | ` * php runs the strip on the node it linked and NO deeper -- a redundant` |
|     - |  3085 | `` * declaration one level down survives an `appendChild` -- but a FRAGMENT is`` |
|     - |  3086 | ` * spliced by a second function that walks each moved child WHOLE, so the same` |
|     - |  3087 | ` * subtree arriving that way comes out stripped at every depth. bDeep is that` |
|     - |  3088 | ` * difference, and both halves are measurable.` |
|     - |  3089 | ` *` |
|     - |  3090 | ` * The deep walk judges every node against the same scope -- the INSERTION` |
|     - |  3091 | ` * POINT, not each node's own parent -- so a declaration duplicated inside the` |
|     - |  3092 | ` * moved subtree survives when the new parent does not make it too.` |
|     - |  3093 | ` */` |
|   470 |  3094 | `static void DomNsOnInsertEx(xmlNodePtr pNode,int bDeep)` |
|     2 |  3095 | `{` |
|   472 |  3096 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|   186 |  3097 | `		return;` |
|     - |  3098 | `	}` |
|   287 |  3099 | `	if( bDeep ){` |
|    73 |  3100 | `		xmlNodePtr pCur = pNode,pAt = pNode->parent;` |
|   163 |  3101 | `		while( pCur ){` |
|    91 |  3102 | `			if( pCur->type == XML_ELEMENT_NODE ){` |
|    87 |  3103 | `				DomNsStrip(pCur,pAt);` |
|    43 |  3104 | `			}` |
|    91 |  3105 | `			pCur = DomWalkNext(pCur,pNode);` |
|     1 |  3106 | `		}` |
|    37 |  3107 | `	}else{` |
|   215 |  3108 | `		DomNsStrip(pNode,pNode->parent);` |
|     - |  3109 | `	}` |
|   287 |  3110 | `	xmlReconciliateNs(pNode->doc,pNode);` |
|   237 |  3111 | `}` |
|     - |  3112 | `/*` |
|     - |  3113 | ` * The namespace an attribute NODE carries once it is linked onto pElem. Its own` |
|     - |  3114 | ` * ns struct is a declaration of wherever it came FROM, and moving the node does` |
|     - |  3115 | `` * not move that: written unchanged it emitted `p:k="1"` with the prefix bound`` |
|     - |  3116 | ` * NOWHERE, and -- when the target's scope binds that prefix to something else --` |
|     - |  3117 | ` * bound to the WRONG URI, which is the worse half, because those bytes parse` |
|     - |  3118 | ` * back cleanly as an attribute in a namespace the program never wrote.` |
|     - |  3119 | ` *` |
|     - |  3120 | ` * php keeps the spelling when the attribute's own declaration is still in scope` |
|     - |  3121 | ` * here, even if a nearer one binds the same URI under another prefix. Otherwise` |
|     - |  3122 | ` * it takes any binding of the URI in scope -- INCLUDING a prefix-less one, where` |
|     - |  3123 | ` * the attribute then serializes with no prefix at all and still answers the URI,` |
|     - |  3124 | ` * which is NOT how the by-NAME writes resolve (there an attribute always wants a` |
|     - |  3125 | ` * prefixed binding, DomNsReuse) -- and otherwise declares one here under the` |
|     - |  3126 | `` * attribute's own prefix, numbered up when that prefix is taken (`p` -> `p1`).`` |
|     - |  3127 | ` */` |
|    78 |  3128 | `static void DomNsAttrArrive(xmlNodePtr pElem,xmlAttrPtr pAttr)` |
|     2 |  3129 | `{` |
|    80 |  3130 | `	xmlNsPtr pNs = pAttr->ns;` |
|    80 |  3131 | `	if( pNs == 0 \|\| pNs->href == 0 ){` |
|    32 |  3132 | `		return;` |
|     - |  3133 | `	}` |
|    49 |  3134 | `	if( DomUriIs((const char *)pNs->href,DOM_XMLNS_NS_URI) ){` |
|     - |  3135 | `		/* An attribute in the xmlns namespace IS a declaration, and php resolves` |
|     - |  3136 | ``		 * nothing for it: `xmlns="urn:z"` stays spelled that way wherever it is`` |
|     - |  3137 | `		 * written, and never acquires a declaration of the xmlns namespace. */` |
|     7 |  3138 | `		return;` |
|     - |  3139 | `	}` |
|    43 |  3140 | `	if( xmlSearchNs(pElem->doc,pElem,pNs->prefix) == pNs ){` |
|    21 |  3141 | `		return;` |
|     - |  3142 | `	}` |
|    23 |  3143 | `	pNs = xmlSearchNsByHref(pElem->doc,pElem,pAttr->ns->href);` |
|    23 |  3144 | `	if( pNs ){` |
|     9 |  3145 | `		pAttr->ns = pNs;` |
|     9 |  3146 | `		return;` |
|     - |  3147 | `	}` |
|    15 |  3148 | `	pNs = DomNsGenerateEx(pElem,(const char *)pAttr->ns->href,pAttr->ns->prefix,1);` |
|    15 |  3149 | `	if( pNs == 0 ){` |
|   ! 0 |  3150 | `		return;` |
|     - |  3151 | `	}` |
|    15 |  3152 | `	pAttr->ns = pNs;` |
|     - |  3153 | `	/*` |
|     - |  3154 | `	 * A declaration LANDED on this element, and php then judges its whole` |
|     - |  3155 | `	 * subtree from HERE: a descendant whose namespace is declared further down` |
|     - |  3156 | `	 * is re-pointed at a fresh declaration on this element -- even though the` |
|     - |  3157 | `	 * one it had is still in scope where it stands. That is libxml's` |
|     - |  3158 | `	 * xmlReconciliateNs, and it runs ONLY on this path: an arriving attribute` |
|     - |  3159 | `	 * that needed no declaration leaves the subtree exactly as it was, which is` |
|     - |  3160 | `	 * measurable both ways.` |
|     - |  3161 | `	 */` |
|    15 |  3162 | `	xmlReconciliateNs(pElem->doc,pElem);` |
|    41 |  3163 | `}` |
|     - |  3164 | `/* A namespace DECLARATION on this element: php's setAttributeNS writes one` |
|     - |  3165 | `` * when the name is `xmlns` or its prefix is, and REBINDS the one already`` |
|     - |  3166 | ` * there rather than adding a second. */` |
|    14 |  3167 | `static void DomNsDeclare(xmlNodePtr pElem,const xmlChar *zPrefix,const char *zHref)` |
|     1 |  3168 | `{` |
|     - |  3169 | `	xmlNsPtr pNs;` |
|    17 |  3170 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|     7 |  3171 | `		int bSame = zPrefix == 0 ? pNs->prefix == 0` |
|     3 |  3172 | `			: (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix));` |
|     5 |  3173 | `		if( bSame ){` |
|     3 |  3174 | `			xmlChar *zNew = xmlStrdup((const xmlChar *)zHref);` |
|     3 |  3175 | `			if( zNew == 0 ){` |
|   ! 0 |  3176 | `				return;` |
|     - |  3177 | `			}` |
|     3 |  3178 | `			if( pNs->href ){` |
|     3 |  3179 | `				xmlFree((xmlChar *)pNs->href);` |
|     1 |  3180 | `			}` |
|     3 |  3181 | `			pNs->href = zNew;` |
|     3 |  3182 | `			return;` |
|     - |  3183 | `		}` |
|     2 |  3184 | `	}` |
|    13 |  3185 | `	xmlNewNs(pElem,(const xmlChar *)zHref,zPrefix);` |
|     8 |  3186 | `}` |
|     - |  3187 |  |
|     - |  3188 | `/*` |
|     - |  3189 | ` * ===== Namespace DECLARATIONS, which php answers from the attribute surface =====` |
|     - |  3190 | ` *` |
|     - |  3191 | `` * `xmlns:x="urn:x"` is not an attribute in libxml -- it is an xmlNs on the`` |
|     - |  3192 | ``  * element's nsDef chain -- but php answers it from `getAttributeNode('xmlns:x')` `` |
|     - |  3193 | `` * (as a DOMNameSpaceNode), from `getAttribute`, `hasAttribute`,`` |
|     - |  3194 | `` * `removeAttribute`, `toggleAttribute` and `getAttributeNames`, which is how a`` |
|     - |  3195 | ` * program reads or drops one.  Here every one of those said the declaration was` |
|     - |  3196 | ``  * not there: `hasAttribute('xmlns:x')` was false and `getAttribute('xmlns:x')` `` |
|     - |  3197 | ` * was "" on a document whose root declares it.` |
|     - |  3198 | ` *` |
|     - |  3199 | ` * The lookup is the element's OWN declarations, not the ones in scope: a child` |
|     - |  3200 | ` * answers false for a prefix its parent declared.` |
|     - |  3201 | ` */` |
|     - |  3202 | `#define DOM_XMLNS_NAME "xmlns"` |
|     - |  3203 |  |
|     - |  3204 | `/* The declaration this element makes for zPrefix (NULL for the DEFAULT one). */` |
|    68 |  3205 | `static xmlNsPtr DomNsDeclOf(xmlNodePtr pElem,const xmlChar *zPrefix)` |
|     1 |  3206 | `{` |
|     - |  3207 | `	xmlNsPtr pNs;` |
|    69 |  3208 | `	if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|   ! 0 |  3209 | `		return 0;` |
|     - |  3210 | `	}` |
|   111 |  3211 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|   148 |  3212 | `		if( zPrefix == 0 ? pNs->prefix == 0` |
|    70 |  3213 | `		                 : (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix)) ){` |
|    45 |  3214 | `			return pNs;` |
|     - |  3215 | `		}` |
|    22 |  3216 | `	}` |
|    25 |  3217 | `	return 0;` |
|    35 |  3218 | `}` |
|     - |  3219 | ``/* ...under the NAME php spells it with: `xmlns` or `xmlns:<prefix>`. */`` |
|   110 |  3220 | `static xmlNsPtr DomNsDeclByName(xmlNodePtr pElem,const char *zName)` |
|     1 |  3221 | `{` |
|   111 |  3222 | `	sxu32 n = (sxu32)SyStrlen(DOM_XMLNS_NAME);` |
|   111 |  3223 | `	if( SyStrlen(zName) < n \|\| SyStrncmp(zName,DOM_XMLNS_NAME,n) != 0 ){` |
|    67 |  3224 | `		return 0;` |
|     - |  3225 | `	}` |
|    45 |  3226 | `	if( zName[n] == 0 ){` |
|    15 |  3227 | `		return DomNsDeclOf(pElem,0);` |
|     - |  3228 | `	}` |
|    31 |  3229 | `	if( zName[n] != ':' ){` |
|   ! 0 |  3230 | `		return 0;` |
|     - |  3231 | `	}` |
|    31 |  3232 | `	return DomNsDeclOf(pElem,(const xmlChar *)(zName+n+1));` |
|    56 |  3233 | `}` |
|     - |  3234 | `/*` |
|     - |  3235 | ` * Dropping one: the declaration leaves the element's chain, but the xmlNs` |
|     - |  3236 | ` * itself must NOT be freed -- nodes below can still point at it, and php's own` |
|     - |  3237 | ` * answer for that case is a document that keeps saying what it said. The` |
|     - |  3238 | ` * document's oldNs chain owns it from here, so it dies with the document.` |
|     - |  3239 | ` */` |
|     8 |  3240 | `static void DomNsDeclRemove(xmlNodePtr pElem,xmlNsPtr pNs)` |
|     1 |  3241 | `{` |
|     9 |  3242 | `	xmlNsPtr pPrev = 0,pCur;` |
|     9 |  3243 | `	xmlDocPtr pDoc = pElem->doc;` |
|     9 |  3244 | `	for( pCur = pElem->nsDef ; pCur ; pPrev = pCur,pCur = pCur->next ){` |
|     9 |  3245 | `		if( pCur != pNs ){` |
|   ! 0 |  3246 | `			continue;` |
|     - |  3247 | `		}` |
|     9 |  3248 | `		if( pPrev ){` |
|   ! 0 |  3249 | `			pPrev->next = pCur->next;` |
|   ! 0 |  3250 | `		}else{` |
|     9 |  3251 | `			pElem->nsDef = pCur->next;` |
|     - |  3252 | `		}` |
|     9 |  3253 | `		pCur->next = 0;` |
|     9 |  3254 | `		if( pDoc == 0 ){` |
|   ! 0 |  3255 | `			return;` |
|     - |  3256 | `		}` |
|     9 |  3257 | `		if( pDoc->oldNs == 0 ){` |
|     9 |  3258 | `			pDoc->oldNs = pCur;` |
|     5 |  3259 | `		}else{` |
|   ! 0 |  3260 | `			xmlNsPtr pTail = pDoc->oldNs;` |
|   ! 0 |  3261 | `			while( pTail->next ){` |
|   ! 0 |  3262 | `				pTail = pTail->next;` |
|   ! 0 |  3263 | `			}` |
|   ! 0 |  3264 | `			pTail->next = pCur;` |
|     - |  3265 | `		}` |
|     9 |  3266 | `		return;` |
|   ! 0 |  3267 | `	}` |
|     5 |  3268 | `}` |
|     - |  3269 |  |
|     - |  3270 | `/*` |
|     - |  3271 | ` * php's wrapper for one: DOMNameSpaceNode, a class of its own that does NOT` |
|     - |  3272 | ` * extend DOMNode and answers ten properties.  A FRESH object every time (php's` |
|     - |  3273 | `` * two calls are never `===`), so it needs none of the identity cache.`` |
|     - |  3274 | ` */` |
|     - |  3275 | `/*` |
|     - |  3276 | ` * The handle for one declaration, kept in the document's identity cache under` |
|     - |  3277 | `` * the xmlNs pointer.  php's two lookups answer two OBJECTS (`===` is false) but`` |
|     - |  3278 | `` * the same declaration, and a shared handle is what makes them `==` -- and what`` |
|     - |  3279 | `` * stops a loop over `getAttributeNode('xmlns:x')` from allocating one per call.`` |
|     - |  3280 | ` */` |
|    60 |  3281 | `static phl_domnode * DomNsRes(ph7_vm *pVm,ph7_class_instance *pDoc,phl_xmldoc *pShell,xmlNsPtr pNs)` |
|     1 |  3282 | `{` |
|    61 |  3283 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|    61 |  3284 | `	ph7_hashmap_node *pEntry = 0;` |
|     - |  3285 | `	phl_domnode *pRes;` |
|     - |  3286 | `	ph7_value sKey,sVal;` |
|    61 |  3287 | `	if( pCache == 0 ){` |
|   ! 0 |  3288 | `		return DomNewRes(&(*pVm),pShell,pNs);` |
|     - |  3289 | `	}` |
|    61 |  3290 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNs);` |
|    61 |  3291 | `	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|    33 |  3292 | `		ph7_value *pHit = HashmapExtractNodeValue(pEntry);` |
|    33 |  3293 | `		if( pHit && (pHit->iFlags & MEMOBJ_RES) ){` |
|    33 |  3294 | `			PH7_MemObjRelease(&sKey);` |
|    33 |  3295 | `			return (phl_domnode *)pHit->x.pOther;` |
|     - |  3296 | `		}` |
|   ! 0 |  3297 | `	}` |
|    29 |  3298 | `	pRes = DomNewRes(&(*pVm),pShell,pNs);` |
|    29 |  3299 | `	if( pRes ){` |
|    29 |  3300 | `		PH7_MemObjInit(&(*pVm),&sVal);` |
|    29 |  3301 | `		sVal.x.pOther = pRes;` |
|    29 |  3302 | `		sVal.iFlags = MEMOBJ_RES;` |
|    29 |  3303 | `		PH7_HashmapInsert(pCache,&sKey,&sVal);` |
|    14 |  3304 | `	}` |
|    29 |  3305 | `	PH7_MemObjRelease(&sKey);` |
|    29 |  3306 | `	return pRes;` |
|    31 |  3307 | `}` |
|    60 |  3308 | `static ph7_class_instance * DomNewNsNode(ph7_vm *pVm,ph7_class_instance *pDoc,` |
|     - |  3309 | `	phl_xmldoc *pShell,xmlNsPtr pNs,xmlNodePtr pElem)` |
|     1 |  3310 | `{` |
|    91 |  3311 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DOMNameSpaceNode",` |
|    60 |  3312 | `		(sxu32)SyStrlen("DOMNameSpaceNode"),FALSE,0);` |
|    61 |  3313 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|    61 |  3314 | `	phl_domnode *pRes = pObj ? DomNsRes(&(*pVm),pDoc,pShell,pNs) : 0;` |
|    61 |  3315 | `	if( pRes == 0 ){` |
|   ! 0 |  3316 | `		if( pObj ){` |
|   ! 0 |  3317 | `			PH7_ClassInstanceUnref(pObj);` |
|   ! 0 |  3318 | `		}` |
|   ! 0 |  3319 | `		return 0;` |
|     - |  3320 | `	}` |
|    61 |  3321 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|    61 |  3322 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|    61 |  3323 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_NS_OWNER,DomWrap(&(*pVm),pDoc,pShell,pElem));` |
|    61 |  3324 | `	return pObj;   /* the CALLER owns this reference */` |
|    31 |  3325 | `}` |
|     - |  3326 | `/* Answer one from a native method (php hands back a fresh object every time). */` |
|    18 |  3327 | `static int DomResultNsNode(ph7_context *pCtx,phl_domnode *pNd,xmlNsPtr pNs,xmlNodePtr pElem)` |
|     1 |  3328 | `{` |
|    19 |  3329 | `	ph7_class_instance *pObj = DomNewNsNode(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNs,pElem);` |
|    19 |  3330 | `	if( pObj == 0 ){` |
|   ! 0 |  3331 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3332 | `	}` |
|    19 |  3333 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    19 |  3334 | `	return PH7_OK;` |
|    10 |  3335 | `}` |
|     - |  3336 |  |
|     - |  3337 | `/*` |
|     - |  3338 | ` * An attribute of pElem by QUALIFIED name, php's own two-step lookup.` |
|     - |  3339 | ` *` |
|     - |  3340 | ` * libxml's xmlHasProp() matches the stored name and ignores namespaces` |
|     - |  3341 | ` * entirely, which is wrong in both directions and was the answer the whole` |
|     - |  3342 | `` * by-name surface gave: `getAttribute('x:b')` found NOTHING (the stored name is`` |
|     - |  3343 | `` * `b`, the prefix lives in the node's ns) while `getAttribute('b')` found the`` |
|     - |  3344 | `` * NAMESPACED one and `removeAttribute('b')` deleted it.  php looks for an`` |
|     - |  3345 | ` * attribute in NO namespace under the whole name first -- which is what finds` |
|     - |  3346 | ` * one written under an unresolvable prefix, stored with the colon in its name --` |
|     - |  3347 | ` * and only then splits the prefix, resolves it in the element's scope and asks` |
|     - |  3348 | ` * again by (local name, URI).  An unresolvable prefix finds nothing.` |
|     - |  3349 | ` *` |
|     - |  3350 | ` * A DTD-declared DEFAULT is not an attribute here: xmlHasNsProp answers the` |
|     - |  3351 | ` * DECLARATION node for those, which is a different node kind entirely.` |
|     - |  3352 | ` */` |
|   234 |  3353 | `static xmlAttrPtr DomAttrNoNs(xmlNodePtr pElem,const char *zName)` |
|     2 |  3354 | `{` |
|     - |  3355 | `	xmlAttrPtr pAttr;` |
|   236 |  3356 | `	if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|   ! 0 |  3357 | `		return 0;` |
|     - |  3358 | `	}` |
|   236 |  3359 | `	pAttr = xmlHasNsProp(pElem,(const xmlChar *)zName,0);` |
|   236 |  3360 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|   119 |  3361 | `}` |
|   208 |  3362 | `static xmlAttrPtr DomAttrByName(xmlNodePtr pElem,const char *zName)` |
|     2 |  3363 | `{` |
|   210 |  3364 | `	xmlAttrPtr pAttr = DomAttrNoNs(pElem,zName);` |
|   210 |  3365 | `	if( pAttr == 0 && pElem ){` |
|    69 |  3366 | `		xmlChar *zPrefix = 0;` |
|    69 |  3367 | `		xmlChar *zLocal = xmlSplitQName2((const xmlChar *)zName,&zPrefix);` |
|    69 |  3368 | `		if( zLocal ){` |
|    41 |  3369 | `			if( zPrefix ){` |
|    41 |  3370 | `				xmlNsPtr pNs = xmlSearchNs(pElem->doc,pElem,zPrefix);` |
|    41 |  3371 | `				if( pNs ){` |
|    15 |  3372 | `					pAttr = xmlHasNsProp(pElem,zLocal,pNs->href);` |
|     7 |  3373 | `				}` |
|    41 |  3374 | `				xmlFree(zPrefix);` |
|    20 |  3375 | `			}` |
|    41 |  3376 | `			xmlFree(zLocal);` |
|    20 |  3377 | `		}` |
|    34 |  3378 | `	}` |
|   210 |  3379 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|     2 |  3380 | `}` |
|   122 |  3381 | `static xmlAttrPtr DomAttrByNs(xmlNodePtr pElem,const xmlChar *zUri,const char *zLocal)` |
|     1 |  3382 | `{` |
|   123 |  3383 | `	xmlAttrPtr pAttr = pElem ? xmlHasNsProp(pElem,(const xmlChar *)zLocal,zUri) : 0;` |
|   123 |  3384 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|     1 |  3385 | `}` |
|     - |  3386 |  |
|     - |  3387 | `/* DOMElement::getAttribute(string $qualifiedName): string -- "" when absent */` |
|    44 |  3388 | `DOM_METHOD(vm_builtin_DOMElement_getAttribute)` |
|     1 |  3389 | `{` |
|    45 |  3390 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    45 |  3391 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    45 |  3392 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    45 |  3393 | `	xmlNsPtr pDecl = DomNsDeclByName(pElem,zName);` |
|    45 |  3394 | `	xmlAttrPtr pAttr = pDecl ? 0 : DomAttrByName(pElem,zName);` |
|    45 |  3395 | `	xmlChar *zVal = pAttr ? xmlNodeListGetString(pAttr->doc,pAttr->children,1) : 0;` |
|    45 |  3396 | `	if( pDecl ){` |
|     - |  3397 | `		/* A declaration's "value" is the URI it binds. */` |
|     7 |  3398 | `		ph7_result_string(pCtx,pDecl->href ? (const char *)pDecl->href : "",-1);` |
|     7 |  3399 | `		return PH7_OK;` |
|     - |  3400 | `	}` |
|    39 |  3401 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|    39 |  3402 | `	if( zVal ){` |
|    33 |  3403 | `		xmlFree(zVal);` |
|    16 |  3404 | `	}` |
|    39 |  3405 | `	return PH7_OK;` |
|    23 |  3406 | `}` |
|     - |  3407 | `/* DOMElement::hasAttribute(string $qualifiedName): bool */` |
|    18 |  3408 | `DOM_METHOD(vm_builtin_DOMElement_hasAttribute)` |
|     1 |  3409 | `{` |
|    19 |  3410 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    19 |  3411 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    43 |  3412 | `	ph7_result_bool(pCtx,pNd != 0` |
|    27 |  3413 | `		&& (DomAttrByName((xmlNodePtr)pNd->pNode,zName) != 0` |
|    15 |  3414 | `		 \|\| DomNsDeclByName((xmlNodePtr)pNd->pNode,zName) != 0));` |
|    19 |  3415 | `	return PH7_OK;` |
|     1 |  3416 | `}` |
|     - |  3417 | `/* DOMElement::setAttribute(string $qualifiedName, string $value): DOMAttr -- php` |
|     - |  3418 | ` * answers the attribute NODE it wrote, so the write is followed by a wrap. */` |
|    24 |  3419 | `DOM_METHOD(vm_builtin_DOMElement_setAttribute)` |
|     1 |  3420 | `{` |
|    25 |  3421 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    25 |  3422 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|    25 |  3423 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     - |  3424 | `	xmlAttrPtr pAttr;` |
|    25 |  3425 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     7 |  3426 | `		return DomThrowAlways(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  3427 | `	}` |
|    19 |  3428 | `	if( DomNsDeclByName((xmlNodePtr)pNd->pNode,zName) ){` |
|     - |  3429 | `		/* php will not write THROUGH a declaration this element already makes:` |
|     - |  3430 | `		 * the write is dropped and the answer is false. (A name that is not one` |
|     - |  3431 | `		 * yet becomes an ordinary attribute, colon and all.) */` |
|     3 |  3432 | `		ph7_result_bool(pCtx,0);` |
|     3 |  3433 | `		return PH7_OK;` |
|     - |  3434 | `	}` |
|    17 |  3435 | `	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);` |
|    17 |  3436 | `	pAttr = DomAttrByName((xmlNodePtr)pNd->pNode,zName);` |
|    17 |  3437 | `	if( pAttr == 0 ){` |
|   ! 0 |  3438 | `		ph7_result_null(pCtx);` |
|   ! 0 |  3439 | `		return PH7_OK;` |
|     - |  3440 | `	}` |
|    17 |  3441 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|    13 |  3442 | `}` |
|     - |  3443 | `/* DOMElement::removeAttribute(string $qualifiedName): bool */` |
|    10 |  3444 | `DOM_METHOD(vm_builtin_DOMElement_removeAttribute)` |
|     1 |  3445 | `{` |
|    11 |  3446 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    11 |  3447 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    11 |  3448 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    11 |  3449 | `	xmlAttrPtr pAttr = DomAttrByName(pElem,zName);` |
|    11 |  3450 | `	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|    11 |  3451 | `	if( pDecl ){` |
|     - |  3452 | `		/* The declaration goes, and whatever still needs it gets it back: php` |
|     - |  3453 | `		 * answers TRUE either way, and a binding nothing uses simply vanishes. */` |
|     5 |  3454 | `		DomNsDeclRemove(pElem,pDecl);` |
|     5 |  3455 | `		DomNsReconcile(pElem);` |
|     5 |  3456 | `		ph7_result_bool(pCtx,1);` |
|     5 |  3457 | `		return PH7_OK;` |
|     - |  3458 | `	}` |
|     7 |  3459 | `	if( pAttr == 0 ){` |
|     - |  3460 | `		/* Absent (or a DTD default): php returns false */` |
|     3 |  3461 | `		ph7_result_bool(pCtx,0);` |
|     3 |  3462 | `		return PH7_OK;` |
|     - |  3463 | `	}` |
|     5 |  3464 | `	xmlRemoveProp(pAttr);` |
|     5 |  3465 | `	ph7_result_bool(pCtx,1);` |
|     5 |  3466 | `	return PH7_OK;` |
|     6 |  3467 | `}` |
|     - |  3468 | `/*` |
|     - |  3469 | `` * A `?string $namespace` argument as libxml wants it: NULL for php's null,`` |
|     - |  3470 | ` * which is how a caller asks for the attribute in NO namespace, and the bytes` |
|     - |  3471 | ` * otherwise.  Handing libxml "" for a null instead is not the same question --` |
|     - |  3472 | ` * "" matches a namespace whose URI is the empty string, which no document has,` |
|     - |  3473 | `` * so `getAttributeNS(null, 'href')` answered "" for every plain attribute.`` |
|     - |  3474 | ` */` |
|   132 |  3475 | `static const xmlChar * DomArgUri(int nArg,ph7_value **apArg,int iArg)` |
|     1 |  3476 | `{` |
|   133 |  3477 | `	const char *z = DomArgStrOrNull(nArg,apArg,iArg);` |
|   133 |  3478 | `	return (const xmlChar *)z;` |
|     1 |  3479 | `}` |
|     - |  3480 | `/* DOMElement::getAttributeNS(?string $namespace, string $localName): string */` |
|    20 |  3481 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNS)` |
|     1 |  3482 | `{` |
|    21 |  3483 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    21 |  3484 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|    21 |  3485 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    21 |  3486 | `	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,zUri) : 0;` |
|    21 |  3487 | `	if( zVal == 0 && pNd && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|     - |  3488 | `		/* The other door, the one hasAttributeNS already knew about: a` |
|     - |  3489 | ``		 * DECLARATION answers its URI here. `getAttributeNS($XMLNS, 'p')` was ""`` |
|     - |  3490 | ``		 * on an element declaring `xmlns:p`, where php answers the namespace. */`` |
|     9 |  3491 | `		xmlNsPtr pDecl = DomNsDeclOf((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal);` |
|     9 |  3492 | `		if( pDecl && pDecl->href ){` |
|     3 |  3493 | `			ph7_result_string(pCtx,(const char *)pDecl->href,-1);` |
|     3 |  3494 | `			return PH7_OK;` |
|     - |  3495 | `		}` |
|     3 |  3496 | `	}` |
|    19 |  3497 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|    19 |  3498 | `	if( zVal ){` |
|    11 |  3499 | `		xmlFree(zVal);` |
|     5 |  3500 | `	}` |
|    19 |  3501 | `	return PH7_OK;` |
|    11 |  3502 | `}` |
|     - |  3503 | `/* DOMElement::setAttributeNS(?string $namespace, string $qualifiedName, string $value): void */` |
|    66 |  3504 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNS)` |
|     1 |  3505 | `{` |
|    67 |  3506 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    67 |  3507 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|    67 |  3508 | `	const char *zQname = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|    67 |  3509 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|    67 |  3510 | `	int bHasUri = zUri != 0 && zUri[0] != 0;` |
|     - |  3511 | `	xmlNodePtr pNode;` |
|    67 |  3512 | `	xmlNsPtr pNs = 0;` |
|     - |  3513 | `	dom_qname sQ;` |
|     - |  3514 | `	int rc,bDecl,nOldDefs;` |
|    67 |  3515 | `	if( pNd == 0 ){` |
|   ! 0 |  3516 | `		return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  3517 | `	}` |
|    67 |  3518 | `	if( zQname[0] == 0 ){` |
|     - |  3519 | `		/* php screens the EMPTY name at the parameter, before the DOM sees it. */` |
|     3 |  3520 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  3521 | `			"DOMElement::setAttributeNS(): Argument #2 ($qualifiedName) must not be empty");` |
|     - |  3522 | `	}` |
|    65 |  3523 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_SET,&sQ);` |
|    65 |  3524 | `	if( rc ){` |
|    17 |  3525 | `		return DomThrowVoid(pCtx,rc);` |
|     - |  3526 | `	}` |
|    49 |  3527 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|    49 |  3528 | `	nOldDefs = DomNsDefCount(pNode);` |
|     - |  3529 | `	/* The DECLARATION spelling is the xmlns NAMESPACE plus an xmlns name --` |
|     - |  3530 | ``	 * `xmlns:z` binds z, plain `xmlns` binds the default one. Everything else`` |
|     - |  3531 | ``	 * is an ordinary attribute, including a bare `xmlns` under some other URI:`` |
|     - |  3532 | ``	 * php writes it as an ATTRIBUTE whose name happens to be `xmlns`, which`` |
|     - |  3533 | `	 * serializes beside the declaration already there. */` |
|    57 |  3534 | `	bDecl = DomUriIs(zUri,DOM_XMLNS_NS_URI)` |
|    53 |  3535 | `		&& (sQ.zPrefix ? xmlStrEqual(sQ.zPrefix,(const xmlChar *)"xmlns")` |
|    10 |  3536 | `		               : xmlStrEqual(sQ.zLocal,(const xmlChar *)"xmlns"));` |
|    49 |  3537 | `	if( bHasUri && !bDecl ){` |
|     - |  3538 | `		/* An ordinary attribute: find or declare the namespace it names. The` |
|     - |  3539 | `		 * xmlns URI is not special here -- php declares it like any other,` |
|     - |  3540 | `		 * EXCEPT under a prefix, which is the one binding it will not write.` |
|     - |  3541 | ``		 * The same goes for the prefix `xmlns` itself: php will REUSE a binding`` |
|     - |  3542 | ``		 * for it (which is how `setAttributeNS(XML_NS,'xmlns:z')` ends up as`` |
|     - |  3543 | ``		 * `xml:z`) and refuses to declare one. */`` |
|    35 |  3544 | `		if( sQ.zPrefix && DomUriIs(zUri,DOM_XMLNS_NS_URI) ){` |
|     3 |  3545 | `			DomQNameRelease(&sQ);` |
|     3 |  3546 | `			return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  3547 | `		}` |
|    33 |  3548 | `		pNs = DomNsResolve(pNode,zUri,sQ.zPrefix,0);` |
|    33 |  3549 | `		if( pNs == 0 ){` |
|     5 |  3550 | `			DomQNameRelease(&sQ);` |
|     5 |  3551 | `			return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  3552 | `		}` |
|    14 |  3553 | `	}` |
|    43 |  3554 | `	if( bDecl ){` |
|     - |  3555 | ``		/* The declaration spelling: `xmlns:z` binds z, plain `xmlns` binds the`` |
|     - |  3556 | `		 * default namespace, and the VALUE is the URI being bound. */` |
|    13 |  3557 | `		DomNsDeclare(pNode,sQ.zPrefix ? sQ.zLocal : 0,zVal);` |
|     7 |  3558 | `	}else{` |
|    31 |  3559 | `		xmlSetNsProp(pNode,pNs,sQ.zLocal,(const xmlChar *)zVal);` |
|     - |  3560 | `	}` |
|     - |  3561 | `	/* Either path may have declared something here -- and a declaration, new or` |
|     - |  3562 | `	 * rebound, can take the spelling away from what is already below it. */` |
|    43 |  3563 | `	if( bDecl \|\| DomNsDefCount(pNode) != nOldDefs ){` |
|    27 |  3564 | `		DomNsReconcile(pNode);` |
|    13 |  3565 | `	}` |
|    43 |  3566 | `	DomQNameRelease(&sQ);` |
|    43 |  3567 | `	return PH7_OK;` |
|    34 |  3568 | `}` |
|     - |  3569 |  |
|     - |  3570 | `/* ===== Attribute NODES ===== */` |
|     - |  3571 |  |
|     - |  3572 | `/*` |
|     - |  3573 | ` * The half of the attribute surface that hands out (and takes) the attribute` |
|     - |  3574 | ` * NODE rather than its string.  It is how a program moves an attribute between` |
|     - |  3575 | ` * elements, reads one it has held across an edit, or asks which attributes an` |
|     - |  3576 | ``  * element carries at all -- and none of it existed here, so `getAttributeNode` `` |
|     - |  3577 | `` * was a `Call to undefined method` and every idiom built on it stopped at the`` |
|     - |  3578 | ` * first line.` |
|     - |  3579 | ` */` |
|     - |  3580 |  |
|     - |  3581 | `/* The xmlAttr a DOMAttr-typed argument stands for (already screened by ZPP:` |
|     - |  3582 | ` * anything that is not a DOMAttr never reaches the body). */` |
|    84 |  3583 | `static xmlAttrPtr DomAttrArg(ph7_value *pVal)` |
|     1 |  3584 | `{` |
|    85 |  3585 | `	phl_domnode *pNd = DomObjArg(pVal);` |
|    85 |  3586 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    85 |  3587 | `	return (pNode && pNode->type == XML_ATTRIBUTE_NODE) ? (xmlAttrPtr)pNode : 0;` |
|     1 |  3588 | `}` |
|     - |  3589 | `/* The plain setAttributeNode spelling asks libxml's own name-only question --` |
|     - |  3590 | ``  * php's does too, so an attribute named `b` displaces a namespaced `x:b` `` |
|     - |  3591 | ` * there where the NS spelling would not. */` |
|    84 |  3592 | `static xmlAttrPtr DomAttrByLocal(xmlNodePtr pElem,const char *zName)` |
|     2 |  3593 | `{` |
|    86 |  3594 | `	xmlAttrPtr pAttr = pElem ? xmlHasProp(pElem,(const xmlChar *)zName) : 0;` |
|    86 |  3595 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|     2 |  3596 | `}` |
|     - |  3597 | `/* Append an attribute to an element's property list. The list is its own chain` |
|     - |  3598 | ` * (pElem->properties), not the child chain, which is why DomLinkLast will not` |
|     - |  3599 | ` * do: an attribute spliced among the CHILDREN serializes inside the tag body. */` |
|    72 |  3600 | `static void DomAttrLinkLast(xmlNodePtr pElem,xmlAttrPtr pAttr)` |
|     2 |  3601 | `{` |
|    74 |  3602 | `	xmlAttrPtr pLast = pElem->properties;` |
|    74 |  3603 | `	pAttr->parent = pElem;` |
|    74 |  3604 | `	pAttr->doc = pElem->doc;` |
|    74 |  3605 | `	pAttr->next = 0;` |
|    74 |  3606 | `	if( pLast == 0 ){` |
|    60 |  3607 | `		pElem->properties = pAttr;` |
|    60 |  3608 | `		pAttr->prev = 0;` |
|    60 |  3609 | `		return;` |
|     - |  3610 | `	}` |
|    21 |  3611 | `	while( pLast->next ){` |
|     7 |  3612 | `		pLast = pLast->next;` |
|     1 |  3613 | `	}` |
|    15 |  3614 | `	pLast->next = pAttr;` |
|    15 |  3615 | `	pAttr->prev = pLast;` |
|    38 |  3616 | `}` |
|     - |  3617 | `/* ...and at a POSITION: before pRef, which must be one of pElem's own --` |
|     - |  3618 | ` * insertBefore against an attribute reference, and replaceChild's` |
|     - |  3619 | ` * attribute-for-attribute swap, the two places php lets a caller state the` |
|     - |  3620 | ` * property list's order. */` |
|     6 |  3621 | `static void DomAttrLinkBefore(xmlNodePtr pElem,xmlAttrPtr pAttr,xmlAttrPtr pRef)` |
|     1 |  3622 | `{` |
|     7 |  3623 | `	pAttr->parent = pElem;` |
|     7 |  3624 | `	pAttr->doc = pElem->doc;` |
|     7 |  3625 | `	pAttr->next = pRef;` |
|     7 |  3626 | `	pAttr->prev = pRef->prev;` |
|     7 |  3627 | `	if( pRef->prev ){` |
|   ! 0 |  3628 | `		pRef->prev->next = pAttr;` |
|   ! 0 |  3629 | `	}else{` |
|     7 |  3630 | `		pElem->properties = pAttr;` |
|     - |  3631 | `	}` |
|     7 |  3632 | `	pRef->prev = pAttr;` |
|     7 |  3633 | `}` |
|     - |  3634 | `/* Detach an attribute from its element (or the orphan set) without freeing it:` |
|     - |  3635 | ` * php hands the caller back the node it displaced, alive. */` |
|    86 |  3636 | `static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr)` |
|     2 |  3637 | `{` |
|    88 |  3638 | `	if( pAttr->parent ){` |
|    31 |  3639 | `		xmlUnlinkNode((xmlNodePtr)pAttr);` |
|    16 |  3640 | `	}else{` |
|    58 |  3641 | `		DomOrphanRemove(pShell,(xmlNodePtr)pAttr);` |
|     - |  3642 | `	}` |
|    88 |  3643 | `}` |
|     - |  3644 | `/* DOMElement::getAttributeNode(string $qualifiedName): DOMAttr\|false -- FALSE` |
|     - |  3645 | ` * for an absent one, where the NS spelling below answers null. */` |
|   112 |  3646 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNode)` |
|     2 |  3647 | `{` |
|   114 |  3648 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   114 |  3649 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|   114 |  3650 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   114 |  3651 | `	xmlAttrPtr pAttr = DomAttrByName(pElem,zName);` |
|   114 |  3652 | `	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|   114 |  3653 | `	if( pDecl ){` |
|    17 |  3654 | `		return DomResultNsNode(pCtx,pNd,pDecl,pElem);` |
|     - |  3655 | `	}` |
|    98 |  3656 | `	if( pAttr == 0 ){` |
|     7 |  3657 | `		ph7_result_bool(pCtx,0);` |
|     7 |  3658 | `		return PH7_OK;` |
|     - |  3659 | `	}` |
|    92 |  3660 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|    58 |  3661 | `}` |
|     - |  3662 | `/* DOMElement::getAttributeNodeNS(?string $namespace, string $localName): ?DOMAttr */` |
|    34 |  3663 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNodeNS)` |
|     1 |  3664 | `{` |
|    35 |  3665 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    35 |  3666 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    35 |  3667 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|    35 |  3668 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    35 |  3669 | `	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|     - |  3670 | `		/* Here the LOCAL name is the prefix being declared -- and the DEFAULT` |
|     - |  3671 | ``		 * declaration, whose local name would be `xmlns`, is not reachable this`` |
|     - |  3672 | `		 * way at all. */` |
|     5 |  3673 | `		xmlNsPtr pDecl = DomNsDeclOf(pElem,(const xmlChar *)zLocal);` |
|     5 |  3674 | `		if( pDecl == 0 ){` |
|     3 |  3675 | `			ph7_result_null(pCtx);` |
|     3 |  3676 | `			return PH7_OK;` |
|     - |  3677 | `		}` |
|     3 |  3678 | `		return DomResultNsNode(pCtx,pNd,pDecl,pElem);` |
|     - |  3679 | `	}` |
|    31 |  3680 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)DomAttrByNs(pElem,zUri,zLocal));` |
|    18 |  3681 | `}` |
|     - |  3682 | `/*` |
|     - |  3683 | ` * DOMElement::setAttributeNode(DOMAttr $attr): ?DOMAttr and its NS spelling.` |
|     - |  3684 | ` *` |
|     - |  3685 | ` * php answers the attribute it DISPLACED (alive and ownerless) or null, and` |
|     - |  3686 | ` * the two spellings differ only in how they decide what "the same attribute"` |
|     - |  3687 | ` * is: the plain one matches on the local name alone, the NS one on the name` |
|     - |  3688 | ` * and the namespace URI together.  An attribute that already belongs to` |
|     - |  3689 | ` * another element of the same document is MOVED, not copied.` |
|     - |  3690 | ` */` |
|    64 |  3691 | `static int DomSetAttrNode(ph7_context *pCtx,int nArg,ph7_value **apArg,int bNS)` |
|     1 |  3692 | `{` |
|    65 |  3693 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    65 |  3694 | `	xmlAttrPtr pAttr = nArg > 0 ? DomAttrArg(apArg[0]) : 0;` |
|    65 |  3695 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     - |  3696 | `	xmlAttrPtr pOld;` |
|    65 |  3697 | `	if( pElem == 0 \|\| pAttr == 0 ){` |
|   ! 0 |  3698 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  3699 | `	}` |
|    65 |  3700 | `	if( pAttr->doc != pElem->doc && pAttr->doc != 0 ){` |
|     7 |  3701 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|     - |  3702 | `	}` |
|     - |  3703 | `	/* A constructed, document-less attribute is ADOPTED, like every insertion` |
|     - |  3704 | ``	 * door -- `$el->setAttributeNode(new DOMAttr('k','v'))` is how a built`` |
|     - |  3705 | `	 * attribute reaches a real document. */` |
|    59 |  3706 | `	DomAdoptIntoRecv(pCtx,apArg[0],DomObjArg(apArg[0]));` |
|    53 |  3707 | `	pOld = bNS ? DomAttrByNs(pElem,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name)` |
|    35 |  3708 | `	           : DomAttrByLocal(pElem,(const char *)pAttr->name);` |
|    59 |  3709 | `	if( pOld == pAttr ){` |
|     - |  3710 | `		/* Already this element's, under this spelling: php does nothing at all` |
|     - |  3711 | `		 * and answers null rather than handing the node back to itself. */` |
|     3 |  3712 | `		ph7_result_null(pCtx);` |
|     3 |  3713 | `		return PH7_OK;` |
|     - |  3714 | `	}` |
|    57 |  3715 | `	if( pOld ){` |
|     5 |  3716 | `		xmlUnlinkNode((xmlNodePtr)pOld);` |
|     5 |  3717 | `		DomOrphanAdd(pNd->pShell,(xmlNodePtr)pOld);` |
|     2 |  3718 | `	}` |
|    57 |  3719 | `	DomAttrDetach(pNd->pShell,pAttr);` |
|    57 |  3720 | `	DomAttrLinkLast(pElem,pAttr);` |
|    57 |  3721 | `	DomNsAttrArrive(pElem,pAttr);` |
|    57 |  3722 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pOld);` |
|    33 |  3723 | `}` |
|    18 |  3724 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNode)` |
|     1 |  3725 | `{` |
|    19 |  3726 | `	return DomSetAttrNode(pCtx,nArg,apArg,0);` |
|     1 |  3727 | `}` |
|    46 |  3728 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNodeNS)` |
|     1 |  3729 | `{` |
|    47 |  3730 | `	return DomSetAttrNode(pCtx,nArg,apArg,1);` |
|     1 |  3731 | `}` |
|     - |  3732 | `/* DOMElement::removeAttributeNode(DOMAttr $attr): DOMAttr -- an attribute that` |
|     - |  3733 | ` * is not THIS element's is php's Not Found, whichever element owns it. */` |
|    10 |  3734 | `DOM_METHOD(vm_builtin_DOMElement_removeAttributeNode)` |
|     1 |  3735 | `{` |
|    11 |  3736 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    11 |  3737 | `	xmlAttrPtr pAttr = nArg > 0 ? DomAttrArg(apArg[0]) : 0;` |
|    11 |  3738 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    11 |  3739 | `	if( pElem == 0 \|\| pAttr == 0 \|\| pAttr->parent != pElem ){` |
|     9 |  3740 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  3741 | `	}` |
|     3 |  3742 | `	xmlUnlinkNode((xmlNodePtr)pAttr);` |
|     3 |  3743 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|     3 |  3744 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     6 |  3745 | `}` |
|     - |  3746 | `/* The qualified name a serializer would write for an attribute. */` |
|    20 |  3747 | `static int DomAttrQName(xmlAttrPtr pAttr,SyBlob *pOut)` |
|     1 |  3748 | `{` |
|    21 |  3749 | `	if( pAttr->ns && pAttr->ns->prefix ){` |
|     3 |  3750 | `		SyBlobAppend(pOut,(const void *)pAttr->ns->prefix,SyStrlen((const char *)pAttr->ns->prefix));` |
|     3 |  3751 | `		SyBlobAppend(pOut,(const void *)":",sizeof(char));` |
|     1 |  3752 | `	}` |
|    21 |  3753 | `	SyBlobAppend(pOut,(const void *)pAttr->name,SyStrlen((const char *)pAttr->name));` |
|    21 |  3754 | `	return PH7_OK;` |
|     1 |  3755 | `}` |
|     - |  3756 | `/* DOMElement::getAttributeNames(): array -- the qualified names, in document` |
|     - |  3757 | ` * order, as a LIST (php re-keys from zero). */` |
|    18 |  3758 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNames)` |
|     1 |  3759 | `{` |
|    19 |  3760 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    19 |  3761 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|    19 |  3762 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     - |  3763 | `	xmlAttrPtr pAttr;` |
|     - |  3764 | `	xmlNsPtr pNs;` |
|     9 |  3765 | `	SXUNUSED(nArg);` |
|     9 |  3766 | `	SXUNUSED(apArg);` |
|    19 |  3767 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|   ! 0 |  3768 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3769 | `	}` |
|     - |  3770 | `	/* php lists the element's own DECLARATIONS first, in the order it makes` |
|     - |  3771 | `	 * them, and the attributes after. */` |
|    35 |  3772 | `	for( pNs = pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE` |
|    60 |  3773 | `			? ((xmlNodePtr)pNd->pNode)->nsDef : 0 ; pNs ; pNs = pNs->next ){` |
|     - |  3774 | `		SyBlob sName;` |
|    15 |  3775 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    15 |  3776 | `		SyBlobAppend(&sName,(const void *)DOM_XMLNS_NAME,SyStrlen(DOM_XMLNS_NAME));` |
|    15 |  3777 | `		if( pNs->prefix ){` |
|     9 |  3778 | `			SyBlobAppend(&sName,(const void *)":",sizeof(char));` |
|     9 |  3779 | `			SyBlobAppend(&sName,(const void *)pNs->prefix,SyStrlen((const char *)pNs->prefix));` |
|     4 |  3780 | `		}` |
|    15 |  3781 | `		ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|    15 |  3782 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    15 |  3783 | `		ph7_value_reset_string_cursor(pVal);` |
|    15 |  3784 | `		SyBlobRelease(&sName);` |
|     8 |  3785 | `	}` |
|    39 |  3786 | `	for( pAttr = DomAttrList(pNd ? (xmlNodePtr)pNd->pNode : 0) ; pAttr ; pAttr = pAttr->next ){` |
|     - |  3787 | `		SyBlob sName;` |
|    21 |  3788 | `		if( pAttr->type != XML_ATTRIBUTE_NODE ){` |
|   ! 0 |  3789 | `			continue;` |
|     - |  3790 | `		}` |
|    21 |  3791 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|    21 |  3792 | `		DomAttrQName(pAttr,&sName);` |
|    21 |  3793 | `		ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|    21 |  3794 | `		ph7_array_add_elem(pArray,0,pVal);` |
|    21 |  3795 | `		ph7_value_reset_string_cursor(pVal);` |
|    21 |  3796 | `		SyBlobRelease(&sName);` |
|    11 |  3797 | `	}` |
|    19 |  3798 | `	ph7_result_value(pCtx,pArray);` |
|    19 |  3799 | `	return PH7_OK;` |
|    10 |  3800 | `}` |
|     - |  3801 | `/* DOMElement::hasAttributeNS(?string $namespace, string $localName): bool */` |
|    22 |  3802 | `DOM_METHOD(vm_builtin_DOMElement_hasAttributeNS)` |
|     1 |  3803 | `{` |
|    23 |  3804 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    23 |  3805 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    23 |  3806 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|    23 |  3807 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    23 |  3808 | `	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|     - |  3809 | `		/*` |
|     - |  3810 | `		 * Two doors onto that namespace, and php answers about EITHER: a` |
|     - |  3811 | `		 * DECLARATION, which is not an attribute in libxml at all, and a real` |
|     - |  3812 | `` 		 * attribute in it -- which is what `createAttributeNS($XMLNS, ...)` `` |
|     - |  3813 | `		 * makes, and which this only asked the first door about. (A DEFAULT` |
|     - |  3814 | `		 * declaration is not one of them: php answers false for the local name` |
|     - |  3815 | ``		 * `xmlns`, and the prefix comparison below never matches it.)`` |
|     - |  3816 | `		 */` |
|    29 |  3817 | `		ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0` |
|    14 |  3818 | `			\|\| DomNsDeclOf(pElem,(const xmlChar *)zLocal) != 0);` |
|    17 |  3819 | `		return PH7_OK;` |
|     - |  3820 | `	}` |
|     7 |  3821 | `	ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0);` |
|     7 |  3822 | `	return PH7_OK;` |
|    12 |  3823 | `}` |
|     - |  3824 | `/* DOMElement::removeAttributeNS(?string $namespace, string $localName): void --` |
|     - |  3825 | ` * an absent one is silence, as php's is. */` |
|     6 |  3826 | `DOM_METHOD(vm_builtin_DOMElement_removeAttributeNS)` |
|     1 |  3827 | `{` |
|     7 |  3828 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     7 |  3829 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     7 |  3830 | `	xmlAttrPtr pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,DomArgUri(nArg,apArg,0),zLocal) : 0;` |
|     7 |  3831 | `	if( pAttr ){` |
|     5 |  3832 | `		xmlRemoveProp(pAttr);` |
|     2 |  3833 | `	}` |
|     7 |  3834 | `	return PH7_OK;` |
|     1 |  3835 | `}` |
|     - |  3836 | `/*` |
|     - |  3837 | ` * DOMElement::toggleAttribute(string $qualifiedName, ?bool $force = null): bool` |
|     - |  3838 | ` *` |
|     - |  3839 | ` * php's 8.3 verb: with no $force it flips (removing answers false, adding a` |
|     - |  3840 | ` * value-less attribute answers true), and with one it only ADDS or only` |
|     - |  3841 | ` * REMOVES -- an add that finds the attribute already there leaves its value` |
|     - |  3842 | ` * alone.  The name is validated first, so a bad one is refused before the` |
|     - |  3843 | ` * element is touched.` |
|     - |  3844 | ` */` |
|    20 |  3845 | `DOM_METHOD(vm_builtin_DOMElement_toggleAttribute)` |
|     1 |  3846 | `{` |
|    21 |  3847 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    21 |  3848 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    21 |  3849 | `	int bForceGiven = nArg > 1 && !ph7_value_is_null(apArg[1]);` |
|    21 |  3850 | `	int bForce = bForceGiven && ph7_value_to_bool(apArg[1]);` |
|    21 |  3851 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     - |  3852 | `	xmlAttrPtr pAttr;` |
|     - |  3853 | `	xmlNsPtr pDecl;` |
|    21 |  3854 | `	if( pElem == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     5 |  3855 | `		return DomThrowAlways(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  3856 | `	}` |
|    17 |  3857 | `	pAttr = DomAttrByName(pElem,zName);` |
|    17 |  3858 | `	pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|    17 |  3859 | `	if( bForceGiven ? bForce : (pAttr == 0 && pDecl == 0) ){` |
|     7 |  3860 | `		if( pAttr == 0 && pDecl == 0 ){` |
|     5 |  3861 | `			sxu32 nXmlns = (sxu32)SyStrlen(DOM_XMLNS_NAME);` |
|     6 |  3862 | `			int bXmlnsName = SyStrlen(zName) >= nXmlns` |
|     3 |  3863 | `				&& SyStrncmp(zName,DOM_XMLNS_NAME,nXmlns) == 0` |
|     6 |  3864 | `				&& (zName[nXmlns] == 0 \|\| zName[nXmlns] == ':');` |
|     5 |  3865 | `			if( bXmlnsName ){` |
|     - |  3866 | `				/* An xmlns name toggled ON becomes a DECLARATION bound to the` |
|     - |  3867 | `				 * empty URI, not an attribute -- which is why it comes out` |
|     - |  3868 | `				 * before the attributes rather than after them. */` |
|     3 |  3869 | `				DomNsDeclare(pElem,zName[nXmlns] == ':' ? (const xmlChar *)(zName+nXmlns+1) : 0,"");` |
|     2 |  3870 | `			}else{` |
|     3 |  3871 | `				xmlSetProp(pElem,(const xmlChar *)zName,(const xmlChar *)"");` |
|     - |  3872 | `			}` |
|     2 |  3873 | `		}` |
|     7 |  3874 | `		ph7_result_bool(pCtx,1);` |
|     7 |  3875 | `		return PH7_OK;` |
|     - |  3876 | `	}` |
|    11 |  3877 | `	if( pAttr ){` |
|     5 |  3878 | `		xmlRemoveProp(pAttr);` |
|     7 |  3879 | `	}else if( pDecl ){` |
|     5 |  3880 | `		DomNsDeclRemove(pElem,pDecl);` |
|     5 |  3881 | `		DomNsReconcile(pElem);` |
|     2 |  3882 | `	}` |
|     9 |  3883 | `	ph7_result_bool(pCtx,0);` |
|     9 |  3884 | `	return PH7_OK;` |
|     9 |  3885 | `}` |
|     - |  3886 | `/*` |
|     - |  3887 | `` * The ID three.  php's `setIdAttribute*` is what makes an attribute the one`` |
|     - |  3888 | `` * `getElementById()` answers by, in a document with no DTD to say so, and`` |
|     - |  3889 | `` * `DOMAttr::isId()` is how a caller reads the flag back.  All three refuse a`` |
|     - |  3890 | ` * name the element does not carry with Not Found -- including an attribute that` |
|     - |  3891 | ` * belongs to a DIFFERENT element, which is why the node spelling checks the` |
|     - |  3892 | ` * owner rather than trusting the argument.` |
|     - |  3893 | ` */` |
|    26 |  3894 | `static int DomMarkId(xmlAttrPtr pAttr,int bIsId)` |
|     1 |  3895 | `{` |
|    27 |  3896 | `	if( bIsId ){` |
|    23 |  3897 | `		if( pAttr->atype != XML_ATTRIBUTE_ID ){` |
|    23 |  3898 | `			xmlChar *zVal = xmlNodeListGetString(pAttr->doc,pAttr->children,1);` |
|    23 |  3899 | `			if( zVal ){` |
|    23 |  3900 | `				xmlAddID(0,pAttr->doc,zVal,pAttr);` |
|    23 |  3901 | `				xmlFree(zVal);` |
|    11 |  3902 | `			}` |
|    11 |  3903 | `		}` |
|    23 |  3904 | `		pAttr->atype = XML_ATTRIBUTE_ID;` |
|    12 |  3905 | `	}else{` |
|     5 |  3906 | `		if( pAttr->atype == XML_ATTRIBUTE_ID ){` |
|     5 |  3907 | `			xmlRemoveID(pAttr->doc,pAttr);` |
|     2 |  3908 | `		}` |
|     5 |  3909 | `		pAttr->atype = (xmlAttributeType)0;` |
|     - |  3910 | `	}` |
|    27 |  3911 | `	return PH7_OK;` |
|     1 |  3912 | `}` |
|    26 |  3913 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttribute)` |
|     1 |  3914 | `{` |
|    27 |  3915 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    27 |  3916 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|     - |  3917 | `	/* php's ID lookup is the STRICT one -- the whole name in NO namespace, with` |
|     - |  3918 | ``	 * no prefix resolution, so `setIdAttribute('p:k')` is Not Found even on an`` |
|     - |  3919 | ``	 * element that carries `p:k`. */`` |
|    27 |  3920 | `	xmlAttrPtr pAttr = pNd ? DomAttrNoNs((xmlNodePtr)pNd->pNode,zName) : 0;` |
|    27 |  3921 | `	if( pAttr == 0 ){` |
|     9 |  3922 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  3923 | `	}` |
|    19 |  3924 | `	return DomMarkId(pAttr,nArg > 1 && ph7_value_to_bool(apArg[1]));` |
|    14 |  3925 | `}` |
|     - |  3926 | `/* php's second parameter is spelled $qualifiedName and matched as a LOCAL one:` |
|     - |  3927 | ` * the namespace decides the rest. */` |
|     8 |  3928 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttributeNS)` |
|     1 |  3929 | `{` |
|     9 |  3930 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     9 |  3931 | `	const char *zLocal = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|     9 |  3932 | `	xmlAttrPtr pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,DomArgUri(nArg,apArg,0),zLocal) : 0;` |
|     9 |  3933 | `	if( pAttr == 0 ){` |
|     5 |  3934 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  3935 | `	}` |
|     5 |  3936 | `	return DomMarkId(pAttr,nArg > 2 && ph7_value_to_bool(apArg[2]));` |
|     5 |  3937 | `}` |
|    10 |  3938 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttributeNode)` |
|     1 |  3939 | `{` |
|    11 |  3940 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    11 |  3941 | `	xmlAttrPtr pAttr = nArg > 1 ? DomAttrArg(apArg[0]) : 0;` |
|    11 |  3942 | `	if( pNd == 0 \|\| pAttr == 0 \|\| pAttr->parent != (xmlNodePtr)pNd->pNode ){` |
|     7 |  3943 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|     - |  3944 | `	}` |
|     5 |  3945 | `	return DomMarkId(pAttr,nArg > 1 && ph7_value_to_bool(apArg[1]));` |
|     6 |  3946 | `}` |
|     - |  3947 | `/* DOMAttr::isId(): bool */` |
|    42 |  3948 | `DOM_METHOD(vm_builtin_DOMAttr_isId)` |
|     1 |  3949 | `{` |
|    43 |  3950 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    43 |  3951 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    21 |  3952 | `	SXUNUSED(nArg);` |
|    21 |  3953 | `	SXUNUSED(apArg);` |
|    85 |  3954 | `	ph7_result_bool(pCtx,pNode != 0 && pNode->type == XML_ATTRIBUTE_NODE` |
|    42 |  3955 | `		&& ((xmlAttrPtr)pNode)->atype == XML_ATTRIBUTE_ID);` |
|    43 |  3956 | `	return PH7_OK;` |
|     1 |  3957 | `}` |
|     - |  3958 | `/*` |
|     - |  3959 | ` * DOMDocument::getElementById(string $elementId): ?DOMElement` |
|     - |  3960 | ` *` |
|     - |  3961 | `` * The other half of the ID three: what `setIdAttribute()` is FOR. libxml keeps`` |
|     - |  3962 | `` * the table (a DTD `ATTLIST ... ID` fills it at parse time, `xmlAddID` fills it`` |
|     - |  3963 | ` * when a program marks one), and php answers the attribute's element -- but` |
|     - |  3964 | ` * only while that element is still IN the document, so an element removed from` |
|     - |  3965 | ` * the tree stops being findable even though its attribute still carries the` |
|     - |  3966 | ` * flag. A DTD-declared DEFAULT has no attribute node behind it and libxml says` |
|     - |  3967 | ` * so with its own sentinel.` |
|     - |  3968 | ` */` |
|    44 |  3969 | `DOM_METHOD(vm_builtin_DOMDocument_getElementById)` |
|     1 |  3970 | `{` |
|    45 |  3971 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    45 |  3972 | `	const char *zId = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    45 |  3973 | `	xmlAttrPtr pAttr = pNd ? xmlGetID((xmlDocPtr)pNd->pNode,(const xmlChar *)zId) : 0;` |
|    44 |  3974 | `	if( pAttr == 0 \|\| pAttr == (xmlAttrPtr)-1 \|\| pAttr->type != XML_ATTRIBUTE_NODE` |
|    26 |  3975 | `	 \|\| pAttr->parent == 0 \|\| !DomIsConnected(pAttr->parent) ){` |
|    23 |  3976 | `		ph7_result_null(pCtx);` |
|    23 |  3977 | `		return PH7_OK;` |
|     - |  3978 | `	}` |
|    23 |  3979 | `	return DomResultNodeOf(pCtx,pNd,pAttr->parent);` |
|    23 |  3980 | `}` |
|     - |  3981 | `/* DOMDocument::createAttribute(string $localName): DOMAttr -- ownerless: php` |
|     - |  3982 | ` * gives it this document but NO element until it is set on one. */` |
|    66 |  3983 | `DOM_METHOD(vm_builtin_DOMDocument_createAttribute)` |
|     2 |  3984 | `{` |
|    68 |  3985 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    68 |  3986 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     - |  3987 | `	xmlAttrPtr pAttr;` |
|    68 |  3988 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     9 |  3989 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  3990 | `	}` |
|    60 |  3991 | `	pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,(const xmlChar *)zName,0);` |
|    60 |  3992 | `	if( pAttr == 0 ){` |
|   ! 0 |  3993 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  3994 | `	}` |
|    60 |  3995 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|    60 |  3996 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|    35 |  3997 | `}` |
|     - |  3998 | `/*` |
|     - |  3999 | ` * DOMDocument::createAttributeNS(?string $namespace, string $qualifiedName): DOMAttr` |
|     - |  4000 | ` *` |
|     - |  4001 | ` * The namespace is declared on the document's ROOT ELEMENT, not on the` |
|     - |  4002 | ` * attribute -- which is why a document that has no root element yet cannot` |
|     - |  4003 | ` * answer at all, and says so with php's warning and a false.` |
|     - |  4004 | ` */` |
|    42 |  4005 | `DOM_METHOD(vm_builtin_DOMDocument_createAttributeNS)` |
|     1 |  4006 | `{` |
|    43 |  4007 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    43 |  4008 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|    43 |  4009 | `	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     - |  4010 | `	xmlNodePtr pRoot;` |
|     - |  4011 | `	xmlAttrPtr pAttr;` |
|     - |  4012 | `	dom_qname sQ;` |
|     - |  4013 | `	int rc;` |
|    43 |  4014 | `	if( pNd == 0 ){` |
|   ! 0 |  4015 | `		return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  4016 | `	}` |
|    43 |  4017 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_ATTR,&sQ);` |
|    43 |  4018 | `	if( rc ){` |
|    17 |  4019 | `		return DomThrow(pCtx,rc);` |
|     - |  4020 | `	}` |
|    27 |  4021 | `	pRoot = xmlDocGetRootElement((xmlDocPtr)pNd->pNode);` |
|    27 |  4022 | `	if( pRoot == 0 ){` |
|     3 |  4023 | `		DomQNameRelease(&sQ);` |
|     - |  4024 | ``		/* The context prints php's `DOMDocument::createAttributeNS(): ` itself. */`` |
|     3 |  4025 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Document Missing Root Element");` |
|     3 |  4026 | `		ph7_result_bool(pCtx,0);` |
|     3 |  4027 | `		return PH7_OK;` |
|     - |  4028 | `	}` |
|    25 |  4029 | `	pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,sQ.zLocal,0);` |
|    25 |  4030 | `	if( pAttr == 0 ){` |
|   ! 0 |  4031 | `		DomQNameRelease(&sQ);` |
|   ! 0 |  4032 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  4033 | `	}` |
|    25 |  4034 | `	if( zUri && zUri[0] && sQ.zPrefix ){` |
|    15 |  4035 | `		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,sQ.zPrefix,1));` |
|    18 |  4036 | `	}else if( zUri && zUri[0]` |
|    11 |  4037 | `	 && xmlStrEqual(sQ.zLocal,(const xmlChar *)DOM_XMLNS_NAME) ){` |
|     - |  4038 | `		/*` |
|     - |  4039 | ``		 * The unprefixed `xmlns` -- the only name the grammar lets through with`` |
|     - |  4040 | `		 * the xmlns namespace, and the name a DEFAULT declaration is written` |
|     - |  4041 | `		 * with. It IS in that namespace and php answers so, but nothing is` |
|     - |  4042 | `		 * DECLARED for it: the binding is the attribute's alone, so it is parked` |
|     - |  4043 | `		 * on the document, which is what frees it. Without this the attribute` |
|     - |  4044 | ``		 * answered namespaceURI null, `hasAttributeNS($XMLNS, 'xmlns')` was`` |
|     - |  4045 | ``		 * false for it once written, and `getAttribute('xmlns')` -- a by-NAME`` |
|     - |  4046 | `		 * lookup, which skips a namespaced attribute -- answered its value where` |
|     - |  4047 | `		 * php answers "".` |
|     - |  4048 | `		 */` |
|     5 |  4049 | `		xmlNsPtr pNs = DomNsReuse(pRoot,zUri);` |
|     5 |  4050 | `		if( pNs == 0 ){` |
|     3 |  4051 | `			pNs = xmlNewNs(0,(const xmlChar *)zUri,0);` |
|     3 |  4052 | `			if( pNs ){` |
|     3 |  4053 | `				DomNsPark((xmlNodePtr)pAttr,pNs);` |
|     1 |  4054 | `			}` |
|     1 |  4055 | `		}` |
|     5 |  4056 | `		if( pNs ){` |
|     5 |  4057 | `			xmlSetNs((xmlNodePtr)pAttr,pNs);` |
|     3 |  4058 | `		}` |
|     9 |  4059 | `	}else if( zUri && zUri[0] ){` |
|     5 |  4060 | `		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,0,1));` |
|     2 |  4061 | `	}` |
|    25 |  4062 | `	DomQNameRelease(&sQ);` |
|    25 |  4063 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|    25 |  4064 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|    22 |  4065 | `}` |
|     - |  4066 |  |
|     - |  4067 | `/* ===== getElementsByTagName (live) ===== */` |
|     - |  4068 |  |
|     - |  4069 | `/* Length-carrying: the name comes from a declared string SLOT, whose bytes are` |
|     - |  4070 | ` * NOT NUL-terminated (PH7_NativeAttrStr borrows the blob as-is). */` |
|  1314 |  4071 | `static int DomLenEq(const xmlChar *zHave,const char *zWant,int nWant)` |
|     1 |  4072 | `{` |
|  1870 |  4073 | `	return zHave != 0 && (int)SyStrlen((const char *)zHave) == nWant` |
|  1971 |  4074 | `		&& SyMemcmp((const void *)zHave,(const void *)zWant,(sxu32)nWant) == 0;` |
|     1 |  4075 | `}` |
|     - |  4076 | `/*` |
|     - |  4077 | ` * One element against a (namespace, local name) pair. nUri < 0 is the name-only` |
|     - |  4078 | `` * query, `getElementsByTagName` -- the sentinel is the LENGTH and not the`` |
|     - |  4079 | ` * pointer because an empty declared string slot reads back as a NULL one, which` |
|     - |  4080 | ` * is exactly the namespace-aware "in NO namespace" case. Under the` |
|     - |  4081 | ` * namespace-aware query php's three cases are NOT symmetric, and that asymmetry` |
|     - |  4082 | ` * is the whole content of the rule:` |
|     - |  4083 | ` *` |
|     - |  4084 | `` *   `*`          every element, in a namespace or in none`` |
|     - |  4085 | ` *   null or ""   only the elements in NO namespace (php maps its null argument` |
|     - |  4086 | ` *                and the empty string to the same question)` |
|     - |  4087 | ` *   a URI        only the elements in it` |
|     - |  4088 | ` *` |
|     - |  4089 | `` * The local name is `*` for every name, and an exact match otherwise -- against`` |
|     - |  4090 | `` * libxml's `name`, which is the LOCAL name, so a prefix never enters into it.`` |
|     - |  4091 | ` */` |
|  1874 |  4092 | `static int DomGebtnMatch(xmlNodePtr pNode,const char *zUri,int nUri,` |
|     - |  4093 | `	const char *zName,int nName)` |
|     1 |  4094 | `{` |
|  1875 |  4095 | `	if( pNode->type != XML_ELEMENT_NODE ){` |
|    47 |  4096 | `		return 0;` |
|     - |  4097 | `	}` |
|  1829 |  4098 | `	if( !(nName == 1 && zName[0] == '*') && !DomLenEq(pNode->name,zName,nName) ){` |
|   581 |  4099 | `		return 0;` |
|     - |  4100 | `	}` |
|  1249 |  4101 | `	if( nUri < 0 \|\| (nUri == 1 && zUri[0] == '*') ){` |
|   375 |  4102 | `		return 1;` |
|     - |  4103 | `	}` |
|   503 |  4104 | `	if( nUri == 0 ){` |
|   193 |  4105 | `		return pNode->ns == 0;` |
|     - |  4106 | `	}` |
|   311 |  4107 | `	return pNode->ns != 0 && DomLenEq(pNode->ns->href,zUri,nUri);` |
|   752 |  4108 | `}` |
|     - |  4109 | `/* The list is LIVE: nothing is snapshotted, both queries re-walk the subtree` |
|     - |  4110 | ` * every time DOMNodeList asks. Passing iWant < 0 counts instead of indexing. */` |
|   338 |  4111 | `static xmlNodePtr DomGebtnWalk(xmlNodePtr pRoot,const char *zUri,int nUri,` |
|     - |  4112 | `	const char *zName,int nName,int iWant,int *pnCount)` |
|     1 |  4113 | `{` |
|   339 |  4114 | `	xmlNodePtr pCur = pRoot ? pRoot->children : 0;` |
|   339 |  4115 | `	int iCount = 0;` |
|  1633 |  4116 | `	while( pCur ){` |
|  1503 |  4117 | `		if( DomGebtnMatch(pCur,zUri,nUri,zName,nName) ){` |
|   547 |  4118 | `			if( iWant >= 0 && iCount == iWant ){` |
|   209 |  4119 | `				return pCur;` |
|     - |  4120 | `			}` |
|   339 |  4121 | `			iCount++;` |
|   169 |  4122 | `		}` |
|  1295 |  4123 | `		pCur = DomWalkNext(pCur,pRoot);` |
|     1 |  4124 | `	}` |
|   131 |  4125 | `	if( pnCount ){` |
|    71 |  4126 | `		*pnCount = iCount;` |
|    35 |  4127 | `	}` |
|   131 |  4128 | `	return 0;` |
|   170 |  4129 | `}` |
|     - |  4130 |  |
|     - |  4131 | `/* ===== DOMDocument ===== */` |
|     - |  4132 |  |
|     - |  4133 | `/*` |
|     - |  4134 | ` * DOMDocument::__construct(string $version = '1.0', string $encoding = '')` |
|     - |  4135 | ` *` |
|     - |  4136 | `` * The chunk reached the document through `parent::__construct(__dom_doc_new(..))`;`` |
|     - |  4137 | ` * a native constructor writes its own two slots, and $__doc is the document` |
|     - |  4138 | ` * ITSELF (php's ownerDocument is null on a document, which __get answers).` |
|     - |  4139 | ` */` |
|  1640 |  4140 | `DOM_METHOD(vm_builtin_DOMDocument_construct)` |
|     5 |  4141 | `{` |
|  1645 |  4142 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1645 |  4143 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  1645 |  4144 | `	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";` |
|  1645 |  4145 | `	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     - |  4146 | `	xmlDocPtr pDoc;` |
|     - |  4147 | `	phl_xmldoc *pShell;` |
|     - |  4148 | `	phl_domnode *pRes;` |
|  1645 |  4149 | `	if( pThis == 0 ){` |
|   ! 0 |  4150 | `		return PH7_OK;` |
|     - |  4151 | `	}` |
|     - |  4152 | ``	/* The version goes through as WRITTEN -- `new DOMDocument('')` is a document`` |
|     - |  4153 | ``	 * whose `version` reads "" and whose declaration says `version=""`, which is`` |
|     - |  4154 | `	 * php's answer (only an omitted argument takes the "1.0" default, and that` |
|     - |  4155 | `	 * one is the signature's). */` |
|  1645 |  4156 | `	pDoc = xmlNewDoc((const xmlChar *)zVersion);` |
|  1645 |  4157 | `	if( pDoc == 0 ){` |
|   ! 0 |  4158 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  4159 | `	}` |
|  1645 |  4160 | `	if( zEncoding[0] ){` |
|     5 |  4161 | `		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);` |
|     2 |  4162 | `	}` |
|  1645 |  4163 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|  1645 |  4164 | `	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|  1645 |  4165 | `	if( pRes == 0 ){` |
|   ! 0 |  4166 | `		if( pShell == 0 ){` |
|   ! 0 |  4167 | `			xmlFreeDoc(pDoc);` |
|   ! 0 |  4168 | `		}` |
|   ! 0 |  4169 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  4170 | `	}` |
|  1645 |  4171 | `	DomSetRes(pVm,pThis,pRes);` |
|  1645 |  4172 | `	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);` |
|  1645 |  4173 | `	return PH7_OK;` |
|   825 |  4174 | `}` |
|     - |  4175 | `/*` |
|     - |  4176 | ` * The URI php stamps on a document parsed from MEMORY.` |
|     - |  4177 | ` *` |
|     - |  4178 | ` * A file parse takes its URI from the file; a memory parse has none, and php` |
|     - |  4179 | ` * gives it the process's CURRENT DIRECTORY with a trailing separator so that a` |
|     - |  4180 | `` * relative `xml:base` (and every `baseURI` under it) resolves against the same`` |
|     - |  4181 | ` * place a relative include would. The path is the bytes getcwd() answers, not a` |
|     - |  4182 | ` * URI: a space stays a space. Asks the VFS rather than the C library so the` |
|     - |  4183 | ` * win32 backend answers its own spelling.` |
|     - |  4184 | ` *` |
|     - |  4185 | ` * The answer travels through the context's RESULT slot, which is where the VFS` |
|     - |  4186 | ` * writes it -- every caller sets its own return value afterwards.` |
|     - |  4187 | ` */` |
|  1548 |  4188 | `static void DomCwdUri(ph7_context *pCtx,SyBlob *pOut)` |
|     5 |  4189 | `{` |
|  1553 |  4190 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|  1553 |  4191 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|  1553 |  4192 | `	PH7_MemObjRelease(pCtx->pRet);` |
|  1548 |  4193 | `	if( pVfs && pVfs->xGetcwd && pVfs->xGetcwd(pCtx) == PH7_OK` |
|  1553 |  4194 | `	 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0 ){` |
|  1553 |  4195 | `		const char *zDir = (const char *)SyBlobData(&pCtx->pRet->sBlob);` |
|  1553 |  4196 | `		sxu32 nDir = SyBlobLength(&pCtx->pRet->sBlob);` |
|  1553 |  4197 | `		SyBlobAppend(pOut,zDir,nDir);` |
|  1553 |  4198 | `		if( nDir < 1 \|\| (zDir[nDir - 1] != '/' && zDir[nDir - 1] != '\\') ){` |
|  1553 |  4199 | `			SyBlobAppend(pOut,"/",sizeof(char));` |
|   774 |  4200 | `		}` |
|   774 |  4201 | `	}` |
|  1553 |  4202 | `	PH7_MemObjRelease(pCtx->pRet);` |
|  1553 |  4203 | `	SyBlobNullAppend(pOut);` |
|  1553 |  4204 | `}` |
|     - |  4205 | `/* Stamp it, unless the parse already gave the document one. */` |
|  1562 |  4206 | `static void DomStampCwd(ph7_context *pCtx,xmlDocPtr pDoc)` |
|     5 |  4207 | `{` |
|     - |  4208 | `	SyBlob sDir;` |
|  1567 |  4209 | `	if( pDoc == 0 \|\| pDoc->URL ){` |
|    15 |  4210 | `		return;` |
|     - |  4211 | `	}` |
|  1553 |  4212 | `	DomCwdUri(pCtx,&sDir);` |
|  1553 |  4213 | `	if( SyBlobLength(&sDir) > 0 ){` |
|  1553 |  4214 | `		pDoc->URL = xmlStrdup((const xmlChar *)SyBlobData(&sDir));` |
|   774 |  4215 | `	}` |
|  1553 |  4216 | `	SyBlobRelease(&sDir);` |
|   786 |  4217 | `}` |
|     - |  4218 | `/*` |
|     - |  4219 | ` * The parse options one of php's load methods actually runs with: the caller's` |
|     - |  4220 | `` * `$options`, OR'd with what the document's own directives ask for.`` |
|     - |  4221 | ` *` |
|     - |  4222 | ` *   preserveWhiteSpace = false  ->  NOBLANKS   (drop ignorable whitespace)` |
|     - |  4223 | ` *   substituteEntities = true   ->  NOENT      (expand entity references)` |
|     - |  4224 | ` *   validateOnParse    = true   ->  DTDVALID   (validate against the DTD)` |
|     - |  4225 | ` *   resolveExternals   = true   ->  DTDATTR    (apply the DTD's default` |
|     - |  4226 | ` *                                               attributes -- which is also` |
|     - |  4227 | ` *                                               what makes libxml LOAD an` |
|     - |  4228 | ` *                                               external subset)` |
|     - |  4229 | ` *   recover            = true   ->  RECOVER    (keep what parsed)` |
|     - |  4230 | ` *` |
|     - |  4231 | ` * The two directions never cancel: a directive can only ADD to the argument,` |
|     - |  4232 | `` * which is why `loadXML($s, LIBXML_NOENT)` expands entities on a document whose`` |
|     - |  4233 | `` * `substituteEntities` is false.`` |
|     - |  4234 | ` */` |
|  1568 |  4235 | `static int DomParseOptions(ph7_class_instance *pThis,int iOpts)` |
|     5 |  4236 | `{` |
|  1573 |  4237 | `	if( pThis == 0 ){` |
|   ! 0 |  4238 | `		return iOpts;` |
|     - |  4239 | `	}` |
|  1573 |  4240 | `	if( !PH7_NativeAttrTruthy(pThis,"preserveWhiteSpace") ){` |
|    17 |  4241 | `		iOpts \|= XML_PARSE_NOBLANKS;` |
|     8 |  4242 | `	}` |
|  1573 |  4243 | `	if( PH7_NativeAttrTruthy(pThis,"substituteEntities") ){` |
|     9 |  4244 | `		iOpts \|= XML_PARSE_NOENT;` |
|     4 |  4245 | `	}` |
|  1573 |  4246 | `	if( PH7_NativeAttrTruthy(pThis,"validateOnParse") ){` |
|    39 |  4247 | `		iOpts \|= XML_PARSE_DTDVALID;` |
|    19 |  4248 | `	}` |
|  1573 |  4249 | `	if( PH7_NativeAttrTruthy(pThis,"resolveExternals") ){` |
|     9 |  4250 | `		iOpts \|= XML_PARSE_DTDATTR;` |
|     4 |  4251 | `	}` |
|  1573 |  4252 | `	if( PH7_NativeAttrTruthy(pThis,"recover") ){` |
|    13 |  4253 | `		iOpts \|= XML_PARSE_RECOVER;` |
|     6 |  4254 | `	}` |
|  1573 |  4255 | `	return iOpts;` |
|   789 |  4256 | `}` |
|     - |  4257 | `/*` |
|     - |  4258 | `` * A RECOVERING parse reports its diagnostics whatever `error_reporting()` says.`` |
|     - |  4259 | ` *` |
|     - |  4260 | ` * php forces E_WARNING back into the mask for the duration of a parse it is` |
|     - |  4261 | ` * recovering from -- the point being that a document which came back DAMAGED` |
|     - |  4262 | ` * must not do so in silence, however the script has configured reporting. It` |
|     - |  4263 | ` * forces that one level only: a libxml WARNING (an E_NOTICE) stays suppressed.` |
|     - |  4264 | ` * Answers the previous state, which the caller restores.` |
|     - |  4265 | ` */` |
|     - |  4266 | `typedef struct { sxi32 iMask; int bOn; } phl_dom_errsave;` |
|  1568 |  4267 | `static phl_dom_errsave DomForceWarnings(ph7_vm *pVm,int bRecover)` |
|     5 |  4268 | `{` |
|     - |  4269 | `	phl_dom_errsave sSave;` |
|  1573 |  4270 | `	sSave.iMask = pVm->iErrMask;` |
|  1573 |  4271 | `	sSave.bOn = pVm->bErrReport;` |
|  1573 |  4272 | `	if( bRecover ){` |
|    15 |  4273 | `		pVm->iErrMask \|= E_WARNING;` |
|    15 |  4274 | `		pVm->bErrReport = 1;` |
|     7 |  4275 | `	}` |
|  1573 |  4276 | `	return sSave;` |
|     5 |  4277 | `}` |
|  1568 |  4278 | `static void DomRestoreWarnings(ph7_vm *pVm,phl_dom_errsave sSave)` |
|     5 |  4279 | `{` |
|  1573 |  4280 | `	pVm->iErrMask = sSave.iMask;` |
|  1573 |  4281 | `	pVm->bErrReport = sSave.bOn;` |
|  1573 |  4282 | `}` |
|     - |  4283 | `/*` |
|     - |  4284 | ` * The path php names a loaded file by: the VFS's canonical absolute name, or` |
|     - |  4285 | ` * the working directory joined to it when the file does not exist and there is` |
|     - |  4286 | ` * nothing to canonicalize. Answers it NUL-terminated in *pOut.` |
|     - |  4287 | ` */` |
|    12 |  4288 | `static void DomAbsPath(ph7_context *pCtx,const char *zFile,SyBlob *pOut)` |
|     1 |  4289 | `{` |
|    13 |  4290 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     7 |  4291 | `	int bAbs = zFile[0] == '/' \|\| zFile[0] == '\\'` |
|    12 |  4292 | `		\|\| (zFile[0] && zFile[1] == ':');   /* the win32 spelling */` |
|    13 |  4293 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|    13 |  4294 | `	PH7_MemObjRelease(pCtx->pRet);` |
|    12 |  4295 | `	if( pVfs && pVfs->xRealpath && pVfs->xRealpath(zFile,pCtx) == PH7_OK` |
|    10 |  4296 | `	 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0` |
|     9 |  4297 | `	 && SyBlobLength(&pCtx->pRet->sBlob) > 0 ){` |
|     9 |  4298 | `		SyBlobAppend(pOut,SyBlobData(&pCtx->pRet->sBlob),SyBlobLength(&pCtx->pRet->sBlob));` |
|     5 |  4299 | `	}else{` |
|     - |  4300 | `		/* Not there to canonicalize -- but php still resolves the part that IS` |
|     - |  4301 | `		 * there (expand_filepath walks each existing component through its` |
|     - |  4302 | `		 * links), so a missing file under a linked directory is named by the` |
|     - |  4303 | `		 * directory's real path; on macOS every temp path is one (/var ->` |
|     - |  4304 | `		 * /private/var). The longest existing prefix is resolved and the missing` |
|     - |  4305 | `		 * tail kept as written. A bare drive ("C:") is never a prefix: it would` |
|     - |  4306 | `		 * resolve to that drive's working directory. */` |
|     - |  4307 | `		SyBlob sFull,sPre;` |
|     - |  4308 | `		const char *zFull;` |
|     5 |  4309 | `		sxu32 nFull,i,nTail = 0;` |
|     5 |  4310 | `		SyBlobInit(&sFull,&pCtx->pVm->sAllocator);` |
|     5 |  4311 | `		SyBlobInit(&sPre,&pCtx->pVm->sAllocator);` |
|     5 |  4312 | `		if( !bAbs ){` |
|     - |  4313 | `			SyBlob sDir;` |
|   ! 0 |  4314 | `			DomCwdUri(pCtx,&sDir);` |
|   ! 0 |  4315 | `			SyBlobAppend(&sFull,SyBlobData(&sDir),SyBlobLength(&sDir));` |
|   ! 0 |  4316 | `			SyBlobRelease(&sDir);` |
|   ! 0 |  4317 | `		}` |
|     5 |  4318 | `		SyBlobAppend(&sFull,zFile,(sxu32)SyStrlen(zFile));` |
|     5 |  4319 | `		SyBlobNullAppend(&sFull);` |
|     5 |  4320 | `		zFull = (const char *)SyBlobData(&sFull);` |
|     5 |  4321 | `		nFull = (sxu32)SyStrlen(zFull);` |
|    37 |  4322 | `		for( i = nFull ; i > 1 && pVfs && pVfs->xRealpath ; --i ){` |
|    37 |  4323 | `			if( (zFull[i-1] != '/' && zFull[i-1] != '\\') \|\| zFull[i-2] == ':' ){` |
|    33 |  4324 | `				continue;` |
|     - |  4325 | `			}` |
|     5 |  4326 | `			SyBlobReset(&sPre);` |
|     5 |  4327 | `			SyBlobAppend(&sPre,zFull,i-1);` |
|     5 |  4328 | `			SyBlobNullAppend(&sPre);` |
|     5 |  4329 | `			PH7_MemObjRelease(pCtx->pRet);` |
|     4 |  4330 | `			if( pVfs->xRealpath((const char *)SyBlobData(&sPre),pCtx) == PH7_OK` |
|     4 |  4331 | `			 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0` |
|     5 |  4332 | `			 && SyBlobLength(&pCtx->pRet->sBlob) > 0 ){` |
|     5 |  4333 | `				SyBlobAppend(pOut,SyBlobData(&pCtx->pRet->sBlob),SyBlobLength(&pCtx->pRet->sBlob));` |
|     5 |  4334 | `				nTail = nFull - (i-1);` |
|     5 |  4335 | `				break;` |
|     - |  4336 | `			}` |
|   ! 0 |  4337 | `		}` |
|     5 |  4338 | `		if( nTail > 0 ){` |
|     5 |  4339 | `			SyBlobAppend(pOut,zFull + (nFull - nTail),nTail);` |
|     3 |  4340 | `		}else{` |
|   ! 0 |  4341 | `			SyBlobAppend(pOut,zFull,nFull);` |
|     - |  4342 | `		}` |
|     5 |  4343 | `		SyBlobRelease(&sPre);` |
|     5 |  4344 | `		SyBlobRelease(&sFull);` |
|     - |  4345 | `	}` |
|    13 |  4346 | `	PH7_MemObjRelease(pCtx->pRet);` |
|    13 |  4347 | `	SyBlobNullAppend(pOut);` |
|    13 |  4348 | `}` |
|     - |  4349 | `/*` |
|     - |  4350 | ` * Point the receiver at a freshly parsed tree, as both load methods do: the` |
|     - |  4351 | ` * document object keeps its identity and everything under the OLD tree becomes` |
|     - |  4352 | ` * stale, so the per-document wrapper cache is dropped with it. Answers 0 when` |
|     - |  4353 | `` * there is no tree to install, which is each method's `false`; the previous`` |
|     - |  4354 | ` * tree is left alone in that case, as php leaves it.` |
|     - |  4355 | ` */` |
|  1588 |  4356 | `static int DomInstallParsed(ph7_context *pCtx,ph7_class_instance *pThis,xmlDocPtr pDoc)` |
|     5 |  4357 | `{` |
|  1593 |  4358 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1593 |  4359 | `	phl_xmldoc *pShell = pDoc ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;` |
|  1593 |  4360 | `	phl_domnode *pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|     - |  4361 | `	ph7_value *pNodes;` |
|  1593 |  4362 | `	if( pRes == 0 ){` |
|    15 |  4363 | `		if( pDoc && pShell == 0 ){` |
|   ! 0 |  4364 | `			xmlFreeDoc(pDoc);` |
|   ! 0 |  4365 | `		}` |
|    15 |  4366 | `		return 0;` |
|     - |  4367 | `	}` |
|  1579 |  4368 | `	DomSetRes(pVm,pThis,pRes);` |
|  1579 |  4369 | `	pNodes = PH7_NativeAttr(pThis,DOM_NODES);` |
|  1579 |  4370 | `	if( pNodes ){` |
|  1579 |  4371 | `		PH7_MemObjRelease(pNodes);` |
|  1579 |  4372 | `		PH7_MemObjToHashmap(pNodes);` |
|   787 |  4373 | `	}` |
|  1579 |  4374 | `	return 1;` |
|   799 |  4375 | `}` |
|     - |  4376 | `/* DOMDocument::loadXML(string $source, int $options = 0): bool -- the receiver` |
|     - |  4377 | ` * is REPOINTED at a new tree, so its identity cache is dropped with it. */` |
|  1564 |  4378 | `DOM_METHOD(vm_builtin_DOMDocument_loadXML)` |
|     5 |  4379 | `{` |
|  1569 |  4380 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1569 |  4381 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  1569 |  4382 | `	int nLen = 0;` |
|  1569 |  4383 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";` |
|  1569 |  4384 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     - |  4385 | `	phl_dom_errsave sErr;` |
|     - |  4386 | `	xmlDocPtr pDoc;` |
|     - |  4387 | `	sxu32 nMark;` |
|  1569 |  4388 | `	if( pThis == 0 ){` |
|   ! 0 |  4389 | `		return PH7_OK;` |
|     - |  4390 | `	}` |
|  1569 |  4391 | `	if( nLen < 1 ){` |
|     3 |  4392 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4393 | `			"DOMDocument::loadXML(): Argument #1 ($source) must not be empty");` |
|     - |  4394 | `	}` |
|  1567 |  4395 | `	iOpts = DomParseOptions(pThis,iOpts);` |
|  1567 |  4396 | `	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);` |
|  1567 |  4397 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|  1567 |  4398 | `	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);` |
|  1567 |  4399 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,"DOMDocument::loadXML",iOpts);` |
|  1567 |  4400 | `	DomRestoreWarnings(pVm,sErr);` |
|  1567 |  4401 | `	DomStampCwd(pCtx,pDoc);` |
|  1567 |  4402 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|  1567 |  4403 | `	return PH7_OK;` |
|   787 |  4404 | `}` |
|     - |  4405 | `/*` |
|     - |  4406 | ` * DOMDocument::load(string $filename, int $options = 0): bool` |
|     - |  4407 | ` *` |
|     - |  4408 | ` * The same parse as loadXML from a FILE, and php reads that file through its` |
|     - |  4409 | ` * own stream layer (which is what makes a wrapper and a userland stream valid` |
|     - |  4410 | ` * destinations there, and what this does too) while letting libxml word the` |
|     - |  4411 | ` * failure. What only a differential decides:` |
|     - |  4412 | ` *` |
|     - |  4413 | `` *   * a file that is not THERE is libxml's own `I/O warning : failed to load`` |
|     - |  4414 | `` *     external entity "<path>"` and nothing else, while one that exists and`` |
|     - |  4415 | ` *     cannot be opened ALSO gets php's stream warning in front of it;` |
|     - |  4416 | ``  *   * the document's URI is the RESOLVED absolute path -- so `load('a/../b.xml')` `` |
|     - |  4417 | ` *     answers the canonical name -- and it is a URI, not a path: a space in it` |
|     - |  4418 | `` *     comes back as `%20`;`` |
|     - |  4419 | ` *   * a failed load leaves the receiver's previous tree exactly where it was.` |
|     - |  4420 | ` */` |
|     - |  4421 | `/*` |
|     - |  4422 | ` * Read a file for one of the two file-loading methods: the bytes into *pBody,` |
|     - |  4423 | ` * the name libxml is to know it by into *pPath. Answers 0 when the file could` |
|     - |  4424 | ` * not be read at all, with php's diagnostics already raised -- the receiver is` |
|     - |  4425 | ` * left alone then, and the method answers false.` |
|     - |  4426 | ` */` |
|     - |  4427 | `static int DomReadFileAs(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|     - |  4428 | `	SyBlob *pBody,SyBlob *pPath,int bVerbatim);` |
|    12 |  4429 | `static int DomReadFile(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|     - |  4430 | `	SyBlob *pBody,SyBlob *pPath)` |
|     1 |  4431 | `{` |
|    13 |  4432 | `	return DomReadFileAs(pCtx,zFile,nFile,zFn,pBody,pPath,0);` |
|     1 |  4433 | `}` |
|     - |  4434 | `/*` |
|     - |  4435 | ` * bVerbatim: libxml is to know the file by the path AS WRITTEN rather than by` |
|     - |  4436 | ` * its canonical absolute name -- loadHTMLFile's rule (see its call).` |
|     - |  4437 | ` */` |
|    18 |  4438 | `static int DomReadFileAs(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|     - |  4439 | `	SyBlob *pBody,SyBlob *pPath,int bVerbatim)` |
|     1 |  4440 | `{` |
|    19 |  4441 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  4442 | `	const ph7_io_stream *pStream;` |
|     - |  4443 | `	void *pHandle;` |
|    19 |  4444 | `	if( bVerbatim ){` |
|     7 |  4445 | `		SyBlobInit(pPath,&pVm->sAllocator);` |
|     7 |  4446 | `		SyBlobAppend(pPath,zFile,(sxu32)nFile);` |
|     7 |  4447 | `		SyBlobNullAppend(pPath);` |
|     4 |  4448 | `	}else{` |
|    13 |  4449 | `		DomAbsPath(pCtx,zFile,pPath);` |
|     - |  4450 | `	}` |
|    19 |  4451 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|    19 |  4452 | `	pHandle = (pStream && pStream->xRead) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|    18 |  4453 | `		PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx)) : 0;` |
|    19 |  4454 | `	if( pHandle == 0 ){` |
|     - |  4455 | `		/* php's stream layer says nothing about a file that is simply absent --` |
|     - |  4456 | `		 * only libxml does, in its own words and with no source location. A file` |
|     - |  4457 | `		 * that IS there and would not open (a mode, a lock) gets both. */` |
|     7 |  4458 | `		const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|     - |  4459 | `		SyBlob sMsg;` |
|     - |  4460 | `		sxu32 nMark;` |
|     7 |  4461 | `		if( pVfs && pVfs->xFileExists && pVfs->xFileExists(zFile) == PH7_OK ){` |
|   ! 0 |  4462 | `			VfsThrowOpenWarning(pCtx,zFile);` |
|   ! 0 |  4463 | `		}` |
|     7 |  4464 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     7 |  4465 | `		SyBlobFormat(&sMsg,"failed to load external entity \"%s\"\n",` |
|     6 |  4466 | `			(const char *)SyBlobData(pPath));` |
|     7 |  4467 | `		SyBlobNullAppend(&sMsg);` |
|     - |  4468 | `		/* Both of libxml's channels, as php feeds them: the structured copy is` |
|     - |  4469 | ``		 * what `libxml_get_errors()`/`libxml_get_last_error()` answer (level`` |
|     - |  4470 | `		 * WARNING, no file, no line), and the generic one is the text that gets` |
|     - |  4471 | `		 * PRINTED -- with the severity spelled into it and at E_WARNING. */` |
|     7 |  4472 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    10 |  4473 | `		PH7_LibxmlQueueError(pVm,XML_ERR_WARNING,XML_IO_LOAD_ERROR,0,0,` |
|     6 |  4474 | `			(const char *)SyBlobData(&sMsg),0);` |
|     7 |  4475 | `		if( pVm->bLibxmlInternalErr ){` |
|     3 |  4476 | `			xmlSetStructuredErrorFunc(0,0);   /* nothing to drain: it stays queued */` |
|     2 |  4477 | `		}else{` |
|     - |  4478 | `			SyBlob sGen;` |
|     5 |  4479 | `			PH7_LibxmlDropErrors(pVm,nMark);` |
|     5 |  4480 | `			SyBlobInit(&sGen,&pVm->sAllocator);` |
|     5 |  4481 | `			SyBlobFormat(&sGen,"I/O warning : %s",(const char *)SyBlobData(&sMsg));` |
|     5 |  4482 | `			SyBlobNullAppend(&sGen);` |
|     5 |  4483 | `			PH7_LibxmlRaiseGeneric(pVm,zFn,(const char *)SyBlobData(&sGen));` |
|     5 |  4484 | `			SyBlobRelease(&sGen);` |
|     - |  4485 | `		}` |
|     7 |  4486 | `		SyBlobRelease(&sMsg);` |
|     7 |  4487 | `		SyBlobRelease(pPath);` |
|     7 |  4488 | `		return 0;` |
|     - |  4489 | `	}` |
|    13 |  4490 | `	SyBlobInit(pBody,&pVm->sAllocator);` |
|    13 |  4491 | `	PH7_StreamReadWholeFile(pHandle,pStream,pBody);` |
|    13 |  4492 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    13 |  4493 | `	return 1;` |
|    10 |  4494 | `}` |
|    16 |  4495 | `DOM_METHOD(vm_builtin_DOMDocument_load)` |
|     1 |  4496 | `{` |
|    17 |  4497 | `	ph7_vm *pVm = pCtx->pVm;` |
|    17 |  4498 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  4499 | `	const char *zFile;` |
|    17 |  4500 | `	int nFile = 0;` |
|    17 |  4501 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     - |  4502 | `	phl_dom_errsave sErr;` |
|     - |  4503 | `	SyBlob sBody,sPath;` |
|     - |  4504 | `	xmlDocPtr pDoc;` |
|     - |  4505 | `	sxu32 nMark;` |
|    17 |  4506 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|    17 |  4507 | `	if( pThis == 0 ){` |
|   ! 0 |  4508 | `		return PH7_OK;` |
|     - |  4509 | `	}` |
|    17 |  4510 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|     3 |  4511 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4512 | `			"DOMDocument::load(): Argument #1 ($filename) must not contain any null bytes");` |
|     - |  4513 | `	}` |
|    15 |  4514 | `	if( nFile < 1 ){` |
|     3 |  4515 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4516 | `			"DOMDocument::load(): Argument #1 ($filename) must not be empty");` |
|     - |  4517 | `	}` |
|    13 |  4518 | `	if( !DomReadFile(pCtx,zFile,nFile,"DOMDocument::load",&sBody,&sPath) ){` |
|     5 |  4519 | `		ph7_result_bool(pCtx,0);` |
|     5 |  4520 | `		return PH7_OK;` |
|     - |  4521 | `	}` |
|     9 |  4522 | `	if( SyBlobLength(&sBody) < 1 ){` |
|     - |  4523 | `		/* libxml's memory parser will not even start on nothing, so the error` |
|     - |  4524 | `		 * php's FILE parser raises there is queued by hand -- same level, same` |
|     - |  4525 | ``		 * code, same wording, so `libxml_get_errors()` reports what php's does`` |
|     - |  4526 | `		 * and the drain prints php's sentence. */` |
|     3 |  4527 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     4 |  4528 | `		PH7_LibxmlQueueError(pVm,XML_ERR_FATAL,XML_ERR_DOCUMENT_EMPTY,1,1,` |
|     2 |  4529 | `			"Document is empty\n",(const char *)SyBlobData(&sPath));` |
|     3 |  4530 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::load");` |
|     3 |  4531 | `		SyBlobRelease(&sBody);` |
|     3 |  4532 | `		SyBlobRelease(&sPath);` |
|     3 |  4533 | `		ph7_result_bool(pCtx,0);` |
|     3 |  4534 | `		return PH7_OK;` |
|     - |  4535 | `	}` |
|     7 |  4536 | `	iOpts = DomParseOptions(pThis,iOpts);` |
|     7 |  4537 | `	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);` |
|     7 |  4538 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     - |  4539 | `	/* The path is the parse's URL: libxml turns it into the document's URI and` |
|     - |  4540 | `	 * names it in every diagnostic the parse raises. */` |
|    10 |  4541 | `	pDoc = xmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),` |
|     6 |  4542 | `		(const char *)SyBlobData(&sPath),0,iOpts);` |
|     7 |  4543 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,"DOMDocument::load",iOpts);` |
|     7 |  4544 | `	DomRestoreWarnings(pVm,sErr);` |
|     7 |  4545 | `	SyBlobRelease(&sBody);` |
|     7 |  4546 | `	SyBlobRelease(&sPath);` |
|     7 |  4547 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|     7 |  4548 | `	return PH7_OK;` |
|     9 |  4549 | `}` |
|     - |  4550 | `/*` |
|     - |  4551 | ` * DOMDocument::loadHTML(string $source, int $options = 0): bool` |
|     - |  4552 | ` * DOMDocument::loadHTMLFile(string $filename, int $options = 0): bool` |
|     - |  4553 | ` *` |
|     - |  4554 | ` * The other parser: HTML is not XML and libxml has a second one for it, which` |
|     - |  4555 | `` * closes what the markup left open, supplies the `html`/`body` php's`` |
|     - |  4556 | `` * `LIBXML_HTML_NOIMPLIED` asks it not to, and stamps the DTD`` |
|     - |  4557 | `` * `LIBXML_HTML_NODEFDTD` asks it not to. What comes out is an HTML DOCUMENT --`` |
|     - |  4558 | ` * node type 13, its own serializer -- and the differences from the XML side are` |
|     - |  4559 | ` * measured ones: the document's own directives reach NOTHING here (only` |
|     - |  4560 | `` * `$options` does), a document parsed from a STRING is given no URI at all`` |
|     - |  4561 | ` * (where loadXML stamps the working directory), and there is no well-formedness` |
|     - |  4562 | ` * to fail on, so the answer is true for anything that is not empty.` |
|     - |  4563 | ` */` |
|    26 |  4564 | `static int DomLoadHtml(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile)` |
|     1 |  4565 | `{` |
|    27 |  4566 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  4567 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    27 |  4568 | `	const char *zFn = bFile ? "DOMDocument::loadHTMLFile" : "DOMDocument::loadHTML";` |
|    27 |  4569 | `	const char *zArg = bFile ? "filename" : "source";` |
|     - |  4570 | `	const char *zSrc;` |
|    27 |  4571 | `	int nSrc = 0;` |
|    27 |  4572 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     - |  4573 | `	SyBlob sBody,sPath;` |
|     - |  4574 | `	xmlDocPtr pDoc;` |
|     - |  4575 | `	sxu32 nMark;` |
|    27 |  4576 | `	zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|    27 |  4577 | `	if( pThis == 0 ){` |
|   ! 0 |  4578 | `		return PH7_OK;` |
|     - |  4579 | `	}` |
|    27 |  4580 | `	if( bFile && nSrc != (int)SyStrlen(zSrc) ){` |
|   ! 0 |  4581 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|   ! 0 |  4582 | `			"%s(): Argument #1 ($filename) must not contain any null bytes",zFn);` |
|     - |  4583 | `	}` |
|    27 |  4584 | `	if( nSrc < 1 ){` |
|     7 |  4585 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     2 |  4586 | `			"%s(): Argument #1 ($%s) must not be empty",zFn,zArg);` |
|     - |  4587 | `	}` |
|    23 |  4588 | `	if( bFile ){` |
|     - |  4589 | `		/* loadHTMLFile hands libxml the path AS WRITTEN -- php's` |
|     - |  4590 | `		 * htmlCreateFileParserCtxt(source): no working directory joined and no` |
|     - |  4591 | `		 * link resolved, unlike load(), which canonicalizes first. It is the name` |
|     - |  4592 | `		 * the document's URI and every diagnostic carry. */` |
|     7 |  4593 | `		if( !DomReadFileAs(pCtx,zSrc,nSrc,zFn,&sBody,&sPath,1) ){` |
|     3 |  4594 | `			ph7_result_bool(pCtx,0);` |
|     3 |  4595 | `			return PH7_OK;` |
|     - |  4596 | `		}` |
|     3 |  4597 | `	}else{` |
|     - |  4598 | `		/* A string has no URI: php leaves the document's null. */` |
|    17 |  4599 | `		SyBlobInit(&sBody,&pVm->sAllocator);` |
|    17 |  4600 | `		SyBlobAppend(&sBody,zSrc,(sxu32)nSrc);` |
|    17 |  4601 | `		SyBlobInit(&sPath,&pVm->sAllocator);` |
|    17 |  4602 | `		SyBlobNullAppend(&sPath);` |
|     - |  4603 | `	}` |
|    21 |  4604 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    21 |  4605 | `	if( SyBlobLength(&sBody) > 0 ){` |
|    28 |  4606 | `		pDoc = htmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),` |
|     9 |  4607 | `			bFile ? (const char *)SyBlobData(&sPath) : 0,0,iOpts);` |
|    10 |  4608 | `	}else{` |
|     - |  4609 | `		/* An empty FILE is still a document here, unlike on the XML side: php's` |
|     - |  4610 | ``		 * HTML parser says `Document is empty` and hands back the DTD-only`` |
|     - |  4611 | ``		 * document `htmlNewDoc` builds. libxml's memory parser will not start on`` |
|     - |  4612 | `		 * nothing, so both halves are made by hand. (An empty STRING never gets` |
|     - |  4613 | `		 * this far -- it is the ValueError above.) */` |
|     4 |  4614 | `		PH7_LibxmlQueueError(pVm,XML_ERR_ERROR,XML_ERR_DOCUMENT_EMPTY,1,1,` |
|     2 |  4615 | `			"Document is empty\n",(const char *)SyBlobData(&sPath));` |
|     3 |  4616 | `		pDoc = htmlNewDoc(0,0);` |
|     3 |  4617 | `		if( pDoc ){` |
|     3 |  4618 | `			pDoc->URL = xmlStrdup((const xmlChar *)SyBlobData(&sPath));` |
|     1 |  4619 | `		}` |
|     - |  4620 | `	}` |
|    21 |  4621 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFn,iOpts);` |
|    21 |  4622 | `	SyBlobRelease(&sBody);` |
|    21 |  4623 | `	SyBlobRelease(&sPath);` |
|    21 |  4624 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|    21 |  4625 | `	return PH7_OK;` |
|    14 |  4626 | `}` |
|    18 |  4627 | `DOM_METHOD(vm_builtin_DOMDocument_loadHTML)` |
|     1 |  4628 | `{` |
|    19 |  4629 | `	return DomLoadHtml(pCtx,nArg,apArg,FALSE);` |
|     1 |  4630 | `}` |
|     8 |  4631 | `DOM_METHOD(vm_builtin_DOMDocument_loadHTMLFile)` |
|     1 |  4632 | `{` |
|     9 |  4633 | `	return DomLoadHtml(pCtx,nArg,apArg,TRUE);` |
|     1 |  4634 | `}` |
|     - |  4635 | `/*` |
|     - |  4636 | ` * The two save options php reads, and what they mean to libxml.` |
|     - |  4637 | ` *` |
|     - |  4638 | `` * `LIBXML_NOEMPTYTAG` turns `<e/>` into `<e></e>` and reaches BOTH dumps -- a`` |
|     - |  4639 | `` * node's as much as a document's -- while `LIBXML_NOXMLDECL` only reaches the`` |
|     - |  4640 | ` * whole-document one (a node's output has no declaration to drop). Every other` |
|     - |  4641 | `` * bit of `$options` is ignored, unknown ones included. php spells the first one`` |
|     - |  4642 | ` * with libxml's library-wide switch; this file asks for it per dump instead,` |
|     - |  4643 | ` * which says the same thing without touching global state (and without the` |
|     - |  4644 | ` * deprecated symbol: the MSVC gate refuses it under /WX).` |
|     - |  4645 | ` *` |
|     - |  4646 | ` * Both dumps therefore run through libxml's save API. The DOCUMENT's goes out` |
|     - |  4647 | ` * in the encoding its declaration names -- which is also how a document whose` |
|     - |  4648 | ` * encoding has no converter fails, with no context to write through -- and a` |
|     - |  4649 | ` * NODE's is always UTF-8, as php's is.` |
|     - |  4650 | ` */` |
|     - |  4651 | `#define DOM_SAVE_NOXMLDECL  2` |
|     - |  4652 | `#define DOM_SAVE_NOEMPTYTAG 4` |
|  1090 |  4653 | `static int DomSaveFlags(int bFormat,int iOpts,int bDoc)` |
|     2 |  4654 | `{` |
|     - |  4655 | ``	/* AS_XML because the receiver may be an HTML document (`loadHTML` makes`` |
|     - |  4656 | `	 * one): libxml's save context would hand such a document to the HTML` |
|     - |  4657 | `	 * serializer, and php's XML savers write XML whatever the document is --` |
|     - |  4658 | ``	 * declaration, `<br/>` and all. */`` |
|  1092 |  4659 | `	int iSave = XML_SAVE_AS_XML \| (bFormat ? XML_SAVE_FORMAT : 0);` |
|  1092 |  4660 | `	if( iOpts & DOM_SAVE_NOEMPTYTAG ){` |
|    13 |  4661 | `		iSave \|= XML_SAVE_NO_EMPTY;` |
|     6 |  4662 | `	}` |
|  1092 |  4663 | `	if( bDoc && (iOpts & DOM_SAVE_NOXMLDECL) ){` |
|     5 |  4664 | `		iSave \|= XML_SAVE_NO_DECL;` |
|     2 |  4665 | `	}` |
|  1092 |  4666 | `	return iSave;` |
|     2 |  4667 | `}` |
|     - |  4668 | `/*` |
|     - |  4669 | ` * Serialize a whole document (pNode == 0) or one node the way php's savers do.` |
|     - |  4670 | ` * Answers the bytes in *pzOut (xmlFree'd by the caller) and their count, or -1.` |
|     - |  4671 | ` */` |
|  1090 |  4672 | `static int DomDumpTree(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,int iOpts,xmlChar **pzOut)` |
|     2 |  4673 | `{` |
|  1092 |  4674 | `	xmlBufferPtr pBuf = xmlBufferCreate();` |
|     - |  4675 | `	xmlSaveCtxtPtr pSave;` |
|  1092 |  4676 | `	int nOut = 0;` |
|  1092 |  4677 | `	*pzOut = 0;` |
|  1092 |  4678 | `	if( pBuf == 0 ){` |
|   ! 0 |  4679 | `		return -1;` |
|     - |  4680 | `	}` |
|     - |  4681 | `	/* A NODE's dump is UTF-8 whatever the document declares, and naming that` |
|     - |  4682 | `	 * encoding is also what keeps libxml from ESCAPING every non-ASCII character` |
|     - |  4683 | ``	 * (its no-encoding path writes `&#xE9;`, which is right for a document that`` |
|     - |  4684 | `	 * declares nothing and wrong for a node). A DOCUMENT's goes out in its own` |
|     - |  4685 | `	 * declared encoding, or in that escaping form when it declares none -- which` |
|     - |  4686 | `	 * is what php answers there. */` |
|  1092 |  4687 | `	pSave = xmlSaveToBuffer(pBuf,pNode ? "UTF-8" : (const char *)pDoc->encoding,` |
|   545 |  4688 | `		DomSaveFlags(bFormat,iOpts,pNode == 0));` |
|  1092 |  4689 | `	if( pSave == 0 ){` |
|   ! 0 |  4690 | `		xmlBufferFree(pBuf);` |
|   ! 0 |  4691 | `		return -1;` |
|     - |  4692 | `	}` |
|  1092 |  4693 | `	if( (pNode ? xmlSaveTree(pSave,pNode) : xmlSaveDoc(pSave,pDoc)) < 0 ){` |
|   ! 0 |  4694 | `		nOut = -1;` |
|   ! 0 |  4695 | `	}` |
|  1092 |  4696 | `	if( xmlSaveClose(pSave) < 0 ){` |
|   ! 0 |  4697 | `		nOut = -1;` |
|   ! 0 |  4698 | `	}` |
|  1092 |  4699 | `	if( nOut == 0 ){` |
|  1092 |  4700 | `		nOut = (int)xmlBufferLength(pBuf);` |
|  1092 |  4701 | `		*pzOut = xmlStrndup(xmlBufferContent(pBuf),nOut);` |
|  1092 |  4702 | `		if( *pzOut == 0 ){` |
|   ! 0 |  4703 | `			nOut = -1;` |
|   ! 0 |  4704 | `		}` |
|   545 |  4705 | `	}` |
|  1092 |  4706 | `	xmlBufferFree(pBuf);` |
|  1092 |  4707 | `	return nOut;` |
|   547 |  4708 | `}` |
|     - |  4709 | `/* DOMDocument::saveXML(?DOMNode $node = null, int $options = 0): string\|false */` |
|  1080 |  4710 | `DOM_METHOD(vm_builtin_DOMDocument_saveXML)` |
|     2 |  4711 | `{` |
|  1082 |  4712 | `	ph7_vm *pVm = pCtx->pVm;` |
|  1082 |  4713 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  1082 |  4714 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|  1082 |  4715 | `	phl_domnode *pTgt = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|  1082 |  4716 | `	int bFormat = pThis && PH7_NativeAttrTruthy(pThis,"formatOutput");` |
|  1082 |  4717 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     - |  4718 | `	int bWhole;` |
|  1082 |  4719 | `	xmlChar *zOut = 0;` |
|     - |  4720 | `	int nOut;` |
|     - |  4721 | `	sxu32 nMark;` |
|  1082 |  4722 | `	if( pDocNd == 0 ){` |
|   ! 0 |  4723 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  4724 | `		return PH7_OK;` |
|     - |  4725 | `	}` |
|  1080 |  4726 | `	if( pTgt && pTgt->pNode` |
|   686 |  4727 | `	 && ((xmlNodePtr)pTgt->pNode)->doc != (xmlDocPtr)pDocNd->pNode ){` |
|     - |  4728 | `		/* Another document's node -- or a constructed one that belongs to none` |
|     - |  4729 | `		 * yet -- is not this document's to serialize. */` |
|     5 |  4730 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|     - |  4731 | `	}` |
|  1078 |  4732 | `	bWhole = pTgt == 0 \|\| pTgt->pNode == pDocNd->pNode;` |
|  1078 |  4733 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|  1078 |  4734 | `	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,bWhole ? 0 : (xmlNodePtr)pTgt->pNode,` |
|   538 |  4735 | `		bFormat,iOpts,&zOut);` |
|  1078 |  4736 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|  1078 |  4737 | `	if( nOut < 0 ){` |
|     - |  4738 | `		/* php says so rather than answering an empty document: the encoding the` |
|     - |  4739 | `		 * declaration names has no converter and nothing was written. */` |
|   ! 0 |  4740 | `		if( bWhole ){` |
|   ! 0 |  4741 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Could not save document");` |
|   ! 0 |  4742 | `		}` |
|   ! 0 |  4743 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  4744 | `		return PH7_OK;` |
|     - |  4745 | `	}` |
|  1078 |  4746 | `	ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|  1078 |  4747 | `	xmlFree(zOut);` |
|  1078 |  4748 | `	return PH7_OK;` |
|   542 |  4749 | `}` |
|     - |  4750 | `/*` |
|     - |  4751 | ` * DOMDocument::saveHTML(?DOMNode $node = null): string\|false` |
|     - |  4752 | ` * DOMDocument::saveHTMLFile(string $filename): int\|false` |
|     - |  4753 | ` *` |
|     - |  4754 | ` * The HTML serializer, which is a different one: a void element comes out` |
|     - |  4755 | `` * `<br>` rather than `<br/>`, a character with an HTML entity name comes out`` |
|     - |  4756 | ` * under that name, and the whole document carries its DOCTYPE and no XML` |
|     - |  4757 | ` * declaration. Neither method takes save OPTIONS -- php declares one parameter` |
|     - |  4758 | `` * each -- but both read `formatOutput`, a NODE's dump included.`` |
|     - |  4759 | ` *` |
|     - |  4760 | ` * A node from ANOTHER document is php's Wrong Document Error (in whichever mode` |
|     - |  4761 | ` * this document is in), which is the only refusal either one has.` |
|     - |  4762 | ` */` |
|    26 |  4763 | `static int DomDumpHtml(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,xmlChar **pzOut)` |
|     1 |  4764 | `{` |
|     - |  4765 | `	xmlBufferPtr pBuf;` |
|     - |  4766 | `	xmlOutputBufferPtr pOut;` |
|     - |  4767 | `	int nOut;` |
|    27 |  4768 | `	*pzOut = 0;` |
|    27 |  4769 | `	if( pNode == 0 ){` |
|    23 |  4770 | `		nOut = 0;` |
|    23 |  4771 | `		htmlDocDumpMemoryFormat(pDoc,pzOut,&nOut,bFormat ? 1 : 0);` |
|    23 |  4772 | `		return *pzOut ? nOut : -1;` |
|     - |  4773 | `	}` |
|     5 |  4774 | `	pBuf = xmlBufferCreate();` |
|     - |  4775 | `	/* The buffer is the write TARGET, not the output buffer's own storage:` |
|     - |  4776 | `	 * closing the latter leaves it to us to free. */` |
|     5 |  4777 | `	pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|     5 |  4778 | `	if( pOut == 0 ){` |
|   ! 0 |  4779 | `		if( pBuf ){` |
|   ! 0 |  4780 | `			xmlBufferFree(pBuf);` |
|   ! 0 |  4781 | `		}` |
|   ! 0 |  4782 | `		return -1;` |
|     - |  4783 | `	}` |
|     5 |  4784 | `	htmlNodeDumpFormatOutput(pOut,pDoc,pNode,0,bFormat ? 1 : 0);` |
|     5 |  4785 | `	xmlOutputBufferFlush(pOut);` |
|     5 |  4786 | `	nOut = (int)xmlBufferLength(pBuf);` |
|     5 |  4787 | `	*pzOut = xmlStrndup(xmlBufferContent(pBuf),nOut);` |
|     5 |  4788 | `	xmlOutputBufferClose(pOut);` |
|     5 |  4789 | `	xmlBufferFree(pBuf);` |
|     5 |  4790 | `	return *pzOut ? nOut : -1;` |
|    14 |  4791 | `}` |
|    32 |  4792 | `static int DomSaveHtml(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile)` |
|     1 |  4793 | `{` |
|    33 |  4794 | `	ph7_vm *pVm = pCtx->pVm;` |
|    33 |  4795 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    33 |  4796 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|    33 |  4797 | `	phl_domnode *pTgt = (!bFile && nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|    33 |  4798 | `	int bFormat = pThis && PH7_NativeAttrTruthy(pThis,"formatOutput");` |
|    33 |  4799 | `	const ph7_io_stream *pStream = 0;` |
|    33 |  4800 | `	const char *zFile = "";` |
|    33 |  4801 | `	int nFile = 0,nOut;` |
|    33 |  4802 | `	xmlChar *zOut = 0;` |
|     - |  4803 | `	void *pHandle;` |
|     - |  4804 | `	sxu32 nMark;` |
|    33 |  4805 | `	if( bFile ){` |
|     7 |  4806 | `		zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|     7 |  4807 | `		if( nFile != (int)SyStrlen(zFile) ){` |
|   ! 0 |  4808 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4809 | `				"DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not contain any null bytes");` |
|     - |  4810 | `		}` |
|     7 |  4811 | `		if( nFile < 1 ){` |
|     3 |  4812 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4813 | `				"DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not be empty");` |
|     - |  4814 | `		}` |
|     2 |  4815 | `	}` |
|    31 |  4816 | `	if( pDocNd == 0 ){` |
|   ! 0 |  4817 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  4818 | `		return PH7_OK;` |
|     - |  4819 | `	}` |
|    31 |  4820 | `	if( pTgt && pTgt->pShell != pDocNd->pShell ){` |
|     5 |  4821 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|     - |  4822 | `	}` |
|    27 |  4823 | `	if( bFile ){` |
|     - |  4824 | `		/* Writing to a FILE goes through libxml's file saver, which stamps the` |
|     - |  4825 | `` 		 * document with the encoding it is about to use: an `http-equiv` `` |
|     - |  4826 | ``		 * Content-Type meta appears in `<head>` -- in the DOCUMENT, not just in`` |
|     - |  4827 | ``		 * the output, so the next `saveHTML()` shows it too -- and it always`` |
|     - |  4828 | `		 * says UTF-8, whatever the document's own encoding is. php inherits` |
|     - |  4829 | `		 * that; the string saver READS the same meta and adds none. */` |
|     5 |  4830 | `		htmlSetMetaEncoding((xmlDocPtr)pDocNd->pNode,(const xmlChar *)"UTF-8");` |
|     2 |  4831 | `	}` |
|    27 |  4832 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    42 |  4833 | `	nOut = DomDumpHtml((xmlDocPtr)pDocNd->pNode,` |
|    15 |  4834 | `		(pTgt && pTgt->pNode != pDocNd->pNode) ? (xmlNodePtr)pTgt->pNode : 0,bFormat,&zOut);` |
|    27 |  4835 | `	PH7_LibxmlDropErrors(pVm,nMark);` |
|    27 |  4836 | `	if( nOut < 0 ){` |
|   ! 0 |  4837 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  4838 | `		return PH7_OK;` |
|     - |  4839 | `	}` |
|    27 |  4840 | `	if( !bFile ){` |
|    23 |  4841 | `		ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|    23 |  4842 | `		xmlFree(zOut);` |
|    23 |  4843 | `		return PH7_OK;` |
|     - |  4844 | `	}` |
|     5 |  4845 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|     5 |  4846 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|     - |  4847 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|     4 |  4848 | `		ph7_function_name(pCtx)) : 0;` |
|     5 |  4849 | `	if( pHandle == 0 ){` |
|     3 |  4850 | `		xmlFree(zOut);` |
|     3 |  4851 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     - |  4852 | `		/* php answers the bytes it WROTE, which is none of them -- not false. */` |
|     3 |  4853 | `		ph7_result_int(pCtx,0);` |
|     3 |  4854 | `		return PH7_OK;` |
|     - |  4855 | `	}` |
|     3 |  4856 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|   ! 0 |  4857 | `		nOut = 0;` |
|   ! 0 |  4858 | `	}` |
|     3 |  4859 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     3 |  4860 | `	xmlFree(zOut);` |
|     3 |  4861 | `	ph7_result_int(pCtx,nOut);` |
|     3 |  4862 | `	return PH7_OK;` |
|    17 |  4863 | `}` |
|    26 |  4864 | `DOM_METHOD(vm_builtin_DOMDocument_saveHTML)` |
|     1 |  4865 | `{` |
|    27 |  4866 | `	return DomSaveHtml(pCtx,nArg,apArg,FALSE);` |
|     1 |  4867 | `}` |
|     6 |  4868 | `DOM_METHOD(vm_builtin_DOMDocument_saveHTMLFile)` |
|     1 |  4869 | `{` |
|     7 |  4870 | `	return DomSaveHtml(pCtx,nArg,apArg,TRUE);` |
|     1 |  4871 | `}` |
|     - |  4872 | `/*` |
|     - |  4873 | ` * DOMDocument::save(string $filename, int $options = 0): int\|false` |
|     - |  4874 | ` *` |
|     - |  4875 | ` * saveXML's bytes written to a file, and the COUNT of them rather than the` |
|     - |  4876 | ` * bytes -- through the stream layer, which is where php's` |
|     - |  4877 | `` * `save(<path>): Failed to open stream: <reason>` comes from. Two rules only a`` |
|     - |  4878 | `` * differential decides: `LIBXML_NOXMLDECL` does NOT reach this one (php reads`` |
|     - |  4879 | ` * it in saveXML only, so a saved document always carries its declaration),` |
|     - |  4880 | ``  * and a document whose declared encoding has no converter is a silent `false` `` |
|     - |  4881 | ` * here where saveXML says "Could not save document".` |
|     - |  4882 | ` */` |
|    18 |  4883 | `DOM_METHOD(vm_builtin_DOMDocument_save)` |
|     1 |  4884 | `{` |
|    19 |  4885 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 |  4886 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    19 |  4887 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     - |  4888 | `	const ph7_io_stream *pStream;` |
|     - |  4889 | `	const char *zFile;` |
|    19 |  4890 | `	int nFile = 0;` |
|    19 |  4891 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|    19 |  4892 | `	int bFormat = pThis && PH7_NativeAttrTruthy(pThis,"formatOutput");` |
|     - |  4893 | `	int nOut;` |
|    19 |  4894 | `	xmlChar *zOut = 0;` |
|     - |  4895 | `	void *pHandle;` |
|     - |  4896 | `	sxu32 nMark;` |
|    19 |  4897 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|    19 |  4898 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|     3 |  4899 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4900 | `			"DOMDocument::save(): Argument #1 ($filename) must not contain any null bytes");` |
|     - |  4901 | `	}` |
|    17 |  4902 | `	if( nFile < 1 ){` |
|     3 |  4903 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  4904 | `			"DOMDocument::save(): Argument #1 ($filename) must not be empty");` |
|     - |  4905 | `	}` |
|    15 |  4906 | `	if( pDocNd == 0 ){` |
|   ! 0 |  4907 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  4908 | `		return PH7_OK;` |
|     - |  4909 | `	}` |
|    15 |  4910 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    15 |  4911 | `	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,0,bFormat,iOpts & ~DOM_SAVE_NOXMLDECL,&zOut);` |
|     - |  4912 | `	/* php reports this failure through the return value alone. */` |
|    15 |  4913 | `	PH7_LibxmlDropErrors(pVm,nMark);` |
|    15 |  4914 | `	if( nOut < 0 ){` |
|   ! 0 |  4915 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  4916 | `		return PH7_OK;` |
|     - |  4917 | `	}` |
|    15 |  4918 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|    15 |  4919 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|     - |  4920 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|    14 |  4921 | `		ph7_function_name(pCtx)) : 0;` |
|    15 |  4922 | `	if( pHandle == 0 ){` |
|     3 |  4923 | `		xmlFree(zOut);` |
|     3 |  4924 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     3 |  4925 | `		ph7_result_bool(pCtx,0);` |
|     3 |  4926 | `		return PH7_OK;` |
|     - |  4927 | `	}` |
|    13 |  4928 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|   ! 0 |  4929 | `		nOut = -1;` |
|   ! 0 |  4930 | `	}` |
|    13 |  4931 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    13 |  4932 | `	xmlFree(zOut);` |
|    13 |  4933 | `	if( nOut < 0 ){` |
|   ! 0 |  4934 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  4935 | `		return PH7_OK;` |
|     - |  4936 | `	}` |
|    13 |  4937 | `	ph7_result_int(pCtx,nOut);` |
|    13 |  4938 | `	return PH7_OK;` |
|    10 |  4939 | `}` |
|     - |  4940 | `/*` |
|     - |  4941 | ` * The four DOMDocument::create* methods, which differ only in the node kind` |
|     - |  4942 | ` * they ask libxml for. Fresh nodes start as orphans, so a node that is created` |
|     - |  4943 | ` * and never appended is still freed with its document.` |
|     - |  4944 | ` */` |
|   430 |  4945 | `static int DomDocCreate(ph7_context *pCtx,int iKind,const char *zName,const char *zVal,int nVal)` |
|     3 |  4946 | `{` |
|   433 |  4947 | `	ph7_vm *pVm = pCtx->pVm;` |
|   433 |  4948 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     - |  4949 | `	xmlDocPtr pDoc;` |
|   433 |  4950 | `	xmlNodePtr pNode = 0;` |
|     - |  4951 | `	sxu32 nMark;` |
|   433 |  4952 | `	if( pDocNd == 0 ){` |
|   ! 0 |  4953 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  4954 | `	}` |
|   433 |  4955 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|   433 |  4956 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   433 |  4957 | `	switch( iKind ){` |
|    86 |  4958 | `	case XML_ELEMENT_NODE:` |
|   175 |  4959 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     7 |  4960 | `			break; /* Invalid Character Error */` |
|     - |  4961 | `		}` |
|     - |  4962 | `		/* php passes the value through xmlNewDocNode, which entity-parses` |
|     - |  4963 | `		 * it (quirk preserved: bad entities warn and drop the content). */` |
|   169 |  4964 | `		pNode = xmlNewDocNode(pDoc,0,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|   169 |  4965 | `		break;` |
|    56 |  4966 | `	case XML_TEXT_NODE:` |
|   114 |  4967 | `		pNode = xmlNewDocText(pDoc,(const xmlChar *)zVal);` |
|   114 |  4968 | `		break;` |
|     4 |  4969 | `	case XML_CDATA_SECTION_NODE:` |
|     9 |  4970 | `		pNode = xmlNewCDataBlock(pDoc,(const xmlChar *)zVal,nVal);` |
|     9 |  4971 | `		break;` |
|     5 |  4972 | `	case XML_COMMENT_NODE:` |
|    11 |  4973 | `		pNode = xmlNewDocComment(pDoc,(const xmlChar *)zVal);` |
|    11 |  4974 | `		break;` |
|    10 |  4975 | `	case XML_PI_NODE:` |
|     - |  4976 | `		/* php validates the TARGET the same way it validates an element name,` |
|     - |  4977 | ``		 * so `createProcessingInstruction('a b')` is Invalid Character Error`` |
|     - |  4978 | `		 * rather than a document that will not parse back. */` |
|    21 |  4979 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|    11 |  4980 | `			break;` |
|     - |  4981 | `		}` |
|     - |  4982 | `		/* Empty data stays a NULL content pointer, matching php's node state:` |
|     - |  4983 | ``		 * `<?bare?>` serializes with no separator space, `nodeValue` reads`` |
|     - |  4984 | ``		 * null -- and `data` reads "", because THAT getter coerces. */`` |
|    11 |  4985 | `		pNode = xmlNewDocPI(pDoc,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|    11 |  4986 | `		break;` |
|    12 |  4987 | `	case XML_ENTITY_REF_NODE:` |
|    25 |  4988 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     9 |  4989 | `			break;` |
|     - |  4990 | `		}` |
|    17 |  4991 | `		pNode = xmlNewReference(pDoc,(const xmlChar *)zName);` |
|    17 |  4992 | `		break;` |
|    42 |  4993 | `	case XML_DOCUMENT_FRAG_NODE:` |
|    85 |  4994 | `		pNode = xmlNewDocFragment(pDoc);` |
|    84 |  4995 | `		break;` |
|     - |  4996 | `	}` |
|   648 |  4997 | `	PH7_LibxmlCaptureEnd(pVm,nMark,` |
|   215 |  4998 | `		iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");` |
|   433 |  4999 | `	if( pNode == 0 ){` |
|    25 |  5000 | `		if( iKind == XML_ELEMENT_NODE \|\| iKind == XML_PI_NODE \|\| iKind == XML_ENTITY_REF_NODE ){` |
|     - |  5001 | `			/* The three factories that take a NAME are the three that can be` |
|     - |  5002 | `			 * handed one libxml refuses. */` |
|    25 |  5003 | `			return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|     - |  5004 | `		}` |
|   ! 0 |  5005 | `		ph7_result_null(pCtx);` |
|   ! 0 |  5006 | `		return PH7_OK;` |
|     - |  5007 | `	}` |
|   409 |  5008 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|   409 |  5009 | `	return DomResultNodeOf(pCtx,pDocNd,pNode);` |
|   218 |  5010 | `}` |
|     - |  5011 | `/* DOMDocument::createElement(string $localName, string $value = ''): DOMElement */` |
|   172 |  5012 | `DOM_METHOD(vm_builtin_DOMDocument_createElement)` |
|     3 |  5013 | `{` |
|   175 |  5014 | `	int nVal = 0;` |
|   175 |  5015 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|   175 |  5016 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";` |
|   175 |  5017 | `	return DomDocCreate(pCtx,XML_ELEMENT_NODE,zName,zVal,nVal);` |
|     3 |  5018 | `}` |
|     - |  5019 | `/*` |
|     - |  5020 | ` * DOMDocument::createElementNS(?string $namespace, string $qualifiedName,` |
|     - |  5021 | ` *                              string $value = '')` |
|     - |  5022 | ` *` |
|     - |  5023 | ` * The only way to build a namespaced ELEMENT -- until this existed a program` |
|     - |  5024 | `` * could read a namespaced document and not write one, and `Call to undefined`` |
|     - |  5025 | `` * method` was the answer to the first line of every modern DOM example.`` |
|     - |  5026 | ` *` |
|     - |  5027 | ` * php's rules, measured:` |
|     - |  5028 | ` *` |
|     - |  5029 | ` *   * A NULL namespace is a plain element; an EMPTY-STRING one is not the same` |
|     - |  5030 | `` *     thing, it declares `xmlns=""` on the element and answers `''` for`` |
|     - |  5031 | ` *     namespaceURI. Either with a PREFIXED name is a Namespace Error, since a` |
|     - |  5032 | ` *     prefix names a namespace.` |
|     - |  5033 | ` *   * The declaration lands on the NEW element, always: a fresh node has no` |
|     - |  5034 | ` *     parent, so nothing the document declares elsewhere is in scope yet. What` |
|     - |  5035 | ` *     the document already makes is settled later, when the element is linked` |
|     - |  5036 | ` *     in and the redundant declaration is stripped (DomNsOnInsertEx).` |
|     - |  5037 | ` *   * The $value is not text -- php hands it to libxml, which entity-parses it,` |
|     - |  5038 | `` *     so `&amp;` becomes `&`, an undefined entity is a warning and `<` is`` |
|     - |  5039 | ` *     escaped. The same quirk createElement already carries.` |
|     - |  5040 | ` */` |
|   162 |  5041 | `DOM_METHOD(vm_builtin_DOMDocument_createElementNS)` |
|     1 |  5042 | `{` |
|   163 |  5043 | `	ph7_vm *pVm = pCtx->pVm;` |
|   163 |  5044 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|   163 |  5045 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|   163 |  5046 | `	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|   163 |  5047 | `	int nVal = 0;` |
|   163 |  5048 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],&nVal) : "";` |
|     - |  5049 | `	xmlNodePtr pNode;` |
|     - |  5050 | `	dom_qname sQ;` |
|     - |  5051 | `	sxu32 nMark;` |
|     - |  5052 | `	int rc;` |
|   163 |  5053 | `	if( pDocNd == 0 ){` |
|   ! 0 |  5054 | `		return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  5055 | `	}` |
|   163 |  5056 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_ELEM,&sQ);` |
|   163 |  5057 | `	if( rc ){` |
|    59 |  5058 | `		return DomThrow(pCtx,rc);` |
|     - |  5059 | `	}` |
|   105 |  5060 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   157 |  5061 | `	pNode = xmlNewDocNode((xmlDocPtr)pDocNd->pNode,0,sQ.zLocal,` |
|   104 |  5062 | `		nVal ? (const xmlChar *)zVal : 0);` |
|   105 |  5063 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::createElementNS");` |
|   105 |  5064 | `	if( pNode == 0 ){` |
|   ! 0 |  5065 | `		DomQNameRelease(&sQ);` |
|   ! 0 |  5066 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  5067 | `	}` |
|   105 |  5068 | `	if( zUri != 0 ){` |
|    99 |  5069 | `		xmlNsPtr pNs = DomNsForCreate(pNode,zUri,sQ.zPrefix);` |
|    99 |  5070 | `		if( pNs == 0 ){` |
|    15 |  5071 | `			DomQNameRelease(&sQ);` |
|    15 |  5072 | `			xmlFreeNode(pNode);   /* never handed out, never an orphan */` |
|    15 |  5073 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  5074 | `		}` |
|    85 |  5075 | `		xmlSetNs(pNode,pNs);` |
|    42 |  5076 | `	}` |
|    91 |  5077 | `	DomQNameRelease(&sQ);` |
|    91 |  5078 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|    91 |  5079 | `	return DomResultNodeOf(pCtx,pDocNd,pNode);` |
|    82 |  5080 | `}` |
|     - |  5081 | `/*` |
|     - |  5082 | ` * DOMDocument::importNode(DOMNode $node, bool $deep = false): DOMNode\|false` |
|     - |  5083 | ` *` |
|     - |  5084 | ` * A node of ANOTHER document copied into this one, which is the only way to` |
|     - |  5085 | ` * carry a subtree across: every mutator refuses a node whose document is not` |
|     - |  5086 | ` * the parent's with php's Wrong Document Error, so without this a program that` |
|     - |  5087 | ` * read two files could not build a third out of them.` |
|     - |  5088 | ` *` |
|     - |  5089 | ` * php's rules, measured:` |
|     - |  5090 | ` *` |
|     - |  5091 | ` *   * A node ALREADY of this document is answered unchanged -- the same object,` |
|     - |  5092 | ` *     not a copy, and not detached from wherever it is.` |
|     - |  5093 | `` *   * A DOCUMENT is refused with a warning and `false`, not an exception.`` |
|     - |  5094 | ` *   * Shallow does not mean bare: an element brings its attributes and its` |
|     - |  5095 | ` *     namespace declarations, only its children stay behind. A fragment brings` |
|     - |  5096 | ` *     nothing but itself, and an attribute brings its value whatever $deep says.` |
|     - |  5097 | ` *   * The copy is an ORPHAN of this document (no parent, and freed with it), and` |
|     - |  5098 | ` *     a second import of the same node is a second copy.` |
|     - |  5099 | ` *   * A namespaced ATTRIBUTE is the one kind libxml cannot finish: its copy` |
|     - |  5100 | ` *     arrives with no namespace at all, and php re-points it at a PREFIXED` |
|     - |  5101 | ` *     binding of the same URI on the target's ROOT -- reusing one the root` |
|     - |  5102 | `` *     already has (so the prefix can change, `p:b` arriving as `z:b`) and`` |
|     - |  5103 | ` *     declaring it there otherwise.` |
|     - |  5104 | ` */` |
|    48 |  5105 | `DOM_METHOD(vm_builtin_DOMDocument_importNode)` |
|     1 |  5106 | `{` |
|    49 |  5107 | `	ph7_vm *pVm = pCtx->pVm;` |
|    49 |  5108 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|    49 |  5109 | `	phl_domnode *pSrc = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|    49 |  5110 | `	int bDeep = nArg > 1 && ph7_value_to_bool(apArg[1]);` |
|     - |  5111 | `	xmlDocPtr pDoc;` |
|     - |  5112 | `	xmlNodePtr pNode,pCopy;` |
|     - |  5113 | `	sxu32 nMark;` |
|    49 |  5114 | `	if( pDocNd == 0 \|\| pSrc == 0 ){` |
|   ! 0 |  5115 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5116 | `		return PH7_OK;` |
|     - |  5117 | `	}` |
|    49 |  5118 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|    49 |  5119 | `	pNode = (xmlNodePtr)pSrc->pNode;` |
|    49 |  5120 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|     - |  5121 | ``		/* The context prints php's `DOMDocument::importNode(): ` itself. */`` |
|     3 |  5122 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Cannot import: Node Type Not Supported");` |
|     3 |  5123 | `		ph7_result_bool(pCtx,0);` |
|     3 |  5124 | `		return PH7_OK;` |
|     - |  5125 | `	}` |
|    47 |  5126 | `	if( pNode->doc == pDoc ){` |
|     5 |  5127 | `		ph7_result_value(pCtx,apArg[0]);` |
|     5 |  5128 | `		return PH7_OK;` |
|     - |  5129 | `	}` |
|    43 |  5130 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     - |  5131 | ``	/* 2 is libxml's `node + namespaces + attributes, no children`, which is what`` |
|     - |  5132 | `	 * makes a shallow import carry the attributes; cloneNode asks the same way. */` |
|    43 |  5133 | `	pCopy = xmlDocCopyNode(pNode,pDoc,bDeep ? 1 : 2);` |
|    43 |  5134 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::importNode");` |
|    43 |  5135 | `	if( pCopy == 0 ){` |
|   ! 0 |  5136 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5137 | `		return PH7_OK;` |
|     - |  5138 | `	}` |
|    43 |  5139 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pNode->ns != 0 && pNode->ns->href != 0 ){` |
|    13 |  5140 | `		xmlNodePtr pRoot = xmlDocGetRootElement(pDoc);` |
|    13 |  5141 | `		xmlNsPtr pNs = pRoot` |
|    12 |  5142 | `			? DomNsResolve(pRoot,(const char *)pNode->ns->href,pNode->ns->prefix,1) : 0;` |
|    13 |  5143 | `		if( pNs == 0 ){` |
|     - |  5144 | `			/* No root element to declare on. php answers an attribute that IS in` |
|     - |  5145 | `			 * the namespace anyway, through a declaration no element makes; the` |
|     - |  5146 | `			 * document owns it so that it is freed with it. */` |
|   ! 0 |  5147 | `			pNs = xmlNewNs(0,pNode->ns->href,pNode->ns->prefix);` |
|   ! 0 |  5148 | `			if( pNs ){` |
|   ! 0 |  5149 | `				DomNsPark(pCopy,pNs);` |
|   ! 0 |  5150 | `			}` |
|   ! 0 |  5151 | `		}` |
|    13 |  5152 | `		xmlSetNs(pCopy,pNs);` |
|     6 |  5153 | `	}` |
|    43 |  5154 | `	DomOrphanAdd(pDocNd->pShell,pCopy);` |
|    43 |  5155 | `	return DomResultNodeOf(pCtx,pDocNd,pCopy);` |
|    25 |  5156 | `}` |
|     - |  5157 | `/*` |
|     - |  5158 | `` * Re-home one node's WRAPPER. `adoptNode` moves the node itself between`` |
|     - |  5159 | ` * documents and answers the SAME object, which has to keep working: its $__doc` |
|     - |  5160 | `` * slot is what `ownerDocument` reads, its handle's shell is what will free the`` |
|     - |  5161 | ` * node, and its place in a document's identity cache is what makes` |
|     - |  5162 | `` * `$doc->documentElement === $doc->documentElement` true. All three move.`` |
|     - |  5163 | ` *` |
|     - |  5164 | ` * The target cache takes its reference BEFORE the source lets go, so the object` |
|     - |  5165 | ` * cannot be freed in between.` |
|     - |  5166 | ` */` |
|   174 |  5167 | `static void DomAdoptWrapper(ph7_vm *pVm,ph7_hashmap *pFrom,ph7_hashmap *pTo,` |
|     - |  5168 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode)` |
|     1 |  5169 | `{` |
|   175 |  5170 | `	ph7_hashmap_node *pEntry = 0;` |
|     - |  5171 | `	ph7_class_instance *pObj;` |
|     - |  5172 | `	ph7_value sKey,*pHit;` |
|   175 |  5173 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|   175 |  5174 | `	if( PH7_HashmapLookup(pFrom,&sKey,&pEntry) != SXRET_OK \|\| pEntry == 0 ){` |
|    79 |  5175 | `		PH7_MemObjRelease(&sKey);` |
|    79 |  5176 | `		return;   /* PHP never asked for this node: nothing to move */` |
|     - |  5177 | `	}` |
|    97 |  5178 | `	pHit = HashmapExtractNodeValue(pEntry);` |
|    97 |  5179 | `	pObj = (pHit && (pHit->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pHit->x.pOther : 0;` |
|    97 |  5180 | `	if( pObj ){` |
|    97 |  5181 | `		phl_domnode *pRes = DomResOf(pObj);` |
|     - |  5182 | `		ph7_value sVal;` |
|    97 |  5183 | `		PH7_MemObjInit(&(*pVm),&sVal);` |
|    97 |  5184 | `		sVal.x.pOther = pObj;` |
|    97 |  5185 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    97 |  5186 | `		PH7_HashmapInsert(pTo,&sKey,&sVal);` |
|    97 |  5187 | `		if( pRes ){` |
|    97 |  5188 | `			pRes->pShell = pDstShell;` |
|    48 |  5189 | `		}` |
|    97 |  5190 | `		PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDstDoc);` |
|    48 |  5191 | `	}` |
|    97 |  5192 | `	PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|    97 |  5193 | `	PH7_MemObjRelease(&sKey);` |
|    89 |  5194 | `}` |
|     - |  5195 | `/* ...for every node of the adopted subtree, attributes and their text included:` |
|     - |  5196 | `` * php's adoption reaches all of them, which `$kid->ownerDocument` shows. */`` |
|    86 |  5197 | `static void DomAdoptWrappers(ph7_vm *pVm,ph7_class_instance *pSrcDoc,` |
|     - |  5198 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode)` |
|     1 |  5199 | `{` |
|    87 |  5200 | `	ph7_hashmap *pFrom = DomCache(&(*pVm),pSrcDoc);` |
|    87 |  5201 | `	ph7_hashmap *pTo = DomCache(&(*pVm),pDstDoc);` |
|    87 |  5202 | `	xmlNodePtr pCur = pNode;` |
|    87 |  5203 | `	if( pFrom == 0 \|\| pTo == 0 \|\| pFrom == pTo ){` |
|   ! 0 |  5204 | `		return;` |
|     - |  5205 | `	}` |
|   237 |  5206 | `	while( pCur ){` |
|   151 |  5207 | `		DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,pCur);` |
|   151 |  5208 | `		if( pCur->type == XML_ELEMENT_NODE ){` |
|     - |  5209 | `			xmlAttrPtr pAttr;` |
|    60 |  5210 | `			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){` |
|     - |  5211 | `				xmlNodePtr pKid;` |
|    13 |  5212 | `				DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,(xmlNodePtr)pAttr);` |
|    25 |  5213 | `				for( pKid = pAttr->children ; pKid ; pKid = pKid->next ){` |
|    13 |  5214 | `					DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,pKid);` |
|     7 |  5215 | `				}` |
|     7 |  5216 | `			}` |
|    24 |  5217 | `		}` |
|   151 |  5218 | `		pCur = DomWalkNext(pCur,pNode);` |
|     1 |  5219 | `	}` |
|    44 |  5220 | `}` |
|     - |  5221 | `/*` |
|     - |  5222 | ` * DOMDocument::adoptNode(DOMNode $node): DOMNode\|false` |
|     - |  5223 | ` *` |
|     - |  5224 | ` * The other half of importNode: the node is MOVED rather than copied, so the` |
|     - |  5225 | ` * source loses it and every wrapper PHP holds onto it keeps working and starts` |
|     - |  5226 | ` * answering this document.` |
|     - |  5227 | ` *` |
|     - |  5228 | ` * php's rules, measured:` |
|     - |  5229 | ` *` |
|     - |  5230 | ` *   * The answer is the SAME object, and it is always UNLINKED first -- even` |
|     - |  5231 | ` *     when it already belongs to this document, which is observable:` |
|     - |  5232 | `` *     `$d->adoptNode($d->documentElement)` leaves the document empty.`` |
|     - |  5233 | ` *   * A DOCUMENT is the Not Supported refusal (raised in the mode the ARGUMENT's` |
|     - |  5234 | `` *     document is in, not the receiver's); a FRAGMENT is a plain `false` with`` |
|     - |  5235 | ` *     no error at all.` |
|     - |  5236 | ` *   * An attribute is taken off its element. Every node under what moved changes` |
|     - |  5237 | ` *     document too, wrappers included.` |
|     - |  5238 | ` *   * NOTHING is re-declared: an adopted element keeps pointing at its old` |
|     - |  5239 | ` *     namespace and answers the same namespaceURI while carrying no declaration` |
|     - |  5240 | ` *     of it -- the declaration appears when it is LINKED, from the reconcile.` |
|     - |  5241 | ` */` |
|    52 |  5242 | `DOM_METHOD(vm_builtin_DOMDocument_adoptNode)` |
|     1 |  5243 | `{` |
|    53 |  5244 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 |  5245 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|    53 |  5246 | `	phl_domnode *pSrc = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|    53 |  5247 | `	ph7_class_instance *pSrcDoc = nArg > 0 ? DomObjArgDoc(apArg[0]) : 0;` |
|     - |  5248 | `	xmlNodePtr pNode;` |
|    53 |  5249 | `	if( pDocNd == 0 \|\| pSrc == 0 ){` |
|   ! 0 |  5250 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5251 | `		return PH7_OK;` |
|     - |  5252 | `	}` |
|    53 |  5253 | `	pNode = (xmlNodePtr)pSrc->pNode;` |
|    53 |  5254 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|     - |  5255 | `		/* The one refusal in this file that consults the ARGUMENT's document` |
|     - |  5256 | `		 * rather than the receiver's: php reaches for the strictness of the` |
|     - |  5257 | ``		 * node it was handed, so `$strict->adoptNode($lax)` warns and`` |
|     - |  5258 | ``		 * `$lax->adoptNode($strict)` throws. */`` |
|    11 |  5259 | `		return DomThrowFor(pCtx,pSrcDoc,DOM_ERR_NOT_SUPPORTED,DOM_REFUSE_FALSE);` |
|     - |  5260 | `	}` |
|    43 |  5261 | `	if( pNode->type == XML_DOCUMENT_FRAG_NODE ){` |
|     3 |  5262 | `		ph7_result_bool(pCtx,0);` |
|     3 |  5263 | `		return PH7_OK;` |
|     - |  5264 | `	}` |
|    41 |  5265 | `	DomDetach(pSrc->pShell,pNode);` |
|    41 |  5266 | `	if( pNode->doc != (xmlDocPtr)pDocNd->pNode ){` |
|     - |  5267 | `		/*` |
|     - |  5268 | `		 * NOT xmlSetTreeDoc: a parsed document interns its node names in its` |
|     - |  5269 | `		 * own dictionary, so a node re-homed by hand keeps names owned by the` |
|     - |  5270 | `		 * document it LEFT -- and freeing the target document then frees` |
|     - |  5271 | `		 * strings the source's dictionary owns. ASan called it what it is, a` |
|     - |  5272 | `		 * bad free. xmlDOMWrapAdoptNode is libxml's own re-homing: it moves the` |
|     - |  5273 | `		 * strings, the attribute values and the ID table entries with the node.` |
|     - |  5274 | `		 * (A CONSTRUCTED node has no document and no dictionary at all, and` |
|     - |  5275 | `		 * xmlSetTreeDoc IS its whole move.)` |
|     - |  5276 | `		 */` |
|    39 |  5277 | `		if( pNode->doc == 0 ){` |
|     3 |  5278 | `			xmlSetTreeDoc(pNode,(xmlDocPtr)pDocNd->pNode);` |
|     2 |  5279 | `		}else{` |
|    37 |  5280 | `			sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    37 |  5281 | `			xmlDOMWrapAdoptNode(0,pNode->doc,pNode,(xmlDocPtr)pDocNd->pNode,0,0);` |
|    37 |  5282 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::adoptNode");` |
|     - |  5283 | `		}` |
|    39 |  5284 | `		DomAdoptWrappers(pVm,pSrcDoc,DomThisDoc(pCtx),pDocNd->pShell,pNode);` |
|    19 |  5285 | `	}` |
|    41 |  5286 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|    41 |  5287 | `	ph7_result_value(pCtx,apArg[0]);` |
|    41 |  5288 | `	return PH7_OK;` |
|    27 |  5289 | `}` |
|     - |  5290 | `/*` |
|     - |  5291 | ` * DOMElement::insertAdjacentElement(string $where, DOMElement $element): ?DOMElement` |
|     - |  5292 | ` * DOMElement::insertAdjacentText(string $where, string $data): void` |
|     - |  5293 | ` *` |
|     - |  5294 | ` * php's dom_insert_adjacent, transcribed.  The WHERE word is matched` |
|     - |  5295 | ` * case-insensitively against the four positions and anything else is the` |
|     - |  5296 | ` * Syntax refusal (code 12, new to DomErrText) -- even on a receiver no` |
|     - |  5297 | ` * position could serve; beforebegin/afterend on a parentless receiver answer` |
|     - |  5298 | ` * null BEFORE anything moves; and then the argument is ADOPTED into this` |
|     - |  5299 | ` * document -- a node of another document is MOVED here, wrappers and all,` |
|     - |  5300 | ` * where every other insertion method refuses it with Wrong Document Error.` |
|     - |  5301 | ` * Only then does the pre-insertion validity run, so a refusal (the receiver` |
|     - |  5302 | ` * inside the argument) leaves the adopted argument DETACHED --` |
|     - |  5303 | `` * `$in->insertAdjacentElement('afterbegin',$host)` costs the tree the whole`` |
|     - |  5304 | ` * host subtree, php's own answer -- and the insertion point is read AFTER the` |
|     - |  5305 | ` * adopt unlinked the argument, which is what makes inserting one's own next` |
|     - |  5306 | `` * sibling `afterend` a no-op rather than a swap.`` |
|     - |  5307 | ` *` |
|     - |  5308 | ` * One deliberate divergence: php SEGFAULTS on` |
|     - |  5309 | `` * `$a->insertAdjacentElement('beforebegin',$a)` -- its adopt unlinks the`` |
|     - |  5310 | ` * receiver and the insertion then walks a NULL parent.  PHL answers the` |
|     - |  5311 | ` * Hierarchy refusal its validity was about to reach.` |
|     - |  5312 | ` */` |
|    42 |  5313 | `static int DomInsertAdjacentOp(ph7_context *pCtx,phl_domnode *pRecv,const char *zWhere,` |
|     - |  5314 | `	phl_xmldoc *pArgShell,xmlNodePtr pOther,ph7_class_instance *pArgDoc)` |
|     1 |  5315 | `{` |
|    43 |  5316 | `	ph7_vm *pVm = pCtx->pVm;` |
|    43 |  5317 | `	xmlNodePtr pThis = (xmlNodePtr)pRecv->pNode;` |
|     - |  5318 | `	xmlNodePtr pParent,pRef;` |
|     - |  5319 | `	int iPos,iErr;` |
|    43 |  5320 | `	if( DomNameIsCi(zWhere,"beforebegin") ){` |
|    11 |  5321 | `		iPos = 0;` |
|    38 |  5322 | `	}else if( DomNameIsCi(zWhere,"afterbegin") ){` |
|    19 |  5323 | `		iPos = 1;` |
|    24 |  5324 | `	}else if( DomNameIsCi(zWhere,"beforeend") ){` |
|     5 |  5325 | `		iPos = 2;` |
|    13 |  5326 | `	}else if( DomNameIsCi(zWhere,"afterend") ){` |
|     7 |  5327 | `		iPos = 3;` |
|     4 |  5328 | `	}else{` |
|     5 |  5329 | `		DomThrowVoid(pCtx,DOM_ERR_SYNTAX);` |
|     5 |  5330 | `		return -1;` |
|     - |  5331 | `	}` |
|    39 |  5332 | `	if( (iPos == 0 \|\| iPos == 3) && pThis->parent == 0 ){` |
|     7 |  5333 | `		return 1;   /* the null answer, nothing moved */` |
|     - |  5334 | `	}` |
|     - |  5335 | `	/* The adopt: detach, re-home across documents (adoptNode's machinery),` |
|     - |  5336 | `	 * and park until linked. A document-less argument -- a constructed node --` |
|     - |  5337 | `	 * has no dict-interned strings to move, so xmlSetTreeDoc is its whole` |
|     - |  5338 | `	 * move; the wrappers cross either way. */` |
|    33 |  5339 | `	DomDetach(pArgShell,pOther);` |
|    33 |  5340 | `	if( pOther->doc != pThis->doc ){` |
|     5 |  5341 | `		if( pOther->doc == 0 ){` |
|   ! 0 |  5342 | `			xmlSetTreeDoc(pOther,pThis->doc);` |
|   ! 0 |  5343 | `		}else{` |
|     5 |  5344 | `			sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     5 |  5345 | `			xmlDOMWrapAdoptNode(0,pOther->doc,pOther,pThis->doc,0,0);` |
|     5 |  5346 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMElement::insertAdjacentElement");` |
|     - |  5347 | `		}` |
|     5 |  5348 | `		DomAdoptWrappers(pVm,pArgDoc,DomThisDoc(pCtx),pRecv->pShell,pOther);` |
|     2 |  5349 | `	}` |
|    33 |  5350 | `	DomOrphanAdd(pRecv->pShell,pOther);` |
|    33 |  5351 | `	switch( iPos ){` |
|     7 |  5352 | `	case 0:  pParent = pThis->parent;  pRef = pThis;            break;` |
|    19 |  5353 | `	case 1:  pParent = pThis;          pRef = pThis->children;  break;` |
|     5 |  5354 | `	case 2:  pParent = pThis;          pRef = 0;                break;` |
|     5 |  5355 | `	default: pParent = pThis->parent;  pRef = pThis->next;      break;` |
|     - |  5356 | `	}` |
|    33 |  5357 | `	if( pParent == 0 ){` |
|     - |  5358 | `		/* The argument WAS the receiver: adopting it took the parent away. */` |
|   ! 0 |  5359 | `		DomThrowVoid(pCtx,DOM_ERR_HIERARCHY);` |
|   ! 0 |  5360 | `		return -1;` |
|     - |  5361 | `	}` |
|    33 |  5362 | `	iErr = DomInsertValidity(pParent,pOther,0);` |
|    33 |  5363 | `	if( iErr ){` |
|     7 |  5364 | `		DomThrowVoid(pCtx,iErr);` |
|     7 |  5365 | `		return -1;` |
|     - |  5366 | `	}` |
|    27 |  5367 | `	if( pRef == pOther ){` |
|   ! 0 |  5368 | `		pRef = pOther->next;` |
|   ! 0 |  5369 | `	}` |
|    27 |  5370 | `	DomDetach(pRecv->pShell,pOther);` |
|    27 |  5371 | `	if( pRef ){` |
|    11 |  5372 | `		DomLinkBefore(pParent,pOther,pRef);` |
|     6 |  5373 | `	}else{` |
|    17 |  5374 | `		DomLinkLast(pParent,pOther);` |
|     - |  5375 | `	}` |
|    27 |  5376 | `	DomNsOnInsertEx(pOther,0);` |
|    27 |  5377 | `	return 0;` |
|    22 |  5378 | `}` |
|    30 |  5379 | `DOM_METHOD(vm_builtin_DOMElement_insertAdjacentElement)` |
|     1 |  5380 | `{` |
|    31 |  5381 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    31 |  5382 | `	phl_domnode *pOther = nArg > 1 ? DomObjArg(apArg[1]) : 0;` |
|    31 |  5383 | `	const char *zWhere = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    31 |  5384 | `	if( pNd == 0 \|\| pOther == 0 ){` |
|   ! 0 |  5385 | `		return PH7_OK;` |
|     - |  5386 | `	}` |
|    45 |  5387 | `	if( DomInsertAdjacentOp(pCtx,pNd,zWhere,pOther->pShell,(xmlNodePtr)pOther->pNode,` |
|    46 |  5388 | `		DomObjArgDoc(apArg[1])) == 0 ){` |
|     - |  5389 | `		/* The answer is the argument itself, now linked. */` |
|    19 |  5390 | `		ph7_result_value(pCtx,apArg[1]);` |
|     9 |  5391 | `	}` |
|    31 |  5392 | `	return PH7_OK;` |
|    16 |  5393 | `}` |
|    12 |  5394 | `DOM_METHOD(vm_builtin_DOMElement_insertAdjacentText)` |
|     1 |  5395 | `{` |
|    13 |  5396 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    13 |  5397 | `	const char *zWhere = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     - |  5398 | `	const char *zData;` |
|    13 |  5399 | `	int nData = 0;` |
|     - |  5400 | `	xmlNodePtr pText;` |
|    13 |  5401 | `	if( pNd == 0 ){` |
|   ! 0 |  5402 | `		return PH7_OK;` |
|     - |  5403 | `	}` |
|    13 |  5404 | `	zData = nArg > 1 ? ph7_value_to_string(apArg[1],&nData) : "";` |
|    13 |  5405 | `	pText = xmlNewDocTextLen(((xmlNodePtr)pNd->pNode)->doc,(const xmlChar *)zData,nData);` |
|    13 |  5406 | `	if( pText == 0 ){` |
|   ! 0 |  5407 | `		return PH7_OK;` |
|     - |  5408 | `	}` |
|     - |  5409 | `	/* Park it FIRST. The op's two earliest refusals -- the Syntax word and` |
|     - |  5410 | `	 * the parentless beforebegin/afterend null -- return before its own` |
|     - |  5411 | `	 * DomOrphanAdd runs, and an unparked fresh node outlives every owner` |
|     - |  5412 | `	 * (the leak checker is what noticed). Parking is idempotent, the op's` |
|     - |  5413 | `	 * detach removes exactly one entry, and the linked node ends OFF the` |
|     - |  5414 | `	 * orphan list -- so the early paths leave it parked in the shell where` |
|     - |  5415 | `	 * teardown frees it, unobservable, which is php's answer. */` |
|    13 |  5416 | `	DomOrphanAdd(pNd->pShell,pText);` |
|    13 |  5417 | `	DomInsertAdjacentOp(pCtx,pNd,zWhere,pNd->pShell,pText,0);` |
|    13 |  5418 | `	return PH7_OK;` |
|     7 |  5419 | `}` |
|     - |  5420 | `/* DOMDocument::createTextNode / createComment / createCDATASection(string $data) */` |
|   130 |  5421 | `static int DomDocCreateData(ph7_context *pCtx,int iKind,int nArg,ph7_value **apArg)` |
|     2 |  5422 | `{` |
|   132 |  5423 | `	int nVal = 0;` |
|   132 |  5424 | `	const char *zVal = nArg > 0 ? ph7_value_to_string(apArg[0],&nVal) : "";` |
|   132 |  5425 | `	return DomDocCreate(pCtx,iKind,"",zVal,nVal);` |
|     2 |  5426 | `}` |
|   112 |  5427 | `DOM_METHOD(vm_builtin_DOMDocument_createTextNode)` |
|     2 |  5428 | `{` |
|   114 |  5429 | `	return DomDocCreateData(pCtx,XML_TEXT_NODE,nArg,apArg);` |
|     2 |  5430 | `}` |
|     - |  5431 | `/* DOMDocument::createProcessingInstruction(string $target, string $data = '')` |
|     - |  5432 | ` * / createEntityReference(string $name) / createDocumentFragment() */` |
|    20 |  5433 | `DOM_METHOD(vm_builtin_DOMDocument_createPI)` |
|     1 |  5434 | `{` |
|    21 |  5435 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    21 |  5436 | `	int nVal = 0;` |
|    21 |  5437 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";` |
|    21 |  5438 | `	return DomDocCreate(pCtx,XML_PI_NODE,zName,zVal,nVal);` |
|     1 |  5439 | `}` |
|    24 |  5440 | `DOM_METHOD(vm_builtin_DOMDocument_createEntityRef)` |
|     1 |  5441 | `{` |
|    25 |  5442 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    25 |  5443 | `	return DomDocCreate(pCtx,XML_ENTITY_REF_NODE,zName,"",0);` |
|     1 |  5444 | `}` |
|    84 |  5445 | `DOM_METHOD(vm_builtin_DOMDocument_createFragment)` |
|     1 |  5446 | `{` |
|    42 |  5447 | `	SXUNUSED(nArg);` |
|    42 |  5448 | `	SXUNUSED(apArg);` |
|    85 |  5449 | `	return DomDocCreate(pCtx,XML_DOCUMENT_FRAG_NODE,"","",0);` |
|     1 |  5450 | `}` |
|    10 |  5451 | `DOM_METHOD(vm_builtin_DOMDocument_createComment)` |
|     1 |  5452 | `{` |
|    11 |  5453 | `	return DomDocCreateData(pCtx,XML_COMMENT_NODE,nArg,apArg);` |
|     1 |  5454 | `}` |
|     8 |  5455 | `DOM_METHOD(vm_builtin_DOMDocument_createCDATASection)` |
|     1 |  5456 | `{` |
|     9 |  5457 | `	return DomDocCreateData(pCtx,XML_CDATA_SECTION_NODE,nArg,apArg);` |
|     1 |  5458 | `}` |
|     - |  5459 | `/*` |
|     - |  5460 | `` * php's normalization, which both `DOMNode::normalize()` and`` |
|     - |  5461 | `` * `DOMDocument::normalizeDocument()` are: adjacent text nodes merge into the`` |
|     - |  5462 | ` * FIRST of the run, and a text node left EMPTY is then dropped from the tree` |
|     - |  5463 | ` * entirely -- including one that was empty to begin with, which is what makes` |
|     - |  5464 | `` * `$el->normalize()` the way a program gets rid of the zero-length text nodes an`` |
|     - |  5465 | ` * edit leaves behind. Dropping them was the half missing here: a document that` |
|     - |  5466 | `` * had been normalized still serialized `<k></k>` where php writes `<k/>`, and`` |
|     - |  5467 | `` * still counted the empty node in `childNodes->length`.`` |
|     - |  5468 | ` *` |
|     - |  5469 | ` * Merged-away and dropped siblings are PARKED as orphans, never freed, so any` |
|     - |  5470 | ` * PHP wrapper to them stays valid -- php keeps exactly those alive too, through` |
|     - |  5471 | ` * its own wrapper refcount, and a variable holding one reads its old content and` |
|     - |  5472 | `` * a NULL `parentNode` in both engines.`` |
|     - |  5473 | ` *` |
|     - |  5474 | ` * The walk descends into a child ELEMENT and into that element's ATTRIBUTES` |
|     - |  5475 | ` * (an attribute's value is a child text list of its own, and a program that` |
|     - |  5476 | ` * built one in pieces has the same run of nodes to merge). What it does NOT` |
|     - |  5477 | ` * touch is the RECEIVER's own attributes -- php's switch reaches an attribute` |
|     - |  5478 | `` * only through a child element -- so `$el->normalize()` leaves `$el`'s`` |
|     - |  5479 | `` * attributes alone while `$el->parentNode->normalize()` normalizes them.`` |
|     - |  5480 | ` */` |
|    56 |  5481 | `static void DomNormalizeTree(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|     1 |  5482 | `{` |
|    57 |  5483 | `	xmlNodePtr pChild = pNode->children;` |
|   127 |  5484 | `	while( pChild ){` |
|    71 |  5485 | `		if( pChild->type == XML_TEXT_NODE ){` |
|     - |  5486 | `			xmlNodePtr pNext;` |
|    37 |  5487 | `			while( pChild->next && pChild->next->type == XML_TEXT_NODE ){` |
|     9 |  5488 | `				pNext = pChild->next;` |
|     9 |  5489 | `				if( pNext->content ){` |
|     9 |  5490 | `					xmlNodeAddContent(pChild,pNext->content);` |
|     4 |  5491 | `				}` |
|     9 |  5492 | `				xmlUnlinkNode(pNext);` |
|     9 |  5493 | `				DomOrphanAdd(pShell,pNext);` |
|     1 |  5494 | `			}` |
|    29 |  5495 | `			if( pChild->content == 0 \|\| pChild->content[0] == 0 ){` |
|     5 |  5496 | `				pNext = pChild->next;` |
|     5 |  5497 | `				xmlUnlinkNode(pChild);` |
|     5 |  5498 | `				DomOrphanAdd(pShell,pChild);` |
|     5 |  5499 | `				pChild = pNext;` |
|     5 |  5500 | `				continue;` |
|     1 |  5501 | `			}` |
|    55 |  5502 | `		}else if( pChild->type == XML_ELEMENT_NODE ){` |
|     - |  5503 | `			xmlAttrPtr pAttr;` |
|    29 |  5504 | `			DomNormalizeTree(pShell,pChild);` |
|    43 |  5505 | `			for( pAttr = pChild->properties ; pAttr ; pAttr = pAttr->next ){` |
|    15 |  5506 | `				DomNormalizeTree(pShell,(xmlNodePtr)pAttr);` |
|     8 |  5507 | `			}` |
|    29 |  5508 | `		}else if( pChild->type == XML_ATTRIBUTE_NODE ){` |
|     - |  5509 | `			/* Unreachable from a tree walk (attributes are not children), but` |
|     - |  5510 | `			 * php's switch states it and a fragment/DTD shape could reach it. */` |
|   ! 0 |  5511 | `			DomNormalizeTree(pShell,pChild);` |
|   ! 0 |  5512 | `		}` |
|    67 |  5513 | `		pChild = pChild->next;` |
|     1 |  5514 | `	}` |
|    57 |  5515 | `}` |
|     - |  5516 | `/* DOMDocument::normalizeDocument(): void and DOMNode::normalize(): void -- php` |
|     - |  5517 | ` * runs the same walk from the receiver, so the two share one body. */` |
|    14 |  5518 | `DOM_METHOD(vm_builtin_DOMDocument_normalizeDocument)` |
|     1 |  5519 | `{` |
|    15 |  5520 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     7 |  5521 | `	SXUNUSED(nArg);` |
|     7 |  5522 | `	SXUNUSED(apArg);` |
|    15 |  5523 | `	if( pNd ){` |
|    15 |  5524 | `		DomNormalizeTree(pNd->pShell,(xmlNodePtr)pNd->pNode);` |
|     7 |  5525 | `	}` |
|    15 |  5526 | `	return PH7_OK;` |
|     1 |  5527 | `}` |
|     - |  5528 | `/* DOMNode::getNodePath(): ?string -- the XPath that selects this node, or null` |
|     - |  5529 | ` * for one that is not addressable at all (anything under a fragment). php hands` |
|     - |  5530 | ` * libxml's answer straight back, positional predicate and all. */` |
|    60 |  5531 | `DOM_METHOD(vm_builtin_DOMNode_getNodePath)` |
|     1 |  5532 | `{` |
|    61 |  5533 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    61 |  5534 | `	xmlChar *zPath = pNd ? xmlGetNodePath((xmlNodePtr)pNd->pNode) : 0;` |
|    30 |  5535 | `	SXUNUSED(nArg);` |
|    30 |  5536 | `	SXUNUSED(apArg);` |
|    61 |  5537 | `	if( zPath == 0 ){` |
|    15 |  5538 | `		ph7_result_null(pCtx);` |
|    15 |  5539 | `		return PH7_OK;` |
|     - |  5540 | `	}` |
|    47 |  5541 | `	ph7_result_string(pCtx,(const char *)zPath,-1);` |
|    47 |  5542 | `	xmlFree(zPath);` |
|    47 |  5543 | `	return PH7_OK;` |
|    31 |  5544 | `}` |
|     - |  5545 |  |
|     - |  5546 | `/* ===== Character data: the in-place edit family ===== */` |
|     - |  5547 |  |
|     - |  5548 | `/*` |
|     - |  5549 | ` * Every offset and count on this surface is measured in UTF-8 CHARACTERS, not` |
|     - |  5550 | `` * bytes -- php runs `xmlUTF8Strlen` over the content and `xmlUTF8Strsub` to cut`` |
|     - |  5551 | `` * it -- so `$t->length` on "áé漢字" is 4 and `substringData(0,1)` is one`` |
|     - |  5552 | `` * character rather than one byte. PHL measured `length` with strlen(), which is`` |
|     - |  5553 | ` * a silently wrong answer for every non-ASCII document: 10 where php says 4,` |
|     - |  5554 | ` * and every offset a program then computed from it landed mid-character.` |
|     - |  5555 | ` *` |
|     - |  5556 | ` * libxml's own UTF-8 helpers are used rather than PHL's, so malformed content` |
|     - |  5557 | ` * counts and cuts identically in both engines.` |
|     - |  5558 | ` */` |
|   120 |  5559 | `static int DomCharLength(xmlNodePtr pNode)` |
|     1 |  5560 | `{` |
|   121 |  5561 | `	return (pNode && pNode->content) ? xmlUTF8Strlen(pNode->content) : 0;` |
|     1 |  5562 | `}` |
|     - |  5563 | `/*` |
|     - |  5564 | ` * php's Index Size Error: a negative bound, or an offset past the end. The` |
|     - |  5565 | ` * COUNT is clamped rather than refused once the offset is in range.` |
|     - |  5566 | ` *` |
|     - |  5567 | ` * The upper bound is compared UNSIGNED on three of the five and SIGNED on the` |
|     - |  5568 | ``  * other two, and only malformed content tells them apart: `xmlUTF8Strlen` `` |
|     - |  5569 | `` * answers -1 for content that is not valid UTF-8 (`$t->length` reports that`` |
|     - |  5570 | ` * -1), and as an UNSIGNED bound a -1 means "no limit" -- so substringData,` |
|     - |  5571 | ` * insertData and splitText all work on such a node and let libxml's own cutting` |
|     - |  5572 | ` * decide what comes back, while deleteData and replaceData refuse it outright,` |
|     - |  5573 | ` * for every offset and every count. php's own split, kept because a program` |
|     - |  5574 | ` * handed a byte string that is not UTF-8 gets a value back from three of these` |
|     - |  5575 | ` * and an exception from the other two.` |
|     - |  5576 | ` */` |
|    88 |  5577 | `static int DomCharRange(ph7_context *pCtx,xmlNodePtr pNode,ph7_int64 iOffset,` |
|     - |  5578 | `	ph7_int64 iCount,int bHasCount,int bUnsignedBound,int *pnLen,int *pRc)` |
|     1 |  5579 | `{` |
|    89 |  5580 | `	int nLen = DomCharLength(pNode);` |
|   114 |  5581 | `	int bPastEnd = bUnsignedBound ? (sxu32)iOffset > (sxu32)nLen` |
|    63 |  5582 | `	                              : iOffset > (ph7_int64)nLen;` |
|    89 |  5583 | `	*pnLen = nLen;` |
|    88 |  5584 | `	if( iOffset < 0 \|\| (bHasCount && iCount < 0)` |
|    79 |  5585 | `	 \|\| iOffset > (ph7_int64)SXI32_HIGH \|\| iCount > (ph7_int64)SXI32_HIGH` |
|    77 |  5586 | `	 \|\| bPastEnd ){` |
|    33 |  5587 | `		*pRc = DomThrow(pCtx,DOM_ERR_INDEX_SIZE);` |
|    33 |  5588 | `		return -1;` |
|     - |  5589 | `	}` |
|    57 |  5590 | `	return 0;` |
|    45 |  5591 | `}` |
|     - |  5592 | `/* DOMCharacterData::substringData(int $offset, int $count): string */` |
|    36 |  5593 | `DOM_METHOD(vm_builtin_DOMCharacterData_substringData)` |
|     1 |  5594 | `{` |
|    37 |  5595 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    37 |  5596 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    37 |  5597 | `	ph7_int64 iOffset = nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    37 |  5598 | `	ph7_int64 iCount = nArg > 1 ? ph7_value_to_int64(apArg[1]) : 0;` |
|     - |  5599 | `	xmlChar *zSub;` |
|    37 |  5600 | `	int nLen,rc = PH7_OK;` |
|    37 |  5601 | `	if( pNode == 0 ){` |
|   ! 0 |  5602 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5603 | `		return PH7_OK;` |
|     - |  5604 | `	}` |
|    37 |  5605 | `	if( DomCharRange(pCtx,pNode,iOffset,iCount,TRUE,TRUE,&nLen,&rc) != 0 ){` |
|    15 |  5606 | `		return rc;` |
|     - |  5607 | `	}` |
|    23 |  5608 | `	if( pNode->content == 0 ){` |
|     - |  5609 | `		/* php reads a NULL content pointer -- the omitted-argument` |
|     - |  5610 | `		 * constructor's node -- as "": the range still screens (so an offset` |
|     - |  5611 | `		 * past zero is Index Size), and what is left of nothing is "". */` |
|     3 |  5612 | `		ph7_result_string(pCtx,"",0);` |
|     3 |  5613 | `		return PH7_OK;` |
|     - |  5614 | `	}` |
|    21 |  5615 | `	if( (sxu32)(iOffset+iCount) > (sxu32)nLen ){` |
|     7 |  5616 | `		iCount = (ph7_int64)nLen - iOffset;` |
|     3 |  5617 | `	}` |
|    21 |  5618 | `	zSub = xmlUTF8Strsub(pNode->content,(int)iOffset,(int)iCount);` |
|    21 |  5619 | `	ph7_result_string(pCtx,zSub ? (const char *)zSub : "",-1);` |
|    21 |  5620 | `	if( zSub ){` |
|    21 |  5621 | `		xmlFree(zSub);` |
|    10 |  5622 | `	}` |
|    21 |  5623 | `	return PH7_OK;` |
|    19 |  5624 | `}` |
|     - |  5625 | `/* DOMCharacterData::appendData(string $data): true -- raw bytes, no entity` |
|     - |  5626 | `` * parsing, which is why `appendData('&amp;')` stores those five characters. */`` |
|     6 |  5627 | `DOM_METHOD(vm_builtin_DOMCharacterData_appendData)` |
|     1 |  5628 | `{` |
|     7 |  5629 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     7 |  5630 | `	int nData = 0;` |
|     7 |  5631 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : "";` |
|     7 |  5632 | `	if( pNd ){` |
|     7 |  5633 | `		xmlTextConcat((xmlNodePtr)pNd->pNode,(const xmlChar *)zData,nData);` |
|     3 |  5634 | `	}` |
|     7 |  5635 | `	ph7_result_bool(pCtx,1);` |
|     7 |  5636 | `	return PH7_OK;` |
|     1 |  5637 | `}` |
|     - |  5638 | `/*` |
|     - |  5639 | ` * The three writers, which php builds the same way: the head up to $offset, the` |
|     - |  5640 | ` * replacement, then whatever the count left of the tail.` |
|     - |  5641 | ` *` |
|     - |  5642 | ` * insertData is (offset, 0, data), deleteData is (offset, count, ""), and` |
|     - |  5643 | ` * replaceData is both -- php's own three bodies say the same thing three times.` |
|     - |  5644 | ` */` |
|    52 |  5645 | `static int DomCharSplice(ph7_context *pCtx,ph7_int64 iOffset,ph7_int64 iCount,` |
|     - |  5646 | `	int bHasCount,const char *zData,int nData)` |
|     1 |  5647 | `{` |
|    53 |  5648 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    53 |  5649 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    53 |  5650 | `	xmlChar *zHead,*zTail = 0;` |
|    53 |  5651 | `	int nLen,rc = PH7_OK;` |
|    53 |  5652 | `	if( pNode == 0 ){` |
|   ! 0 |  5653 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5654 | `		return PH7_OK;` |
|     - |  5655 | `	}` |
|    53 |  5656 | `	if( pNode->content == 0 ){` |
|     - |  5657 | `		/* A writer normalizes the omitted-argument constructor's NULL content` |
|     - |  5658 | ``		 * to "" and proceeds, php's own answer: `insertData(0,'i')` on a`` |
|     - |  5659 | ``		 * `new DOMComment()` writes "i", and its nodeValue reads "" after. */`` |
|     5 |  5660 | `		xmlNodeSetContent(pNode,(const xmlChar *)"");` |
|     2 |  5661 | `	}` |
|     - |  5662 | `	/* insertData has no count and takes the unsigned bound; the two that DO` |
|     - |  5663 | `	 * take one take the signed bound. */` |
|    53 |  5664 | `	if( DomCharRange(pCtx,pNode,iOffset,iCount,bHasCount,!bHasCount,&nLen,&rc) != 0 ){` |
|    19 |  5665 | `		return rc;` |
|     - |  5666 | `	}` |
|    35 |  5667 | `	if( (sxu32)(iOffset+iCount) > (sxu32)nLen ){` |
|    11 |  5668 | `		iCount = (ph7_int64)nLen - iOffset;` |
|     5 |  5669 | `	}` |
|    27 |  5670 | `	zHead = iOffset > 0 ? xmlUTF8Strndup(pNode->content,(int)iOffset)` |
|    25 |  5671 | `	                    : xmlStrdup((const xmlChar *)"");` |
|    35 |  5672 | `	if( iOffset + iCount < (ph7_int64)nLen ){` |
|    31 |  5673 | `		zTail = xmlUTF8Strsub(pNode->content,(int)(iOffset+iCount),` |
|    20 |  5674 | `			(int)((ph7_int64)nLen - iOffset - iCount));` |
|    10 |  5675 | `	}` |
|    35 |  5676 | `	xmlNodeSetContent(pNode,zHead ? zHead : (const xmlChar *)"");` |
|    35 |  5677 | `	if( nData > 0 ){` |
|    21 |  5678 | `		xmlNodeAddContentLen(pNode,(const xmlChar *)zData,nData);` |
|    10 |  5679 | `	}` |
|    35 |  5680 | `	if( zTail ){` |
|    21 |  5681 | `		xmlNodeAddContent(pNode,zTail);` |
|    10 |  5682 | `	}` |
|    35 |  5683 | `	if( zHead ){` |
|    35 |  5684 | `		xmlFree(zHead);` |
|    17 |  5685 | `	}` |
|    35 |  5686 | `	if( zTail ){` |
|    21 |  5687 | `		xmlFree(zTail);` |
|    10 |  5688 | `	}` |
|    35 |  5689 | `	ph7_result_bool(pCtx,1);` |
|    35 |  5690 | `	return PH7_OK;` |
|    27 |  5691 | `}` |
|     - |  5692 | `/* DOMCharacterData::insertData(int $offset, string $data): true */` |
|    14 |  5693 | `DOM_METHOD(vm_builtin_DOMCharacterData_insertData)` |
|     1 |  5694 | `{` |
|    15 |  5695 | `	int nData = 0;` |
|    15 |  5696 | `	const char *zData = nArg > 1 ? ph7_value_to_string(apArg[1],&nData) : "";` |
|    15 |  5697 | `	return DomCharSplice(pCtx,nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0,0,FALSE,zData,nData);` |
|     1 |  5698 | `}` |
|     - |  5699 | `/* DOMCharacterData::deleteData(int $offset, int $count): true */` |
|    22 |  5700 | `DOM_METHOD(vm_builtin_DOMCharacterData_deleteData)` |
|     1 |  5701 | `{` |
|    45 |  5702 | `	return DomCharSplice(pCtx,nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0,` |
|    22 |  5703 | `		nArg > 1 ? ph7_value_to_int64(apArg[1]) : 0,TRUE,"",0);` |
|     1 |  5704 | `}` |
|     - |  5705 | `/* DOMCharacterData::replaceData(int $offset, int $count, string $data): true */` |
|    16 |  5706 | `DOM_METHOD(vm_builtin_DOMCharacterData_replaceData)` |
|     1 |  5707 | `{` |
|    17 |  5708 | `	int nData = 0;` |
|    17 |  5709 | `	const char *zData = nArg > 2 ? ph7_value_to_string(apArg[2],&nData) : "";` |
|    33 |  5710 | `	return DomCharSplice(pCtx,nArg > 2 ? ph7_value_to_int64(apArg[0]) : 0,` |
|    16 |  5711 | `		nArg > 2 ? ph7_value_to_int64(apArg[1]) : 0,TRUE,zData,nData);` |
|     1 |  5712 | `}` |
|     - |  5713 | `/*` |
|     - |  5714 | ` * DOMText::splitText(int $offset): DOMText\|false` |
|     - |  5715 | ` *` |
|     - |  5716 | ` * The receiver keeps the head and a SECOND node takes the tail, spliced in` |
|     - |  5717 | ` * right after it. Two details only the oracle states: an offset past the end is` |
|     - |  5718 | `` * plain `false` where a negative one is a ValueError, and splitting a CDATA`` |
|     - |  5719 | `` * section produces a TEXT node -- so `<![CDATA[abcdef]]>` split at 2 serializes`` |
|     - |  5720 | `` * as `<![CDATA[ab]]>cdef`.`` |
|     - |  5721 | ` */` |
|    22 |  5722 | `DOM_METHOD(vm_builtin_DOMText_splitText)` |
|     1 |  5723 | `{` |
|    23 |  5724 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    23 |  5725 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    23 |  5726 | `	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - |  5727 | `	xmlChar *zHead,*zTail;` |
|     - |  5728 | `	xmlNodePtr pNew;` |
|     - |  5729 | `	int nLen;` |
|    23 |  5730 | `	if( iOffset < 0 ){` |
|     3 |  5731 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  5732 | `			"DOMText::splitText(): Argument #1 ($offset) must be greater than or equal to 0");` |
|     - |  5733 | `	}` |
|    20 |  5734 | `	if( pNode == 0` |
|    21 |  5735 | `	 \|\| (pNode->type != XML_TEXT_NODE && pNode->type != XML_CDATA_SECTION_NODE) ){` |
|   ! 0 |  5736 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5737 | `		return PH7_OK;` |
|     - |  5738 | `	}` |
|    21 |  5739 | `	if( pNode->content == 0 ){` |
|     - |  5740 | `		/* The omitted-argument constructor's node splits as "": both halves` |
|     - |  5741 | `		 * empty, php's answer. The split WRITES, so normalizing is its own. */` |
|     3 |  5742 | `		xmlNodeSetContent(pNode,(const xmlChar *)"");` |
|     1 |  5743 | `	}` |
|    21 |  5744 | `	nLen = DomCharLength(pNode);` |
|    21 |  5745 | `	if( iOffset > (ph7_int64)nLen ){` |
|     3 |  5746 | `		ph7_result_bool(pCtx,0);` |
|     3 |  5747 | `		return PH7_OK;` |
|     - |  5748 | `	}` |
|    19 |  5749 | `	zHead = xmlUTF8Strndup(pNode->content,(int)iOffset);` |
|    19 |  5750 | `	zTail = xmlUTF8Strsub(pNode->content,(int)iOffset,(int)((ph7_int64)nLen - iOffset));` |
|    19 |  5751 | `	xmlNodeSetContent(pNode,zHead ? zHead : (const xmlChar *)"");` |
|    19 |  5752 | `	pNew = xmlNewDocText(pNode->doc,zTail ? zTail : (const xmlChar *)"");` |
|    19 |  5753 | `	if( zHead ){` |
|    19 |  5754 | `		xmlFree(zHead);` |
|     9 |  5755 | `	}` |
|    19 |  5756 | `	if( zTail ){` |
|    19 |  5757 | `		xmlFree(zTail);` |
|     9 |  5758 | `	}` |
|    19 |  5759 | `	if( pNew == 0 ){` |
|   ! 0 |  5760 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  5761 | `		return PH7_OK;` |
|     - |  5762 | `	}` |
|    19 |  5763 | `	if( pNode->parent ){` |
|     - |  5764 | `		/* Spliced by hand, as everything in this file is: xmlAddNextSibling` |
|     - |  5765 | `		 * MERGES two adjacent text nodes and frees one of them. */` |
|    13 |  5766 | `		if( pNode->next ){` |
|     9 |  5767 | `			DomLinkBefore(pNode->parent,pNew,pNode->next);` |
|     5 |  5768 | `		}else{` |
|     5 |  5769 | `			DomLinkLast(pNode->parent,pNew);` |
|     - |  5770 | `		}` |
|     7 |  5771 | `	}else{` |
|     7 |  5772 | `		DomOrphanAdd(pNd->pShell,pNew);` |
|     - |  5773 | `	}` |
|    19 |  5774 | `	return DomResultNodeOf(pCtx,pNd,pNew);` |
|    12 |  5775 | `}` |
|     - |  5776 | `/* DOMText::isWhitespaceInElementContent() and its 8.x rename` |
|     - |  5777 | ` * isElementContentWhitespace(): one body, libxml's blank-node test. */` |
|    12 |  5778 | `DOM_METHOD(vm_builtin_DOMText_isWhitespace)` |
|     1 |  5779 | `{` |
|    13 |  5780 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     6 |  5781 | `	SXUNUSED(nArg);` |
|     6 |  5782 | `	SXUNUSED(apArg);` |
|    13 |  5783 | `	ph7_result_bool(pCtx,pNd && xmlIsBlankNode((xmlNodePtr)pNd->pNode));` |
|    13 |  5784 | `	return PH7_OK;` |
|     1 |  5785 | `}` |
|     - |  5786 |  |
|     - |  5787 | `/* ===== C14N ===== */` |
|     - |  5788 |  |
|     - |  5789 | `/*` |
|     - |  5790 | ` * php canonicalizes a NODE by handing libxml the node SET an XPath produces` |
|     - |  5791 | `` * from it -- `(.//. \| .//@* \| .//namespace::*)` with the node as context -- and`` |
|     - |  5792 | ` * a DOCUMENT by handing it no set at all, which is how a document's top-level` |
|     - |  5793 | ` * comments reach the output where a node's cannot. Running a VISIBILITY` |
|     - |  5794 | ` * callback instead (the shape this file had) is close but not the same: an` |
|     - |  5795 | `` * ATTRIBUTE canonicalizes to its own ` b="2"` under php, where a "keep the`` |
|     - |  5796 | ` * target's subtree" callback answers the empty string.` |
|     - |  5797 | ` *` |
|     - |  5798 | `` * All four of php's parameters are read here. `$exclusive` picks Exclusive`` |
|     - |  5799 | `` * C14N, `$withComments` keeps comments, `$xpath` REPLACES the default node set`` |
|     - |  5800 | ``  * with the caller's query (and may register prefixes for it), and `$nsPrefixes` `` |
|     - |  5801 | ` * lists the namespace prefixes an exclusive canonicalization must declare even` |
|     - |  5802 | ` * where they are unused. Only the two bools were honoured before, so` |
|     - |  5803 | `` * `C14N(true)` -- the mode every XML-DSig signer asks for -- silently`` |
|     - |  5804 | ` * canonicalized inclusively and produced bytes that will not verify.` |
|     - |  5805 | ` */` |
|     - |  5806 |  |
|     - |  5807 | ``/* The `namespaces` sub-array of `$xpath`: prefix => URI, string pairs only. */`` |
|   108 |  5808 | `static int DomC14NRegisterNs(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  5809 | `{` |
|   109 |  5810 | `	xmlXPathContextPtr pXCtx = (xmlXPathContextPtr)pUserData;` |
|   109 |  5811 | `	if( pKey && pVal && ph7_value_is_string(pKey) && ph7_value_is_string(pVal) ){` |
|   163 |  5812 | `		xmlXPathRegisterNs(pXCtx,(const xmlChar *)ph7_value_to_string(pKey,0),` |
|   108 |  5813 | `			(const xmlChar *)ph7_value_to_string(pVal,0));` |
|    54 |  5814 | `	}` |
|   109 |  5815 | `	return PH7_OK;` |
|     1 |  5816 | `}` |
|     - |  5817 | `/*` |
|     - |  5818 | `` * The `$nsPrefixes` list, collected into the NULL-terminated array libxml`` |
|     - |  5819 | ` * wants. Non-string entries are skipped, exactly as php skips them.` |
|     - |  5820 | ` *` |
|     - |  5821 | ` * The bytes are COPIED. ph7_array_walk hands its callback a temporary copy of` |
|     - |  5822 | ` * each value and releases it the moment the callback returns, so keeping the` |
|     - |  5823 | ` * pointer leaves a dangling one -- which libxml then compares against real` |
|     - |  5824 | `` * prefixes and matches at random, so `C14N(true,false,null,['u','p'])` declared`` |
|     - |  5825 | ` * whichever prefix the freed memory happened to still read as.` |
|     - |  5826 | ` */` |
|     - |  5827 | `typedef struct DomC14NPrefixes DomC14NPrefixes;` |
|     - |  5828 | `struct DomC14NPrefixes {` |
|     - |  5829 | `	SyBlob sPool;    /* the prefix bytes, NUL-terminated one after another */` |
|     - |  5830 | `	SySet aOfs;      /* each prefix's offset into sPool */` |
|     - |  5831 | `	xmlChar **apPrefix;` |
|     - |  5832 | `};` |
|    18 |  5833 | `static int DomC14NCollectPrefix(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  5834 | `{` |
|    19 |  5835 | `	DomC14NPrefixes *pList = (DomC14NPrefixes *)pUserData;` |
|     9 |  5836 | `	SXUNUSED(pKey);` |
|    19 |  5837 | `	if( pVal && ph7_value_is_string(pVal) ){` |
|    17 |  5838 | `		sxu32 nOfs = SyBlobLength(&pList->sPool);` |
|    17 |  5839 | `		int nByte = 0;` |
|    17 |  5840 | `		const char *zVal = ph7_value_to_string(pVal,&nByte);` |
|    17 |  5841 | `		SySetPut(&pList->aOfs,(const void *)&nOfs);` |
|    17 |  5842 | `		SyBlobAppend(&pList->sPool,zVal,(sxu32)nByte);` |
|    17 |  5843 | `		SyBlobAppend(&pList->sPool,"",1);` |
|     8 |  5844 | `	}` |
|    19 |  5845 | `	return PH7_OK;` |
|     1 |  5846 | `}` |
|     - |  5847 | `/*` |
|     - |  5848 | ` * Canonicalize the receiver into *pzOut (xmlFree'd by the caller) and answer` |
|     - |  5849 | ` * its byte count, or -1 when php answers false/"" instead. *pRc carries a` |
|     - |  5850 | ` * refusal php raises before anything is written.` |
|     - |  5851 | ` *` |
|     - |  5852 | `` * iXPathPos is the 1-based position of `$xpath` in the CALLING method's`` |
|     - |  5853 | ` * parameter list: 3 on C14N, 4 on C14NFile, and php's messages print it.` |
|     - |  5854 | ` */` |
|   118 |  5855 | `static int DomC14NRun(ph7_context *pCtx,int nArg,ph7_value **apArg,int iXPathPos,` |
|     - |  5856 | `	const char *zFn,xmlChar **pzOut,int *pRc)` |
|     1 |  5857 | `{` |
|   119 |  5858 | `	ph7_vm *pVm = pCtx->pVm;` |
|   119 |  5859 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   119 |  5860 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   119 |  5861 | `	int iFirst = iXPathPos - 3;   /* index of $exclusive */` |
|   119 |  5862 | `	int bExclusive = nArg > iFirst && ph7_value_to_bool(apArg[iFirst]);` |
|   119 |  5863 | `	int bComments = nArg > iFirst+1 && ph7_value_to_bool(apArg[iFirst+1]);` |
|    76 |  5864 | `	ph7_value *pXPath = (nArg > iFirst+2 && ph7_value_is_array(apArg[iFirst+2]))` |
|    83 |  5865 | `		? apArg[iFirst+2] : 0;` |
|    68 |  5866 | `	ph7_value *pPrefixes = (nArg > iFirst+3 && ph7_value_is_array(apArg[iFirst+3]))` |
|    75 |  5867 | `		? apArg[iFirst+3] : 0;` |
|     - |  5868 | `	DomC14NPrefixes sPrefixes;` |
|   119 |  5869 | `	xmlXPathContextPtr pXCtx = 0;` |
|   119 |  5870 | `	xmlXPathObjectPtr pXObj = 0;` |
|   119 |  5871 | `	xmlNodeSetPtr pSet = 0;` |
|     - |  5872 | `	sxu32 nMark,n;` |
|     - |  5873 | `	int nOut;` |
|   119 |  5874 | `	*pzOut = 0;` |
|   119 |  5875 | `	sPrefixes.apPrefix = 0;` |
|   119 |  5876 | `	SyBlobInit(&sPrefixes.sPool,&pVm->sAllocator);` |
|   119 |  5877 | `	SySetInit(&sPrefixes.aOfs,&pVm->sAllocator,sizeof(sxu32));` |
|   119 |  5878 | `	if( pNode == 0 ){` |
|   ! 0 |  5879 | `		nOut = -1;` |
|   ! 0 |  5880 | `		goto done;` |
|     - |  5881 | `	}` |
|   119 |  5882 | `	if( pNode->doc == 0 ){` |
|     - |  5883 | `		/* php's plain Error, no DOM code: canonicalization asks libxml for the` |
|     - |  5884 | `		 * document's context, and a constructed node has none. */` |
|     5 |  5885 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Node must be associated with a document");` |
|     5 |  5886 | `		nOut = -1;` |
|     5 |  5887 | `		goto done;` |
|     - |  5888 | `	}` |
|   115 |  5889 | `	if( pXPath ){` |
|    17 |  5890 | `		ph7_value *pQuery = ph7_array_fetch(pXPath,"query",(int)sizeof("query")-1);` |
|     - |  5891 | `		ph7_value *pNs;` |
|    17 |  5892 | `		if( pQuery == 0 ){` |
|     7 |  5893 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|     2 |  5894 | `				"%s(): Argument #%d ($xpath) must have a \"query\" key",zFn,iXPathPos);` |
|     5 |  5895 | `			nOut = -1;` |
|     5 |  5896 | `			goto done;` |
|     - |  5897 | `		}` |
|    13 |  5898 | `		if( !ph7_value_is_string(pQuery) ){` |
|     4 |  5899 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  5900 | `				"%s(): Argument #%d ($xpath) \"query\" option must be a string, %s given",` |
|     1 |  5901 | `				zFn,iXPathPos,ph7_type_name(pQuery));` |
|     3 |  5902 | `			nOut = -1;` |
|     3 |  5903 | `			goto done;` |
|     - |  5904 | `		}` |
|    11 |  5905 | `		pXCtx = xmlXPathNewContext(pNode->doc);` |
|    11 |  5906 | `		if( pXCtx == 0 ){` |
|   ! 0 |  5907 | `			nOut = -1;` |
|   ! 0 |  5908 | `			goto done;` |
|     - |  5909 | `		}` |
|    11 |  5910 | `		pXCtx->node = pNode;` |
|    11 |  5911 | `		pNs = ph7_array_fetch(pXPath,"namespaces",(int)sizeof("namespaces")-1);` |
|    11 |  5912 | `		if( pNs && ph7_value_is_array(pNs) ){` |
|     3 |  5913 | `			ph7_array_walk(pNs,DomC14NRegisterNs,pXCtx);` |
|     1 |  5914 | `		}` |
|    11 |  5915 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    11 |  5916 | `		pXObj = xmlXPathEvalExpression((const xmlChar *)ph7_value_to_string(pQuery,0),pXCtx);` |
|     - |  5917 | `		/* php lets libxml's own complaint out first ("Invalid expression"), THEN` |
|     - |  5918 | `		 * raises its refusal, so the queue is flushed rather than dropped. */` |
|    11 |  5919 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    11 |  5920 | `		pXCtx->node = 0;` |
|   104 |  5921 | `	}else if( pNode->type != XML_DOCUMENT_NODE ){` |
|    55 |  5922 | `		pXCtx = xmlXPathNewContext(pNode->doc);` |
|    55 |  5923 | `		if( pXCtx == 0 ){` |
|   ! 0 |  5924 | `			nOut = -1;` |
|   ! 0 |  5925 | `			goto done;` |
|     - |  5926 | `		}` |
|    55 |  5927 | `		pXCtx->node = pNode;` |
|    55 |  5928 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    55 |  5929 | `		pXObj = xmlXPathEvalExpression(` |
|    27 |  5930 | `			(const xmlChar *)"(.//. \| .//@* \| .//namespace::*)",pXCtx);` |
|    55 |  5931 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    55 |  5932 | `		pXCtx->node = 0;` |
|    27 |  5933 | `	}` |
|   109 |  5934 | `	if( pXCtx ){` |
|    65 |  5935 | `		if( pXObj == 0 \|\| pXObj->type != XPATH_NODESET ){` |
|     3 |  5936 | `			*pRc = PH7_VmThrowException(pCtx,"Error","XPath query did not return a nodeset");` |
|     3 |  5937 | `			nOut = -1;` |
|     3 |  5938 | `			goto done;` |
|     - |  5939 | `		}` |
|    63 |  5940 | `		pSet = pXObj->nodesetval;` |
|    31 |  5941 | `	}` |
|     - |  5942 | ``	/* php reads `$nsPrefixes` only AFTER the query has been resolved, so a bad`` |
|     - |  5943 | `	 * query's refusal reaches the caller with no notice in front of it. */` |
|   107 |  5944 | `	if( pPrefixes ){` |
|    17 |  5945 | `		if( bExclusive ){` |
|    15 |  5946 | `			ph7_array_walk(pPrefixes,DomC14NCollectPrefix,&sPrefixes);` |
|    15 |  5947 | `			n = SySetUsed(&sPrefixes.aOfs);` |
|    15 |  5948 | `			if( n > 0 ){` |
|    25 |  5949 | `				sPrefixes.apPrefix = (xmlChar **)SyMemBackendAlloc(&pVm->sAllocator,` |
|    12 |  5950 | `					(sxu32)((n+1)*sizeof(xmlChar *)));` |
|    13 |  5951 | `				if( sPrefixes.apPrefix == 0 ){` |
|   ! 0 |  5952 | `					nOut = -1;` |
|   ! 0 |  5953 | `					goto done;` |
|     - |  5954 | `				}` |
|     - |  5955 | `				/* Offsets, not pointers, until the pool has stopped growing. */` |
|    29 |  5956 | `				for( n = 0 ; n < SySetUsed(&sPrefixes.aOfs) ; ++n ){` |
|    25 |  5957 | `					sPrefixes.apPrefix[n] = (xmlChar *)SyBlobData(&sPrefixes.sPool)` |
|    16 |  5958 | `						+ ((sxu32 *)SySetBasePtr(&sPrefixes.aOfs))[n];` |
|     9 |  5959 | `				}` |
|    13 |  5960 | `				sPrefixes.apPrefix[n] = 0;` |
|     6 |  5961 | `			}` |
|     8 |  5962 | `		}else{` |
|     - |  5963 | `			/* php's E_NOTICE, and the list is then ignored outright. */` |
|     3 |  5964 | `			ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|     - |  5965 | `				"Inclusive namespace prefixes only allowed in exclusive mode.");` |
|     - |  5966 | `		}` |
|     8 |  5967 | `	}` |
|     - |  5968 | `	/* Canonicalized into an output buffer of our own rather than through` |
|     - |  5969 | `	 * xmlC14NDocDumpMemory, which is php's shape and one diagnostic quieter:` |
|     - |  5970 | `	 * that wrapper adds an "Internal error : saving doc to output buffer" of` |
|     - |  5971 | `	 * its own on top of libxml's real complaint, and php -- which drives the` |
|     - |  5972 | `	 * save itself -- never prints it. */` |
|     - |  5973 | `	{` |
|   107 |  5974 | `		xmlBufferPtr pBuf = xmlBufferCreate();` |
|   107 |  5975 | `		xmlOutputBufferPtr pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|   107 |  5976 | `		if( pOut == 0 ){` |
|   ! 0 |  5977 | `			if( pBuf ){` |
|   ! 0 |  5978 | `				xmlBufferFree(pBuf);` |
|   ! 0 |  5979 | `			}` |
|   ! 0 |  5980 | `			nOut = -1;` |
|   ! 0 |  5981 | `			goto done;` |
|     - |  5982 | `		}` |
|   107 |  5983 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   160 |  5984 | `		nOut = xmlC14NDocSaveTo(pNode->doc,pSet,` |
|    53 |  5985 | `			bExclusive ? XML_C14N_EXCLUSIVE_1_0 : XML_C14N_1_0,` |
|    53 |  5986 | `			sPrefixes.apPrefix,bComments,pOut);` |
|   107 |  5987 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|   107 |  5988 | `		xmlOutputBufferFlush(pOut);` |
|   107 |  5989 | `		if( nOut >= 0 ){` |
|   101 |  5990 | `			const xmlChar *zBuf = xmlBufferContent(pBuf);` |
|   101 |  5991 | `			nOut = (int)xmlBufferLength(pBuf);` |
|     - |  5992 | `			/* An EMPTY canonicalization is a real answer -- a detached node` |
|     - |  5993 | `			 * is visible from nowhere in the document -- so the bytes are` |
|     - |  5994 | `			 * always allocated, even when there are none. */` |
|   101 |  5995 | `			*pzOut = xmlStrndup(zBuf ? zBuf : (const xmlChar *)"",nOut);` |
|   101 |  5996 | `			if( *pzOut == 0 ){` |
|   ! 0 |  5997 | `				nOut = -1;` |
|   ! 0 |  5998 | `			}` |
|    50 |  5999 | `		}` |
|   107 |  6000 | `		xmlOutputBufferClose(pOut);` |
|   107 |  6001 | `		xmlBufferFree(pBuf);` |
|     - |  6002 | `	}` |
|   107 |  6003 | `	if( nOut < 0 && *pzOut ){` |
|   ! 0 |  6004 | `		xmlFree(*pzOut);` |
|   ! 0 |  6005 | `		*pzOut = 0;` |
|   ! 0 |  6006 | `	}` |
|    53 |  6007 | `done:` |
|   119 |  6008 | `	if( pXObj ){` |
|    63 |  6009 | `		xmlXPathFreeObject(pXObj);` |
|    31 |  6010 | `	}` |
|   119 |  6011 | `	if( pXCtx ){` |
|    65 |  6012 | `		xmlXPathFreeContext(pXCtx);` |
|    32 |  6013 | `	}` |
|   119 |  6014 | `	if( sPrefixes.apPrefix ){` |
|    13 |  6015 | `		SyMemBackendFree(&pVm->sAllocator,(void *)sPrefixes.apPrefix);` |
|     6 |  6016 | `	}` |
|   119 |  6017 | `	SyBlobRelease(&sPrefixes.sPool);` |
|   119 |  6018 | `	SySetRelease(&sPrefixes.aOfs);` |
|   119 |  6019 | `	return nOut;` |
|     1 |  6020 | `}` |
|     - |  6021 | `/*` |
|     - |  6022 | ` * DOMNode::C14N(bool $exclusive = false, bool $withComments = false,` |
|     - |  6023 | ` *               ?array $xpath = null, ?array $nsPrefixes = null): string\|false` |
|     - |  6024 | ` *` |
|     - |  6025 | ` * FALSE when the canonicalization fails, which is the answer a signer has to` |
|     - |  6026 | ` * be able to tell from a document that canonicalizes to nothing: a detached` |
|     - |  6027 | ` * node and a fragment are both the EMPTY STRING (nothing of either is visible` |
|     - |  6028 | ` * from the document, and that is a real answer), while an entity REFERENCE` |
|     - |  6029 | ` * anywhere in the tree -- an ordinary document parsed without` |
|     - |  6030 | `` * `substituteEntities` -- is a refusal libxml states and php reports as false.`` |
|     - |  6031 | ` * Answering "" for both signed the empty string instead of failing.` |
|     - |  6032 | ` */` |
|   100 |  6033 | `DOM_METHOD(vm_builtin_DOMNode_C14N)` |
|     1 |  6034 | `{` |
|   101 |  6035 | `	xmlChar *zOut = 0;` |
|   101 |  6036 | `	int rc = PH7_OK;` |
|   101 |  6037 | `	int nOut = DomC14NRun(pCtx,nArg,apArg,3,"DOMNode::C14N",&zOut,&rc);` |
|   101 |  6038 | `	if( rc != PH7_OK ){` |
|    11 |  6039 | `		return rc;` |
|     - |  6040 | `	}` |
|    91 |  6041 | `	if( nOut < 0 ){` |
|     5 |  6042 | `		ph7_result_bool(pCtx,0);` |
|     5 |  6043 | `		return PH7_OK;` |
|     - |  6044 | `	}` |
|    87 |  6045 | `	ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|    87 |  6046 | `	xmlFree(zOut);` |
|    87 |  6047 | `	return PH7_OK;` |
|    51 |  6048 | `}` |
|     - |  6049 | `/*` |
|     - |  6050 | ` * DOMNode::C14NFile(string $uri, bool $exclusive = false,` |
|     - |  6051 | ` *                   bool $withComments = false, ?array $xpath = null,` |
|     - |  6052 | ` *                   ?array $nsPrefixes = null): int\|false` |
|     - |  6053 | ` *` |
|     - |  6054 | ` * The same canonicalization written to a destination instead of answered, and` |
|     - |  6055 | ` * the byte count rather than the bytes. The destination goes through the stream` |
|     - |  6056 | `` * layer -- php's libxml I/O is wired to php's streams, so `php://stdout` and a`` |
|     - |  6057 | ` * userland wrapper are both valid here -- which is also where php's` |
|     - |  6058 | ` * "Failed to open stream" warning comes from.` |
|     - |  6059 | ` */` |
|    22 |  6060 | `DOM_METHOD(vm_builtin_DOMNode_C14NFile)` |
|     1 |  6061 | `{` |
|    23 |  6062 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  6063 | `	const ph7_io_stream *pStream;` |
|     - |  6064 | `	void *pHandle;` |
|    23 |  6065 | `	xmlChar *zOut = 0;` |
|     - |  6066 | `	const char *zFile;` |
|    23 |  6067 | `	int nFile = 0,nOut,rc = PH7_OK;` |
|    23 |  6068 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|    23 |  6069 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|     3 |  6070 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  6071 | `			"DOMNode::C14NFile(): Argument #1 ($uri) must not contain any null bytes");` |
|     - |  6072 | `	}` |
|    21 |  6073 | `	if( nFile < 1 ){` |
|     3 |  6074 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|     - |  6075 | `	}` |
|    19 |  6076 | `	nOut = DomC14NRun(pCtx,nArg,apArg,4,"DOMNode::C14NFile",&zOut,&rc);` |
|    19 |  6077 | `	if( rc != PH7_OK ){` |
|     3 |  6078 | `		return rc;` |
|     - |  6079 | `	}` |
|    17 |  6080 | `	if( nOut < 0 ){` |
|     3 |  6081 | `		ph7_result_bool(pCtx,0);` |
|     3 |  6082 | `		return PH7_OK;` |
|     - |  6083 | `	}` |
|    15 |  6084 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|    15 |  6085 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|     - |  6086 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|    14 |  6087 | `		ph7_function_name(pCtx)) : 0;` |
|    15 |  6088 | `	if( pHandle == 0 ){` |
|     5 |  6089 | `		xmlFree(zOut);` |
|     5 |  6090 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|     5 |  6091 | `		ph7_result_bool(pCtx,0);` |
|     5 |  6092 | `		return PH7_OK;` |
|     - |  6093 | `	}` |
|    11 |  6094 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|   ! 0 |  6095 | `		nOut = -1;` |
|   ! 0 |  6096 | `	}` |
|    11 |  6097 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|    11 |  6098 | `	xmlFree(zOut);` |
|    11 |  6099 | `	if( nOut < 0 ){` |
|   ! 0 |  6100 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  6101 | `		return PH7_OK;` |
|     - |  6102 | `	}` |
|    11 |  6103 | `	ph7_result_int(pCtx,nOut);` |
|    11 |  6104 | `	return PH7_OK;` |
|    12 |  6105 | `}` |
|     - |  6106 | `/*` |
|     - |  6107 | ` * DOMNode::__sleep(): array and DOMNode::__wakeup(): void` |
|     - |  6108 | ` *` |
|     - |  6109 | ` * php declares both on DOMNode and both do one thing: refuse. They are the` |
|     - |  6110 | ``  * MECHANISM behind the refusal, not decoration -- `serialize()` finds `__sleep` `` |
|     - |  6111 | `` * and `unserialize()` calls `__wakeup`, which is why a subclass that declares`` |
|     - |  6112 | ` * its own escapes both. Without them, PHL refused serialize() from its own deny` |
|     - |  6113 | ` * handler (same sentence) but UNSERIALIZE went through in silence and handed` |
|     - |  6114 | ` * back a DOM object with no node behind it, which then answered nothing for` |
|     - |  6115 | ` * every property a program read off it.` |
|     - |  6116 | ` */` |
|    30 |  6117 | `DOM_METHOD(vm_builtin_DOMNode_sleep)` |
|     1 |  6118 | `{` |
|    31 |  6119 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    15 |  6120 | `	SXUNUSED(nArg);` |
|    15 |  6121 | `	SXUNUSED(apArg);` |
|    31 |  6122 | `	if( pThis == 0 ){` |
|   ! 0 |  6123 | `		return PH7_OK;` |
|     - |  6124 | `	}` |
|    46 |  6125 | `	return PH7_VmThrowException(pCtx,"Exception",` |
|     - |  6126 | `		"Serialization of '%z' is not allowed, unless serialization methods "` |
|    30 |  6127 | `		"are implemented in a subclass",&pThis->pClass->sName);` |
|    16 |  6128 | `}` |
|    16 |  6129 | `DOM_METHOD(vm_builtin_DOMNode_wakeup)` |
|     1 |  6130 | `{` |
|    17 |  6131 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     8 |  6132 | `	SXUNUSED(nArg);` |
|     8 |  6133 | `	SXUNUSED(apArg);` |
|    17 |  6134 | `	if( pThis == 0 ){` |
|   ! 0 |  6135 | `		return PH7_OK;` |
|     - |  6136 | `	}` |
|    25 |  6137 | `	return PH7_VmThrowException(pCtx,"Exception",` |
|     - |  6138 | `		"Unserialization of '%z' is not allowed, unless unserialization methods "` |
|    16 |  6139 | `		"are implemented in a subclass",&pThis->pClass->sName);` |
|     9 |  6140 | `}` |
|     - |  6141 |  |
|     - |  6142 | `/* ===== DOMNodeList and DOMNamedNodeMap ===== */` |
|     - |  6143 |  |
|     - |  6144 | `/*` |
|     - |  6145 | ` * A node list is one of three things, and which one it is decides both count()` |
|     - |  6146 | ` * and item(). Two of the three are LIVE views (they re-walk the tree on every` |
|     - |  6147 | ` * question, which is what makes getElementsByTagName track mutations); the third` |
|     - |  6148 | ` * is the document-order snapshot DOMXPath::query froze.` |
|     - |  6149 | ` */` |
|     - |  6150 | `#define DNL_CHILD 0   /* $node->childNodes */` |
|     - |  6151 | `#define DNL_GEBTN 1   /* getElementsByTagName($name) */` |
|     - |  6152 | `#define DNL_SNAP  2   /* DOMXPath::query() */` |
|     - |  6153 | `#define DNL_GEBTNNS 3 /* getElementsByTagNameNS($uri, $localName) */` |
|     - |  6154 | `/* ...and a NAMED map is one of two: an element's attribute list, or one of the` |
|     - |  6155 | ` * two DTD declaration TABLES, which are libxml hash tables rather than node` |
|     - |  6156 | ` * lists -- the reason a parameter entity, which is a child of the DTD like` |
|     - |  6157 | `` * every other declaration, is not in `entities`. */`` |
|     - |  6158 | `#define DNL_ENTS  4   /* $doctype->entities */` |
|     - |  6159 | `#define DNL_NOTS  5   /* $doctype->notations */` |
|     - |  6160 | `#define DNL_KIND  "__kind"` |
|     - |  6161 | `#define DNL_OWNER "__owner"` |
|     - |  6162 | `#define DNL_NAME  "__name"` |
|     - |  6163 | `#define DNL_URI   "__uri"` |
|     - |  6164 | `#define DNL_SNAP_SLOT "__snap"` |
|     - |  6165 |  |
|     - |  6166 | `/* The node a live list is a view OF. */` |
|  1190 |  6167 | `static phl_domnode * DomListOwner(ph7_class_instance *pList)` |
|     4 |  6168 | `{` |
|  1194 |  6169 | `	return DomResOf(PH7_NativeAttrObj(pList,DNL_OWNER));` |
|     4 |  6170 | `}` |
|   176 |  6171 | `static ph7_hashmap * DomListSnap(ph7_class_instance *pList)` |
|     1 |  6172 | `{` |
|   177 |  6173 | `	ph7_value *pVal = PH7_NativeAttr(pList,DNL_SNAP_SLOT);` |
|   177 |  6174 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 |  6175 | `		return 0;` |
|     - |  6176 | `	}` |
|   177 |  6177 | `	return (ph7_hashmap *)pVal->x.pOther;` |
|    89 |  6178 | `}` |
|   272 |  6179 | `static int DomListCount(ph7_class_instance *pList)` |
|     2 |  6180 | `{` |
|     - |  6181 | `	phl_domnode *pOwner;` |
|   274 |  6182 | `	const char *zName,*zUri = 0;` |
|   274 |  6183 | `	int nName,nUri = -1,iCount = 0;` |
|   274 |  6184 | `	if( pList == 0 ){` |
|   ! 0 |  6185 | `		return 0;` |
|     - |  6186 | `	}` |
|   274 |  6187 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){` |
|    69 |  6188 | `		ph7_hashmap *pMap = DomListSnap(pList);` |
|    69 |  6189 | `		return pMap ? (int)pMap->nEntry : 0;` |
|     - |  6190 | `	}` |
|   206 |  6191 | `	pOwner = DomListOwner(pList);` |
|   206 |  6192 | `	if( pOwner == 0 ){` |
|     3 |  6193 | `		return 0;` |
|     - |  6194 | `	}` |
|   204 |  6195 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){` |
|   134 |  6196 | `		return DomChildCount((xmlNodePtr)pOwner->pNode,0);` |
|     - |  6197 | `	}` |
|    71 |  6198 | `	PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);` |
|    71 |  6199 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_GEBTNNS ){` |
|    49 |  6200 | `		PH7_NativeAttrStr(pList,DNL_URI,&zUri,&nUri);` |
|    24 |  6201 | `	}` |
|    71 |  6202 | `	DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,-1,&iCount);` |
|    71 |  6203 | `	return iCount;` |
|   138 |  6204 | `}` |
|     - |  6205 | `/* The wrapper at one index, or NULL past the end. BORROWED, like every wrap. */` |
|   718 |  6206 | `static ph7_class_instance * DomListItem(ph7_vm *pVm,ph7_class_instance *pList,int iIndex)` |
|     2 |  6207 | `{` |
|     - |  6208 | `	ph7_class_instance *pDoc;` |
|     - |  6209 | `	phl_domnode *pOwner;` |
|   720 |  6210 | `	xmlNodePtr pNode = 0;` |
|   720 |  6211 | `	const char *zName,*zUri = 0;` |
|   720 |  6212 | `	int nName,nUri = -1;` |
|   720 |  6213 | `	if( pList == 0 \|\| iIndex < 0 ){` |
|   ! 0 |  6214 | `		return 0;` |
|     - |  6215 | `	}` |
|   720 |  6216 | `	pDoc = PH7_NativeAttrObj(pList,DOM_DOC);` |
|   720 |  6217 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){` |
|   109 |  6218 | `		ph7_hashmap *pMap = DomListSnap(pList);` |
|   109 |  6219 | `		ph7_hashmap_node *pEntry = 0;` |
|     - |  6220 | `		ph7_value sKey,*pHit;` |
|     - |  6221 | `		phl_domnode *pRes;` |
|   109 |  6222 | `		if( pMap == 0 ){` |
|   ! 0 |  6223 | `			return 0;` |
|     - |  6224 | `		}` |
|   109 |  6225 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)iIndex);` |
|   109 |  6226 | `		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){` |
|    21 |  6227 | `			pEntry = 0;` |
|    10 |  6228 | `		}` |
|   109 |  6229 | `		PH7_MemObjRelease(&sKey);` |
|   109 |  6230 | `		pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;` |
|   109 |  6231 | `		if( pHit && (pHit->iFlags & MEMOBJ_OBJ) ){` |
|     - |  6232 | `			/* A namespace:: axis entry holds the DOMNameSpaceNode ITSELF (a` |
|     - |  6233 | `			 * fresh object per query, one object per list -- php's answer);` |
|     - |  6234 | `			 * the snapshot owns it, so this stays a borrow like DomWrap's. */` |
|    39 |  6235 | `			return (ph7_class_instance *)pHit->x.pOther;` |
|     - |  6236 | `		}` |
|    71 |  6237 | `		pRes = (pHit && (pHit->iFlags & MEMOBJ_RES)) ? (phl_domnode *)pHit->x.pOther : 0;` |
|    71 |  6238 | `		return pRes ? DomWrap(&(*pVm),pDoc,pRes->pShell,(xmlNodePtr)pRes->pNode) : 0;` |
|     - |  6239 | `	}` |
|   612 |  6240 | `	pOwner = DomListOwner(pList);` |
|   612 |  6241 | `	if( pOwner == 0 ){` |
|    13 |  6242 | `		return 0;` |
|     - |  6243 | `	}` |
|   600 |  6244 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){` |
|   332 |  6245 | `		pNode = DomChildAt((xmlNodePtr)pOwner->pNode,iIndex);` |
|   167 |  6246 | `	}else{` |
|   269 |  6247 | `		PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);` |
|   269 |  6248 | `		if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_GEBTNNS ){` |
|   147 |  6249 | `			PH7_NativeAttrStr(pList,DNL_URI,&zUri,&nUri);` |
|    73 |  6250 | `		}` |
|   269 |  6251 | `		pNode = DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,iIndex,0);` |
|     - |  6252 | `	}` |
|   600 |  6253 | `	return DomWrap(&(*pVm),pDoc,pOwner->pShell,pNode);` |
|   361 |  6254 | `}` |
|     - |  6255 | `/*` |
|     - |  6256 | ` * Build one. pOwnerObj is the node the live view is of (NULL for a snapshot),` |
|     - |  6257 | ` * pSnap the frozen list (NULL otherwise). The caller owns the reference.` |
|     - |  6258 | ` */` |
|   626 |  6259 | `static ph7_class_instance * DomNewCollection(ph7_vm *pVm,const char *zClass,` |
|     - |  6260 | `	ph7_class_instance *pDoc,int iKind,ph7_class_instance *pOwnerObj,` |
|     - |  6261 | `	const char *zName,const char *zUri,ph7_value *pSnap)` |
|     4 |  6262 | `{` |
|   630 |  6263 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|   630 |  6264 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|   630 |  6265 | `	if( pObj == 0 ){` |
|   ! 0 |  6266 | `		return 0;` |
|     - |  6267 | `	}` |
|   630 |  6268 | `	PH7_NativeSetAttrInt(&(*pVm),pObj,DNL_KIND,iKind);` |
|   630 |  6269 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|   630 |  6270 | `	if( pOwnerObj ){` |
|   536 |  6271 | `		PH7_NativeSetAttrObj(&(*pVm),pObj,DNL_OWNER,pOwnerObj);` |
|   266 |  6272 | `	}` |
|   630 |  6273 | `	if( zName ){` |
|   147 |  6274 | `		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_NAME,zName,(int)SyStrlen(zName));` |
|    73 |  6275 | `	}` |
|   630 |  6276 | `	if( zUri ){` |
|    45 |  6277 | `		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_URI,zUri,(int)SyStrlen(zUri));` |
|    22 |  6278 | `	}` |
|   630 |  6279 | `	if( pSnap ){` |
|    95 |  6280 | `		ph7_value *pSlot = PH7_NativeAttr(pObj,DNL_SNAP_SLOT);` |
|    95 |  6281 | `		if( pSlot ){` |
|    95 |  6282 | `			PH7_MemObjStore(pSnap,pSlot);` |
|    47 |  6283 | `		}` |
|    47 |  6284 | `	}` |
|   630 |  6285 | `	return pObj;` |
|   317 |  6286 | `}` |
|     - |  6287 | `/* DOMNodeList::count(): int and ::item(int $index): ?DOMNode */` |
|    10 |  6288 | `DOM_METHOD(vm_builtin_DOMNodeList_count)` |
|     1 |  6289 | `{` |
|     5 |  6290 | `	SXUNUSED(nArg);` |
|     5 |  6291 | `	SXUNUSED(apArg);` |
|    11 |  6292 | `	ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));` |
|    11 |  6293 | `	return PH7_OK;` |
|     1 |  6294 | `}` |
|     - |  6295 | `/*` |
|     - |  6296 | ` * The index both collections take, screened before it is narrowed.` |
|     - |  6297 | ` *` |
|     - |  6298 | `` * php's is a `int` position in a list that cannot hold more than INT_MAX`` |
|     - |  6299 | ` * entries, so everything outside [0, INT_MAX] is out of range -- and the two` |
|     - |  6300 | ` * classes then disagree about what to DO with one: the list answers null and` |
|     - |  6301 | ` * the named map raises a ValueError naming the bound.  Narrowing first was a` |
|     - |  6302 | ``  * silent wrong answer either way: `item(4294967296)` and `item(PHP_INT_MIN)` `` |
|     - |  6303 | ` * truncate to 0 and answered the FIRST node of the collection.` |
|     - |  6304 | ` *` |
|     - |  6305 | ` * Answers 1 when the index is usable.` |
|     - |  6306 | ` */` |
|     - |  6307 | `#define DOM_INDEX_MAX 2147483647` |
|   396 |  6308 | `static int DomCollectionIndex(int nArg,ph7_value **apArg,int *piIndex)` |
|     2 |  6309 | `{` |
|   398 |  6310 | `	ph7_int64 iWant = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|   398 |  6311 | `	*piIndex = 0;` |
|   398 |  6312 | `	if( iWant < 0 \|\| iWant > DOM_INDEX_MAX ){` |
|    23 |  6313 | `		return 0;` |
|     - |  6314 | `	}` |
|   376 |  6315 | `	*piIndex = (int)iWant;` |
|   376 |  6316 | `	return 1;` |
|   200 |  6317 | `}` |
|   312 |  6318 | `DOM_METHOD(vm_builtin_DOMNodeList_item)` |
|     1 |  6319 | `{` |
|     - |  6320 | `	int iIndex;` |
|   313 |  6321 | `	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){` |
|    13 |  6322 | `		ph7_result_null(pCtx);` |
|    13 |  6323 | `		return PH7_OK;` |
|     - |  6324 | `	}` |
|   301 |  6325 | `	return DomResultWrap(pCtx,DomListItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));` |
|   157 |  6326 | `}` |
|     - |  6327 | ``/* php exposes `length` on both collections as a virtual property; the`` |
|     - |  6328 | ` * DOM_PROP_ACCESSORS pair below turns each recognizer into __get + __isset. */` |
|   266 |  6329 | `static int DomListProp(ph7_context *pCtx,const char *zName)` |
|     2 |  6330 | `{` |
|   268 |  6331 | `	if( DomNameIs(zName,"length") ){` |
|   264 |  6332 | `		ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));` |
|   264 |  6333 | `		return 1;` |
|     - |  6334 | `	}` |
|     5 |  6335 | `	return 0;` |
|   135 |  6336 | `}` |
|     - |  6337 | `/*` |
|     - |  6338 | ` * DOMNamedNodeMap: an element's attributes, keyed by name.` |
|     - |  6339 | ` *` |
|     - |  6340 | ` * It shares DOMNodeList's slots (the owner element in $__owner) but walks the` |
|     - |  6341 | ` * attribute list rather than the child list, so it gets its own two readers.` |
|     - |  6342 | ` */` |
|     - |  6343 | `/* Is this map one of the DTD DECLARATION tables rather than an element's` |
|     - |  6344 | ` * attribute list? The two are walked with entirely different machinery. */` |
|   468 |  6345 | `static int DomMapIsTable(ph7_class_instance *pMap)` |
|     3 |  6346 | `{` |
|   471 |  6347 | `	sxi64 iKind = pMap ? PH7_NativeAttrInt(pMap,DNL_KIND) : (sxi64)DNL_CHILD;` |
|   471 |  6348 | `	return iKind == DNL_ENTS \|\| iKind == DNL_NOTS;` |
|     3 |  6349 | `}` |
|     - |  6350 | `/*` |
|     - |  6351 | ` * The table itself, which is NULL for a doctype that declares nothing of that` |
|     - |  6352 | ` * kind: libxml allocates the hash only when the first declaration arrives, so` |
|     - |  6353 | ` * an absent table is an EMPTY map and not an error.` |
|     - |  6354 | ` */` |
|    94 |  6355 | `static xmlHashTablePtr DomMapHash(ph7_class_instance *pMap,phl_domnode *pOwner)` |
|     2 |  6356 | `{` |
|    96 |  6357 | `	xmlDtdPtr pDtd = pOwner ? (xmlDtdPtr)pOwner->pNode : 0;` |
|    94 |  6358 | `	if( !DomMapIsTable(pMap) \|\| pDtd == 0` |
|    96 |  6359 | `	 \|\| (pDtd->type != XML_DTD_NODE && pDtd->type != XML_DOCUMENT_TYPE_NODE) ){` |
|   ! 0 |  6360 | `		return 0;` |
|     - |  6361 | `	}` |
|    96 |  6362 | `	return (xmlHashTablePtr)(PH7_NativeAttrInt(pMap,DNL_KIND) == DNL_NOTS` |
|    47 |  6363 | `		? pDtd->notations : pDtd->entities);` |
|    49 |  6364 | `}` |
|     - |  6365 | `/*` |
|     - |  6366 | ` * A NOTATION declaration answered as a node.` |
|     - |  6367 | ` *` |
|     - |  6368 | `` * libxml's xmlNotation is `{name, PublicID, SystemID}` -- three strings and no`` |
|     - |  6369 | ` * type field -- so it cannot be handed to anything that walks a node.  php` |
|     - |  6370 | ` * builds an entity-shaped stand-in around it (the two structs share their` |
|     - |  6371 | ` * header, which is why the same reader answers both) with NO document and NO` |
|     - |  6372 | `` * parent, and that absence is php-visible: a notation's `ownerDocument` is`` |
|     - |  6373 | `` * null, its `isConnected` false and its `getRootNode()` itself.`` |
|     - |  6374 | ` *` |
|     - |  6375 | ` * php builds a FRESH one per lookup and frees it with the object; PHL builds` |
|     - |  6376 | ` * one per declaration and keeps it on the document's shell, so the wrapper` |
|     - |  6377 | `` * identity every other node has holds here too (PLAN §7.4: `$map->item(0) ===`` |
|     - |  6378 | `` * $map->item(0)` is true here and false there).`` |
|     - |  6379 | ` */` |
|    34 |  6380 | `static xmlNodePtr DomNotationNode(phl_xmldoc *pShell,xmlNotationPtr pNot)` |
|     2 |  6381 | `{` |
|     - |  6382 | `	xmlEntityPtr *apHave;` |
|     - |  6383 | `	xmlEntityPtr pNode;` |
|     - |  6384 | `	sxu32 n;` |
|    36 |  6385 | `	if( pShell == 0 \|\| pNot == 0 ){` |
|   ! 0 |  6386 | `		return 0;` |
|     - |  6387 | `	}` |
|    36 |  6388 | `	apHave = (xmlEntityPtr *)SySetBasePtr(&pShell->aNotations);` |
|    54 |  6389 | `	for( n = 0 ; n < SySetUsed(&pShell->aNotations) ; ++n ){` |
|    44 |  6390 | `		if( apHave[n]->_private == (void *)pNot ){` |
|    26 |  6391 | `			return (xmlNodePtr)apHave[n];` |
|     - |  6392 | `		}` |
|    11 |  6393 | `	}` |
|    12 |  6394 | `	pNode = (xmlEntityPtr)xmlMalloc(sizeof(xmlEntity));` |
|    12 |  6395 | `	if( pNode == 0 ){` |
|   ! 0 |  6396 | `		return 0;` |
|     - |  6397 | `	}` |
|    12 |  6398 | `	SyZero(pNode,sizeof(xmlEntity));` |
|    12 |  6399 | `	pNode->type = XML_NOTATION_NODE;` |
|    12 |  6400 | `	pNode->name = xmlStrdup(pNot->name);` |
|    12 |  6401 | `	pNode->ExternalID = xmlStrdup(pNot->PublicID);` |
|    12 |  6402 | `	pNode->SystemID = xmlStrdup(pNot->SystemID);` |
|     - |  6403 | `	/* The declaration this stands for, so a second lookup finds it again. */` |
|    12 |  6404 | `	pNode->_private = (void *)pNot;` |
|    12 |  6405 | `	if( SySetPut(&pShell->aNotations,(const void *)&pNode) != SXRET_OK ){` |
|   ! 0 |  6406 | `		if( pNode->name ){` |
|   ! 0 |  6407 | `			xmlFree((xmlChar *)pNode->name);` |
|   ! 0 |  6408 | `		}` |
|   ! 0 |  6409 | `		if( pNode->ExternalID ){` |
|   ! 0 |  6410 | `			xmlFree((xmlChar *)pNode->ExternalID);` |
|   ! 0 |  6411 | `		}` |
|   ! 0 |  6412 | `		if( pNode->SystemID ){` |
|   ! 0 |  6413 | `			xmlFree((xmlChar *)pNode->SystemID);` |
|   ! 0 |  6414 | `		}` |
|   ! 0 |  6415 | `		xmlFree(pNode);` |
|   ! 0 |  6416 | `		return 0;` |
|     - |  6417 | `	}` |
|    12 |  6418 | `	return (xmlNodePtr)pNode;` |
|    19 |  6419 | `}` |
|     - |  6420 | `/* The payload a table map hands back, as a node: an entity declaration IS one,` |
|     - |  6421 | ` * a notation declaration needs its stand-in. */` |
|    82 |  6422 | `static xmlNodePtr DomTablePayload(ph7_class_instance *pMap,phl_domnode *pOwner,void *pPayload)` |
|     2 |  6423 | `{` |
|    84 |  6424 | `	if( pPayload == 0 ){` |
|    15 |  6425 | `		return 0;` |
|     - |  6426 | `	}` |
|    70 |  6427 | `	if( PH7_NativeAttrInt(pMap,DNL_KIND) == DNL_NOTS ){` |
|    36 |  6428 | `		return DomNotationNode(pOwner->pShell,(xmlNotationPtr)pPayload);` |
|     - |  6429 | `	}` |
|    35 |  6430 | `	return (xmlNodePtr)pPayload;` |
|    43 |  6431 | `}` |
|     - |  6432 | `/*` |
|     - |  6433 | ` * php walks these tables with xmlHashScan and takes the n-th thing it is` |
|     - |  6434 | ` * handed, so the ORDER a map answers in is the hash's and not the document's.` |
|     - |  6435 | ` * The same walk, so the same order.` |
|     - |  6436 | ` */` |
|     - |  6437 | `typedef struct DomHashPick DomHashPick;` |
|     - |  6438 | `struct DomHashPick {` |
|     - |  6439 | `	int iWant;              /* index still to be stepped over */` |
|     - |  6440 | `	void *pHit;             /* the payload at index 0 of what is left */` |
|     - |  6441 | `};` |
|    90 |  6442 | `static void DomHashPickOne(void *pPayload,void *pData,const xmlChar *zName)` |
|     2 |  6443 | `{` |
|    92 |  6444 | `	DomHashPick *pPick = (DomHashPick *)pData;` |
|    45 |  6445 | `	SXUNUSED(zName);` |
|    92 |  6446 | `	if( pPick->iWant > 0 ){` |
|    19 |  6447 | `		pPick->iWant--;` |
|    83 |  6448 | `	}else if( pPick->pHit == 0 ){` |
|    34 |  6449 | `		pPick->pHit = pPayload;` |
|    16 |  6450 | `	}` |
|    92 |  6451 | `}` |
|    40 |  6452 | `static void * DomHashAt(xmlHashTablePtr pTab,int iIndex)` |
|     2 |  6453 | `{` |
|     - |  6454 | `	DomHashPick sPick;` |
|    42 |  6455 | `	if( pTab == 0 \|\| iIndex < 0 \|\| iIndex >= xmlHashSize(pTab) ){` |
|     9 |  6456 | `		return 0;` |
|     - |  6457 | `	}` |
|    34 |  6458 | `	sPick.iWant = iIndex;` |
|    34 |  6459 | `	sPick.pHit = 0;` |
|    34 |  6460 | `	xmlHashScan(pTab,DomHashPickOne,&sPick);` |
|    34 |  6461 | `	return sPick.pHit;` |
|    22 |  6462 | `}` |
|   250 |  6463 | `static ph7_class_instance * DomMapItem(ph7_vm *pVm,ph7_class_instance *pMap,int iIndex)` |
|     3 |  6464 | `{` |
|   253 |  6465 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|     - |  6466 | `	xmlNodePtr pNode;` |
|   253 |  6467 | `	if( pOwner == 0 \|\| iIndex < 0 ){` |
|     3 |  6468 | `		return 0;` |
|     - |  6469 | `	}` |
|   251 |  6470 | `	pNode = DomMapIsTable(pMap)` |
|    40 |  6471 | `		? DomTablePayload(pMap,pOwner,DomHashAt(DomMapHash(pMap,pOwner),iIndex))` |
|   228 |  6472 | `		: (xmlNodePtr)DomAttrAt((xmlNodePtr)pOwner->pNode,iIndex);` |
|   251 |  6473 | `	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,pNode);` |
|   128 |  6474 | `}` |
|    28 |  6475 | `static int DomMapCount(ph7_class_instance *pMap)` |
|     1 |  6476 | `{` |
|    29 |  6477 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|    29 |  6478 | `	if( DomMapIsTable(pMap) ){` |
|    13 |  6479 | `		xmlHashTablePtr pTab = DomMapHash(pMap,pOwner);` |
|    13 |  6480 | `		return pTab ? xmlHashSize(pTab) : 0;` |
|     - |  6481 | `	}` |
|    17 |  6482 | `	return pOwner ? DomAttrCount((xmlNodePtr)pOwner->pNode) : 0;` |
|    15 |  6483 | `}` |
|     6 |  6484 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_count)` |
|     1 |  6485 | `{` |
|     3 |  6486 | `	SXUNUSED(nArg);` |
|     3 |  6487 | `	SXUNUSED(apArg);` |
|     7 |  6488 | `	ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));` |
|     7 |  6489 | `	return PH7_OK;` |
|     1 |  6490 | `}` |
|    84 |  6491 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_item)` |
|     2 |  6492 | `{` |
|     - |  6493 | `	int iIndex;` |
|    86 |  6494 | `	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){` |
|     - |  6495 | `		/* The map REFUSES what the list answers null for, and names the bound. */` |
|    11 |  6496 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  6497 | `			"DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and %d",` |
|     - |  6498 | `			DOM_INDEX_MAX);` |
|     - |  6499 | `	}` |
|    76 |  6500 | `	return DomResultWrap(pCtx,DomMapItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));` |
|    44 |  6501 | `}` |
|     - |  6502 | `/*` |
|     - |  6503 | `` * The by-NAME lookup, which `getNamedItem()` and the `$map['href']` subscript`` |
|     - |  6504 | ` * share.` |
|     - |  6505 | ` *` |
|     - |  6506 | ` * The MAP asks libxml's name-only question, where DOMElement's own` |
|     - |  6507 | `` * getAttributeNode resolves the prefix: `getNamedItem('k')` finds the`` |
|     - |  6508 | `` * namespaced `p:k` that `getAttribute('k')` does not. A DECLARATION table is`` |
|     - |  6509 | ` * keyed by that name to begin with, so it is one lookup.` |
|     - |  6510 | ` */` |
|    82 |  6511 | `static ph7_class_instance * DomMapNamed(ph7_vm *pVm,ph7_class_instance *pMap,const char *zName)` |
|     2 |  6512 | `{` |
|    84 |  6513 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|     - |  6514 | `	xmlNodePtr pHit;` |
|    84 |  6515 | `	if( pOwner == 0 ){` |
|   ! 0 |  6516 | `		return 0;` |
|     - |  6517 | `	}` |
|    84 |  6518 | `	pHit = DomMapIsTable(pMap)` |
|    54 |  6519 | `		? DomTablePayload(pMap,pOwner,` |
|    18 |  6520 | `			xmlHashLookup(DomMapHash(pMap,pOwner),(const xmlChar *)zName))` |
|    64 |  6521 | `		: (xmlNodePtr)DomAttrByLocal((xmlNodePtr)pOwner->pNode,zName);` |
|    84 |  6522 | `	if( pHit == 0 ){` |
|    29 |  6523 | `		return 0;` |
|     - |  6524 | `	}` |
|    56 |  6525 | `	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,pHit);` |
|    43 |  6526 | `}` |
|     - |  6527 | `/* DOMNamedNodeMap::getNamedItem(string $qualifiedName): ?DOMAttr */` |
|    42 |  6528 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItem)` |
|     2 |  6529 | `{` |
|    44 |  6530 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    44 |  6531 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    44 |  6532 | `	return DomResultWrap(pCtx,DomMapNamed(pCtx->pVm,pThis,zName));` |
|     2 |  6533 | `}` |
|     - |  6534 | `/* DOMNamedNodeMap::getNamedItemNS(?string $namespace, string $localName): ?DOMNode */` |
|    16 |  6535 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItemNS)` |
|     1 |  6536 | `{` |
|    17 |  6537 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    17 |  6538 | `	phl_domnode *pOwner = pThis ? DomListOwner(pThis) : 0;` |
|    17 |  6539 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    17 |  6540 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     - |  6541 | `	/* A NULL namespace is the map's ANY here, not the element's "in no` |
|     - |  6542 | ``	 * namespace": `$el->attributes->getNamedItemNS(null,'k')` answers a`` |
|     - |  6543 | ``	 * namespaced `p:k` where `$el->getAttributeNodeNS(null,'k')` answers null.`` |
|     - |  6544 | `	 * An EMPTY namespace is neither -- it matches a URI no document has.` |
|     - |  6545 | `	 * A DECLARATION table has no namespaces at all and php reads right past` |
|     - |  6546 | `	 * the argument there: the URI decides nothing, the name decides` |
|     - |  6547 | `	 * everything. */` |
|    25 |  6548 | `	xmlNodePtr pHit = pOwner == 0 ? 0` |
|    29 |  6549 | `		: DomMapIsTable(pThis)` |
|     9 |  6550 | `			? DomTablePayload(pThis,pOwner,` |
|     3 |  6551 | `				xmlHashLookup(DomMapHash(pThis,pOwner),(const xmlChar *)zLocal))` |
|    18 |  6552 | `		: zUri ? (xmlNodePtr)DomAttrByNs((xmlNodePtr)pOwner->pNode,zUri,zLocal)` |
|     6 |  6553 | `		       : (xmlNodePtr)DomAttrByLocal((xmlNodePtr)pOwner->pNode,zLocal);` |
|    17 |  6554 | `	if( pHit == 0 ){` |
|     7 |  6555 | `		ph7_result_null(pCtx);` |
|     7 |  6556 | `		return PH7_OK;` |
|     - |  6557 | `	}` |
|    16 |  6558 | `	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,PH7_NativeAttrObj(pThis,DOM_DOC),` |
|     5 |  6559 | `		pOwner->pShell,pHit));` |
|     9 |  6560 | `}` |
|    26 |  6561 | `static int DomMapProp(ph7_context *pCtx,const char *zName)` |
|     1 |  6562 | `{` |
|    27 |  6563 | `	if( DomNameIs(zName,"length") ){` |
|    23 |  6564 | `		ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));` |
|    23 |  6565 | `		return 1;` |
|     - |  6566 | `	}` |
|     5 |  6567 | `	return 0;` |
|    14 |  6568 | `}` |
|     - |  6569 | `/*` |
|     - |  6570 | `` * `$list[0]`, `$map['href']` and `isset($list[0])` -- ph7_class::xDim for the`` |
|     - |  6571 | ` * two collections, which are php's read_dimension / has_dimension handlers.` |
|     - |  6572 | ` *` |
|     - |  6573 | ` * php 8.3 gave both classes those handlers WITHOUT declaring ArrayAccess, so` |
|     - |  6574 | `` * `$list instanceof ArrayAccess` is FALSE there and the subscript reads anyway`` |
|     - |  6575 | ` * -- which is how every modern DOM example is written, and which no interface` |
|     - |  6576 | ` * list can state. Only the READ half exists: a store, an append and an unset` |
|     - |  6577 | `` * are all `Cannot use object of type C as array`, which is what the opcode`` |
|     - |  6578 | ` * answers for a class carrying no ArrayAccess.` |
|     - |  6579 | ` *` |
|     - |  6580 | ` * What an offset MEANS is one rule for both classes, and it is not the array` |
|     - |  6581 | `` * one. A STRING that STARTS with a number is an INDEX -- `"1x"` is 1 and`` |
|     - |  6582 | `` * `" 2 "` is 2, silently, php's is_numeric_string with errors allowed -- and`` |
|     - |  6583 | ` * one that does not is a NAME, which the list has no door for at all (so` |
|     - |  6584 | `` * `$list['x']` is null even on a document whose child element is named x).`` |
|     - |  6585 | ` * Every other offset type takes the ordinary int cast, warning exactly where` |
|     - |  6586 | `` * php's `(int)` warns (`$map[new stdClass]` is index 1 and a warning).`` |
|     - |  6587 | ` *` |
|     - |  6588 | ` * Then the two classes DISAGREE about an index outside [0, INT_MAX]: the list` |
|     - |  6589 | `` * answers null and the map raises `item()`'s own ValueError -- worded the way`` |
|     - |  6590 | ` * php words it with no function frame active to name the argument, so the` |
|     - |  6591 | `` * `DOMNamedNodeMap::item(): Argument #1 ($index) ` head is not there. isset()`` |
|     - |  6592 | ` * never refuses: php asks has_dimension, and that one answers a plain false.` |
|     - |  6593 | ` */` |
|     - |  6594 | `/*` |
|     - |  6595 | ` * Classify one offset. Answers 1 for a NAME (pScratch holds the string), 0 for` |
|     - |  6596 | ` * an INDEX in *piIndex. Works on a COPY: the conversion is destructive, the` |
|     - |  6597 | ` * caller's offset must survive it (empty() asks the same offset twice), and` |
|     - |  6598 | `` * php's own `$map[$k]` leaves $k alone. The caller releases pScratch.`` |
|     - |  6599 | ` */` |
|   308 |  6600 | `static int DomDimClassify(ph7_vm *pVm,ph7_value *pOffset,ph7_value *pScratch,sxi64 *piIndex)` |
|     2 |  6601 | `{` |
|   310 |  6602 | `	PH7_MemObjInit(&(*pVm),pScratch);` |
|   310 |  6603 | `	PH7_MemObjStore(pOffset,pScratch);` |
|   310 |  6604 | `	*piIndex = 0;` |
|   310 |  6605 | `	if( (pScratch->iFlags & MEMOBJ_STRING) && !PH7_MemObjStringNumericPrefix(pScratch,0) ){` |
|    69 |  6606 | `		return 1; /* a NAME: the caller reads the bytes out of the copy */` |
|     - |  6607 | `	}` |
|     - |  6608 | `	/* php reads an int out of every other offset type -- null is 0, a bool its` |
|     - |  6609 | `	 * value, a float truncated, an array 0 or 1 by emptiness, an object 1 behind` |
|     - |  6610 | ``	 * `could not be converted to int`. The cast is what hands back the reference`` |
|     - |  6611 | `	 * an object / array copy took (MemObjIntValue unrefs before it overwrites the` |
|     - |  6612 | `	 * type in place), so the release below has only a string blob left to free. */` |
|   242 |  6613 | `	PH7_MemObjWarnIntCast(pScratch);` |
|   242 |  6614 | `	*piIndex = ph7_value_to_int64(pScratch);` |
|   242 |  6615 | `	return 0;` |
|   156 |  6616 | `}` |
|     - |  6617 | `/* Hand the hook's answer back: the node, or nothing at all for a miss (which` |
|     - |  6618 | ` * the caller initialized NULL, and which isset() reads as false). */` |
|   218 |  6619 | `static void DomDimAnswer(ph7_vm *pVm,PH7_NativeDimCtx *pCtx,ph7_class_instance *pHit)` |
|     2 |  6620 | `{` |
|     - |  6621 | `	ph7_value sVal;` |
|   220 |  6622 | `	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){` |
|    27 |  6623 | `		pCtx->pResult->x.iVal = pHit ? 1 : 0;` |
|    27 |  6624 | `		MemObjSetType(pCtx->pResult,MEMOBJ_BOOL);` |
|    50 |  6625 | `		return;` |
|     - |  6626 | `	}` |
|   194 |  6627 | `	if( pHit == 0 ){` |
|    47 |  6628 | `		return;` |
|     - |  6629 | `	}` |
|   148 |  6630 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|   148 |  6631 | `	sVal.x.pOther = pHit;      /* borrowed, like every DomWrap answer */` |
|   148 |  6632 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|   148 |  6633 | `	PH7_MemObjStore(&sVal,pCtx->pResult);   /* takes its own reference */` |
|   111 |  6634 | `}` |
|     - |  6635 | ``/* php's refusal for the keyless `$list[]` spelling, which reaches the handler`` |
|     - |  6636 | ` * with no offset at all. */` |
|     4 |  6637 | `static void DomDimNoOffset(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|     1 |  6638 | `{` |
|     5 |  6639 | `	SyString *pName = &pThis->pClass->sName;` |
|     5 |  6640 | `	pCtx->zThrowClass = "Error";` |
|     7 |  6641 | `	SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|     4 |  6642 | `		"Cannot access %.*s without offset",(int)pName->nByte,pName->zString);` |
|     5 |  6643 | `}` |
|   178 |  6644 | `static void DomListDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|     2 |  6645 | `{` |
|     - |  6646 | `	ph7_value sKey;` |
|     - |  6647 | `	sxi64 iIndex;` |
|     - |  6648 | `	int bNamed;` |
|   180 |  6649 | `	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){` |
|     - |  6650 | `		/* The WRITE modes only ask for a refusal WORDING, and these two classes` |
|     - |  6651 | ``		 * have none of their own: php answers `Cannot use object of type C as`` |
|     - |  6652 | ``		 * array` for a store, an append and an unset, which is what the caller`` |
|     - |  6653 | `		 * formats when the hook declines. */` |
|    43 |  6654 | `		return;` |
|     - |  6655 | `	}` |
|   168 |  6656 | `	if( pCtx->pOffset == 0 ){` |
|     5 |  6657 | `		DomDimNoOffset(pThis,pCtx);` |
|     5 |  6658 | `		return;` |
|     - |  6659 | `	}` |
|   164 |  6660 | `	bNamed = DomDimClassify(&(*pVm),pCtx->pOffset,&sKey,&iIndex);` |
|   164 |  6661 | `	PH7_MemObjRelease(&sKey);` |
|   164 |  6662 | `	if( bNamed \|\| iIndex < 0 \|\| iIndex > DOM_INDEX_MAX ){` |
|    58 |  6663 | `		return; /* a name the list cannot answer, or an index it answers null to */` |
|     - |  6664 | `	}` |
|   108 |  6665 | `	DomDimAnswer(&(*pVm),pCtx,DomListItem(&(*pVm),pThis,(int)iIndex));` |
|    91 |  6666 | `}` |
|   150 |  6667 | `static void DomMapDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|     2 |  6668 | `{` |
|     - |  6669 | `	ph7_value sKey;` |
|     - |  6670 | `	sxi64 iIndex;` |
|     - |  6671 | `	int bNamed;` |
|   152 |  6672 | `	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){` |
|    42 |  6673 | `		return;   /* see DomListDim: the write side is php's generic sentence */` |
|     - |  6674 | `	}` |
|   148 |  6675 | `	if( pCtx->pOffset == 0 ){` |
|   ! 0 |  6676 | `		DomDimNoOffset(pThis,pCtx);` |
|   ! 0 |  6677 | `		return;` |
|     - |  6678 | `	}` |
|   148 |  6679 | `	bNamed = DomDimClassify(&(*pVm),pCtx->pOffset,&sKey,&iIndex);` |
|   148 |  6680 | `	if( bNamed ){` |
|    41 |  6681 | `		DomDimAnswer(&(*pVm),pCtx,DomMapNamed(&(*pVm),pThis,ph7_value_to_string(&sKey,0)));` |
|    41 |  6682 | `		PH7_MemObjRelease(&sKey);` |
|    41 |  6683 | `		return;` |
|     - |  6684 | `	}` |
|   108 |  6685 | `	PH7_MemObjRelease(&sKey);` |
|   108 |  6686 | `	if( iIndex < 0 \|\| iIndex > DOM_INDEX_MAX ){` |
|     - |  6687 | `		/* The range is screened BEFORE the map is looked at, so a map with no` |
|     - |  6688 | ``		 * owner at all -- `(new DOMNamedNodeMap())[-1]` -- refuses too. */`` |
|    36 |  6689 | `		if( pCtx->iMode == PH7_NATIVE_DIM_READ ){` |
|    28 |  6690 | `			pCtx->zThrowClass = "ValueError";` |
|    28 |  6691 | `			SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|     - |  6692 | `				"must be between 0 and %d",DOM_INDEX_MAX);` |
|    13 |  6693 | `		}` |
|    36 |  6694 | `		return;` |
|     - |  6695 | `	}` |
|    74 |  6696 | `	DomDimAnswer(&(*pVm),pCtx,DomMapItem(&(*pVm),pThis,(int)iIndex));` |
|    77 |  6697 | `}` |
|     - |  6698 | `/*` |
|     - |  6699 | ` * Both collections are IteratorAggregates, as php's are -- the chunk made` |
|     - |  6700 | ``  * DOMNodeList an `Iterator` with its own cursor (so `$list instanceof Iterator` `` |
|     - |  6701 | ` * was true where php says false) and gave DOMNamedNodeMap no iteration at all,` |
|     - |  6702 | `` * which meant `foreach ($el->attributes as $a)` walked the map's own private`` |
|     - |  6703 | ` * slots instead of the attributes.` |
|     - |  6704 | ` *` |
|     - |  6705 | ` * The cursor lives in the shared InternalIterator (oo_native.c); a vtable states` |
|     - |  6706 | ` * only how to REACH a position. DOMNodeList keys by index, DOMNamedNodeMap by` |
|     - |  6707 | ` * attribute name, which is what php answers for each.` |
|     - |  6708 | ` */` |
|   416 |  6709 | `static void DomIterSettle(ph7_vm *pVm,ph7_class_instance *pIt,int bNamed)` |
|     1 |  6710 | `{` |
|   417 |  6711 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|   417 |  6712 | `	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);` |
|     - |  6713 | `	ph7_class_instance *pCur;` |
|   417 |  6714 | `	pCur = bNamed ? DomMapItem(&(*pVm),pSrc,(int)iPos) : DomListItem(&(*pVm),pSrc,(int)iPos);` |
|   417 |  6715 | `	if( pCur == 0 ){` |
|   111 |  6716 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|   111 |  6717 | `		return;` |
|     - |  6718 | `	}` |
|   307 |  6719 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pCur);  /* borrowed: no unref */` |
|   307 |  6720 | `	if( bNamed ){` |
|    77 |  6721 | `		phl_domnode *pNd = DomResOf(pCur);` |
|    77 |  6722 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    77 |  6723 | `		const char *zKey = (pNode && pNode->name) ? (const char *)pNode->name : "";` |
|    77 |  6724 | `		PH7_NativeSetAttrStr(&(*pVm),pIt,PH7_NATIVE_IT_KEY,zKey,(int)SyStrlen(zKey));` |
|    39 |  6725 | `	}else{` |
|   231 |  6726 | `		PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);` |
|     - |  6727 | `	}` |
|   307 |  6728 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|   209 |  6729 | `}` |
|   146 |  6730 | `static void DomListRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  6731 | `{` |
|   147 |  6732 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|   147 |  6733 | `	DomIterSettle(&(*pVm),pIt,0);` |
|   147 |  6734 | `}` |
|   166 |  6735 | `static void DomListNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  6736 | `{` |
|   250 |  6737 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|   166 |  6738 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|   167 |  6739 | `	DomIterSettle(&(*pVm),pIt,0);` |
|   167 |  6740 | `}` |
|    58 |  6741 | `static void DomMapRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  6742 | `{` |
|    59 |  6743 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    59 |  6744 | `	DomIterSettle(&(*pVm),pIt,1);` |
|    59 |  6745 | `}` |
|    46 |  6746 | `static void DomMapNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|     1 |  6747 | `{` |
|    70 |  6748 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    46 |  6749 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    47 |  6750 | `	DomIterSettle(&(*pVm),pIt,1);` |
|    47 |  6751 | `}` |
|     - |  6752 | `static const PH7_NativeIterVtab sDomListIterVtab = { DomListRewind, DomListNext, 0, 0 };` |
|     - |  6753 | `static const PH7_NativeIterVtab sDomMapIterVtab  = { DomMapRewind,  DomMapNext, 0, 0 };` |
|     - |  6754 | `/* Both getIterator()s: a fresh InternalIterator per call, as php's are. */` |
|   106 |  6755 | `DOM_METHOD(vm_builtin_Dom_getIterator)` |
|     1 |  6756 | `{` |
|   107 |  6757 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  6758 | `	ph7_class_instance *pIt;` |
|    53 |  6759 | `	SXUNUSED(nArg);` |
|    53 |  6760 | `	SXUNUSED(apArg);` |
|   107 |  6761 | `	if( pThis == 0 ){` |
|   ! 0 |  6762 | `		return PH7_OK;` |
|     - |  6763 | `	}` |
|   107 |  6764 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|   107 |  6765 | `	if( pIt == 0 ){` |
|   ! 0 |  6766 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  6767 | `	}` |
|   107 |  6768 | `	PH7_NativeResultObject(pCtx,pIt);` |
|   107 |  6769 | `	return PH7_OK;` |
|    54 |  6770 | `}` |
|     - |  6771 |  |
|     - |  6772 | `/* ===== DOMXPath ===== */` |
|     - |  6773 |  |
|     - |  6774 | `/* The prefix => URI table registerNamespace() feeds, replayed onto the fresh` |
|     - |  6775 | ` * evaluation context each query. Hidden slot, materialized by DomXPathSlotMap` |
|     - |  6776 | ` * below with the same three moves the document's identity cache needs. */` |
|     - |  6777 | `#define XP_NSREG "__nsreg"` |
|     - |  6778 | `/*` |
|     - |  6779 | ` * DOMXPath::quote(string $str): string  (static, php 8.4)` |
|     - |  6780 | ` *` |
|     - |  6781 | ` * XPath 1.0 has no escape inside a string literal, so a value is quotable` |
|     - |  6782 | `` * only with the quote character it does not contain: no `'` and it goes in`` |
|     - |  6783 | `` * single quotes, no `"` in double ones, and a value carrying BOTH becomes a`` |
|     - |  6784 | `` * `concat()` of runs, split by the rule stated at the loop below. php's`` |
|     - |  6785 | ` * algorithm exactly, and its output byte for byte.` |
|     - |  6786 | ` */` |
|    38 |  6787 | `DOM_METHOD(vm_builtin_DOMXPath_quote)` |
|     1 |  6788 | `{` |
|    39 |  6789 | `	int nStr = 0;` |
|    39 |  6790 | `	const char *zStr = nArg > 0 ? ph7_value_to_string(apArg[0],&nStr) : "";` |
|    39 |  6791 | `	int bSq = 0,bDq = 0,i;` |
|     - |  6792 | `	SyBlob sOut;` |
|   225 |  6793 | `	for( i = 0 ; i < nStr ; ++i ){` |
|   187 |  6794 | `		if( zStr[i] == '\'' ){` |
|    29 |  6795 | `			bSq = 1;` |
|   173 |  6796 | `		}else if( zStr[i] == '"' ){` |
|    27 |  6797 | `			bDq = 1;` |
|    13 |  6798 | `		}` |
|    94 |  6799 | `	}` |
|    39 |  6800 | `	if( !bSq ){` |
|    17 |  6801 | `		ph7_result_string_format(pCtx,"'%.*s'",nStr,zStr);` |
|    17 |  6802 | `		return PH7_OK;` |
|     - |  6803 | `	}` |
|    23 |  6804 | `	if( !bDq ){` |
|     9 |  6805 | `		ph7_result_string_format(pCtx,"\"%.*s\"",nStr,zStr);` |
|     9 |  6806 | `		return PH7_OK;` |
|     - |  6807 | `	}` |
|    15 |  6808 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    15 |  6809 | `	SyBlobAppend(&sOut,"concat(",sizeof("concat(")-1);` |
|    15 |  6810 | `	i = 0;` |
|    49 |  6811 | `	while( i < nStr ){` |
|     - |  6812 | `		/* Whichever quote kind appears FIRST in what is left decides the run:` |
|     - |  6813 | `		 * the run is wrapped in the OTHER kind and reaches to that first` |
|     - |  6814 | `		 * occurrence, so it swallows every quote of the kind it is not wrapped` |
|     - |  6815 | ``		 * in. `a'b"c'd"e` is four runs that way, and `0"&'<` is two -- the`` |
|     - |  6816 | `		 * first single-quoted, because its first quote character is the double` |
|     - |  6817 | `		 * one. */` |
|    35 |  6818 | `		int iStart = i,j;` |
|    35 |  6819 | `		char cQuote = '"';` |
|    57 |  6820 | `		for( j = i ; j < nStr ; ++j ){` |
|    57 |  6821 | `			if( zStr[j] == '\'' \|\| zStr[j] == '"' ){` |
|    35 |  6822 | `				cQuote = zStr[j] == '"' ? '\'' : '"';` |
|    35 |  6823 | `				break;` |
|     - |  6824 | `			}` |
|    12 |  6825 | `		}` |
|   123 |  6826 | `		while( i < nStr && zStr[i] != cQuote ){` |
|    89 |  6827 | `			i++;` |
|     1 |  6828 | `		}` |
|    35 |  6829 | `		if( iStart > 0 ){` |
|    21 |  6830 | `			SyBlobAppend(&sOut,",",1);` |
|    10 |  6831 | `		}` |
|    35 |  6832 | `		SyBlobAppend(&sOut,&cQuote,1);` |
|    35 |  6833 | `		SyBlobAppend(&sOut,zStr + iStart,(sxu32)(i - iStart));` |
|    35 |  6834 | `		SyBlobAppend(&sOut,&cQuote,1);` |
|     1 |  6835 | `	}` |
|    15 |  6836 | `	SyBlobAppend(&sOut,")",1);` |
|    15 |  6837 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    15 |  6838 | `	SyBlobRelease(&sOut);` |
|    15 |  6839 | `	return PH7_OK;` |
|    20 |  6840 | `}` |
|     - |  6841 |  |
|     - |  6842 | `/* ===== The PHP-function bridge (php:function / php:functionString / own-URI) ===== */` |
|     - |  6843 |  |
|     - |  6844 | ``/* php's reserved URI: `php:function()` is reached through whatever PREFIX the`` |
|     - |  6845 | ` * caller bound to it, so every lookup here is by URI. */` |
|     - |  6846 | `#define XP_PHPNS "http://php.net/xpath"` |
|     - |  6847 | `/* Which callables an evaluation may reach: php's register_phpfunctions. */` |
|     - |  6848 | `#define XP_MODE_NONE   0   /* registerPhpFunctions() never called -- nothing runs */` |
|     - |  6849 | `#define XP_MODE_ALL    1   /* called bare -- any callable name runs */` |
|     - |  6850 | `#define XP_MODE_LIST   2   /* called with a restriction -- the table below decides */` |
|     - |  6851 | `#define XP_FNMODE "__fnmode"` |
|     - |  6852 | `#define XP_FNREG  "__fnreg"    /* restricted: xpath name => the callable to run */` |
|     - |  6853 | `#define XP_NSFN   "__nsfn"     /* own-URI: "<uri>\x01<name>" => callable */` |
|   108 |  6854 | `static ph7_hashmap * DomXPathSlotMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|     1 |  6855 | `{` |
|   109 |  6856 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|   109 |  6857 | `	if( pSlot == 0 ){` |
|   ! 0 |  6858 | `		return 0;` |
|     - |  6859 | `	}` |
|   109 |  6860 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    43 |  6861 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  6862 | `			return 0;` |
|     - |  6863 | `		}` |
|    21 |  6864 | `	}` |
|   109 |  6865 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|    55 |  6866 | `}` |
|     - |  6867 | `/* Insert (or replace) one string-keyed entry. */` |
|    26 |  6868 | `static void DomXPathMapPut(ph7_vm *pVm,ph7_hashmap *pMap,const char *zKey,int nKey,ph7_value *pVal)` |
|     1 |  6869 | `{` |
|     - |  6870 | `	ph7_value sKey;` |
|    27 |  6871 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    27 |  6872 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    27 |  6873 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|    27 |  6874 | `	PH7_MemObjRelease(&sKey);` |
|    27 |  6875 | `}` |
|     - |  6876 | `/* ...and the matching read, or NULL. The value BELONGS to the map. */` |
|    42 |  6877 | `static ph7_value * DomXPathMapGet(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,` |
|     - |  6878 | `	const char *zKey,int nKey)` |
|     1 |  6879 | `{` |
|    43 |  6880 | `	ph7_hashmap *pMap = DomXPathSlotMap(&(*pVm),pThis,zSlot);` |
|    43 |  6881 | `	ph7_hashmap_node *pEntry = 0;` |
|     - |  6882 | `	ph7_value sKey,*pHit;` |
|    43 |  6883 | `	if( pMap == 0 ){` |
|   ! 0 |  6884 | `		return 0;` |
|     - |  6885 | `	}` |
|    43 |  6886 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    43 |  6887 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|    43 |  6888 | `	if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){` |
|     9 |  6889 | `		pEntry = 0;` |
|     4 |  6890 | `	}` |
|    43 |  6891 | `	PH7_MemObjRelease(&sKey);` |
|    43 |  6892 | `	pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;` |
|    43 |  6893 | `	return pHit;` |
|    22 |  6894 | `}` |
|     - |  6895 | `/*` |
|     - |  6896 | ` * What one evaluation needs to reach PHP from inside libxml, and what it` |
|     - |  6897 | ` * brings BACK.` |
|     - |  6898 | ` *` |
|     - |  6899 | ` * The bringing back is the whole design problem: a refusal raised from the` |
|     - |  6900 | ` * callback would run the enclosing catch RIGHT THERE, in the middle of` |
|     - |  6901 | ` * libxml's own recursion (the builtin-throw rail), so nothing is raised here.` |
|     - |  6902 | ` * The reason is PARKED -- a code and the name it quotes -- the evaluation is` |
|     - |  6903 | ` * stopped by setting the parser's error field (not xmlXPathErr, which would` |
|     - |  6904 | ` * queue a libxml diagnostic php does not print), and DomXPathEvalRun raises` |
|     - |  6905 | ` * once libxml has unwound. A throw from the CALLBACK ITSELF is the same` |
|     - |  6906 | ` * story one level up: its dispatch status is parked in rcUnwound and returned` |
|     - |  6907 | `` * from the method verbatim, which is what makes `php:function("boom") or`` |
|     - |  6908 | `` * php:function("after")` run neither the `or` arm nor anything past it --`` |
|     - |  6909 | ` * php's answer.` |
|     - |  6910 | ` */` |
|     - |  6911 | `#define XP_FN_OK        0` |
|     - |  6912 | `#define XP_FN_NOREG     1   /* registerPhpFunctions() was never called */` |
|     - |  6913 | `#define XP_FN_NOHANDLER 2   /* restricted, and this name is not in the table */` |
|     - |  6914 | `#define XP_FN_NOTSTR    3   /* the handler name argument is not a string */` |
|     - |  6915 | `#define XP_FN_NONAME    4   /* php:function() with no arguments at all */` |
|     - |  6916 | `#define XP_FN_BADCB     5   /* the name is not callable */` |
|     - |  6917 | `#define XP_FN_NOTNODE   6   /* the callback answered an object that is not a node */` |
|     - |  6918 | `typedef struct DomXPathFnCtx DomXPathFnCtx;` |
|     - |  6919 | `struct DomXPathFnCtx {` |
|     - |  6920 | `	ph7_context *pCtx;            /* the method's own call context */` |
|     - |  6921 | `	ph7_class_instance *pThis;    /* the DOMXPath */` |
|     - |  6922 | `	ph7_class_instance *pDoc;     /* its document object (where wrappers cache) */` |
|     - |  6923 | `	phl_domnode *pDocNd;` |
|     - |  6924 | `	int iErr;                     /* XP_FN_* -- raised after libxml unwinds */` |
|     - |  6925 | `	SyBlob sErrName;              /* the name that refusal quotes */` |
|     - |  6926 | `	sxi32 rcUnwound;              /* a callback that did not return */` |
|     - |  6927 | `};` |
|     - |  6928 | `/* Stop the evaluation without emitting a libxml diagnostic. */` |
|    22 |  6929 | `static void DomXPathFnStop(xmlXPathParserContextPtr pPCtx,DomXPathFnCtx *pFn,int iErr,` |
|     - |  6930 | `	const char *zName,int nName)` |
|     1 |  6931 | `{` |
|    23 |  6932 | `	if( pFn->iErr == XP_FN_OK ){` |
|    23 |  6933 | `		pFn->iErr = iErr;` |
|    23 |  6934 | `		SyBlobReset(&pFn->sErrName);` |
|    23 |  6935 | `		if( zName && nName > 0 ){` |
|    11 |  6936 | `			SyBlobAppend(&pFn->sErrName,zName,(sxu32)nName);` |
|     5 |  6937 | `		}` |
|    11 |  6938 | `	}` |
|    23 |  6939 | `	pPCtx->error = XPATH_EXPR_ERROR;` |
|    23 |  6940 | `}` |
|     - |  6941 | `/*` |
|     - |  6942 | ` * What a callback that did not RETURN leaves behind, which php's two` |
|     - |  6943 | ` * dispatchers do differently and both visibly.` |
|     - |  6944 | ` *` |
|     - |  6945 | ` * The one that looks a callable up in a REGISTERED table -- restricted` |
|     - |  6946 | ` * php:function, and every own-URI name -- returns without pushing, and libxml,` |
|     - |  6947 | ` * finding its value stack one short, queues its own "Stack usage error" before` |
|     - |  6948 | ` * unwinding; that entry is then on the list libxml_get_errors() answers. The` |
|     - |  6949 | ` * UNRESTRICTED php:function path pushes a value first, so its queue stays` |
|     - |  6950 | ` * clean. Either way the exception is the answer, and nothing further of the` |
|     - |  6951 | ` * expression runs.` |
|     - |  6952 | ` */` |
|     2 |  6953 | `static void DomXPathFnUnwound(xmlXPathParserContextPtr pPCtx,int bRegistered)` |
|     1 |  6954 | `{` |
|     3 |  6955 | `	if( !bRegistered ){` |
|     3 |  6956 | `		valuePush(pPCtx,xmlXPathNewCString(""));` |
|     3 |  6957 | `		pPCtx->error = XPATH_EXPR_ERROR;` |
|     1 |  6958 | `	}` |
|     3 |  6959 | `}` |
|     - |  6960 | `/* One XPath argument as php sees it: a nodeset becomes an ARRAY of wrappers` |
|     - |  6961 | ` * (php's own conversion), the three scalars their php types. bAsString is` |
|     - |  6962 | ` * php:functionString's flag, under which a nodeset arrives as its string` |
|     - |  6963 | ` * value instead. */` |
|    58 |  6964 | `static void DomXPathArgToValue(DomXPathFnCtx *pFn,xmlXPathObjectPtr pArg,int bAsString,` |
|     - |  6965 | `	ph7_value *pOut)` |
|     1 |  6966 | `{` |
|    59 |  6967 | `	ph7_vm *pVm = pFn->pCtx->pVm;` |
|    59 |  6968 | `	if( pArg == 0 ){` |
|   ! 0 |  6969 | `		PH7_MemObjInit(pVm,pOut);` |
|   ! 0 |  6970 | `		return;` |
|     - |  6971 | `	}` |
|    59 |  6972 | `	if( pArg->type == XPATH_NODESET && !bAsString ){` |
|     - |  6973 | `		/* The array is built on its OWN reference rather than the method's call` |
|     - |  6974 | `		 * context: a predicate calls this once per node, and a context-owned` |
|     - |  6975 | `		 * one would live until the whole evaluation ended. */` |
|     9 |  6976 | `		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);` |
|     - |  6977 | `		int i;` |
|     9 |  6978 | `		PH7_MemObjInit(pVm,pOut);` |
|     9 |  6979 | `		if( pMap == 0 ){` |
|   ! 0 |  6980 | `			return;` |
|     - |  6981 | `		}` |
|     - |  6982 | `		/* pOut CARRIES the map's only reference, and the caller's release of` |
|     - |  6983 | `		 * it after the call is what frees it. */` |
|     9 |  6984 | `		pOut->x.pOther = pMap;` |
|     9 |  6985 | `		pOut->iFlags = MEMOBJ_HASHMAP;` |
|    21 |  6986 | `		for( i = 0 ; pArg->nodesetval && i < pArg->nodesetval->nodeNr ; ++i ){` |
|    13 |  6987 | `			xmlNodePtr pNode = pArg->nodesetval->nodeTab[i];` |
|     - |  6988 | `			ph7_value sElem;` |
|     - |  6989 | `			ph7_class_instance *pObj;` |
|    13 |  6990 | `			if( pNode == 0 ){` |
|   ! 0 |  6991 | `				continue;` |
|     - |  6992 | `			}` |
|    13 |  6993 | `			if( pNode->type == XML_NAMESPACE_DECL ){` |
|   ! 0 |  6994 | `				xmlNsPtr pNs = (xmlNsPtr)pNode;` |
|   ! 0 |  6995 | `				xmlNodePtr pElem = (xmlNodePtr)pNs->next;` |
|   ! 0 |  6996 | `				xmlNsPtr pOrig = (pElem && pElem->type == XML_ELEMENT_NODE)` |
|   ! 0 |  6997 | `					? xmlSearchNs((xmlDocPtr)pFn->pDocNd->pNode,pElem,pNs->prefix) : 0;` |
|   ! 0 |  6998 | `				if( pOrig == 0 ){` |
|   ! 0 |  6999 | `					continue;` |
|     - |  7000 | `				}` |
|   ! 0 |  7001 | `				pObj = DomNewNsNode(pVm,pFn->pDoc,pFn->pDocNd->pShell,pOrig,pElem);` |
|   ! 0 |  7002 | `				if( pObj == 0 ){` |
|   ! 0 |  7003 | `					continue;` |
|     - |  7004 | `				}` |
|   ! 0 |  7005 | `				PH7_MemObjInit(pVm,&sElem);` |
|   ! 0 |  7006 | `				sElem.x.pOther = pObj;` |
|   ! 0 |  7007 | `				sElem.iFlags = MEMOBJ_OBJ;` |
|   ! 0 |  7008 | `				ph7_array_add_elem(pOut,0,&sElem);   /* takes its own reference */` |
|   ! 0 |  7009 | `				PH7_ClassInstanceUnref(pObj);        /* ...and ours goes back */` |
|   ! 0 |  7010 | `				continue;` |
|     - |  7011 | `			}` |
|    13 |  7012 | `			pObj = DomWrap(pVm,pFn->pDoc,pFn->pDocNd->pShell,pNode);` |
|    13 |  7013 | `			if( pObj == 0 ){` |
|   ! 0 |  7014 | `				continue;` |
|     - |  7015 | `			}` |
|    13 |  7016 | `			PH7_MemObjInit(pVm,&sElem);` |
|    13 |  7017 | `			sElem.x.pOther = pObj;   /* BORROWED from the cache; the insert refs it */` |
|    13 |  7018 | `			sElem.iFlags = MEMOBJ_OBJ;` |
|    13 |  7019 | `			ph7_array_add_elem(pOut,0,&sElem);` |
|     7 |  7020 | `		}` |
|     9 |  7021 | `		return;` |
|     - |  7022 | `	}` |
|    51 |  7023 | `	switch( pArg->type ){` |
|     3 |  7024 | `	case XPATH_BOOLEAN:` |
|     7 |  7025 | `		PH7_MemObjInitFromBool(pVm,pOut,pArg->boolval);` |
|     7 |  7026 | `		break;` |
|     2 |  7027 | `	case XPATH_NUMBER:` |
|     5 |  7028 | `		PH7_MemObjInitFromReal(pVm,pOut,pArg->floatval);` |
|     5 |  7029 | `		break;` |
|    20 |  7030 | `	default: {` |
|    41 |  7031 | `		xmlChar *zStr = xmlXPathCastToString(pArg);` |
|    41 |  7032 | `		PH7_MemObjInitFromString(pVm,pOut,0);` |
|    41 |  7033 | `		if( zStr ){` |
|    41 |  7034 | `			PH7_MemObjStringAppend(pOut,(const char *)zStr,(sxu32)SyStrlen((const char *)zStr));` |
|    41 |  7035 | `			xmlFree(zStr);` |
|    20 |  7036 | `		}` |
|    40 |  7037 | `		break;` |
|     - |  7038 | `	}` |
|     - |  7039 | `	}` |
|    30 |  7040 | `}` |
|     - |  7041 | `/* The callback's ANSWER, pushed back on the XPath stack: a bool stays a` |
|     - |  7042 | ` * boolean, a DOM node becomes a one-node set, and everything else is php's` |
|     - |  7043 | ` * string conversion -- the SAME three for both spellings, since` |
|     - |  7044 | ` * functionString's flag is about the ARGUMENTS. The conversion is the` |
|     - |  7045 | ` * user-visible one (an array draws php's "Array to string conversion" notice` |
|     - |  7046 | ` * and reads "Array"); a non-node object is the TypeError parked above. */` |
|    58 |  7047 | `static void DomXPathPushResult(xmlXPathParserContextPtr pPCtx,DomXPathFnCtx *pFn,` |
|     - |  7048 | `	ph7_value *pRes)` |
|     1 |  7049 | `{` |
|    59 |  7050 | `	if( (pRes->iFlags & MEMOBJ_BOOL) && (pRes->iFlags & MEMOBJ_STRING) == 0 ){` |
|     3 |  7051 | `		valuePush(pPCtx,xmlXPathNewBoolean(pRes->x.iVal != 0));` |
|     3 |  7052 | `		return;` |
|     - |  7053 | `	}` |
|    57 |  7054 | `	if( pRes->iFlags & MEMOBJ_OBJ ){` |
|     7 |  7055 | `		ph7_class_instance *pObj = (ph7_class_instance *)pRes->x.pOther;` |
|     7 |  7056 | `		phl_domnode *pNd = DomResOf(pObj);` |
|     7 |  7057 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     7 |  7058 | `		if( pNode == 0 \|\| pNode->type == XML_NAMESPACE_DECL ){` |
|     5 |  7059 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOTNODE,0,0);` |
|     5 |  7060 | `			return;` |
|     - |  7061 | `		}` |
|     3 |  7062 | `		valuePush(pPCtx,xmlXPathNewNodeSet(pNode));` |
|     3 |  7063 | `		return;` |
|     - |  7064 | `	}` |
|     - |  7065 | `	{` |
|    51 |  7066 | `		int nStr = 0;` |
|     - |  7067 | `		const char *zStr;` |
|     - |  7068 | `		xmlChar *zDup;` |
|    51 |  7069 | `		if( (pRes->iFlags & MEMOBJ_STRING) == 0 ){` |
|    17 |  7070 | `			sxi32 rcStr = PH7_MemObjToStringUV(pRes);` |
|    17 |  7071 | `			if( PH7_CALLBACK_UNWOUND(rcStr) ){` |
|     - |  7072 | `				/* A __toString() that threw: the same rail as the callback's` |
|     - |  7073 | `				 * own throw, one conversion later. */` |
|   ! 0 |  7074 | `				pFn->rcUnwound = rcStr;` |
|   ! 0 |  7075 | `				return;` |
|     - |  7076 | `			}` |
|     8 |  7077 | `		}` |
|    51 |  7078 | `		zStr = ph7_value_to_string(pRes,&nStr);` |
|    51 |  7079 | `		zDup = xmlStrndup((const xmlChar *)zStr,nStr);` |
|    51 |  7080 | `		valuePush(pPCtx,xmlXPathWrapString(zDup));` |
|     - |  7081 | `	}` |
|    30 |  7082 | `}` |
|     - |  7083 | `/*` |
|     - |  7084 | ` * The one C function behind every PHP-backed XPath name. libxml reaches it` |
|     - |  7085 | ` * through the lookup below, with the called name and URI on the context.` |
|     - |  7086 | ` */` |
|    78 |  7087 | `static void DomXPathPhpFn(xmlXPathParserContextPtr pPCtx,int nArgs)` |
|     1 |  7088 | `{` |
|    79 |  7089 | `	xmlXPathContextPtr pXCtx = pPCtx ? pPCtx->context : 0;` |
|    79 |  7090 | `	DomXPathFnCtx *pFn = pXCtx ? (DomXPathFnCtx *)pXCtx->funcLookupData : 0;` |
|    79 |  7091 | `	const xmlChar *zFn = pXCtx ? pXCtx->function : 0;` |
|    79 |  7092 | `	const xmlChar *zUri = pXCtx ? pXCtx->functionURI : 0;` |
|    79 |  7093 | `	int bPhpNs = zUri && xmlStrEqual(zUri,(const xmlChar *)XP_PHPNS);` |
|    79 |  7094 | `	int bAsString = bPhpNs && zFn && xmlStrEqual(zFn,(const xmlChar *)"functionString");` |
|    79 |  7095 | `	int bRegistered = !bPhpNs;   /* a table lookup rather than the name itself */` |
|     - |  7096 | `	xmlXPathObjectPtr *apArg;` |
|    79 |  7097 | `	ph7_value *apVal = 0,sResult,sName;` |
|    79 |  7098 | `	ph7_value *pCallable = 0;` |
|    79 |  7099 | `	int bNameOwned = 0;` |
|     - |  7100 | `	ph7_vm *pVm;` |
|    79 |  7101 | `	int nSkip = bPhpNs ? 1 : 0;   /* php:function's first argument NAMES the callback */` |
|     - |  7102 | `	int i,nCall;` |
|     - |  7103 | `	sxi32 rc;` |
|    79 |  7104 | `	if( pFn == 0 ){` |
|   ! 0 |  7105 | `		return;` |
|     - |  7106 | `	}` |
|    79 |  7107 | `	pVm = pFn->pCtx->pVm;` |
|     - |  7108 | `	/* Take the arguments off the stack FIRST (valuePop answers them last-first),` |
|     - |  7109 | `	 * so every exit below leaves libxml's stack where it found it. */` |
|    79 |  7110 | `	apArg = nArgs > 0` |
|   114 |  7111 | `		? (xmlXPathObjectPtr *)SyMemBackendAlloc(&pVm->sAllocator,` |
|    76 |  7112 | `			sizeof(xmlXPathObjectPtr) * (sxu32)nArgs)` |
|    39 |  7113 | `		: 0;` |
|    79 |  7114 | `	if( nArgs > 0 && apArg == 0 ){` |
|   ! 0 |  7115 | `		pPCtx->error = XPATH_MEMORY_ERROR;` |
|   ! 0 |  7116 | `		return;` |
|     - |  7117 | `	}` |
|   215 |  7118 | `	for( i = nArgs - 1 ; i >= 0 ; --i ){` |
|   137 |  7119 | `		apArg[i] = valuePop(pPCtx);` |
|    69 |  7120 | `	}` |
|    79 |  7121 | `	if( pFn->iErr != XP_FN_OK \|\| pFn->rcUnwound != 0 ){` |
|   ! 0 |  7122 | `		goto done;   /* a previous call already stopped this evaluation */` |
|     - |  7123 | `	}` |
|    79 |  7124 | `	if( bPhpNs ){` |
|    69 |  7125 | `		int nName = 0;` |
|     - |  7126 | `		const char *zName;` |
|    69 |  7127 | `		sxi64 iMode = PH7_NativeAttrInt(pFn->pThis,XP_FNMODE);` |
|    69 |  7128 | `		if( nArgs < 1 ){` |
|     3 |  7129 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NONAME,0,0);` |
|     3 |  7130 | `			goto done;` |
|     - |  7131 | `		}` |
|    67 |  7132 | `		if( apArg[0] == 0 \|\| apArg[0]->type != XPATH_STRING ){` |
|     3 |  7133 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOTSTR,0,0);` |
|     3 |  7134 | `			goto done;` |
|     - |  7135 | `		}` |
|    65 |  7136 | `		zName = apArg[0]->stringval ? (const char *)apArg[0]->stringval : "";` |
|    65 |  7137 | `		nName = (int)SyStrlen(zName);` |
|    65 |  7138 | `		if( iMode == XP_MODE_NONE ){` |
|     5 |  7139 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOREG,0,0);` |
|     5 |  7140 | `			goto done;` |
|     - |  7141 | `		}` |
|    61 |  7142 | `		if( iMode == XP_MODE_LIST ){` |
|    23 |  7143 | `			bRegistered = 1;` |
|    23 |  7144 | `			pCallable = DomXPathMapGet(pVm,pFn->pThis,XP_FNREG,zName,nName);` |
|    23 |  7145 | `			if( pCallable == 0 ){` |
|     9 |  7146 | `				DomXPathFnStop(pPCtx,pFn,XP_FN_NOHANDLER,zName,nName);` |
|     9 |  7147 | `				goto done;` |
|     - |  7148 | `			}` |
|     8 |  7149 | `		}else{` |
|     - |  7150 | `			/* Unrestricted: the NAME ITSELF is the callable, screened here` |
|     - |  7151 | `			 * because no registration screened it. */` |
|    39 |  7152 | `			PH7_MemObjInitFromString(pVm,&sName,0);` |
|    39 |  7153 | `			PH7_MemObjStringAppend(&sName,zName,(sxu32)nName);` |
|    39 |  7154 | `			bNameOwned = 1;` |
|    39 |  7155 | `			if( !PH7_VmIsCallable(pVm,&sName,TRUE) ){` |
|     3 |  7156 | `				DomXPathFnStop(pPCtx,pFn,XP_FN_BADCB,zName,nName);` |
|     3 |  7157 | `				goto done;` |
|     - |  7158 | `			}` |
|    37 |  7159 | `			pCallable = &sName;` |
|     - |  7160 | `		}` |
|    26 |  7161 | `	}else{` |
|     - |  7162 | `		SyBlob sKey;` |
|    11 |  7163 | `		SyBlobInit(&sKey,&pVm->sAllocator);` |
|    11 |  7164 | `		SyBlobAppend(&sKey,(const char *)zUri,zUri ? (sxu32)SyStrlen((const char *)zUri) : 0);` |
|    11 |  7165 | `		SyBlobAppend(&sKey,"\1",1);` |
|    11 |  7166 | `		SyBlobAppend(&sKey,(const char *)zFn,zFn ? (sxu32)SyStrlen((const char *)zFn) : 0);` |
|    16 |  7167 | `		pCallable = DomXPathMapGet(pVm,pFn->pThis,XP_NSFN,` |
|    10 |  7168 | `			(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey));` |
|    11 |  7169 | `		SyBlobRelease(&sKey);` |
|    11 |  7170 | `		if( pCallable == 0 ){` |
|   ! 0 |  7171 | `			goto done;   /* not ours after all: libxml reports the unknown function */` |
|     - |  7172 | `		}` |
|     - |  7173 | `	}` |
|    61 |  7174 | `	nCall = nArgs - nSkip;` |
|    61 |  7175 | `	if( nCall > 0 ){` |
|    67 |  7176 | `		apVal = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|    44 |  7177 | `			sizeof(ph7_value) * (sxu32)nCall);` |
|    45 |  7178 | `		if( apVal == 0 ){` |
|   ! 0 |  7179 | `			pPCtx->error = XPATH_MEMORY_ERROR;` |
|   ! 0 |  7180 | `			goto done;` |
|     - |  7181 | `		}` |
|   103 |  7182 | `		for( i = 0 ; i < nCall ; ++i ){` |
|    59 |  7183 | `			DomXPathArgToValue(pFn,apArg[i + nSkip],bAsString,&apVal[i]);` |
|    30 |  7184 | `		}` |
|    22 |  7185 | `	}` |
|    61 |  7186 | `	PH7_MemObjInit(pVm,&sResult);` |
|     - |  7187 | `	{` |
|    61 |  7188 | `		ph7_value **apPtr = nCall > 0` |
|    66 |  7189 | `			? (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|    44 |  7190 | `				sizeof(ph7_value *) * (sxu32)nCall)` |
|    30 |  7191 | `			: 0;` |
|    61 |  7192 | `		if( nCall > 0 && apPtr == 0 ){` |
|   ! 0 |  7193 | `			pPCtx->error = XPATH_MEMORY_ERROR;` |
|   ! 0 |  7194 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 |  7195 | `			goto done;` |
|     - |  7196 | `		}` |
|     - |  7197 | `		/* Dispatch off a COPY: pCallable points into a registration map this` |
|     - |  7198 | `		 * very callback can rewrite (a callback calling registerPhpFunctions` |
|     - |  7199 | `		 * on its own DOMXPath), and the map's value would go out from under` |
|     - |  7200 | `		 * the dispatch. */` |
|     - |  7201 | `		ph7_value sCall;` |
|    61 |  7202 | `		PH7_MemObjInit(pVm,&sCall);` |
|    61 |  7203 | `		PH7_MemObjStore(pCallable,&sCall);` |
|   119 |  7204 | `		for( i = 0 ; i < nCall ; ++i ){` |
|    59 |  7205 | `			apPtr[i] = &apVal[i];` |
|    30 |  7206 | `		}` |
|    61 |  7207 | `		rc = PH7_VmCallCallbackByValue(pVm,&sCall,nCall,apPtr,&sResult,0);` |
|    61 |  7208 | `		PH7_MemObjRelease(&sCall);` |
|    61 |  7209 | `		if( apPtr ){` |
|    45 |  7210 | `			SyMemBackendFree(&pVm->sAllocator,apPtr);` |
|    22 |  7211 | `		}` |
|     - |  7212 | `	}` |
|    61 |  7213 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|     3 |  7214 | `		pFn->rcUnwound = rc;` |
|     3 |  7215 | `		DomXPathFnUnwound(pPCtx,bRegistered);` |
|     2 |  7216 | `	}else{` |
|    59 |  7217 | `		DomXPathPushResult(pPCtx,pFn,&sResult);` |
|     - |  7218 | `	}` |
|    61 |  7219 | `	PH7_MemObjRelease(&sResult);` |
|    39 |  7220 | `done:` |
|    79 |  7221 | `	if( bNameOwned ){` |
|    39 |  7222 | `		PH7_MemObjRelease(&sName);` |
|    19 |  7223 | `	}` |
|    79 |  7224 | `	if( apVal ){` |
|   103 |  7225 | `		for( i = 0 ; i < nArgs - nSkip ; ++i ){` |
|    59 |  7226 | `			PH7_MemObjRelease(&apVal[i]);` |
|    30 |  7227 | `		}` |
|    45 |  7228 | `		SyMemBackendFree(&pVm->sAllocator,apVal);` |
|    22 |  7229 | `	}` |
|   215 |  7230 | `	for( i = 0 ; i < nArgs ; ++i ){` |
|   137 |  7231 | `		if( apArg[i] ){` |
|   137 |  7232 | `			xmlXPathFreeObject(apArg[i]);` |
|    68 |  7233 | `		}` |
|    69 |  7234 | `	}` |
|    79 |  7235 | `	if( apArg ){` |
|    77 |  7236 | `		SyMemBackendFree(&pVm->sAllocator,apArg);` |
|    38 |  7237 | `	}` |
|    40 |  7238 | `}` |
|     - |  7239 | `/*` |
|     - |  7240 | ` * libxml's function-resolution hook: answer the bridge for php's two reserved` |
|     - |  7241 | ` * names and for any (URI, name) this object registered, and NULL for` |
|     - |  7242 | ` * everything else -- which is what makes libxml fall through to its own table` |
|     - |  7243 | `` * (so `count()` and friends still resolve).`` |
|     - |  7244 | ` */` |
|   143 |  7245 | `static xmlXPathFunction DomXPathFnLookup(void *pUserData,const xmlChar *zName,const xmlChar *zUri)` |
|     1 |  7246 | `{` |
|   144 |  7247 | `	DomXPathFnCtx *pFn = (DomXPathFnCtx *)pUserData;` |
|     - |  7248 | `	SyBlob sKey;` |
|     - |  7249 | `	ph7_value *pHit;` |
|   144 |  7250 | `	if( pFn == 0 \|\| zUri == 0 \|\| zName == 0 ){` |
|    70 |  7251 | `		return 0;` |
|     - |  7252 | `	}` |
|    75 |  7253 | `	if( xmlStrEqual(zUri,(const xmlChar *)XP_PHPNS) ){` |
|    64 |  7254 | `		if( xmlStrEqual(zName,(const xmlChar *)"function")` |
|    38 |  7255 | `		 \|\| xmlStrEqual(zName,(const xmlChar *)"functionString") ){` |
|    65 |  7256 | `			return DomXPathPhpFn;` |
|     - |  7257 | `		}` |
|   ! 0 |  7258 | `		return 0;` |
|     - |  7259 | `	}` |
|    11 |  7260 | `	SyBlobInit(&sKey,&pFn->pCtx->pVm->sAllocator);` |
|    11 |  7261 | `	SyBlobAppend(&sKey,(const char *)zUri,(sxu32)SyStrlen((const char *)zUri));` |
|    11 |  7262 | `	SyBlobAppend(&sKey,"\1",1);` |
|    11 |  7263 | `	SyBlobAppend(&sKey,(const char *)zName,(sxu32)SyStrlen((const char *)zName));` |
|    16 |  7264 | `	pHit = DomXPathMapGet(pFn->pCtx->pVm,pFn->pThis,XP_NSFN,` |
|    10 |  7265 | `		(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey));` |
|    11 |  7266 | `	SyBlobRelease(&sKey);` |
|    11 |  7267 | `	return pHit ? DomXPathPhpFn : 0;` |
|    39 |  7268 | `}` |
|     - |  7269 | `/* The parked refusal, raised once libxml has unwound. */` |
|    22 |  7270 | `static int DomXPathFnRaise(ph7_context *pCtx,DomXPathFnCtx *pFn)` |
|     1 |  7271 | `{` |
|    23 |  7272 | `	const char *zName = (const char *)SyBlobData(&pFn->sErrName);` |
|    23 |  7273 | `	int nName = (int)SyBlobLength(&pFn->sErrName);` |
|    23 |  7274 | `	switch( pFn->iErr ){` |
|     2 |  7275 | `	case XP_FN_NOREG:` |
|     5 |  7276 | `		return PH7_VmThrowException(pCtx,"Error","No callbacks were registered");` |
|     4 |  7277 | `	case XP_FN_NOHANDLER:` |
|    13 |  7278 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     4 |  7279 | `			"No callback handler \"%.*s\" registered",nName,zName);` |
|     1 |  7280 | `	case XP_FN_NOTSTR:` |
|     3 |  7281 | `		return PH7_VmThrowException(pCtx,"TypeError","Handler name must be a string");` |
|     1 |  7282 | `	case XP_FN_NONAME:` |
|     3 |  7283 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - |  7284 | `			"Function name must be passed as the first argument");` |
|     1 |  7285 | `	case XP_FN_BADCB:` |
|     4 |  7286 | `		return PH7_VmThrowException(pCtx,"Error",` |
|     - |  7287 | `			"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",` |
|     1 |  7288 | `			nName,zName,nName,zName);` |
|     2 |  7289 | `	case XP_FN_NOTNODE:` |
|     5 |  7290 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  7291 | `			"Only objects that are instances of DOM nodes can be converted to an XPath expression");` |
|   ! 0 |  7292 | `	default:` |
|   ! 0 |  7293 | `		break;` |
|     - |  7294 | `	}` |
|   ! 0 |  7295 | `	return PH7_OK;` |
|    12 |  7296 | `}` |
|     - |  7297 | `/*` |
|     - |  7298 | ` * Build the evaluation context for one query()/evaluate() call: a FRESH` |
|     - |  7299 | ` * xmlXPathContext (php keeps a persistent one; replaying the registration` |
|     - |  7300 | ` * table onto a fresh one answers the same), anchored at the explicit context` |
|     - |  7301 | ` * node -- or, with none, at the document ELEMENT, php's own substitution (so` |
|     - |  7302 | ` * query('file') matches a child of the root; an explicitly PASSED document` |
|     - |  7303 | ` * node is NOT substituted and carries no namespaces).` |
|     - |  7304 | ` *` |
|     - |  7305 | ` * bRegNodeNs is php's $registerNodeNS: the context NODE's in-scope` |
|     - |  7306 | ` * declarations go into pXCtx->namespaces, the array xmlXPathNsLookup consults` |
|     - |  7307 | ` * BEFORE the registered table -- which is why a document prefix beats a` |
|     - |  7308 | ` * registerNamespace() one only for that call. The caller frees the returned` |
|     - |  7309 | ` * list with xmlFree AFTER evaluating (the xmlNs entries belong to the tree;` |
|     - |  7310 | ` * only the array is owned).` |
|     - |  7311 | ` */` |
|   228 |  7312 | `static xmlNsPtr * DomXPathCtxOpen(ph7_context *pCtx,phl_domnode *pDocNd,` |
|     - |  7313 | `	phl_domnode *pCtxNd,int bRegNodeNs,xmlXPathContextPtr *ppXCtx)` |
|     1 |  7314 | `{` |
|   229 |  7315 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7316 | `	xmlXPathContextPtr pXCtx;` |
|     - |  7317 | `	ph7_value *pNsReg;` |
|   229 |  7318 | `	xmlNsPtr *aNs = 0;` |
|   229 |  7319 | `	*ppXCtx = 0;` |
|   229 |  7320 | `	pXCtx = xmlXPathNewContext((xmlDocPtr)pDocNd->pNode);` |
|   229 |  7321 | `	if( pXCtx == 0 ){` |
|   ! 0 |  7322 | `		return 0;` |
|     - |  7323 | `	}` |
|   229 |  7324 | `	if( pCtxNd ){` |
|    25 |  7325 | `		pXCtx->node = (xmlNodePtr)pCtxNd->pNode;` |
|    13 |  7326 | `	}else{` |
|   205 |  7327 | `		pXCtx->node = xmlDocGetRootElement((xmlDocPtr)pDocNd->pNode);` |
|     - |  7328 | `	}` |
|   229 |  7329 | `	pNsReg = pThis ? PH7_NativeAttr(pThis,XP_NSREG) : 0;` |
|   229 |  7330 | `	if( pNsReg && (pNsReg->iFlags & MEMOBJ_HASHMAP) ){` |
|    95 |  7331 | `		ph7_array_walk(pNsReg,DomC14NRegisterNs,pXCtx);` |
|    47 |  7332 | `	}` |
|   229 |  7333 | `	if( bRegNodeNs && pXCtx->node ){` |
|   223 |  7334 | `		aNs = xmlGetNsList((xmlDocPtr)pDocNd->pNode,pXCtx->node);` |
|   223 |  7335 | `		if( aNs ){` |
|    45 |  7336 | `			int nNs = 0;` |
|   105 |  7337 | `			while( aNs[nNs] ){` |
|    61 |  7338 | `				nNs++;` |
|     1 |  7339 | `			}` |
|    45 |  7340 | `			pXCtx->namespaces = aNs;` |
|    45 |  7341 | `			pXCtx->nsNr = nNs;` |
|    22 |  7342 | `		}` |
|   111 |  7343 | `	}` |
|   229 |  7344 | `	*ppXCtx = pXCtx;` |
|   229 |  7345 | `	return aNs;` |
|   115 |  7346 | `}` |
|     - |  7347 | `/*` |
|     - |  7348 | ` * Freeze a nodeset result into the document-order snapshot a DNL_SNAP` |
|     - |  7349 | ` * DOMNodeList serves (php's query() is not live), and answer the list. A` |
|     - |  7350 | ` * non-nodeset pObj answers the EMPTY list: php's query() gives that for a` |
|     - |  7351 | `` * scalar-typed expression (`count(//x)`), not false.`` |
|     - |  7352 | ` */` |
|    94 |  7353 | `static int DomXPathResultList(ph7_context *pCtx,ph7_class_instance *pDoc,` |
|     - |  7354 | `	phl_domnode *pDocNd,xmlXPathObjectPtr pObj)` |
|     1 |  7355 | `{` |
|    95 |  7356 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  7357 | `	ph7_class_instance *pList;` |
|    95 |  7358 | `	ph7_value *pSnap = ph7_context_new_array(pCtx);` |
|    95 |  7359 | `	if( pSnap == 0 ){` |
|   ! 0 |  7360 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7361 | `	}` |
|    95 |  7362 | `	if( pObj && pObj->type == XPATH_NODESET && pObj->nodesetval ){` |
|     - |  7363 | `		int i;` |
|   229 |  7364 | `		for( i = 0 ; i < pObj->nodesetval->nodeNr ; i++ ){` |
|   139 |  7365 | `			xmlNodePtr pNode = pObj->nodesetval->nodeTab[i];` |
|     - |  7366 | `			phl_domnode *pWrap;` |
|     - |  7367 | `			ph7_value *pRes;` |
|   139 |  7368 | `			if( pNode == 0 ){` |
|   ! 0 |  7369 | `				continue;` |
|     - |  7370 | `			}` |
|   139 |  7371 | `			if( pNode->type == XML_NAMESPACE_DECL ){` |
|     - |  7372 | `				/*` |
|     - |  7373 | `				 * A namespace:: axis result. libxml hands the set a COPY that` |
|     - |  7374 | `` 				 * dies with the XPath object (xmlXPathNodeSetDupNs, its `next` `` |
|     - |  7375 | `				 * pointing at the element the axis ran ON), so the snapshot` |
|     - |  7376 | `				 * wraps the ORIGINAL in-scope declaration found back through` |
|     - |  7377 | `				 * that element -- as php answers it: a DOMNameSpaceNode whose` |
|     - |  7378 | `				 * parentNode is the axis element even for a declaration an` |
|     - |  7379 | `				 * ANCESTOR made, fresh per query, stored as the OBJECT itself` |
|     - |  7380 | `				 * (item() twice on one list is one object, php's answer too).` |
|     - |  7381 | `				 */` |
|    43 |  7382 | `				xmlNsPtr pNs = (xmlNsPtr)pNode;` |
|    43 |  7383 | `				xmlNodePtr pElem = (xmlNodePtr)pNs->next;` |
|     - |  7384 | `				xmlNsPtr pOrig;` |
|     - |  7385 | `				ph7_class_instance *pNsObj;` |
|    43 |  7386 | `				if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|   ! 0 |  7387 | `					continue; /* not derivable: no element behind the copy */` |
|     - |  7388 | `				}` |
|    43 |  7389 | `				pOrig = xmlSearchNs((xmlDocPtr)pDocNd->pNode,pElem,pNs->prefix);` |
|    43 |  7390 | `				if( pOrig == 0 ){` |
|   ! 0 |  7391 | `					continue;` |
|     - |  7392 | `				}` |
|    43 |  7393 | `				pNsObj = DomNewNsNode(pVm,pDoc,pDocNd->pShell,pOrig,pElem);` |
|    43 |  7394 | `				pRes = ph7_context_new_scalar(pCtx);` |
|    43 |  7395 | `				if( pNsObj == 0 \|\| pRes == 0 ){` |
|   ! 0 |  7396 | `					if( pNsObj ){` |
|   ! 0 |  7397 | `						PH7_ClassInstanceUnref(pNsObj);` |
|   ! 0 |  7398 | `					}` |
|   ! 0 |  7399 | `					break;` |
|     - |  7400 | `				}` |
|     - |  7401 | `				/* pRes CARRIES the constructor's reference (no bump here): the` |
|     - |  7402 | `				 * array's insert takes its own, and the call context's release` |
|     - |  7403 | `				 * of pRes at method end consumes ours -- ending at exactly the` |
|     - |  7404 | `				 * array's one. */` |
|    43 |  7405 | `				pRes->x.pOther = pNsObj;` |
|    43 |  7406 | `				pRes->iFlags = MEMOBJ_OBJ;` |
|    43 |  7407 | `				ph7_array_add_elem(pSnap,0,pRes);` |
|    43 |  7408 | `				continue;` |
|     - |  7409 | `			}` |
|    97 |  7410 | `			pWrap = DomNewRes(pVm,pDocNd->pShell,pNode);` |
|    97 |  7411 | `			pRes = ph7_context_new_scalar(pCtx);` |
|    97 |  7412 | `			if( pWrap == 0 \|\| pRes == 0 ){` |
|   ! 0 |  7413 | `				break;` |
|     - |  7414 | `			}` |
|    97 |  7415 | `			ph7_value_resource(pRes,pWrap);` |
|    97 |  7416 | `			ph7_array_add_elem(pSnap,0,pRes);` |
|    49 |  7417 | `		}` |
|    45 |  7418 | `	}` |
|    95 |  7419 | `	pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_SNAP,0,0,0,pSnap);` |
|    95 |  7420 | `	if( pList == 0 ){` |
|   ! 0 |  7421 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7422 | `	}` |
|    95 |  7423 | `	PH7_NativeResultObject(pCtx,pList);` |
|    95 |  7424 | `	return PH7_OK;` |
|    48 |  7425 | `}` |
|     - |  7426 | `/*` |
|     - |  7427 | ` * The one evaluation body under DOMXPath::query and DOMXPath::evaluate. The` |
|     - |  7428 | ` * two differ only in what they make of the RESULT: query wants a node list` |
|     - |  7429 | ` * (a scalar gets the empty one), evaluate answers the XPath TYPE as php's` |
|     - |  7430 | ` * value -- boolean as bool, number as float, string as string, nodeset as` |
|     - |  7431 | ` * the same snapshot list. An expression that does not evaluate (bad grammar,` |
|     - |  7432 | ` * unknown function, unresolved prefix) answers false from both, with the` |
|     - |  7433 | ` * libxml diagnostics on the shared queue.` |
|     - |  7434 | ` */` |
|   232 |  7435 | `static int DomXPathEvalRun(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|     - |  7436 | `	const char *zMethod,int bTyped)` |
|     1 |  7437 | `{` |
|   233 |  7438 | `	ph7_vm *pVm = pCtx->pVm;` |
|   233 |  7439 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   233 |  7440 | `	ph7_class_instance *pDoc = pThis ? PH7_NativeAttrObj(pThis,"document") : 0;` |
|   233 |  7441 | `	phl_domnode *pDocNd = DomResOf(pDoc);` |
|   233 |  7442 | `	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|   233 |  7443 | `	phl_domnode *pCtxNd = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;` |
|     - |  7444 | ``	/* php's stub says `= true`, but the live default of the third argument is`` |
|     - |  7445 | `	 * the registerNodeNamespaces PROPERTY (the constructor's second argument` |
|     - |  7446 | `	 * lands there, and a later property write moves the default with it). */` |
|   233 |  7447 | `	int bRegNodeNs = nArg > 2 ? ph7_value_to_bool(apArg[2])` |
|   227 |  7448 | `		: (pThis ? PH7_NativeAttrTruthy(pThis,"registerNodeNamespaces") : 1);` |
|     - |  7449 | `	xmlXPathContextPtr pXCtx;` |
|     - |  7450 | `	xmlXPathObjectPtr pObj;` |
|     - |  7451 | `	xmlNsPtr *aNodeNs;` |
|     - |  7452 | `	DomXPathFnCtx sFn;` |
|     - |  7453 | `	sxu32 nMark;` |
|     - |  7454 | `	sxi32 rc;` |
|   233 |  7455 | `	if( pDocNd == 0 ){` |
|   ! 0 |  7456 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  7457 | `		return PH7_OK;` |
|     - |  7458 | `	}` |
|   232 |  7459 | `	if( pCtxNd && pCtxNd->pNode` |
|    29 |  7460 | `	 && ((xmlNodePtr)pCtxNd->pNode)->doc != (xmlDocPtr)pDocNd->pNode ){` |
|     - |  7461 | `		/* php's plain Error, no DOM code -- a context node of another document` |
|     - |  7462 | `		 * (or of none, a constructed node) cannot anchor this evaluation. */` |
|     5 |  7463 | `		return PH7_VmThrowException(pCtx,"Error","Node from wrong document");` |
|     - |  7464 | `	}` |
|   229 |  7465 | `	aNodeNs = DomXPathCtxOpen(pCtx,pDocNd,pCtxNd,bRegNodeNs,&pXCtx);` |
|   229 |  7466 | `	if( pXCtx == 0 ){` |
|   ! 0 |  7467 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  7468 | `		return PH7_OK;` |
|     - |  7469 | `	}` |
|     - |  7470 | `	/* The PHP-function bridge rides this one evaluation: the record lives on` |
|     - |  7471 | `	 * THIS stack frame, and libxml carries a pointer to it as its lookup data. */` |
|   229 |  7472 | `	sFn.pCtx = pCtx;` |
|   229 |  7473 | `	sFn.pThis = pThis;` |
|   229 |  7474 | `	sFn.pDoc = pDoc;` |
|   229 |  7475 | `	sFn.pDocNd = pDocNd;` |
|   229 |  7476 | `	sFn.iErr = XP_FN_OK;` |
|   229 |  7477 | `	sFn.rcUnwound = 0;` |
|   229 |  7478 | `	SyBlobInit(&sFn.sErrName,&pVm->sAllocator);` |
|   229 |  7479 | `	xmlXPathRegisterFuncLookup(pXCtx,DomXPathFnLookup,&sFn);` |
|   229 |  7480 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   229 |  7481 | `	pObj = xmlXPathEvalExpression((const xmlChar *)zExpr,pXCtx);` |
|   229 |  7482 | `	PH7_LibxmlCaptureEnd(pVm,nMark,zMethod);` |
|   229 |  7483 | `	if( aNodeNs ){` |
|    45 |  7484 | `		pXCtx->namespaces = 0;` |
|    45 |  7485 | `		pXCtx->nsNr = 0;` |
|    45 |  7486 | `		xmlFree(aNodeNs);` |
|    22 |  7487 | `	}` |
|   229 |  7488 | `	if( sFn.rcUnwound != 0 \|\| sFn.iErr != XP_FN_OK ){` |
|     - |  7489 | `		/* A callback did not return, or the bridge parked a refusal it could` |
|     - |  7490 | `		 * not raise from inside libxml's recursion. Either way the evaluation` |
|     - |  7491 | `		 * is over and this is its answer -- raised HERE, where the enclosing` |
|     - |  7492 | `		 * catch runs with libxml already unwound. */` |
|    25 |  7493 | `		sxi32 rcFn = sFn.rcUnwound;` |
|    25 |  7494 | `		if( pObj ){` |
|   ! 0 |  7495 | `			xmlXPathFreeObject(pObj);` |
|   ! 0 |  7496 | `		}` |
|    25 |  7497 | `		xmlXPathFreeContext(pXCtx);` |
|    25 |  7498 | `		if( rcFn == 0 ){` |
|    23 |  7499 | `			rcFn = DomXPathFnRaise(pCtx,&sFn);` |
|    12 |  7500 | `		}else{` |
|     3 |  7501 | `			pCtx->nThrowRc = rcFn;` |
|     - |  7502 | `		}` |
|    25 |  7503 | `		SyBlobRelease(&sFn.sErrName);` |
|    25 |  7504 | `		return rcFn;` |
|     - |  7505 | `	}` |
|   205 |  7506 | `	SyBlobRelease(&sFn.sErrName);` |
|   205 |  7507 | `	if( pObj == 0 ){` |
|    27 |  7508 | `		xmlXPathFreeContext(pXCtx);` |
|    27 |  7509 | `		ph7_result_bool(pCtx,0);` |
|    27 |  7510 | `		return PH7_OK;` |
|     - |  7511 | `	}` |
|   179 |  7512 | `	if( !bTyped ){` |
|    85 |  7513 | `		rc = DomXPathResultList(pCtx,pDoc,pDocNd,pObj);` |
|    43 |  7514 | `	}else{` |
|    95 |  7515 | `		switch( pObj->type ){` |
|     2 |  7516 | `		case XPATH_BOOLEAN:` |
|     5 |  7517 | `			ph7_result_bool(pCtx,pObj->boolval);` |
|     5 |  7518 | `			rc = PH7_OK;` |
|     5 |  7519 | `			break;` |
|    15 |  7520 | `		case XPATH_NUMBER:` |
|    31 |  7521 | `			ph7_result_double(pCtx,pObj->floatval);` |
|    31 |  7522 | `			rc = PH7_OK;` |
|    31 |  7523 | `			break;` |
|    25 |  7524 | `		case XPATH_STRING:` |
|    51 |  7525 | `			ph7_result_string(pCtx,pObj->stringval ? (const char *)pObj->stringval : "",-1);` |
|    51 |  7526 | `			rc = PH7_OK;` |
|    51 |  7527 | `			break;` |
|     5 |  7528 | `		case XPATH_NODESET:` |
|    11 |  7529 | `			rc = DomXPathResultList(pCtx,pDoc,pDocNd,pObj);` |
|    11 |  7530 | `			break;` |
|   ! 0 |  7531 | `		default:` |
|   ! 0 |  7532 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  7533 | `			rc = PH7_OK;` |
|   ! 0 |  7534 | `			break;` |
|     - |  7535 | `		}` |
|     - |  7536 | `	}` |
|   179 |  7537 | `	xmlXPathFreeObject(pObj);` |
|   179 |  7538 | `	xmlXPathFreeContext(pXCtx);` |
|   179 |  7539 | `	return rc;` |
|   117 |  7540 | `}` |
|     - |  7541 | `/*` |
|     - |  7542 | ` * DOMXPath::query(string $expression, ?DOMNode $contextNode = null,` |
|     - |  7543 | ` *                 bool $registerNodeNS = true): DOMNodeList\|false` |
|     - |  7544 | ` */` |
|   100 |  7545 | `DOM_METHOD(vm_builtin_DOMXPath_query)` |
|     1 |  7546 | `{` |
|   101 |  7547 | `	return DomXPathEvalRun(pCtx,nArg,apArg,"DOMXPath::query",0);` |
|     1 |  7548 | `}` |
|     - |  7549 | `/*` |
|     - |  7550 | ` * DOMXPath::evaluate(string $expression, ?DOMNode $contextNode = null,` |
|     - |  7551 | ` *                    bool $registerNodeNS = true): mixed` |
|     - |  7552 | ` */` |
|   132 |  7553 | `DOM_METHOD(vm_builtin_DOMXPath_evaluate)` |
|     1 |  7554 | `{` |
|   133 |  7555 | `	return DomXPathEvalRun(pCtx,nArg,apArg,"DOMXPath::evaluate",1);` |
|     1 |  7556 | `}` |
|     - |  7557 | `/* DOMXPath::__construct(DOMDocument $document, bool $registerNodeNS = true) */` |
|    72 |  7558 | `DOM_METHOD(vm_builtin_DOMXPath_construct)` |
|     1 |  7559 | `{` |
|    73 |  7560 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    73 |  7561 | `	if( pThis && nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|   109 |  7562 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,"document",` |
|    72 |  7563 | `			(ph7_class_instance *)apArg[0]->x.pOther);` |
|    36 |  7564 | `	}` |
|    73 |  7565 | `	if( pThis && nArg > 1 ){` |
|     7 |  7566 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,"registerNodeNamespaces",` |
|     4 |  7567 | `			ph7_value_to_bool(apArg[1]));` |
|     2 |  7568 | `	}` |
|    73 |  7569 | `	return PH7_OK;` |
|     1 |  7570 | `}` |
|     - |  7571 | `/*` |
|     - |  7572 | ` * DOMXPath::registerNamespace(string $prefix, string $namespace): bool` |
|     - |  7573 | ` *` |
|     - |  7574 | ` * php hands the pair to xmlXPathRegisterNs on its persistent context and` |
|     - |  7575 | ` * answers its status: only the EMPTY prefix refuses (an invalid NCName one is` |
|     - |  7576 | ` * taken, and an empty URI is a registration too -- the prefix then resolves,` |
|     - |  7577 | ` * to a namespace nothing is in). Here the pair goes into the per-object table` |
|     - |  7578 | ` * the next evaluation replays.` |
|     - |  7579 | ` */` |
|    40 |  7580 | `DOM_METHOD(vm_builtin_DOMXPath_registerNamespace)` |
|     1 |  7581 | `{` |
|    41 |  7582 | `	ph7_vm *pVm = pCtx->pVm;` |
|    41 |  7583 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    41 |  7584 | `	int nPfx = 0;` |
|    41 |  7585 | `	const char *zPfx = nArg > 0 ? ph7_value_to_string(apArg[0],&nPfx) : "";` |
|     - |  7586 | `	ph7_hashmap *pMap;` |
|     - |  7587 | `	ph7_value sKey,sVal;` |
|    41 |  7588 | `	if( nPfx < 1 \|\| nArg < 2 ){` |
|     3 |  7589 | `		ph7_result_bool(pCtx,0);` |
|     3 |  7590 | `		return PH7_OK;` |
|     - |  7591 | `	}` |
|    39 |  7592 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_NSREG);` |
|    39 |  7593 | `	if( pMap == 0 ){` |
|   ! 0 |  7594 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  7595 | `		return PH7_OK;` |
|     - |  7596 | `	}` |
|    39 |  7597 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    39 |  7598 | `	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);` |
|    39 |  7599 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|     - |  7600 | `	{` |
|    39 |  7601 | `		int nUri = 0;` |
|    39 |  7602 | `		const char *zUri = ph7_value_to_string(apArg[1],&nUri);` |
|    39 |  7603 | `		PH7_MemObjStringAppend(&sVal,zUri,(sxu32)nUri);` |
|     - |  7604 | `	}` |
|    39 |  7605 | `	PH7_HashmapInsert(pMap,&sKey,&sVal);` |
|    39 |  7606 | `	PH7_MemObjRelease(&sKey);` |
|    39 |  7607 | `	PH7_MemObjRelease(&sVal);` |
|    39 |  7608 | `	ph7_result_bool(pCtx,1);` |
|    39 |  7609 | `	return PH7_OK;` |
|    21 |  7610 | `}` |
|     - |  7611 |  |
|     - |  7612 | `/* One row of the $restrict ARRAY: the value must be callable, and the NAME an` |
|     - |  7613 | ` * expression calls it by is the string key when there is one -- php's alias --` |
|     - |  7614 | ` * and otherwise the value coerced to a string (an array callable therefore` |
|     - |  7615 | ` * registers under "Array", with php's own conversion notice). */` |
|     - |  7616 | `struct DomXPathRestrict {` |
|     - |  7617 | `	ph7_context *pCtx;` |
|     - |  7618 | `	ph7_hashmap *pMap;` |
|     - |  7619 | `	sxi32 rc;` |
|     - |  7620 | `};` |
|    16 |  7621 | `static int DomXPathRestrictRow(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|     1 |  7622 | `{` |
|    17 |  7623 | `	struct DomXPathRestrict *pWalk = (struct DomXPathRestrict *)pUserData;` |
|    17 |  7624 | `	ph7_vm *pVm = pWalk->pCtx->pVm;` |
|     - |  7625 | `	char zBuf[128];` |
|     - |  7626 | `	const char *zWhy;` |
|    17 |  7627 | `	if( pWalk->rc != PH7_OK ){` |
|   ! 0 |  7628 | `		return PH7_OK;` |
|     - |  7629 | `	}` |
|    17 |  7630 | `	zWhy = PH7_VmCallableReason(pVm,pVal,zBuf,(int)sizeof(zBuf));` |
|    17 |  7631 | `	if( zWhy ){` |
|     7 |  7632 | `		pWalk->rc = PH7_VmThrowException(pWalk->pCtx,"TypeError",` |
|     - |  7633 | `			"DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be an array "` |
|     2 |  7634 | `			"with valid callbacks as values, %s",zWhy);` |
|     5 |  7635 | `		return PH7_ABORT;` |
|     - |  7636 | `	}` |
|    15 |  7637 | `	if( pKey && ph7_value_is_string(pKey) ){` |
|     5 |  7638 | `		int nKey = 0;` |
|     5 |  7639 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|     5 |  7640 | `		DomXPathMapPut(pVm,pWalk->pMap,zKey,nKey,pVal);` |
|     3 |  7641 | `	}else{` |
|     - |  7642 | `		/* ph7_value_to_string COERCES in place, which would rewrite the map's` |
|     - |  7643 | `		 * own value; name off a copy. */` |
|     - |  7644 | `		ph7_value sName;` |
|     9 |  7645 | `		int nName = 0;` |
|     - |  7646 | `		const char *zName;` |
|     9 |  7647 | `		PH7_MemObjInit(pVm,&sName);` |
|     9 |  7648 | `		PH7_MemObjStore(pVal,&sName);` |
|     9 |  7649 | `		zName = ph7_value_to_string(&sName,&nName);` |
|     9 |  7650 | `		DomXPathMapPut(pVm,pWalk->pMap,zName,nName,pVal);` |
|     9 |  7651 | `		PH7_MemObjRelease(&sName);` |
|     - |  7652 | `	}` |
|    13 |  7653 | `	return PH7_OK;` |
|     9 |  7654 | `}` |
|     - |  7655 | `/*` |
|     - |  7656 | ` * DOMXPath::registerPhpFunctions(array\|string\|null $restrict = null): void` |
|     - |  7657 | ` *` |
|     - |  7658 | ` * Bare (or null) opens the door to ANY callable name; a string or an array` |
|     - |  7659 | ` * restricts it to the named ones, accumulating across calls -- a later bare` |
|     - |  7660 | ` * call re-opens without forgetting the table, and a later restriction closes` |
|     - |  7661 | ` * it again with everything registered so far still reachable. Each name is` |
|     - |  7662 | ` * screened for callability HERE, so an evaluation never has to.` |
|     - |  7663 | ` */` |
|    32 |  7664 | `DOM_METHOD(vm_builtin_DOMXPath_registerPhpFunctions)` |
|     1 |  7665 | `{` |
|    33 |  7666 | `	ph7_vm *pVm = pCtx->pVm;` |
|    33 |  7667 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  7668 | `	ph7_hashmap *pMap;` |
|    33 |  7669 | `	if( pThis == 0 ){` |
|   ! 0 |  7670 | `		return PH7_OK;` |
|     - |  7671 | `	}` |
|    33 |  7672 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|    15 |  7673 | `		PH7_NativeSetAttrInt(pVm,pThis,XP_FNMODE,XP_MODE_ALL);` |
|    15 |  7674 | `		return PH7_OK;` |
|     - |  7675 | `	}` |
|    19 |  7676 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_FNREG);` |
|    19 |  7677 | `	if( pMap == 0 ){` |
|   ! 0 |  7678 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7679 | `	}` |
|     - |  7680 | `	/* The mode moves FIRST, and each row is taken as it is screened: php's` |
|     - |  7681 | `	 * refusal leaves the object restricted with everything registered up to` |
|     - |  7682 | `` 	 * the bad row -- `registerPhpFunctions(['strrev','nope','strtolower'])` `` |
|     - |  7683 | `	 * throws, and afterwards strrev runs while strtolower does not. */` |
|    19 |  7684 | `	PH7_NativeSetAttrInt(pVm,pThis,XP_FNMODE,XP_MODE_LIST);` |
|    19 |  7685 | `	if( ph7_value_is_array(apArg[0]) ){` |
|     - |  7686 | `		struct DomXPathRestrict sWalk;` |
|    13 |  7687 | `		sWalk.pCtx = pCtx;` |
|    13 |  7688 | `		sWalk.pMap = pMap;` |
|    13 |  7689 | `		sWalk.rc = PH7_OK;` |
|    13 |  7690 | `		ph7_array_walk(apArg[0],DomXPathRestrictRow,&sWalk);` |
|    13 |  7691 | `		if( sWalk.rc != PH7_OK ){` |
|     5 |  7692 | `			return sWalk.rc;` |
|     - |  7693 | `		}` |
|     5 |  7694 | `	}else{` |
|     - |  7695 | `		char zBuf[128];` |
|     7 |  7696 | `		const char *zWhy = PH7_VmCallableReason(pVm,apArg[0],zBuf,(int)sizeof(zBuf));` |
|     7 |  7697 | `		int nName = 0;` |
|     - |  7698 | `		const char *zName;` |
|     7 |  7699 | `		if( zWhy ){` |
|     4 |  7700 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  7701 | `				"DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be a callable, %s",` |
|     1 |  7702 | `				zWhy);` |
|     - |  7703 | `		}` |
|     5 |  7704 | `		zName = ph7_value_to_string(apArg[0],&nName);` |
|     5 |  7705 | `		DomXPathMapPut(pVm,pMap,zName,nName,apArg[0]);` |
|     - |  7706 | `	}` |
|    13 |  7707 | `	return PH7_OK;` |
|    17 |  7708 | `}` |
|     - |  7709 | `/* php's callback NAME grammar for registerPhpFunctionNS: an XML NCName, which` |
|     - |  7710 | ` * is what an expression can spell as a function name. */` |
|    18 |  7711 | `static int DomXPathIsCallbackName(const char *zName,int nName)` |
|     1 |  7712 | `{` |
|     - |  7713 | `	int i;` |
|    19 |  7714 | `	if( nName < 1 ){` |
|     3 |  7715 | `		return 0;` |
|     - |  7716 | `	}` |
|    17 |  7717 | `	if( xmlValidateNCName((const xmlChar *)zName,0) != 0 ){` |
|     5 |  7718 | `		return 0;` |
|     - |  7719 | `	}` |
|     - |  7720 | `	/* xmlValidateNCName reads to the NUL, and a name may not carry one. */` |
|    67 |  7721 | `	for( i = 0 ; i < nName ; ++i ){` |
|    55 |  7722 | `		if( zName[i] == 0 ){` |
|   ! 0 |  7723 | `			return 0;` |
|     - |  7724 | `		}` |
|    28 |  7725 | `	}` |
|    13 |  7726 | `	return (int)SyStrlen(zName) == nName;` |
|    10 |  7727 | `}` |
|     - |  7728 | `/*` |
|     - |  7729 | ` * DOMXPath::registerPhpFunctionNS(string $namespaceURI, string $name,` |
|     - |  7730 | ` *                                 callable $callable): void` |
|     - |  7731 | ` *` |
|     - |  7732 | ` * php 8.4's narrow door: one callable under one name in the caller's OWN` |
|     - |  7733 | `` * namespace -- no `php:function("name")` indirection, and independent of`` |
|     - |  7734 | ` * registerPhpFunctions' mode (it neither needs it nor opens it). php's own` |
|     - |  7735 | ` * URI is refused, the name must be an NCName, and the callable is screened` |
|     - |  7736 | ` * here.` |
|     - |  7737 | ` */` |
|    20 |  7738 | `DOM_METHOD(vm_builtin_DOMXPath_registerPhpFunctionNS)` |
|     1 |  7739 | `{` |
|    21 |  7740 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 |  7741 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    21 |  7742 | `	int nUri = 0,nName = 0;` |
|    21 |  7743 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],&nUri) : "";` |
|    21 |  7744 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],&nName) : "";` |
|     - |  7745 | `	char zBuf[128];` |
|     - |  7746 | `	const char *zWhy;` |
|     - |  7747 | `	ph7_hashmap *pMap;` |
|     - |  7748 | `	SyBlob sKey;` |
|    21 |  7749 | `	if( pThis == 0 \|\| nArg < 3 ){` |
|   ! 0 |  7750 | `		return PH7_OK;` |
|     - |  7751 | `	}` |
|    21 |  7752 | `	if( nUri == (int)sizeof(XP_PHPNS)-1 && SyMemcmp(zUri,XP_PHPNS,(sxu32)nUri) == 0 ){` |
|     3 |  7753 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  7754 | `			"DOMXPath::registerPhpFunctionNS(): Argument #1 ($namespaceURI) must not be "` |
|     - |  7755 | `			"\"%s\" because it is reserved by PHP",XP_PHPNS);` |
|     - |  7756 | `	}` |
|    19 |  7757 | `	if( !DomXPathIsCallbackName(zName,nName) ){` |
|     7 |  7758 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  7759 | `			"DOMXPath::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name");` |
|     - |  7760 | `	}` |
|    13 |  7761 | `	zWhy = PH7_VmCallableReason(pVm,apArg[2],zBuf,(int)sizeof(zBuf));` |
|    13 |  7762 | `	if( zWhy ){` |
|     4 |  7763 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  7764 | `			"DOMXPath::registerPhpFunctionNS(): Argument #3 ($callable) must be a valid callback, %s",` |
|     1 |  7765 | `			zWhy);` |
|     - |  7766 | `	}` |
|    11 |  7767 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_NSFN);` |
|    11 |  7768 | `	if( pMap == 0 ){` |
|   ! 0 |  7769 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  7770 | `	}` |
|     - |  7771 | `	/* One key from the pair: a URI cannot carry \x01, so the join is` |
|     - |  7772 | `	 * unambiguous without escaping. */` |
|    11 |  7773 | `	SyBlobInit(&sKey,&pVm->sAllocator);` |
|    11 |  7774 | `	SyBlobAppend(&sKey,zUri,(sxu32)nUri);` |
|    11 |  7775 | `	SyBlobAppend(&sKey,"\1",1);` |
|    11 |  7776 | `	SyBlobAppend(&sKey,zName,(sxu32)nName);` |
|    11 |  7777 | `	DomXPathMapPut(pVm,pMap,(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey),apArg[2]);` |
|    11 |  7778 | `	SyBlobRelease(&sKey);` |
|    11 |  7779 | `	return PH7_OK;` |
|    11 |  7780 | `}` |
|     - |  7781 |  |
|     - |  7782 | `/* ===== Schema validation ===== */` |
|     - |  7783 |  |
|     - |  7784 | `/*` |
|     - |  7785 | ` * The four schema doors -- {XML Schema, RelaxNG} x {a FILE, a STRING} -- and` |
|     - |  7786 | `` * php's `validate()` beside them, all one shape:`` |
|     - |  7787 | ` *` |
|     - |  7788 | ` *   parse the schema (loudly: every libxml complaint reaches the caller's` |
|     - |  7789 | ` *   error handler), and if that fails say "Invalid Schema" / "Invalid RelaxNG"` |
|     - |  7790 | ` *   and answer false; otherwise validate the document and answer whether it` |
|     - |  7791 | ` *   came back clean.` |
|     - |  7792 | ` *` |
|     - |  7793 | ` * Only the pair of libxml families differs, so the switch is four calls wide` |
|     - |  7794 | ` * and the plumbing -- the argument screens, the diagnostic capture, the` |
|     - |  7795 | ` * refusals -- is written once.  The names a caller sees are php's: a filename` |
|     - |  7796 | ` * that is empty or carries a NUL is a ValueError naming the argument, raised` |
|     - |  7797 | ` * before anything is opened.` |
|     - |  7798 | ` */` |
|     - |  7799 | `#define DOM_VAL_SCHEMA 0` |
|     - |  7800 | `#define DOM_VAL_RELAX  1` |
|     - |  7801 |  |
|     - |  7802 | `/*` |
|     - |  7803 | ` * Schema, RelaxNG and DTD-validity diagnostics: onto the shared per-VM queue` |
|     - |  7804 | ` * through PH7_LibxmlQueueError, exactly like the global structured handler.` |
|     - |  7805 | ` *` |
|     - |  7806 | ` * php installs libxml's printf-style pair here instead, which is why its` |
|     - |  7807 | ` * validation diagnostics read as libxml writes them -- "I/O warning : failed` |
|     - |  7808 | ` * to load external entity ...", a parse error over three lines with the` |
|     - |  7809 | ` * offending source and a caret under it -- while every message this engine` |
|     - |  7810 | ` * drains is one structured record with its location appended.  The structured` |
|     - |  7811 | ` * handler is the one this file must keep: it is also what feeds` |
|     - |  7812 | `` * `libxml_get_errors()`, and php's own switches to exactly this shape once`` |
|     - |  7813 | `` * `libxml_use_internal_errors(true)` is on.  The ANSWERS agree; the wording of`` |
|     - |  7814 | ` * a failure does not (the error-format class).` |
|     - |  7815 | ` */` |
|     - |  7816 | `#if LIBXML_VERSION >= 21200` |
|    12 |  7817 | `static void DomSchemaErr(void *pUserData,const xmlError *pErr)` |
|     - |  7818 | `#else` |
|     8 |  7819 | `static void DomSchemaErr(void *pUserData,xmlErrorPtr pErr)` |
|     - |  7820 | `#endif` |
|     1 |  7821 | `{` |
|    21 |  7822 | `	if( pErr == 0 ){` |
|   ! 0 |  7823 | `		return;` |
|     - |  7824 | `	}` |
|    33 |  7825 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|    20 |  7826 | `		pErr->int2,pErr->message,pErr->file);` |
|    13 |  7827 | `}` |
|     - |  7828 | `/* php's own last word when a schema will not parse, under the method's name. */` |
|     8 |  7829 | `static void DomValidateSaySo(ph7_vm *pVm,const char *zFn,const char *zWhat)` |
|     1 |  7830 | `{` |
|     - |  7831 | `	SyBlob sMsg;` |
|     9 |  7832 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     9 |  7833 | `	SyBlobFormat(&sMsg,"%s(): %s",zFn,zWhat);` |
|     9 |  7834 | `	SyBlobNullAppend(&sMsg);` |
|     9 |  7835 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|     9 |  7836 | `	SyBlobRelease(&sMsg);` |
|     9 |  7837 | `}` |
|     - |  7838 | `/*` |
|     - |  7839 | ` * The argument every schema door takes: a filename or the schema itself. The` |
|     - |  7840 | ` * two refusals are php's own and answer before any parse.` |
|     - |  7841 | ` */` |
|    36 |  7842 | `static int DomValidateArg(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,` |
|     - |  7843 | `	const char *zFn,const char **pzSrc,int *pnSrc,int *pRc)` |
|     1 |  7844 | `{` |
|    37 |  7845 | `	int nSrc = 0;` |
|    37 |  7846 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|    37 |  7847 | `	const char *zParam = bFile ? "filename" : "source";` |
|    37 |  7848 | `	if( bFile && nSrc != (int)SyStrlen(zSrc) ){` |
|     4 |  7849 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|     1 |  7850 | `			"%s(): Argument #1 ($%s) must not contain any null bytes",zFn,zParam);` |
|     3 |  7851 | `		return 0;` |
|     - |  7852 | `	}` |
|    35 |  7853 | `	if( nSrc < 1 ){` |
|    13 |  7854 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|     4 |  7855 | `			"%s(): Argument #1 ($%s) must not be empty",zFn,zParam);` |
|     9 |  7856 | `		return 0;` |
|     - |  7857 | `	}` |
|    27 |  7858 | `	*pzSrc = zSrc;` |
|    27 |  7859 | `	*pnSrc = nSrc;` |
|    27 |  7860 | `	return 1;` |
|    19 |  7861 | `}` |
|    36 |  7862 | `static int DomValidateRun(ph7_context *pCtx,int nArg,ph7_value **apArg,int iKind,` |
|     - |  7863 | `	int bFile,const char *zFn)` |
|     1 |  7864 | `{` |
|    37 |  7865 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 |  7866 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|    37 |  7867 | `	int nSrc = 0,rc = PH7_OK,iRc;` |
|    37 |  7868 | `	const char *zSrc = "";` |
|     - |  7869 | `	sxu32 nMark;` |
|     - |  7870 | `	/* php reads the option word from the SCHEMA pair only; RelaxNG's two` |
|     - |  7871 | `	 * declare no second parameter at all. LIBXML_SCHEMA_CREATE is the one bit` |
|     - |  7872 | `	 * it acts on -- "write the schema's default values into the document". */` |
|    31 |  7873 | `	int bCreate = iKind == DOM_VAL_SCHEMA && nArg > 1` |
|    47 |  7874 | `		&& (ph7_value_to_int(apArg[1]) & XML_SCHEMA_VAL_VC_I_CREATE) != 0;` |
|    37 |  7875 | `	if( !DomValidateArg(pCtx,nArg,apArg,bFile,zFn,&zSrc,&nSrc,&rc) ){` |
|    11 |  7876 | `		return rc;` |
|     - |  7877 | `	}` |
|    27 |  7878 | `	if( pDocNd == 0 ){` |
|   ! 0 |  7879 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  7880 | `		return PH7_OK;` |
|     - |  7881 | `	}` |
|    27 |  7882 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    27 |  7883 | `	if( iKind == DOM_VAL_SCHEMA ){` |
|    14 |  7884 | `		xmlSchemaParserCtxtPtr pParser = bFile ? xmlSchemaNewParserCtxt(zSrc)` |
|    11 |  7885 | `		                                       : xmlSchemaNewMemParserCtxt(zSrc,nSrc);` |
|     - |  7886 | `		xmlSchemaPtr pSchema;` |
|     - |  7887 | `		xmlSchemaValidCtxtPtr pValid;` |
|    17 |  7888 | `		if( pParser == 0 ){` |
|   ! 0 |  7889 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|   ! 0 |  7890 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  7891 | `			return PH7_OK;` |
|     - |  7892 | `		}` |
|    17 |  7893 | `		xmlSchemaSetParserStructuredErrors(pParser,DomSchemaErr,pVm);` |
|    17 |  7894 | `		pSchema = xmlSchemaParse(pParser);` |
|    17 |  7895 | `		xmlSchemaFreeParserCtxt(pParser);` |
|    17 |  7896 | `		if( pSchema == 0 ){` |
|     5 |  7897 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|     5 |  7898 | `			DomValidateSaySo(pVm,zFn,"Invalid Schema");` |
|     5 |  7899 | `			ph7_result_bool(pCtx,0);` |
|     5 |  7900 | `			return PH7_OK;` |
|     - |  7901 | `		}` |
|    13 |  7902 | `		pValid = xmlSchemaNewValidCtxt(pSchema);` |
|    13 |  7903 | `		if( pValid == 0 ){` |
|   ! 0 |  7904 | `			xmlSchemaFree(pSchema);` |
|   ! 0 |  7905 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|   ! 0 |  7906 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  7907 | `			return PH7_OK;` |
|     - |  7908 | `		}` |
|    13 |  7909 | `		if( bCreate ){` |
|     3 |  7910 | `			xmlSchemaSetValidOptions(pValid,XML_SCHEMA_VAL_VC_I_CREATE);` |
|     1 |  7911 | `		}` |
|    13 |  7912 | `		xmlSchemaSetValidStructuredErrors(pValid,DomSchemaErr,pVm);` |
|    13 |  7913 | `		iRc = xmlSchemaValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);` |
|    13 |  7914 | `		xmlSchemaFreeValidCtxt(pValid);` |
|    13 |  7915 | `		xmlSchemaFree(pSchema);` |
|     7 |  7916 | `	}else{` |
|     9 |  7917 | `		xmlRelaxNGParserCtxtPtr pParser = bFile ? xmlRelaxNGNewParserCtxt(zSrc)` |
|     7 |  7918 | `		                                        : xmlRelaxNGNewMemParserCtxt(zSrc,nSrc);` |
|     - |  7919 | `		xmlRelaxNGPtr pSchema;` |
|     - |  7920 | `		xmlRelaxNGValidCtxtPtr pValid;` |
|    11 |  7921 | `		if( pParser == 0 ){` |
|   ! 0 |  7922 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|   ! 0 |  7923 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  7924 | `			return PH7_OK;` |
|     - |  7925 | `		}` |
|    11 |  7926 | `		xmlRelaxNGSetParserStructuredErrors(pParser,DomSchemaErr,pVm);` |
|    11 |  7927 | `		pSchema = xmlRelaxNGParse(pParser);` |
|    11 |  7928 | `		xmlRelaxNGFreeParserCtxt(pParser);` |
|    11 |  7929 | `		if( pSchema == 0 ){` |
|     5 |  7930 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|     5 |  7931 | `			DomValidateSaySo(pVm,zFn,"Invalid RelaxNG");` |
|     5 |  7932 | `			ph7_result_bool(pCtx,0);` |
|     5 |  7933 | `			return PH7_OK;` |
|     - |  7934 | `		}` |
|     7 |  7935 | `		pValid = xmlRelaxNGNewValidCtxt(pSchema);` |
|     7 |  7936 | `		if( pValid == 0 ){` |
|   ! 0 |  7937 | `			xmlRelaxNGFree(pSchema);` |
|   ! 0 |  7938 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|   ! 0 |  7939 | `			ph7_result_bool(pCtx,0);` |
|   ! 0 |  7940 | `			return PH7_OK;` |
|     - |  7941 | `		}` |
|     7 |  7942 | `		xmlRelaxNGSetValidStructuredErrors(pValid,DomSchemaErr,pVm);` |
|     7 |  7943 | `		iRc = xmlRelaxNGValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);` |
|     7 |  7944 | `		xmlRelaxNGFreeValidCtxt(pValid);` |
|     7 |  7945 | `		xmlRelaxNGFree(pSchema);` |
|     - |  7946 | `	}` |
|    19 |  7947 | `	PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    19 |  7948 | `	ph7_result_bool(pCtx,iRc == 0);` |
|    19 |  7949 | `	return PH7_OK;` |
|    19 |  7950 | `}` |
|     - |  7951 | `/* DOMDocument::schemaValidate(string $filename, int $flags = 0): bool */` |
|    14 |  7952 | `DOM_METHOD(vm_builtin_DOMDocument_schemaValidate)` |
|     1 |  7953 | `{` |
|    15 |  7954 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_SCHEMA,TRUE,"DOMDocument::schemaValidate");` |
|     1 |  7955 | `}` |
|     - |  7956 | `/* DOMDocument::schemaValidateSource(string $source, int $flags = 0): bool */` |
|     8 |  7957 | `DOM_METHOD(vm_builtin_DOMDocument_schemaValidateSource)` |
|     1 |  7958 | `{` |
|     9 |  7959 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_SCHEMA,FALSE,"DOMDocument::schemaValidateSource");` |
|     1 |  7960 | `}` |
|     - |  7961 | `/* DOMDocument::relaxNGValidate(string $filename): bool */` |
|     8 |  7962 | `DOM_METHOD(vm_builtin_DOMDocument_relaxNGValidate)` |
|     1 |  7963 | `{` |
|     9 |  7964 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_RELAX,TRUE,"DOMDocument::relaxNGValidate");` |
|     1 |  7965 | `}` |
|     - |  7966 | `/* DOMDocument::relaxNGValidateSource(string $source): bool */` |
|     6 |  7967 | `DOM_METHOD(vm_builtin_DOMDocument_relaxNGValidateSource)` |
|     1 |  7968 | `{` |
|     7 |  7969 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_RELAX,FALSE,"DOMDocument::relaxNGValidateSource");` |
|     1 |  7970 | `}` |
|     - |  7971 | `/*` |
|     - |  7972 | ` * DOMDocument::validate(): bool -- against the document's OWN DTD, which is` |
|     - |  7973 | ` * the one question of the five that takes no argument. libxml's validity` |
|     - |  7974 | ` * complaints ("no DTD found!", "root and DTD name do not match") reach the` |
|     - |  7975 | ` * caller through the same per-VM queue every other diagnostic here does.` |
|     - |  7976 | ` */` |
|     8 |  7977 | `DOM_METHOD(vm_builtin_DOMDocument_validate)` |
|     1 |  7978 | `{` |
|     9 |  7979 | `	ph7_vm *pVm = pCtx->pVm;` |
|     9 |  7980 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     - |  7981 | `	xmlValidCtxtPtr pValid;` |
|     - |  7982 | `	sxu32 nMark;` |
|     - |  7983 | `	int iRc;` |
|     4 |  7984 | `	SXUNUSED(nArg);` |
|     4 |  7985 | `	SXUNUSED(apArg);` |
|     9 |  7986 | `	if( pDocNd == 0 ){` |
|   ! 0 |  7987 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  7988 | `		return PH7_OK;` |
|     - |  7989 | `	}` |
|     9 |  7990 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     9 |  7991 | `	pValid = xmlNewValidCtxt();` |
|     9 |  7992 | `	if( pValid == 0 ){` |
|   ! 0 |  7993 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::validate");` |
|   ! 0 |  7994 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  7995 | `		return PH7_OK;` |
|     - |  7996 | `	}` |
|     9 |  7997 | `	iRc = xmlValidateDocument(pValid,(xmlDocPtr)pDocNd->pNode);` |
|     9 |  7998 | `	xmlFreeValidCtxt(pValid);` |
|     9 |  7999 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::validate");` |
|     9 |  8000 | `	ph7_result_bool(pCtx,iRc != 0);` |
|     9 |  8001 | `	return PH7_OK;` |
|     5 |  8002 | `}` |
|     - |  8003 | `/*` |
|     - |  8004 | ` * The MARKERS libxml leaves around everything it substituted.` |
|     - |  8005 | ` *` |
|     - |  8006 | ` * An XInclude pass wraps each replacement in an XML_XINCLUDE_START /` |
|     - |  8007 | ` * XML_XINCLUDE_END pair, which are nodes in the tree like any other: they` |
|     - |  8008 | `` * answer from `childNodes`, they shift every index after them, and the first`` |
|     - |  8009 | `` * child of an element whose only content was an `<xi:include>` is one of them`` |
|     - |  8010 | ` * rather than what was included.  php takes them out before answering, so the` |
|     - |  8011 | ` * document a caller gets back is the substituted one and nothing else.  They` |
|     - |  8012 | ` * are parked on the orphan set rather than freed, like every other node this` |
|     - |  8013 | ` * file unlinks.` |
|     - |  8014 | ` */` |
|    14 |  8015 | `static void DomDropXIncludeMarks(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|     1 |  8016 | `{` |
|     - |  8017 | `	xmlNodePtr pNext;` |
|    29 |  8018 | `	while( pNode ){` |
|    15 |  8019 | `		pNext = pNode->next;` |
|    15 |  8020 | `		if( pNode->type == XML_XINCLUDE_START \|\| pNode->type == XML_XINCLUDE_END ){` |
|     5 |  8021 | `			xmlUnlinkNode(pNode);` |
|     5 |  8022 | `			DomOrphanAdd(pShell,pNode);` |
|     3 |  8023 | `		}else{` |
|    11 |  8024 | `			DomDropXIncludeMarks(pShell,pNode->children);` |
|     - |  8025 | `		}` |
|    15 |  8026 | `		pNode = pNext;` |
|     1 |  8027 | `	}` |
|    15 |  8028 | `}` |
|     - |  8029 | `/*` |
|     - |  8030 | ` * DOMDocument::xinclude(int $options = 0): int\|false` |
|     - |  8031 | ` *` |
|     - |  8032 | ` * php answers the COUNT of substitutions libxml made, -1 when one of them` |
|     - |  8033 | ` * failed -- and FALSE when there were none at all, which is not an error and` |
|     - |  8034 | ` * is the one answer a caller has to screen for separately.` |
|     - |  8035 | ` */` |
|     6 |  8036 | `DOM_METHOD(vm_builtin_DOMDocument_xinclude)` |
|     1 |  8037 | `{` |
|     7 |  8038 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 |  8039 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     7 |  8040 | `	int iOpts = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|     - |  8041 | `	sxu32 nMark;` |
|     - |  8042 | `	int nDone;` |
|     7 |  8043 | `	if( pDocNd == 0 ){` |
|   ! 0 |  8044 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  8045 | `		return PH7_OK;` |
|     - |  8046 | `	}` |
|     7 |  8047 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     7 |  8048 | `	nDone = xmlXIncludeProcessFlags((xmlDocPtr)pDocNd->pNode,iOpts);` |
|     7 |  8049 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::xinclude");` |
|     7 |  8050 | `	if( nDone >= 0 ){` |
|     7 |  8051 | `		DomDropXIncludeMarks(pDocNd->pShell,` |
|     4 |  8052 | `			((xmlDocPtr)pDocNd->pNode)->children);` |
|     2 |  8053 | `	}` |
|     7 |  8054 | `	if( nDone == 0 ){` |
|     3 |  8055 | `		ph7_result_bool(pCtx,0);` |
|     2 |  8056 | `	}else{` |
|     5 |  8057 | `		ph7_result_int(pCtx,nDone);` |
|     - |  8058 | `	}` |
|     7 |  8059 | `	return PH7_OK;` |
|     4 |  8060 | `}` |
|     - |  8061 |  |
|     - |  8062 | `/*` |
|     - |  8063 | ` * DOMDocument::registerNodeClass(string $baseClass, ?string $extendedClass): true` |
|     - |  8064 | ` *` |
|     - |  8065 | ` * php lets a program say which class a node should be WRAPPED in, per` |
|     - |  8066 | `` * document: register `MyElement` against `DOMElement` and every element of`` |
|     - |  8067 | ` * that document -- read from the tree or made by a factory -- comes back a` |
|     - |  8068 | ` * MyElement, so a walk can call the program's own methods on what it finds` |
|     - |  8069 | ` * instead of carrying a parallel table of its own.` |
|     - |  8070 | ` *` |
|     - |  8071 | ` * The lookup is by the class the extension would have used and by nothing` |
|     - |  8072 | `` * else: registering against `DOMNode` or `DOMCharacterData` changes NO`` |
|     - |  8073 | ` * wrapping, because an element is wrapped as a DOMElement and a text node as a` |
|     - |  8074 | ` * DOMText, and neither name is the one registered.` |
|     - |  8075 | ` *` |
|     - |  8076 | ` * The map is the document's, stored in a hidden slot beside its identity cache` |
|     - |  8077 | ` * and carried by a document CLONE the way the parser directives are.` |
|     - |  8078 | ` */` |
|     - |  8079 | `/* The map, materialized on the document the way its identity cache is. */` |
|  2732 |  8080 | `static ph7_hashmap * DomNodeClassMap(ph7_vm *pVm,ph7_class_instance *pDoc,int bMake)` |
|     5 |  8081 | `{` |
|  2737 |  8082 | `	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NCLS) : 0;` |
|  2737 |  8083 | `	if( pSlot == 0 \|\| (!bMake && (pSlot->iFlags & MEMOBJ_HASHMAP) == 0) ){` |
|  2699 |  8084 | `		return 0;` |
|     - |  8085 | `	}` |
|    40 |  8086 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     8 |  8087 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|   ! 0 |  8088 | `			return 0;` |
|     - |  8089 | `		}` |
|     3 |  8090 | `	}` |
|    40 |  8091 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|  1371 |  8092 | `}` |
|     - |  8093 | `/* The class a node of pDoc's tree is wrapped in: the registered one when the` |
|     - |  8094 | ` * document names it, php's own otherwise. */` |
|  2718 |  8095 | `static const char * DomWrapClassName(ph7_vm *pVm,ph7_class_instance *pDoc,int iKind,` |
|     - |  8096 | `	SyBlob *pOut)` |
|     5 |  8097 | `{` |
|  2723 |  8098 | `	const char *zBase = DomClassOfKind(iKind);` |
|  2723 |  8099 | `	ph7_hashmap *pMap = DomNodeClassMap(&(*pVm),pDoc,FALSE);` |
|  2723 |  8100 | `	ph7_hashmap_node *pEntry = 0;` |
|     - |  8101 | `	ph7_value sKey,*pHit;` |
|  2723 |  8102 | `	if( pMap == 0 ){` |
|  2699 |  8103 | `		return zBase;` |
|     - |  8104 | `	}` |
|    26 |  8105 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|    26 |  8106 | `	PH7_MemObjStringAppend(&sKey,zBase,(sxu32)SyStrlen(zBase));` |
|    26 |  8107 | `	if( PH7_HashmapLookup(pMap,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|    16 |  8108 | `		pHit = HashmapExtractNodeValue(pEntry);` |
|    16 |  8109 | `		if( pHit && (pHit->iFlags & MEMOBJ_STRING) ){` |
|    16 |  8110 | `			SyBlobAppend(pOut,SyBlobData(&pHit->sBlob),SyBlobLength(&pHit->sBlob));` |
|    16 |  8111 | `			SyBlobNullAppend(pOut);` |
|    16 |  8112 | `			zBase = (const char *)SyBlobData(pOut);` |
|     7 |  8113 | `		}` |
|     7 |  8114 | `	}` |
|    26 |  8115 | `	PH7_MemObjRelease(&sKey);` |
|    26 |  8116 | `	return zBase;` |
|  1364 |  8117 | `}` |
|    24 |  8118 | `DOM_METHOD(vm_builtin_DOMDocument_registerNodeClass)` |
|     2 |  8119 | `{` |
|    26 |  8120 | `	ph7_vm *pVm = pCtx->pVm;` |
|    26 |  8121 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    26 |  8122 | `	int nBase = 0,nExt = 0;` |
|    26 |  8123 | `	const char *zBase = nArg > 0 ? ph7_value_to_string(apArg[0],&nBase) : "";` |
|    26 |  8124 | `	const char *zExt = (nArg > 1 && !ph7_value_is_null(apArg[1]))` |
|    35 |  8125 | `		? ph7_value_to_string(apArg[1],&nExt) : 0;` |
|    26 |  8126 | `	ph7_class *pBase,*pExt = 0,*pNode;` |
|     - |  8127 | `	ph7_hashmap *pMap;` |
|     - |  8128 | `	ph7_value sKey,sVal;` |
|    26 |  8129 | `	pBase = PH7_VmExtractClass(pVm,zBase,(sxu32)nBase,FALSE,0);` |
|    26 |  8130 | `	pNode = PH7_VmExtractClass(pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);` |
|    26 |  8131 | `	if( pBase == 0 \|\| pNode == 0 \|\| !PH7_VmInstanceOf(pBase,pNode) ){` |
|     7 |  8132 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8133 | `			"DOMDocument::registerNodeClass(): Argument #1 ($baseClass) must be a "` |
|     2 |  8134 | `			"class name derived from DOMNode, %.*s given",nBase,zBase);` |
|     - |  8135 | `	}` |
|    22 |  8136 | `	if( zExt ){` |
|    20 |  8137 | `		pExt = PH7_VmExtractClass(pVm,zExt,(sxu32)nExt,FALSE,0);` |
|    20 |  8138 | `		if( pExt == 0 ){` |
|     4 |  8139 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|     - |  8140 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be "` |
|     1 |  8141 | `				"a valid class name or null, %.*s given",nExt,zExt);` |
|     - |  8142 | `		}` |
|    18 |  8143 | `		if( !PH7_VmInstanceOf(pExt,pBase) ){` |
|     - |  8144 | `			/* php's plain Error here, not a TypeError: the name IS a class, it` |
|     - |  8145 | `			 * is simply the wrong one. */` |
|     4 |  8146 | `			return PH7_VmThrowException(pCtx,"Error",` |
|     - |  8147 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be "` |
|     - |  8148 | `				"a class name derived from %z or null, %.*s given",` |
|     1 |  8149 | `				&pBase->sName,nExt,zExt);` |
|     - |  8150 | `		}` |
|    16 |  8151 | `		if( pExt->iFlags & PH7_CLASS_ABSTRACT ){` |
|     3 |  8152 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  8153 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must "` |
|     - |  8154 | `				"not be an abstract class");` |
|     - |  8155 | `		}` |
|     6 |  8156 | `	}` |
|    16 |  8157 | `	pMap = DomNodeClassMap(pVm,pThis,TRUE);` |
|    16 |  8158 | `	if( pMap == 0 ){` |
|   ! 0 |  8159 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8160 | `	}` |
|     - |  8161 | `	/* Keyed by the base's OWN spelling, which is the one the wrap looks up. */` |
|    16 |  8162 | `	PH7_MemObjInitFromString(pVm,&sKey,&pBase->sName);` |
|    16 |  8163 | `	if( pExt ){` |
|    14 |  8164 | `		PH7_MemObjInitFromString(pVm,&sVal,&pExt->sName);` |
|    14 |  8165 | `		PH7_HashmapInsert(pMap,&sKey,&sVal);` |
|    14 |  8166 | `		PH7_MemObjRelease(&sVal);` |
|     8 |  8167 | `	}else{` |
|     3 |  8168 | `		ph7_hashmap_node *pEntry = 0;` |
|     3 |  8169 | `		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|     3 |  8170 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|     1 |  8171 | `		}` |
|     - |  8172 | `	}` |
|    16 |  8173 | `	PH7_MemObjRelease(&sKey);` |
|    16 |  8174 | `	ph7_result_bool(pCtx,1);` |
|    16 |  8175 | `	return PH7_OK;` |
|    14 |  8176 | `}` |
|     - |  8177 |  |
|     - |  8178 | `/* ===== DOMImplementation ===== */` |
|     - |  8179 |  |
|     - |  8180 | `/*` |
|     - |  8181 | ` * php's factory for the two things that cannot be made from a document that` |
|     - |  8182 | ` * does not exist yet: a DOCTYPE, and a document with a namespaced root.` |
|     - |  8183 | ` *` |
|     - |  8184 | `` * It carries no state at all -- `new DOMImplementation` is enough, its three`` |
|     - |  8185 | `` * methods are ordinary instance methods, and `$doc->implementation` answers a`` |
|     - |  8186 | ` * FRESH one on every read.` |
|     - |  8187 | ` */` |
|     - |  8188 |  |
|     - |  8189 | `/* A node that belongs to NO document, wrapped and owned the way a constructed` |
|     - |  8190 | ` * one is: parked on the per-VM limbo shell, its own identity-cache holder. The` |
|     - |  8191 | ` * caller owns the reference. */` |
|    20 |  8192 | `static ph7_class_instance * DomLimboWrap(ph7_vm *pVm,xmlNodePtr pNode)` |
|     1 |  8193 | `{` |
|    21 |  8194 | `	const char *zClass = DomClassOfKind((int)pNode->type);` |
|    21 |  8195 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    21 |  8196 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|    21 |  8197 | `	phl_xmldoc *pShell = pObj ? DomLimboShell(&(*pVm)) : 0;` |
|    21 |  8198 | `	phl_domnode *pRes = pShell ? DomNewRes(&(*pVm),pShell,pNode) : 0;` |
|    21 |  8199 | `	if( pRes == 0 ){` |
|   ! 0 |  8200 | `		if( pObj ){` |
|   ! 0 |  8201 | `			PH7_ClassInstanceUnref(pObj);` |
|   ! 0 |  8202 | `		}` |
|   ! 0 |  8203 | `		return 0;` |
|     - |  8204 | `	}` |
|    21 |  8205 | `	DomOrphanAdd(pShell,pNode);` |
|    21 |  8206 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|    21 |  8207 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pObj);` |
|    21 |  8208 | `	DomCacheStore(&(*pVm),pObj,pNode,pObj);` |
|    21 |  8209 | `	return pObj;` |
|    11 |  8210 | `}` |
|     - |  8211 | `/*` |
|     - |  8212 | ` * DOMImplementation::hasFeature(string $feature, string $version): bool` |
|     - |  8213 | ` *` |
|     - |  8214 | ` * php's table is two rows wide and the version is compared as a STRING: only` |
|     - |  8215 | ` * "1.0", "2.0" and "" are versions at all, and of those "Core" answers for` |
|     - |  8216 | ``  * "1.0" alone where "XML" answers for every one. So `hasFeature('Core','2.0')` `` |
|     - |  8217 | ``  * is false while `hasFeature('XML','2.0')` is true, and `hasFeature('Core','1')` `` |
|     - |  8218 | ` * -- a version that is not spelled the way the table spells it -- is false.` |
|     - |  8219 | ` */` |
|    22 |  8220 | `DOM_METHOD(vm_builtin_DOMImplementation_hasFeature)` |
|     1 |  8221 | `{` |
|    23 |  8222 | `	const char *zFeature = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    23 |  8223 | `	const char *zVersion = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    31 |  8224 | `	int bKnown = DomNameIs(zVersion,"1.0") \|\| DomNameIs(zVersion,"2.0")` |
|    27 |  8225 | `		\|\| zVersion[0] == 0;` |
|    49 |  8226 | `	ph7_result_bool(pCtx,bKnown` |
|    31 |  8227 | `		&& (DomNameIsCi(zFeature,"XML")` |
|    16 |  8228 | `			\|\| (DomNameIsCi(zFeature,"Core") && DomNameIs(zVersion,"1.0"))));` |
|    23 |  8229 | `	return PH7_OK;` |
|     1 |  8230 | `}` |
|     - |  8231 | `/*` |
|     - |  8232 | ` * DOMImplementation::createDocumentType(string $qualifiedName,` |
|     - |  8233 | ` *     string $publicId = '', string $systemId = ''): DOMDocumentType` |
|     - |  8234 | ` *` |
|     - |  8235 | `` * The name is not checked at ALL beyond being non-empty -- `1bad`, `a b` and`` |
|     - |  8236 | `` * `p:q:r` are each a doctype php builds without a word -- because nothing has`` |
|     - |  8237 | `` * parsed it: the name is the bytes the `<!DOCTYPE ...>` line will carry.`` |
|     - |  8238 | ` */` |
|    22 |  8239 | `DOM_METHOD(vm_builtin_DOMImplementation_createDocumentType)` |
|     1 |  8240 | `{` |
|    23 |  8241 | `	int nName = 0;` |
|    23 |  8242 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|    23 |  8243 | `	const char *zPub = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    23 |  8244 | `	const char *zSys = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|     - |  8245 | `	ph7_class_instance *pObj;` |
|     - |  8246 | `	xmlDtdPtr pDtd;` |
|    23 |  8247 | `	if( nName < 1 ){` |
|     3 |  8248 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|     - |  8249 | `			"DOMImplementation::createDocumentType(): Argument #1 ($qualifiedName) "` |
|     - |  8250 | `			"must not be empty");` |
|     - |  8251 | `	}` |
|    41 |  8252 | `	pDtd = xmlNewDtd(0,(const xmlChar *)zName,` |
|    20 |  8253 | `		zPub[0] ? (const xmlChar *)zPub : 0,` |
|    20 |  8254 | `		zSys[0] ? (const xmlChar *)zSys : 0);` |
|    21 |  8255 | `	if( pDtd == 0 ){` |
|   ! 0 |  8256 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8257 | `	}` |
|    21 |  8258 | `	pObj = DomLimboWrap(pCtx->pVm,(xmlNodePtr)pDtd);` |
|    21 |  8259 | `	if( pObj == 0 ){` |
|   ! 0 |  8260 | `		xmlFreeDtd(pDtd);` |
|   ! 0 |  8261 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8262 | `	}` |
|    21 |  8263 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    21 |  8264 | `	return PH7_OK;` |
|    12 |  8265 | `}` |
|     - |  8266 | `/*` |
|     - |  8267 | ` * DOMImplementation::createDocument(?string $namespace = null,` |
|     - |  8268 | ` *     string $qualifiedName = '', ?DOMDocumentType $doctype = null): DOMDocument` |
|     - |  8269 | ` *` |
|     - |  8270 | ` * An empty qualified name is a document with no root at all, which is what` |
|     - |  8271 | ` * makes the three-argument call with only a doctype meaningful.  The name is a` |
|     - |  8272 | `` * QName or nothing (`1bad` and `a:b:c` are the Namespace Error), and the`` |
|     - |  8273 | ` * namespace decides what becomes of its PREFIX: with a URI the element is` |
|     - |  8274 | ` * declared under it, and WITHOUT one the prefix is simply dropped -- php` |
|     - |  8275 | ` * builds the element from the local name and hangs the declaration on it` |
|     - |  8276 | `` * afterwards, so `createDocument('', 'p:root')` is `<root/>`.`` |
|     - |  8277 | ` *` |
|     - |  8278 | ` * A doctype that already belongs to a document is the Wrong Document Error,` |
|     - |  8279 | ` * so the same DOMDocumentType cannot seed two documents.` |
|     - |  8280 | ` */` |
|    26 |  8281 | `DOM_METHOD(vm_builtin_DOMImplementation_createDocument)` |
|     1 |  8282 | `{` |
|    27 |  8283 | `	ph7_vm *pVm = pCtx->pVm;` |
|    27 |  8284 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|    27 |  8285 | `	int nName = 0;` |
|    27 |  8286 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],&nName) : "";` |
|    27 |  8287 | `	phl_domnode *pDtdNd = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? DomObjArg(apArg[2]) : 0;` |
|    27 |  8288 | `	xmlDtdPtr pDtd = pDtdNd ? (xmlDtdPtr)pDtdNd->pNode : 0;` |
|     - |  8289 | `	ph7_class *pClass;` |
|     - |  8290 | `	ph7_class_instance *pObj;` |
|     - |  8291 | `	phl_xmldoc *pShell;` |
|     - |  8292 | `	phl_domnode *pRes;` |
|     - |  8293 | `	xmlDocPtr pDoc;` |
|    27 |  8294 | `	xmlNodePtr pRoot = 0;` |
|    27 |  8295 | `	xmlNsPtr pNs = 0;` |
|    27 |  8296 | `	xmlChar *zPrefix = 0,*zLocal = 0;` |
|    27 |  8297 | `	if( pDtd && pDtd->doc ){` |
|     - |  8298 | `		/* php's own screen, and the reason a doctype seeds ONE document. */` |
|     3 |  8299 | `		return DomThrowAlways(pCtx,DOM_ERR_WRONG_DOC);` |
|     - |  8300 | `	}` |
|    25 |  8301 | `	if( nName > 0 ){` |
|    19 |  8302 | `		if( xmlValidateQName((const xmlChar *)zName,0) != 0 ){` |
|     9 |  8303 | `			return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  8304 | `		}` |
|    11 |  8305 | `		zLocal = xmlSplitQName2((const xmlChar *)zName,&zPrefix);` |
|    11 |  8306 | `		if( zLocal == 0 ){` |
|     7 |  8307 | `			zLocal = xmlStrdup((const xmlChar *)zName);` |
|     3 |  8308 | `		}` |
|    11 |  8309 | `		if( zUri && zUri[0] ){` |
|     - |  8310 | `			/* php asks libxml for the declaration BEFORE it has a node to hang` |
|     - |  8311 | ``			 * it on, and takes a refusal (the `xml` prefix over its own URI is`` |
|     - |  8312 | `			 * one) as the Namespace Error. */` |
|     5 |  8313 | `			pNs = xmlNewNs(0,zUri,zPrefix);` |
|     5 |  8314 | `			if( pNs == 0 ){` |
|   ! 0 |  8315 | `				if( zLocal ){` |
|   ! 0 |  8316 | `					xmlFree(zLocal);` |
|   ! 0 |  8317 | `				}` |
|   ! 0 |  8318 | `				if( zPrefix ){` |
|   ! 0 |  8319 | `					xmlFree(zPrefix);` |
|   ! 0 |  8320 | `				}` |
|   ! 0 |  8321 | `				return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);` |
|     - |  8322 | `			}` |
|     2 |  8323 | `		}` |
|     5 |  8324 | `	}` |
|    17 |  8325 | `	pDoc = xmlNewDoc((const xmlChar *)"1.0");` |
|    17 |  8326 | `	pClass = pDoc ? PH7_VmExtractClass(pVm,"DOMDocument",sizeof("DOMDocument")-1,FALSE,0) : 0;` |
|    17 |  8327 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|    17 |  8328 | `	pShell = pObj ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;` |
|    17 |  8329 | `	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|    17 |  8330 | `	if( pRes == 0 ){` |
|   ! 0 |  8331 | `		if( pDoc && pShell == 0 ){` |
|   ! 0 |  8332 | `			xmlFreeDoc(pDoc);` |
|   ! 0 |  8333 | `		}` |
|   ! 0 |  8334 | `		if( pObj ){` |
|   ! 0 |  8335 | `			PH7_ClassInstanceUnref(pObj);` |
|   ! 0 |  8336 | `		}` |
|   ! 0 |  8337 | `		if( pNs ){` |
|   ! 0 |  8338 | `			xmlFreeNs(pNs);` |
|   ! 0 |  8339 | `		}` |
|   ! 0 |  8340 | `		if( zLocal ){` |
|   ! 0 |  8341 | `			xmlFree(zLocal);` |
|   ! 0 |  8342 | `		}` |
|   ! 0 |  8343 | `		if( zPrefix ){` |
|   ! 0 |  8344 | `			xmlFree(zPrefix);` |
|   ! 0 |  8345 | `		}` |
|   ! 0 |  8346 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  8347 | `	}` |
|    17 |  8348 | `	DomSetRes(pVm,pObj,pRes);` |
|    17 |  8349 | `	PH7_NativeSetAttrObj(pVm,pObj,DOM_DOC,pObj);` |
|    17 |  8350 | `	if( pDtd ){` |
|     - |  8351 | `		/* The doctype MOVES: it leaves the limbo shell for this document's` |
|     - |  8352 | `		 * tree, and every wrapper of it -- its own and its declarations' --` |
|     - |  8353 | `		 * re-homes on adoptNode's machinery, which is what makes the doctype` |
|     - |  8354 | ``		 * answer this document as its `ownerDocument` afterwards rather than`` |
|     - |  8355 | `		 * the holder it was its own. */` |
|     5 |  8356 | `		ph7_class_instance *pDtdObj = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     5 |  8357 | `		DomOrphanRemove(pDtdNd->pShell,(xmlNodePtr)pDtd);` |
|     5 |  8358 | `		pDtd->doc = pDoc;` |
|     5 |  8359 | `		pDoc->intSubset = pDtd;` |
|     5 |  8360 | `		DomLinkLast((xmlNodePtr)pDoc,(xmlNodePtr)pDtd);` |
|     5 |  8361 | `		DomAdoptWrappers(pVm,pDtdObj,pObj,pShell,(xmlNodePtr)pDtd);` |
|     2 |  8362 | `	}` |
|    17 |  8363 | `	if( nName > 0 ){` |
|    11 |  8364 | `		pRoot = xmlNewDocNode(pDoc,0,zLocal,0);` |
|    11 |  8365 | `		if( pRoot ){` |
|    11 |  8366 | `			xmlDocSetRootElement(pDoc,pRoot);` |
|    11 |  8367 | `			if( pNs ){` |
|     5 |  8368 | `				pNs->next = pRoot->nsDef;` |
|     5 |  8369 | `				pRoot->nsDef = pNs;` |
|     5 |  8370 | `				xmlSetNs(pRoot,pNs);` |
|     5 |  8371 | `				pNs = 0;` |
|     2 |  8372 | `			}` |
|     5 |  8373 | `		}` |
|     5 |  8374 | `	}` |
|    17 |  8375 | `	if( pNs ){` |
|   ! 0 |  8376 | `		xmlFreeNs(pNs);` |
|   ! 0 |  8377 | `	}` |
|    17 |  8378 | `	if( zLocal ){` |
|    11 |  8379 | `		xmlFree(zLocal);` |
|     5 |  8380 | `	}` |
|    17 |  8381 | `	if( zPrefix ){` |
|     5 |  8382 | `		xmlFree(zPrefix);` |
|     2 |  8383 | `	}` |
|    17 |  8384 | `	PH7_NativeResultObject(pCtx,pObj);` |
|    17 |  8385 | `	return PH7_OK;` |
|    14 |  8386 | `}` |
|     - |  8387 |  |
|     - |  8388 | `/* ===== The shared __get dispatch ===== */` |
|     - |  8389 |  |
|     - |  8390 | `/*` |
|     - |  8391 | ` * DOMNode's virtual properties.` |
|     - |  8392 | ` *` |
|     - |  8393 | ` * php exposes these through property handlers on the class; PHL answers them` |
|     - |  8394 | ` * from __get, as the chunk did. Returns 1 when it recognised the name, so a` |
|     - |  8395 | ` * subclass's own __get can state its extras and then defer here -- which is` |
|     - |  8396 | `` * what `parent::__get($name)` did.`` |
|     - |  8397 | ` */` |
|  2942 |  8398 | `static int DomNodeProp(ph7_context *pCtx,const char *zName)` |
|     5 |  8399 | `{` |
|  2947 |  8400 | `	ph7_vm *pVm = pCtx->pVm;` |
|  2947 |  8401 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  2947 |  8402 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|  2947 |  8403 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|  2947 |  8404 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|  4418 |  8405 | `	int bIsDoc = pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE);` |
|  2947 |  8406 | `	if( DomNameIs(zName,"nodeName") ){` |
|   595 |  8407 | `		DomNodeName(pCtx,pNode);` |
|  2651 |  8408 | `	}else if( DomNameIs(zName,"nodeValue") ){` |
|    89 |  8409 | `		DomNodeValue(pCtx,pNode);` |
|  2311 |  8410 | `	}else if( DomNameIs(zName,"nodeType") ){` |
|   101 |  8411 | `		ph7_result_int(pCtx,DomNodeTypeOf(pNode));` |
|  2217 |  8412 | `	}else if( DomNameIs(zName,"textContent") ){` |
|    51 |  8413 | `		DomTextContent(pCtx,pNode);` |
|  2142 |  8414 | `	}else if( DomNameIs(zName,"parentNode") ){` |
|   119 |  8415 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);` |
|  2059 |  8416 | `	}else if( DomNameIs(zName,"firstChild") ){` |
|     - |  8417 | `		/* Through the entity-reference resolver: an ADOPTED constructed` |
|     - |  8418 | `		 * reference's raw children are cleared, and php's reader answers the` |
|     - |  8419 | `		 * document's declaration anyway.` |
|     - |  8420 | `		 *` |
|     - |  8421 | `		 * Both ends are gated on php's dom_node_children_valid, which the` |
|     - |  8422 | `		 * DOCTYPE is not: libxml links a DTD's declarations as its children` |
|     - |  8423 | ``		 * and php's `firstChild`/`lastChild`/`hasChildNodes()` answer null,`` |
|     - |  8424 | ``		 * null and false there all the same -- while `childNodes` (which does`` |
|     - |  8425 | `		 * NOT consult it) lists them. */` |
|   585 |  8426 | `		DomResultNodeOf(pCtx,pNd,DomNodeChildFirst(pNode));` |
|  1709 |  8427 | `	}else if( DomNameIs(zName,"lastChild") ){` |
|    81 |  8428 | `		DomResultNodeOf(pCtx,pNd,DomNodeChildLast(pNode));` |
|  1378 |  8429 | `	}else if( DomNameIs(zName,"nextSibling") ){` |
|    63 |  8430 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->next : 0);` |
|  1307 |  8431 | `	}else if( DomNameIs(zName,"previousSibling") ){` |
|     9 |  8432 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->prev : 0);` |
|  1272 |  8433 | `	}else if( DomNameIs(zName,"ownerDocument") ){` |
|     - |  8434 | `		/* A document has no owner document, which is also why DomWrap answers` |
|     - |  8435 | `		 * the document itself rather than a second wrapper for it. The NODE's` |
|     - |  8436 | `		 * document is the source of truth, not the $__doc slot: a constructed` |
|     - |  8437 | `		 * ownerless node's slot points at its own holder, and php answers` |
|     - |  8438 | `		 * null there until an insertion adopts it. */` |
|   126 |  8439 | `		DomResultWrap(pCtx,(bIsDoc \|\| pNode == 0 \|\| pNode->doc == 0) ? 0 : pDoc);` |
|  1205 |  8440 | `	}else if( DomNameIs(zName,"parentElement") ){` |
|     - |  8441 | ``		/* php's `?DOMElement`: the parent when it IS an element, so a root`` |
|     - |  8442 | `		 * element (whose parent is the document) answers null. An ATTRIBUTE` |
|     - |  8443 | `		 * answers its element -- libxml parents an attribute, and php reports` |
|     - |  8444 | ``		 * that parent from both this property and `parentNode`. */`` |
|    43 |  8445 | `		xmlNodePtr pPar = pNode ? pNode->parent : 0;` |
|    43 |  8446 | `		DomResultNodeOf(pCtx,pNd,(pPar && pPar->type == XML_ELEMENT_NODE) ? pPar : 0);` |
|  1122 |  8447 | `	}else if( DomNameIs(zName,"namespaceURI") ){` |
|   371 |  8448 | `		DomNamespaceUri(pCtx,pNode);` |
|   916 |  8449 | `	}else if( DomNameIs(zName,"prefix") ){` |
|   121 |  8450 | `		DomPrefix(pCtx,pNode);` |
|   671 |  8451 | `	}else if( DomNameIs(zName,"localName") ){` |
|   119 |  8452 | `		DomLocalName(pCtx,pNode);` |
|   552 |  8453 | `	}else if( DomNameIs(zName,"isConnected") ){` |
|    71 |  8454 | `		ph7_result_bool(pCtx,DomIsConnected(pNode));` |
|   458 |  8455 | `	}else if( DomNameIs(zName,"baseURI") ){` |
|     - |  8456 | ``		/* php's is libxml's own xmlNodeGetBase(): the nearest `xml:base` on the`` |
|     - |  8457 | `		 * way up, resolved against the DOCUMENT's URI, and that URI itself when` |
|     - |  8458 | `		 * no ancestor declares one. So it answers null exactly when the document` |
|     - |  8459 | ``		 * was never given a URI -- a `new DOMDocument()` that was not loaded --`` |
|     - |  8460 | `		 * and a node created and never appended still answers its document's. */` |
|    33 |  8461 | `		xmlChar *zBase = pNode ? xmlNodeGetBase(pNode->doc,pNode) : 0;` |
|    33 |  8462 | `		if( zBase ){` |
|    23 |  8463 | `			ph7_result_string(pCtx,(const char *)zBase,-1);` |
|    23 |  8464 | `			xmlFree(zBase);` |
|    12 |  8465 | `		}else{` |
|    11 |  8466 | `			ph7_result_null(pCtx);` |
|     1 |  8467 | `		}` |
|   407 |  8468 | `	}else if( DomNameIs(zName,"childNodes") ){` |
|   241 |  8469 | `		ph7_class_instance *pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_CHILD,pThis,0,0,0);` |
|   241 |  8470 | `		if( pList == 0 ){` |
|   ! 0 |  8471 | `			return -1;` |
|     - |  8472 | `		}` |
|   241 |  8473 | `		PH7_NativeResultObject(pCtx,pList);` |
|   271 |  8474 | `	}else if( DomNameIs(zName,"attributes") ){` |
|     - |  8475 | `		/* php: NULL for anything that is not an element. */` |
|   128 |  8476 | `		if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|     5 |  8477 | `			ph7_result_null(pCtx);` |
|     3 |  8478 | `		}else{` |
|   124 |  8479 | `			ph7_class_instance *pMap = DomNewCollection(pVm,"DOMNamedNodeMap",pDoc,DNL_CHILD,pThis,0,0,0);` |
|   124 |  8480 | `			if( pMap == 0 ){` |
|   ! 0 |  8481 | `				return -1;` |
|     - |  8482 | `			}` |
|   124 |  8483 | `			PH7_NativeResultObject(pCtx,pMap);` |
|     - |  8484 | `		}` |
|    65 |  8485 | `	}else{` |
|    25 |  8486 | `		return 0;` |
|     - |  8487 | `	}` |
|  2923 |  8488 | `	return 1;` |
|  1476 |  8489 | `}` |
|     - |  8490 | `/* The argument every __get body reads. */` |
|  6243 |  8491 | `static const char * DomGetName(int nArg,ph7_value **apArg)` |
|     5 |  8492 | `{` |
|  6248 |  8493 | `	return nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     5 |  8494 | `}` |
|     - |  8495 | `/*` |
|     - |  8496 | ` * The __get/__isset/__set trio every DOM class carries.` |
|     - |  8497 | ` *` |
|     - |  8498 | ` * Both ride ONE recognizer per class -- the DomProp_X readers below, which` |
|     - |  8499 | ` * answer 1 when the name is a property of that class and have written its` |
|     - |  8500 | ` * value, and 0 when it is not.  php models these as real (virtual) properties,` |
|     - |  8501 | `` * so the 0 case is its `Undefined property` WARNING rather than a silent null,`` |
|     - |  8502 | `` * and `isset()` is php's own has_property: the name has to exist AND read back`` |
|     - |  8503 | `` * non-null (which is what makes `isset($n->nextSibling)` false on a last child`` |
|     - |  8504 | `` * while `isset($n->nodeName)` is true).  Without the __isset half every`` |
|     - |  8505 | `` * `isset($doc->documentElement)` and every `$node->attributes ?? []` answered`` |
|     - |  8506 | ` * as though the whole surface were absent.` |
|     - |  8507 | ` */` |
|    24 |  8508 | `static int DomUndefProp(ph7_context *pCtx,const char *zName)` |
|     1 |  8509 | `{` |
|    25 |  8510 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    25 |  8511 | `	ph7_result_null(pCtx);` |
|    25 |  8512 | `	if( pThis ){` |
|     - |  8513 | `		/* php names the INSTANCE's class, so a userland subclass of DOMElement` |
|     - |  8514 | `		 * is reported under its own name. */` |
|     - |  8515 | `		SyBlob sMsg;` |
|     - |  8516 | `		SyString sName;` |
|    25 |  8517 | `		SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    25 |  8518 | `		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    25 |  8519 | `		SyBlobFormat(&sMsg,"Undefined property: %z::$%z",&pThis->pClass->sName,&sName);` |
|    25 |  8520 | `		SyBlobNullAppend(&sMsg);` |
|    25 |  8521 | `		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|    25 |  8522 | `		SyBlobRelease(&sMsg);` |
|    12 |  8523 | `	}` |
|    25 |  8524 | `	return PH7_OK;` |
|     1 |  8525 | `}` |
|     - |  8526 | `/*` |
|     - |  8527 | ` * The write half. A per-class WRITER answers one of these; the name it does` |
|     - |  8528 | ` * not write is looked up in the class's READER, which decides between php's` |
|     - |  8529 | ` * two refusals -- a property that exists is read-only, one that does not is a` |
|     - |  8530 | ` * dynamic property (deprecated in php 8.2, so §10 rejects it here, which is` |
|     - |  8531 | ` * what the engine's own store path would have said had the class carried no` |
|     - |  8532 | ` * __set at all).` |
|     - |  8533 | ` */` |
|     - |  8534 | `#define DOM_SET_UNKNOWN  0   /* not a property of this class */` |
|     - |  8535 | `#define DOM_SET_DONE     1   /* written, or a refusal already raised into *pRc */` |
|    88 |  8536 | `static int DomRefuseWrite(ph7_context *pCtx,const char *zName,int bKnown)` |
|     1 |  8537 | `{` |
|    89 |  8538 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - |  8539 | `	SyString sName;` |
|    89 |  8540 | `	if( pThis == 0 ){` |
|   ! 0 |  8541 | `		return PH7_OK;` |
|     - |  8542 | `	}` |
|    89 |  8543 | `	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));` |
|    89 |  8544 | `	return PH7_VmThrowException(pCtx,"Error",` |
|    44 |  8545 | `		bKnown ? "Cannot modify readonly property %z::$%z"` |
|     - |  8546 | `		       : "Cannot create dynamic property %z::$%z",` |
|    88 |  8547 | `		&pThis->pClass->sName,&sName);` |
|    45 |  8548 | `}` |
|     - |  8549 | `#define DOM_PROP_ACCESSORS(CLS,READER,WRITER)                                   \` |
|     - |  8550 | `	DOM_METHOD(vm_builtin_##CLS##_get)                                          \` |
|     - |  8551 | `	{                                                                           \` |
|     - |  8552 | `		const char *zName = DomGetName(nArg,apArg);                             \` |
|     - |  8553 | `		if( READER(pCtx,zName) == 0 ){                                          \` |
|     - |  8554 | `			return DomUndefProp(pCtx,zName);                                    \` |
|     - |  8555 | `		}                                                                       \` |
|     - |  8556 | `		return PH7_OK;                                                          \` |
|     - |  8557 | `	}                                                                           \` |
|     - |  8558 | `	DOM_METHOD(vm_builtin_##CLS##_isset)                                        \` |
|     - |  8559 | `	{                                                                           \` |
|     - |  8560 | `		int bKnown = READER(pCtx,DomGetName(nArg,apArg)) != 0;                  \` |
|     - |  8561 | `		int bNull = (pCtx->pRet->iFlags & MEMOBJ_NULL) != 0;                    \` |
|     - |  8562 | `		ph7_result_bool(pCtx,bKnown && !bNull);                                 \` |
|     - |  8563 | `		return PH7_OK;                                                          \` |
|     - |  8564 | `	}                                                                           \` |
|     - |  8565 | `	DOM_METHOD(vm_builtin_##CLS##_set)                                          \` |
|     - |  8566 | `	{                                                                           \` |
|     - |  8567 | `		const char *zName = DomGetName(nArg,apArg);                             \` |
|     - |  8568 | `		int rc = PH7_OK;                                                        \` |
|     - |  8569 | `		if( WRITER(pCtx,zName,nArg > 1 ? apArg[1] : 0,&rc) == DOM_SET_DONE ){   \` |
|     - |  8570 | `			return rc;                                                          \` |
|     - |  8571 | `		}                                                                       \` |
|     - |  8572 | `		return DomRefuseWrite(pCtx,zName,READER(pCtx,zName) != 0);              \` |
|     - |  8573 | `	}` |
|     - |  8574 | `/* A libxml string slot answered as php answers it: the bytes, or null when the` |
|     - |  8575 | `` * document never carried one (`encoding` on a declaration-less document). */`` |
|   165 |  8576 | `static void DomResultXmlStr(ph7_context *pCtx,const xmlChar *zVal)` |
|     1 |  8577 | `{` |
|   166 |  8578 | `	if( zVal ){` |
|   114 |  8579 | `		ph7_result_string(pCtx,(const char *)zVal,-1);` |
|    58 |  8580 | `	}else{` |
|    53 |  8581 | `		ph7_result_null(pCtx);` |
|     - |  8582 | `	}` |
|   166 |  8583 | `}` |
|     - |  8584 | `/*` |
|     - |  8585 | ` * The DOCUMENT's own state block.` |
|     - |  8586 | ` *` |
|     - |  8587 | ` * Nine of php's twenty-two DOMDocument properties are the XML DECLARATION and` |
|     - |  8588 | ` * the document's URI, read straight off libxml's xmlDoc -- and php spells most` |
|     - |  8589 | ` * of them twice, once under the DOM level-3 name and once under the level-1 one` |
|     - |  8590 | `` * it kept for compatibility (`version`/`xmlVersion`, `encoding`/`xmlEncoding`,`` |
|     - |  8591 | `` * `standalone`/`xmlStandalone`).  The pairs are not synonyms in every`` |
|     - |  8592 | ``  * direction: `xmlEncoding` and `actualEncoding` READ the same slot `encoding` `` |
|     - |  8593 | `` * writes and are themselves read-only, which is what makes `$d->xmlEncoding =`` |
|     - |  8594 | `` * 'UTF-8'` php's readonly Error and `$d->encoding = 'UTF-8'` the write that`` |
|     - |  8595 | `` * changes the bytes `saveXML()` emits.`` |
|     - |  8596 | ` *` |
|     - |  8597 | `` * `actualEncoding` and `config` carry php 8.4's #[\Deprecated]: the notice`` |
|     - |  8598 | `` * fires on a READ and on an `isset()` alike (both go through php's property`` |
|     - |  8599 | ` * handler), which is why it is raised HERE rather than in __get -- and NOT on a` |
|     - |  8600 | ` * write, where the readonly refusal comes first and is raised by the writer` |
|     - |  8601 | ` * below without consulting this reader.` |
|     - |  8602 | ` */` |
|   275 |  8603 | `static int DomDocStateProp(ph7_context *pCtx,const char *zName,xmlDocPtr pDoc)` |
|     1 |  8604 | `{` |
|   276 |  8605 | `	int bDeprAe = DomNameIs(zName,"actualEncoding");` |
|   276 |  8606 | `	if( bDeprAe \|\| DomNameIs(zName,"config") ){` |
|    46 |  8607 | `		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|    15 |  8608 | `			bDeprAe ? "Property DOMDocument::$actualEncoding is deprecated"` |
|     - |  8609 | `			        : "Property DOMDocument::$config is deprecated");` |
|     - |  8610 | ``		/* `config` is php's DOM level-3 configuration slot and has never been`` |
|     - |  8611 | `		 * filled in there: the handler answers null and nothing else. */` |
|    31 |  8612 | `		if( bDeprAe ){` |
|    15 |  8613 | `			DomResultXmlStr(pCtx,pDoc ? pDoc->encoding : 0);` |
|     8 |  8614 | `		}else{` |
|    17 |  8615 | `			ph7_result_null(pCtx);` |
|     - |  8616 | `		}` |
|    31 |  8617 | `		return 1;` |
|     - |  8618 | `	}` |
|   246 |  8619 | `	if( DomNameIs(zName,"encoding") \|\| DomNameIs(zName,"xmlEncoding") ){` |
|    44 |  8620 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->encoding : 0);` |
|    44 |  8621 | `		return 1;` |
|     - |  8622 | `	}` |
|   203 |  8623 | `	if( DomNameIs(zName,"version") \|\| DomNameIs(zName,"xmlVersion") ){` |
|    57 |  8624 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->version : 0);` |
|    57 |  8625 | `		return 1;` |
|     - |  8626 | `	}` |
|   147 |  8627 | `	if( DomNameIs(zName,"documentURI") ){` |
|    27 |  8628 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->URL : 0);` |
|    27 |  8629 | `		return 1;` |
|     - |  8630 | `	}` |
|   121 |  8631 | `	if( DomNameIs(zName,"standalone") \|\| DomNameIs(zName,"xmlStandalone") ){` |
|     - |  8632 | `		/* libxml records four states in one int -- no declaration (-1), a` |
|     - |  8633 | ``		 * declaration without the attribute (-2), `no` (0) and `yes` (1) -- and`` |
|     - |  8634 | `		 * php's bool is true for the last one only. */` |
|    59 |  8635 | `		ph7_result_bool(pCtx,pDoc != 0 && pDoc->standalone == 1);` |
|    59 |  8636 | `		return 1;` |
|     - |  8637 | `	}` |
|    63 |  8638 | `	return 0;` |
|   139 |  8639 | `}` |
|     - |  8640 | `/*` |
|     - |  8641 | ` * The three DOMParentNode properties.  php declares them on the three` |
|     - |  8642 | ` * implementers ONLY -- DOMDocument, DOMElement and DOMDocumentFragment -- so` |
|     - |  8643 | `` * `$text->childElementCount` is the Undefined property warning there, which`` |
|     - |  8644 | ` * is why childElementCount cannot live in DomNodeProp (it did, and every node` |
|     - |  8645 | ` * kind answered 0 in silence where php warns and answers null).` |
|     - |  8646 | ` */` |
|  2122 |  8647 | `static int DomParentNodeProp(ph7_context *pCtx,const char *zName)` |
|     5 |  8648 | `{` |
|  2127 |  8649 | `	int bLast = DomNameIs(zName,"lastElementChild");` |
|  2127 |  8650 | `	if( bLast \|\| DomNameIs(zName,"firstElementChild") ){` |
|    25 |  8651 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|    25 |  8652 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    25 |  8653 | `		xmlNodePtr pChild = pNode ? (bLast ? pNode->last : pNode->children) : 0;` |
|    39 |  8654 | `		while( pChild && pChild->type != XML_ELEMENT_NODE ){` |
|    15 |  8655 | `			pChild = bLast ? pChild->prev : pChild->next;` |
|     1 |  8656 | `		}` |
|    25 |  8657 | `		DomResultNodeOf(pCtx,pNd,pChild);` |
|    25 |  8658 | `		return 1;` |
|     - |  8659 | `	}` |
|  2103 |  8660 | `	if( DomNameIs(zName,"childElementCount") ){` |
|    17 |  8661 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|    17 |  8662 | `		ph7_result_int(pCtx,DomChildCount(pNd ? (xmlNodePtr)pNd->pNode : 0,1));` |
|    17 |  8663 | `		return 1;` |
|     - |  8664 | `	}` |
|  2087 |  8665 | `	return 0;` |
|  1066 |  8666 | `}` |
|     - |  8667 | `/*` |
|     - |  8668 | ` * The two DOMChildNode-side properties.  php declares them on DOMElement and` |
|     - |  8669 | ` * DOMCharacterData only -- an attribute, a PI or the document warns Undefined` |
|     - |  8670 | ` * property -- and they skip every node kind that is not an element.` |
|     - |  8671 | ` */` |
|  2200 |  8672 | `static int DomChildNodeProp(ph7_context *pCtx,const char *zName)` |
|     5 |  8673 | `{` |
|  2205 |  8674 | `	int bNext = DomNameIs(zName,"nextElementSibling");` |
|  2205 |  8675 | `	if( bNext \|\| DomNameIs(zName,"previousElementSibling") ){` |
|    27 |  8676 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|    27 |  8677 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    27 |  8678 | `		xmlNodePtr pSib = pNode ? (bNext ? pNode->next : pNode->prev) : 0;` |
|    51 |  8679 | `		while( pSib && pSib->type != XML_ELEMENT_NODE ){` |
|    25 |  8680 | `			pSib = bNext ? pSib->next : pSib->prev;` |
|     1 |  8681 | `		}` |
|    27 |  8682 | `		DomResultNodeOf(pCtx,pNd,pSib);` |
|    27 |  8683 | `		return 1;` |
|     - |  8684 | `	}` |
|  2179 |  8685 | `	return 0;` |
|  1105 |  8686 | `}` |
|     - |  8687 | `/* DOMDocument adds documentElement and the state block above. */` |
|  2055 |  8688 | `static int DomDocProp(ph7_context *pCtx,const char *zName)` |
|     4 |  8689 | `{` |
|  2059 |  8690 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|  2059 |  8691 | `	if( DomNameIs(zName,"documentElement") ){` |
|  1736 |  8692 | `		DomResultNodeOf(pCtx,pNd,pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0);` |
|  1736 |  8693 | `		return 1;` |
|     - |  8694 | `	}` |
|   325 |  8695 | `	if( DomNameIs(zName,"implementation") ){` |
|     - |  8696 | `		/* A FRESH object on every read, which is php's: the class has no state` |
|     - |  8697 | `		 * and nothing ties one to a document. */` |
|    11 |  8698 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,"DOMImplementation",` |
|     - |  8699 | `			sizeof("DOMImplementation")-1,FALSE,0);` |
|    11 |  8700 | `		ph7_class_instance *pImpl = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|    11 |  8701 | `		if( pImpl ){` |
|    11 |  8702 | `			PH7_NativeResultObject(pCtx,pImpl);` |
|     6 |  8703 | `		}else{` |
|   ! 0 |  8704 | `			ph7_result_null(pCtx);` |
|     - |  8705 | `		}` |
|    11 |  8706 | `		return 1;` |
|     - |  8707 | `	}` |
|   315 |  8708 | `	if( DomNameIs(zName,"doctype") ){` |
|     - |  8709 | `		/* The INTERNAL subset alone, which is php's: a DTD pulled in from the` |
|     - |  8710 | `` 		 * SYSTEM identifier lands in `extSubset` and is not what `doctype` `` |
|     - |  8711 | `		 * answers. Null for a document that declares none. */` |
|    78 |  8712 | `		DomResultNodeOf(pCtx,pNd,` |
|    38 |  8713 | `			pNd ? (xmlNodePtr)xmlGetIntSubset((xmlDocPtr)pNd->pNode) : 0);` |
|    40 |  8714 | `		return 1;` |
|     - |  8715 | `	}` |
|   276 |  8716 | `	if( DomDocStateProp(pCtx,zName,pNd ? (xmlDocPtr)pNd->pNode : 0) ){` |
|   214 |  8717 | `		return 1;` |
|     - |  8718 | `	}` |
|    63 |  8719 | `	if( DomParentNodeProp(pCtx,zName) ){` |
|    11 |  8720 | `		return 1;` |
|     - |  8721 | `	}` |
|    53 |  8722 | `	return DomNodeProp(pCtx,zName);` |
|  1032 |  8723 | `}` |
|     - |  8724 | `/*` |
|     - |  8725 | `` * DOMDocumentType: what the `<!DOCTYPE ...>` line SAYS.`` |
|     - |  8726 | ` *` |
|     - |  8727 | `` * The DTD node has been reachable all along (`$doc->firstChild` on any document`` |
|     - |  8728 | ` * carrying a doctype), so what was missing was not the node but every question` |
|     - |  8729 | ``  * about it: the class, so `instanceof DOMDocumentType` and `get_class()` `` |
|     - |  8730 | ` * answer, and the four identifiers a program reads off one.` |
|     - |  8731 | ` *` |
|     - |  8732 | `` * php's `publicId`/`systemId` here are plain strings that answer "" when the`` |
|     - |  8733 | `` * declaration carries none -- unlike DOMEntity's, which are `?string` and null`` |
|     - |  8734 | ` * for the same absence -- so the two classes cannot share a reader.` |
|     - |  8735 | ` */` |
|   172 |  8736 | `static xmlDtdPtr DomThisDtd(ph7_context *pCtx)` |
|     2 |  8737 | `{` |
|   174 |  8738 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   174 |  8739 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   172 |  8740 | `	if( pNode == 0` |
|   174 |  8741 | `	 \|\| (pNode->type != XML_DTD_NODE && pNode->type != XML_DOCUMENT_TYPE_NODE) ){` |
|   ! 0 |  8742 | `		return 0;` |
|     - |  8743 | `	}` |
|   174 |  8744 | `	return (xmlDtdPtr)pNode;` |
|    88 |  8745 | `}` |
|     - |  8746 | `/*` |
|     - |  8747 | `` * `internalSubset`: the bytes BETWEEN the brackets, rebuilt by dumping each`` |
|     - |  8748 | ` * declaration the subset holds.  php reads them off the DOCUMENT's internal` |
|     - |  8749 | ` * subset rather than the receiver's own children -- so a doctype cloned out of` |
|     - |  8750 | ` * a document that has none answers null -- and answers null, not "", when` |
|     - |  8751 | ` * there is no subset to dump at all.` |
|     - |  8752 | ` */` |
|    18 |  8753 | `static void DomInternalSubset(ph7_context *pCtx,xmlDtdPtr pDtd)` |
|     1 |  8754 | `{` |
|    19 |  8755 | `	xmlDtdPtr pSub = (pDtd && pDtd->doc) ? xmlGetIntSubset(pDtd->doc) : 0;` |
|    19 |  8756 | `	xmlNodePtr pChild = pSub ? pSub->children : 0;` |
|     - |  8757 | `	xmlBufferPtr pBuf;` |
|     - |  8758 | `	xmlOutputBufferPtr pOut;` |
|    19 |  8759 | `	if( pChild == 0 ){` |
|    11 |  8760 | `		ph7_result_null(pCtx);` |
|    11 |  8761 | `		return;` |
|     - |  8762 | `	}` |
|     9 |  8763 | `	pBuf = xmlBufferCreate();` |
|     9 |  8764 | `	pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|     9 |  8765 | `	if( pOut == 0 ){` |
|   ! 0 |  8766 | `		if( pBuf ){` |
|   ! 0 |  8767 | `			xmlBufferFree(pBuf);` |
|   ! 0 |  8768 | `		}` |
|   ! 0 |  8769 | `		ph7_result_null(pCtx);` |
|   ! 0 |  8770 | `		return;` |
|     - |  8771 | `	}` |
|    17 |  8772 | `	for( ; pChild ; pChild = pChild->next ){` |
|     9 |  8773 | `		xmlNodeDumpOutput(pOut,pSub->doc,pChild,0,0,0);` |
|     5 |  8774 | `	}` |
|     9 |  8775 | `	xmlOutputBufferFlush(pOut);` |
|     9 |  8776 | `	ph7_result_string(pCtx,(const char *)xmlBufferContent(pBuf),(int)xmlBufferLength(pBuf));` |
|     9 |  8777 | `	xmlOutputBufferClose(pOut);` |
|     9 |  8778 | `	xmlBufferFree(pBuf);` |
|    10 |  8779 | `}` |
|   172 |  8780 | `static int DomDocTypeProp(ph7_context *pCtx,const char *zName)` |
|     2 |  8781 | `{` |
|   174 |  8782 | `	xmlDtdPtr pDtd = DomThisDtd(pCtx);` |
|   174 |  8783 | `	if( DomNameIs(zName,"name") ){` |
|     - |  8784 | `		/* The name the DOCTYPE declares, which is also its nodeName. */` |
|    33 |  8785 | `		ph7_result_string(pCtx,(pDtd && pDtd->name) ? (const char *)pDtd->name : "",-1);` |
|    33 |  8786 | `		return 1;` |
|     - |  8787 | `	}` |
|   142 |  8788 | `	if( DomNameIs(zName,"publicId") ){` |
|    15 |  8789 | `		ph7_result_string(pCtx,(pDtd && pDtd->ExternalID) ? (const char *)pDtd->ExternalID : "",-1);` |
|    15 |  8790 | `		return 1;` |
|     - |  8791 | `	}` |
|   128 |  8792 | `	if( DomNameIs(zName,"systemId") ){` |
|    15 |  8793 | `		ph7_result_string(pCtx,(pDtd && pDtd->SystemID) ? (const char *)pDtd->SystemID : "",-1);` |
|    15 |  8794 | `		return 1;` |
|     - |  8795 | `	}` |
|   114 |  8796 | `	if( DomNameIs(zName,"internalSubset") ){` |
|    19 |  8797 | `		DomInternalSubset(pCtx,pDtd);` |
|    19 |  8798 | `		return 1;` |
|     - |  8799 | `	}` |
|    96 |  8800 | `	if( DomNameIs(zName,"entities") \|\| DomNameIs(zName,"notations") ){` |
|    54 |  8801 | `		ph7_class_instance *pMap = DomNewCollection(pCtx->pVm,"DOMNamedNodeMap",` |
|    26 |  8802 | `			DomThisDoc(pCtx),DomNameIs(zName,"notations") ? DNL_NOTS : DNL_ENTS,` |
|    13 |  8803 | `			PH7_ContextThis(pCtx),0,0,0);` |
|    28 |  8804 | `		if( pMap ){` |
|    28 |  8805 | `			PH7_NativeResultObject(pCtx,pMap);` |
|    15 |  8806 | `		}else{` |
|   ! 0 |  8807 | `			ph7_result_null(pCtx);` |
|     - |  8808 | `		}` |
|    28 |  8809 | `		return 1;` |
|     - |  8810 | `	}` |
|    69 |  8811 | `	return DomNodeProp(pCtx,zName);` |
|    88 |  8812 | `}` |
|     - |  8813 | `/*` |
|     - |  8814 | `` * DOMEntity: an `<!ENTITY ...>` declaration of the internal subset.`` |
|     - |  8815 | ` *` |
|     - |  8816 | ` * Its three identifiers are the DOM's "for an UNPARSED entity" rule, which php` |
|     - |  8817 | `` * follows to the letter: `publicId`, `systemId` and `notationName` answer null`` |
|     - |  8818 | `` * for every entity that is not `NDATA`-declared, so the external-but-parsed`` |
|     - |  8819 | `` * `<!ENTITY e SYSTEM "e.xml">` reads null from all three while`` |
|     - |  8820 | `` * `<!ENTITY g SYSTEM "g.gif" NDATA gif>` reads its own two and the notation's`` |
|     - |  8821 | `` * name.  (`baseURI`, which DOMNode answers, is where the resolved system`` |
|     - |  8822 | ` * identifier does show for both.)` |
|     - |  8823 | ` *` |
|     - |  8824 | ` * The other three are php 8.4's deprecated block, and like DOMDocument's the` |
|     - |  8825 | `` * notice fires on a READ and on an `isset()` alike -- both go through php's`` |
|     - |  8826 | ` * property handler -- and not on a write, where the readonly refusal comes` |
|     - |  8827 | ` * first and never consults this reader.` |
|     - |  8828 | ` */` |
|    86 |  8829 | `static xmlEntityPtr DomThisEntity(ph7_context *pCtx)` |
|     1 |  8830 | `{` |
|    87 |  8831 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    87 |  8832 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     - |  8833 | `	/* Only a real entity DECLARATION is xmlEntity-shaped. An ELEMENT` |
|     - |  8834 | `	 * declaration wears this class in php and is an xmlElement underneath,` |
|     - |  8835 | `	 * whose fields past the node header are another struct's. */` |
|    87 |  8836 | `	if( pNode == 0 \|\| pNode->type != XML_ENTITY_DECL ){` |
|   ! 0 |  8837 | `		return 0;` |
|     - |  8838 | `	}` |
|    87 |  8839 | `	return (xmlEntityPtr)pNode;` |
|    44 |  8840 | `}` |
|    86 |  8841 | `static int DomEntityProp(ph7_context *pCtx,const char *zName)` |
|     1 |  8842 | `{` |
|    87 |  8843 | `	xmlEntityPtr pEnt = DomThisEntity(pCtx);` |
|    87 |  8844 | `	int bUnparsed = pEnt && pEnt->etype == XML_EXTERNAL_GENERAL_UNPARSED_ENTITY;` |
|    87 |  8845 | `	if( DomNameIs(zName,"publicId") ){` |
|     9 |  8846 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->ExternalID : 0);` |
|     9 |  8847 | `		return 1;` |
|     - |  8848 | `	}` |
|    79 |  8849 | `	if( DomNameIs(zName,"systemId") ){` |
|     9 |  8850 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->SystemID : 0);` |
|     9 |  8851 | `		return 1;` |
|     - |  8852 | `	}` |
|    71 |  8853 | `	if( DomNameIs(zName,"notationName") ){` |
|     - |  8854 | ``		/* libxml keeps an unparsed entity's notation name in `content`. */`` |
|    11 |  8855 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->content : 0);` |
|    11 |  8856 | `		return 1;` |
|     - |  8857 | `	}` |
|    61 |  8858 | `	if( DomNameIs(zName,"actualEncoding") ){` |
|     3 |  8859 | `		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|     - |  8860 | `			"Property DOMEntity::$actualEncoding is deprecated");` |
|     3 |  8861 | `		ph7_result_null(pCtx);` |
|     3 |  8862 | `		return 1;` |
|     - |  8863 | `	}` |
|    59 |  8864 | `	if( DomNameIs(zName,"encoding") ){` |
|     5 |  8865 | `		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|     - |  8866 | `			"Property DOMEntity::$encoding is deprecated");` |
|     5 |  8867 | `		ph7_result_null(pCtx);` |
|     5 |  8868 | `		return 1;` |
|     - |  8869 | `	}` |
|    55 |  8870 | `	if( DomNameIs(zName,"version") ){` |
|     - |  8871 | `		/* php has never filled any of the three in: the handler answers NULL` |
|     - |  8872 | `		 * and does nothing else. */` |
|     3 |  8873 | `		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|     - |  8874 | `			"Property DOMEntity::$version is deprecated");` |
|     3 |  8875 | `		ph7_result_null(pCtx);` |
|     3 |  8876 | `		return 1;` |
|     - |  8877 | `	}` |
|    53 |  8878 | `	return DomNodeProp(pCtx,zName);` |
|    44 |  8879 | `}` |
|     - |  8880 | `/*` |
|     - |  8881 | `` * php declares `schemaTypeInfo` on both DOMElement and DOMAttr and has never`` |
|     - |  8882 | ` * filled it in: ext/dom answers NULL from a handler that does nothing else.` |
|     - |  8883 | ` * It is a declared property all the same, so a read is NOT the undefined-property` |
|     - |  8884 | ` * warning -- which is the whole difference this row buys.` |
|     - |  8885 | ` */` |
|     2 |  8886 | `static int DomSchemaTypeInfo(ph7_context *pCtx)` |
|     1 |  8887 | `{` |
|     3 |  8888 | `	ph7_result_null(pCtx);` |
|     3 |  8889 | `	return 1;` |
|     1 |  8890 | `}` |
|     - |  8891 | `/* DOMElement adds tagName, and the two attribute-backed names php exposes as` |
|     - |  8892 | `` * properties: `className` IS the class attribute and `id` IS the id one, both`` |
|     - |  8893 | ` * answering "" when the attribute is absent. */` |
|  2036 |  8894 | `static int DomElemProp(ph7_context *pCtx,const char *zName)` |
|     5 |  8895 | `{` |
|     - |  8896 | `	phl_domnode *pNd;` |
|  2041 |  8897 | `	int bClass = DomNameIs(zName,"className");` |
|  2041 |  8898 | `	if( DomNameIs(zName,"tagName") ){` |
|    49 |  8899 | `		pNd = DomThisNode(pCtx);` |
|    49 |  8900 | `		DomNodeName(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|    49 |  8901 | `		return 1;` |
|     - |  8902 | `	}` |
|  1993 |  8903 | `	if( bClass \|\| DomNameIs(zName,"id") ){` |
|     - |  8904 | `		xmlChar *zVal;` |
|     9 |  8905 | `		pNd = DomThisNode(pCtx);` |
|     9 |  8906 | `		zVal = pNd ? xmlGetNoNsProp((xmlNodePtr)pNd->pNode,` |
|     8 |  8907 | `			(const xmlChar *)(bClass ? "class" : "id")) : 0;` |
|     9 |  8908 | `		ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|     9 |  8909 | `		if( zVal ){` |
|     5 |  8910 | `			xmlFree(zVal);` |
|     2 |  8911 | `		}` |
|     9 |  8912 | `		return 1;` |
|     - |  8913 | `	}` |
|  1985 |  8914 | `	if( DomNameIs(zName,"schemaTypeInfo") ){` |
|   ! 0 |  8915 | `		return DomSchemaTypeInfo(pCtx);` |
|     - |  8916 | `	}` |
|  1985 |  8917 | `	if( DomParentNodeProp(pCtx,zName) \|\| DomChildNodeProp(pCtx,zName) ){` |
|    43 |  8918 | `		return 1;` |
|     - |  8919 | `	}` |
|  1943 |  8920 | `	return DomNodeProp(pCtx,zName);` |
|  1023 |  8921 | `}` |
|     - |  8922 | `/* DOMDocumentFragment: the three DOMParentNode properties over DOMNode's. */` |
|    80 |  8923 | `static int DomFragProp(ph7_context *pCtx,const char *zName)` |
|     1 |  8924 | `{` |
|    81 |  8925 | `	if( DomParentNodeProp(pCtx,zName) ){` |
|     9 |  8926 | `		return 1;` |
|     - |  8927 | `	}` |
|    73 |  8928 | `	return DomNodeProp(pCtx,zName);` |
|    41 |  8929 | `}` |
|     - |  8930 | `/* DOMAttr adds name/value/ownerElement/specified/schemaTypeInfo. */` |
|   484 |  8931 | `static int DomAttrProp(ph7_context *pCtx,const char *zName)` |
|     3 |  8932 | `{` |
|   487 |  8933 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   487 |  8934 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   487 |  8935 | `	if( DomNameIs(zName,"name") ){` |
|     9 |  8936 | `		DomNodeName(pCtx,pNode);` |
|     9 |  8937 | `		return 1;` |
|     - |  8938 | `	}` |
|   479 |  8939 | `	if( DomNameIs(zName,"value") ){` |
|    54 |  8940 | `		DomNodeValue(pCtx,pNode);` |
|    54 |  8941 | `		return 1;` |
|     - |  8942 | `	}` |
|   427 |  8943 | `	if( DomNameIs(zName,"ownerElement") ){` |
|    52 |  8944 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);` |
|    52 |  8945 | `		return 1;` |
|     - |  8946 | `	}` |
|     - |  8947 | ``	/* php's `specified` is a DOM level-1 remnant: ext/dom answers TRUE for every`` |
|     - |  8948 | `	 * attribute a program can reach, including one it just created. */` |
|   376 |  8949 | `	if( DomNameIs(zName,"specified") ){` |
|     3 |  8950 | `		ph7_result_bool(pCtx,1);` |
|     3 |  8951 | `		return 1;` |
|     - |  8952 | `	}` |
|   374 |  8953 | `	if( DomNameIs(zName,"schemaTypeInfo") ){` |
|     3 |  8954 | `		return DomSchemaTypeInfo(pCtx);` |
|     - |  8955 | `	}` |
|   372 |  8956 | `	return DomNodeProp(pCtx,zName);` |
|   245 |  8957 | `}` |
|     - |  8958 | `/* DOMCharacterData adds data/length; DOMText adds wholeText on top of those. */` |
|   368 |  8959 | `static int DomCharDataProp(ph7_context *pCtx,const char *zName)` |
|     2 |  8960 | `{` |
|   370 |  8961 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   370 |  8962 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   370 |  8963 | `	if( DomNameIs(zName,"data") ){` |
|   116 |  8964 | `		DomDataValue(pCtx,pNode);` |
|   116 |  8965 | `		return 1;` |
|     - |  8966 | `	}` |
|   256 |  8967 | `	if( DomNameIs(zName,"length") ){` |
|     - |  8968 | `		/* php counts UTF-8 CHARACTERS here (xmlUTF8Strlen over the node's own` |
|     - |  8969 | `		 * content), which is the same unit every offset on this class uses. */` |
|    13 |  8970 | `		ph7_result_int(pCtx,DomCharLength(pNode));` |
|    13 |  8971 | `		return 1;` |
|     - |  8972 | `	}` |
|   244 |  8973 | `	return 0;` |
|   186 |  8974 | `}` |
|   368 |  8975 | `static int DomCharProp(ph7_context *pCtx,const char *zName)` |
|     2 |  8976 | `{` |
|   370 |  8977 | `	if( DomCharDataProp(pCtx,zName) \|\| DomChildNodeProp(pCtx,zName) ){` |
|   134 |  8978 | `		return 1;` |
|     - |  8979 | `	}` |
|   238 |  8980 | `	return DomNodeProp(pCtx,zName);` |
|   186 |  8981 | `}` |
|     - |  8982 | `/*` |
|     - |  8983 | ` * DOMText::wholeText is the whole RUN, not the node: php walks back to the` |
|     - |  8984 | ` * first adjacent text-or-CDATA sibling and forward to the last, concatenating` |
|     - |  8985 | ` * all of them, which is what makes it the answer to "what does this element` |
|     - |  8986 | ` * actually say" after an edit has left the text in pieces. Answering the node's` |
|     - |  8987 | ` * own data (what PHL did) is the same string only when the run is one node` |
|     - |  8988 | ` * long, and silently short otherwise.` |
|     - |  8989 | ` */` |
|    54 |  8990 | `static int DomIsTextRun(xmlNodePtr pNode)` |
|     1 |  8991 | `{` |
|    48 |  8992 | `	return pNode != 0` |
|    74 |  8993 | `		&& (pNode->type == XML_TEXT_NODE \|\| pNode->type == XML_CDATA_SECTION_NODE);` |
|     1 |  8994 | `}` |
|   314 |  8995 | `static int DomTextProp(ph7_context *pCtx,const char *zName)` |
|     2 |  8996 | `{` |
|     - |  8997 | `	phl_domnode *pNd;` |
|     - |  8998 | `	xmlNodePtr pNode,pCur;` |
|     - |  8999 | `	SyBlob sOut;` |
|   316 |  9000 | `	if( DomNameIs(zName,"wholeText") ){` |
|    13 |  9001 | `		pNd = DomThisNode(pCtx);` |
|    13 |  9002 | `		pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    13 |  9003 | `		if( !DomIsTextRun(pNode) ){` |
|   ! 0 |  9004 | `			DomNodeValue(pCtx,pNode);` |
|   ! 0 |  9005 | `			return 1;` |
|     - |  9006 | `		}` |
|    15 |  9007 | `		while( DomIsTextRun(pNode->prev) ){` |
|     3 |  9008 | `			pNode = pNode->prev;` |
|     1 |  9009 | `		}` |
|    13 |  9010 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    29 |  9011 | `		for( pCur = pNode ; DomIsTextRun(pCur) ; pCur = pCur->next ){` |
|    17 |  9012 | `			xmlChar *zPart = xmlNodeGetContent(pCur);` |
|    17 |  9013 | `			if( zPart ){` |
|    17 |  9014 | `				SyBlobAppend(&sOut,(const void *)zPart,(sxu32)SyStrlen((const char *)zPart));` |
|    17 |  9015 | `				xmlFree(zPart);` |
|     8 |  9016 | `			}` |
|     9 |  9017 | `		}` |
|    13 |  9018 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    13 |  9019 | `		SyBlobRelease(&sOut);` |
|    13 |  9020 | `		return 1;` |
|     - |  9021 | `	}` |
|   304 |  9022 | `	return DomCharProp(pCtx,zName);` |
|   159 |  9023 | `}` |
|     - |  9024 | `/*` |
|     - |  9025 | ` * php's typed-property store for the DOM's own string-shaped properties: a` |
|     - |  9026 | ` * scalar coerces, null is accepted only where the declared type is nullable` |
|     - |  9027 | ` * (and means the empty string), and an array or an object is a TypeError` |
|     - |  9028 | ` * naming the class that DECLARES the property rather than the one the write` |
|     - |  9029 | ` * went through. The value is coerced through a COPY -- ph7_value_to_string()` |
|     - |  9030 | ` * converts the object it is handed, and that object is the caller's own` |
|     - |  9031 | `` * `$v` in `$node->nodeValue = $v`.`` |
|     - |  9032 | ` */` |
|   224 |  9033 | `static int DomWriteText(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|     - |  9034 | `	const char *zType,ph7_value *pVal,SyBlob *pOut,int *pRc)` |
|     2 |  9035 | `{` |
|   226 |  9036 | `	int bNullable = zType[0] == '?';` |
|     - |  9037 | `	ph7_value sTmp;` |
|   224 |  9038 | `	if( pVal == 0` |
|   224 |  9039 | `	 \|\| (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0` |
|   213 |  9040 | `	 \|\| ((pVal->iFlags & MEMOBJ_NULL) != 0 && !bNullable) ){` |
|     - |  9041 | `		char zBuf[128];` |
|    33 |  9042 | `		const char *zGiven = "null";` |
|    33 |  9043 | `		if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|    11 |  9044 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|    11 |  9045 | `			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sName);` |
|    11 |  9046 | `			zGiven = zBuf;` |
|    28 |  9047 | `		}else if( pVal ){` |
|    23 |  9048 | `			zGiven = ph7_type_name(pVal);` |
|    11 |  9049 | `		}` |
|    49 |  9050 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|    16 |  9051 | `			"Cannot assign %s to property %s::$%s of type %s",zGiven,zOwner,zProp,zType);` |
|    33 |  9052 | `		return 0;` |
|     - |  9053 | `	}` |
|   194 |  9054 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|   194 |  9055 | `	if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|   182 |  9056 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|   182 |  9057 | `		PH7_MemObjLoad(pVal,&sTmp);` |
|   182 |  9058 | `		PH7_MemObjToString(&sTmp);` |
|   182 |  9059 | `		SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|   182 |  9060 | `		PH7_MemObjRelease(&sTmp);` |
|    90 |  9061 | `	}` |
|   194 |  9062 | `	SyBlobNullAppend(pOut);` |
|   194 |  9063 | `	return 1;` |
|   114 |  9064 | `}` |
|     - |  9065 | `/*` |
|     - |  9066 | `` * The same screen for a `bool` property. php's weak mode takes an int, a float`` |
|     - |  9067 | `` * or a string and answers its truthiness (`"0"` and `""` are false), and refuses`` |
|     - |  9068 | `` * null, an array and an object -- the one difference from the `?string` block`` |
|     - |  9069 | `` * above being that a bool property is NOT nullable, so `= null` is the TypeError`` |
|     - |  9070 | ` * rather than the empty write. Answers 1 when the caller may go on and use` |
|     - |  9071 | ` * ph7_value_to_bool(), 0 when the refusal has been raised into *pRc.` |
|     - |  9072 | ` */` |
|    34 |  9073 | `static int DomWriteBool(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|     - |  9074 | `	ph7_value *pVal,int *pRc)` |
|     1 |  9075 | `{` |
|    35 |  9076 | `	if( pVal == 0 \|\| (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL)) != 0 ){` |
|     - |  9077 | `		char zBuf[128];` |
|     9 |  9078 | `		const char *zGiven = "null";` |
|     9 |  9079 | `		if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 |  9080 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|   ! 0 |  9081 | `			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sName);` |
|   ! 0 |  9082 | `			zGiven = zBuf;` |
|     9 |  9083 | `		}else if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|     5 |  9084 | `			zGiven = ph7_type_name(pVal);` |
|     2 |  9085 | `		}` |
|    13 |  9086 | `		*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|     4 |  9087 | `			"Cannot assign %s to property %s::$%s of type bool",zGiven,zOwner,zProp);` |
|     9 |  9088 | `		return 0;` |
|     - |  9089 | `	}` |
|    27 |  9090 | `	return 1;` |
|    18 |  9091 | `}` |
|     - |  9092 | `/* Has this node ever been handed to PHP? The identity cache is the record. */` |
|   156 |  9093 | `static int DomIsWrapped(ph7_vm *pVm,ph7_class_instance *pDoc,xmlNodePtr pNode)` |
|     1 |  9094 | `{` |
|   157 |  9095 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|   157 |  9096 | `	ph7_hashmap_node *pEntry = 0;` |
|     - |  9097 | `	ph7_value sKey;` |
|     - |  9098 | `	int bHit;` |
|   157 |  9099 | `	if( pCache == 0 ){` |
|   ! 0 |  9100 | `		return 0;` |
|     - |  9101 | `	}` |
|   157 |  9102 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|   157 |  9103 | `	bHit = PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry != 0;` |
|   157 |  9104 | `	PH7_MemObjRelease(&sKey);` |
|   157 |  9105 | `	return bHit;` |
|    79 |  9106 | `}` |
|     - |  9107 | `/*` |
|     - |  9108 | ` * php FREES the subtree a content write replaces -- except the nodes a PHP` |
|     - |  9109 | ` * variable still holds a wrapper for, which it unlinks and keeps as roots of` |
|     - |  9110 | ` * their own detached fragments. That is observable: after` |
|     - |  9111 | `` * `$el->textContent = 'flat'`, a variable holding a grandchild still reads its`` |
|     - |  9112 | `` * text and answers NULL for `parentNode`. Parking the subtree whole (which is`` |
|     - |  9113 | ` * what this engine must do -- a wrapper's handle is a raw pointer, so nothing` |
|     - |  9114 | ` * here is ever freed before the document is) left every such parent attached,` |
|     - |  9115 | ` * so the same variable answered its old parent's name. Detach exactly the nodes` |
|     - |  9116 | ` * php would have kept: the OUTERMOST wrapped ones, php's own rule, since it` |
|     - |  9117 | ` * stops recursing at a node it is keeping.` |
|     - |  9118 | ` */` |
|   144 |  9119 | `static void DomPartWrapped(ph7_vm *pVm,ph7_class_instance *pDoc,phl_xmldoc *pShell,xmlNodePtr pNode)` |
|     1 |  9120 | `{` |
|   145 |  9121 | `	xmlNodePtr pChild = pNode ? pNode->children : 0;` |
|   147 |  9122 | `	while( pChild ){` |
|     3 |  9123 | `		xmlNodePtr pNext = pChild->next;` |
|     3 |  9124 | `		if( DomIsWrapped(&(*pVm),pDoc,pChild) ){` |
|   ! 0 |  9125 | `			xmlUnlinkNode(pChild);` |
|   ! 0 |  9126 | `			DomOrphanAdd(pShell,pChild);` |
|   ! 0 |  9127 | `		}else{` |
|     3 |  9128 | `			DomPartWrapped(&(*pVm),pDoc,pShell,pChild);` |
|     - |  9129 | `		}` |
|     3 |  9130 | `		pChild = pNext;` |
|     1 |  9131 | `	}` |
|   145 |  9132 | `}` |
|     - |  9133 | `/*` |
|     - |  9134 | ` * Everything a content write has to do before libxml sees it: the node's` |
|     - |  9135 | ` * children go to the document's ORPHAN set rather than being freed, because a` |
|     - |  9136 | ` * PHP variable may still hold a wrapper for one of them and the wrapper's` |
|     - |  9137 | ` * handle is a raw pointer. (php keeps such a node alive through its own` |
|     - |  9138 | ` * wrapper refcount; this engine parks it, exactly as removeChild does.)` |
|     - |  9139 | ` */` |
|   108 |  9140 | `static void DomDropChildren(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode)` |
|     2 |  9141 | `{` |
|   110 |  9142 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|   110 |  9143 | `	xmlNodePtr pChild = pNode ? pNode->children : 0;` |
|   264 |  9144 | `	while( pChild ){` |
|   155 |  9145 | `		xmlNodePtr pNext = pChild->next;` |
|   155 |  9146 | `		if( !DomIsWrapped(pCtx->pVm,pDoc,pChild) ){` |
|   143 |  9147 | `			DomPartWrapped(pCtx->pVm,pDoc,pShell,pChild);` |
|    71 |  9148 | `		}` |
|   155 |  9149 | `		xmlUnlinkNode(pChild);` |
|   155 |  9150 | `		DomOrphanAdd(pShell,pChild);` |
|   155 |  9151 | `		pChild = pNext;` |
|     1 |  9152 | `	}` |
|   110 |  9153 | `}` |
|     - |  9154 | `/*` |
|     - |  9155 | ` * php's two content writes, which are NOT the same write.` |
|     - |  9156 | ` *` |
|     - |  9157 | `` * `nodeValue` is libxml's xmlNodeSetContent, and on an element or an attribute`` |
|     - |  9158 | `` * that PARSES entity references: `$el->nodeValue = 'a&b'` is libxml's`` |
|     - |  9159 | ` * "unterminated entity reference" and leaves the node EMPTY, while` |
|     - |  9160 | `` * `'a&amp;b'` stores the one character. `textContent` sets one raw text child`` |
|     - |  9161 | ` * instead, so the same two strings store what they say. Every other node kind` |
|     - |  9162 | ` * takes its content literally either way.` |
|     - |  9163 | ` */` |
|    98 |  9164 | `static void DomSetContent(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode,` |
|     - |  9165 | `	const char *zText,int bParseEntities)` |
|     2 |  9166 | `{` |
|   100 |  9167 | `	int bTree = pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE;` |
|     - |  9168 | `	xmlNodePtr pText;` |
|   100 |  9169 | `	DomDropChildren(pCtx,pShell,pNode);` |
|   100 |  9170 | `	if( !bTree \|\| (bParseEntities && zText[0]) ){` |
|     - |  9171 | `		/* The parsing write is the one that can FAIL -- an unterminated entity` |
|     - |  9172 | `		 * reference leaves the node empty and libxml says so. Route that through` |
|     - |  9173 | `		 * the per-VM queue like every other libxml diagnostic here, or it prints` |
|     - |  9174 | ``		 * itself on stderr past error_reporting(), past `@`, and past`` |
|     - |  9175 | `		 * libxml_get_errors(). */` |
|     - |  9176 | `		SyBlob sFn;` |
|    74 |  9177 | `		sxu32 nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|    74 |  9178 | `		xmlNodeSetContent(pNode,(const xmlChar *)zText);` |
|     - |  9179 | `		/* php attributes the warning to the CALLER's scope -- a property write` |
|     - |  9180 | `		 * is not a call, so there is no accessor name to print. */` |
|    74 |  9181 | `		SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|    74 |  9182 | `		PH7_VmActiveFuncName(pCtx->pVm,&sFn);` |
|    74 |  9183 | `		PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,(const char *)SyBlobData(&sFn));` |
|    74 |  9184 | `		SyBlobRelease(&sFn);` |
|    74 |  9185 | `		return;` |
|     - |  9186 | `	}` |
|     - |  9187 | `	/* One raw text child -- and php leaves one even for the EMPTY string, which` |
|     - |  9188 | ``	 * is why `$el->nodeValue = ''` serializes as <r></r> rather than <r/>.`` |
|     - |  9189 | `	 * (The entity-parsing write is the exception: a string libxml refuses, like` |
|     - |  9190 | ``	 * `'a&b'`, leaves the element with no children at all.) */`` |
|    27 |  9191 | `	pText = xmlNewDocText(pNode->doc,(const xmlChar *)zText);` |
|    27 |  9192 | `	if( pText ){` |
|    27 |  9193 | `		DomLinkLast(pNode,pText);` |
|    13 |  9194 | `	}` |
|    51 |  9195 | `}` |
|     - |  9196 | `/*` |
|     - |  9197 | ` * The same refusal, raised from a property WRITE.` |
|     - |  9198 | ` *` |
|     - |  9199 | ` * A write is not a call, so php has no accessor name to print in front of the` |
|     - |  9200 | `` * warning and attributes it to the CALLER's scope instead (`f(): Namespace`` |
|     - |  9201 | `` * Error`, `Unknown: ...` at file scope) -- the shape DomSetContent already uses`` |
|     - |  9202 | ` * for the libxml diagnostic a content write can produce. Nothing is answered` |
|     - |  9203 | ` * either way: a property write has no return value.` |
|     - |  9204 | ` */` |
|    10 |  9205 | `static int DomThrowWrite(ph7_context *pCtx,int iCode)` |
|     1 |  9206 | `{` |
|    11 |  9207 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|     - |  9208 | `	SyBlob sFn;` |
|     - |  9209 | `	SyString sName;` |
|     - |  9210 | `	int rc;` |
|    11 |  9211 | `	if( pDoc == 0 \|\| PH7_NativeAttrTruthy(pDoc,"strictErrorChecking") ){` |
|     7 |  9212 | `		return DomThrowAlways(pCtx,iCode);` |
|     - |  9213 | `	}` |
|     5 |  9214 | `	SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|     5 |  9215 | `	PH7_VmActiveFuncName(pCtx->pVm,&sFn);` |
|     5 |  9216 | `	SyStringInitFromBuf(&sName,SyBlobData(&sFn),SyBlobLength(&sFn));` |
|     5 |  9217 | `	rc = PH7_VmThrowError(pCtx->pVm,&sName,PH7_CTX_WARNING,DomErrText(iCode));` |
|     5 |  9218 | `	SyBlobRelease(&sFn);` |
|     5 |  9219 | `	return rc;` |
|     6 |  9220 | `}` |
|     - |  9221 | `/*` |
|     - |  9222 | ` * The node kinds a value write REACHES.` |
|     - |  9223 | ` *` |
|     - |  9224 | ` * php's two writers do not accept the same list, and everything off it is a` |
|     - |  9225 | ` * silent NO-OP -- the write is accepted and the node is left exactly as it was.` |
|     - |  9226 | ` * PHL had one exclusion, the document, and wrote to everything else, which cost` |
|     - |  9227 | ` * three answers and one crash:` |
|     - |  9228 | ` *` |
|     - |  9229 | ` *   - an ENTITY REFERENCE's children are the entity DECLARATION's, shared by` |
|     - |  9230 | ` *     every reference to it and owned by the DTD; dropping them freed nodes the` |
|     - |  9231 | `` *     document frees again at teardown -- `$ref->nodeValue = 'x'` aborted the`` |
|     - |  9232 | ` *     process on a double free;` |
|     - |  9233 | ` *   - a DOCTYPE's children are the declarations of the internal SUBSET, so` |
|     - |  9234 | ``  *     `$doc->doctype->nodeValue = 'x'` silently emptied `<!DOCTYPE r [ ... ]>` `` |
|     - |  9235 | ` *     of every entity, element and attribute declaration in it;` |
|     - |  9236 | `` *   - a FRAGMENT is emptied by `textContent` and left alone by `nodeValue`,`` |
|     - |  9237 | ` *     which is the one kind where the two writers really do disagree.` |
|     - |  9238 | ` */` |
|    74 |  9239 | `static int DomValueWritable(xmlNodePtr pNode,int bValue)` |
|     1 |  9240 | `{` |
|    75 |  9241 | `	if( pNode == 0 ){` |
|   ! 0 |  9242 | `		return 0;` |
|     - |  9243 | `	}` |
|    75 |  9244 | `	switch( pNode->type ){` |
|    29 |  9245 | `	case XML_ELEMENT_NODE:` |
|     - |  9246 | `	case XML_ATTRIBUTE_NODE:` |
|     - |  9247 | `	case XML_TEXT_NODE:` |
|     - |  9248 | `	case XML_COMMENT_NODE:` |
|     - |  9249 | `	case XML_CDATA_SECTION_NODE:` |
|     - |  9250 | `	case XML_PI_NODE:` |
|    59 |  9251 | `		return 1;` |
|     2 |  9252 | `	case XML_DOCUMENT_FRAG_NODE:` |
|     5 |  9253 | `		return !bValue;` |
|     6 |  9254 | `	default:` |
|    13 |  9255 | `		return 0;` |
|     - |  9256 | `	}` |
|    38 |  9257 | `}` |
|     - |  9258 | `/* DOMNode's three writable properties. */` |
|   170 |  9259 | `static int DomSetNodeProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     1 |  9260 | `{` |
|   171 |  9261 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   171 |  9262 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   171 |  9263 | `	int bValue = DomNameIs(zName,"nodeValue");` |
|     - |  9264 | `	SyBlob sVal;` |
|   171 |  9265 | `	if( bValue \|\| DomNameIs(zName,"textContent") ){` |
|     - |  9266 | `		/* php's one REFUSAL among the no-ops, and it answers before the type` |
|     - |  9267 | ``		 * check does: `$ref->textContent = []` is the readonly Error where`` |
|     - |  9268 | ``		 * `$ref->nodeValue = []` is the TypeError. Left unwritten here, the`` |
|     - |  9269 | `		 * accessor macro falls through to it. */` |
|    87 |  9270 | `		if( !bValue && pNode && pNode->type == XML_ENTITY_REF_NODE ){` |
|     5 |  9271 | `			return DOM_SET_UNKNOWN;` |
|     - |  9272 | `		}` |
|   123 |  9273 | `		if( DomWriteText(pCtx,"DOMNode",bValue ? "nodeValue" : "textContent",` |
|    83 |  9274 | `			bValue ? "?string" : "string",pVal,&sVal,pRc) == 0 ){` |
|     9 |  9275 | `			return DOM_SET_DONE;` |
|     - |  9276 | `		}` |
|    75 |  9277 | `		if( DomValueWritable(pNode,bValue) ){` |
|    61 |  9278 | `			DomSetContent(pCtx,pNd->pShell,pNode,(const char *)SyBlobData(&sVal),bValue);` |
|    30 |  9279 | `		}` |
|    75 |  9280 | `		SyBlobRelease(&sVal);` |
|    75 |  9281 | `		return DOM_SET_DONE;` |
|     - |  9282 | `	}` |
|    85 |  9283 | `	if( DomNameIs(zName,"prefix") ){` |
|     - |  9284 | `		xmlNsPtr pNs;` |
|     - |  9285 | `		xmlNodePtr pDecl;` |
|    29 |  9286 | `		if( DomWriteText(pCtx,"DOMNode","prefix","string",pVal,&sVal,pRc) == 0 ){` |
|     5 |  9287 | `			return DOM_SET_DONE;` |
|     - |  9288 | `		}` |
|     - |  9289 | `		/* Only a node that HAS a namespace can be re-prefixed; php ignores the` |
|     - |  9290 | `		 * write for anything else, including an element in no namespace. */` |
|    25 |  9291 | `		if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->href ){` |
|    21 |  9292 | `			const char *zPrefix = (const char *)SyBlobData(&sVal);` |
|     - |  9293 | `			/* An attribute's declaration goes on its ELEMENT. */` |
|    21 |  9294 | `			pDecl = pNode->type == XML_ATTRIBUTE_NODE ? pNode->parent : pNode;` |
|    20 |  9295 | `			if( DomNameIs(zPrefix,"xml")` |
|    15 |  9296 | `			 && !DomNameIs((const char *)pNode->ns->href,` |
|     - |  9297 | `				"http://www.w3.org/XML/1998/namespace") ){` |
|     - |  9298 | ``				/* php's reserved-prefix refusal: `xml` may only name ITS namespace. */`` |
|     9 |  9299 | `				SyBlobRelease(&sVal);` |
|     9 |  9300 | `				*pRc = DomThrowWrite(pCtx,DOM_ERR_NAMESPACE);` |
|     9 |  9301 | `				return DOM_SET_DONE;` |
|     - |  9302 | `			}` |
|     - |  9303 | `			/* php looks only at the declarations THIS node carries -- an` |
|     - |  9304 | `			 * ancestor's is not reused, which is why re-prefixing a child grows` |
|     - |  9305 | ``			 * a second `xmlns:q` beside the one its parent already has. */`` |
|    29 |  9306 | `			for( pNs = pDecl ? pDecl->nsDef : 0 ; pNs ; pNs = pNs->next ){` |
|    19 |  9307 | `				const char *zHave = pNs->prefix ? (const char *)pNs->prefix : "";` |
|    18 |  9308 | `				if( DomNameIs(zHave,zPrefix) && pNs->href` |
|     5 |  9309 | `				 && xmlStrEqual(pNs->href,pNode->ns->href) ){` |
|     3 |  9310 | `					break;` |
|     - |  9311 | `				}` |
|     9 |  9312 | `			}` |
|    13 |  9313 | `			if( pNs == 0 ){` |
|     - |  9314 | `				/* None binds this prefix to the node's own URI: php declares one,` |
|     - |  9315 | ``				 * which is how `$el->prefix = ''` grows an `xmlns="..."` on the`` |
|     - |  9316 | `				 * element itself -- and how a prefix already bound HERE to another` |
|     - |  9317 | `				 * URI becomes libxml's refusal and php's Namespace Error. */` |
|    16 |  9318 | `				pNs = pDecl ? xmlNewNs(pDecl,pNode->ns->href,` |
|    10 |  9319 | `					zPrefix[0] ? (const xmlChar *)zPrefix : 0) : 0;` |
|     5 |  9320 | `			}` |
|    13 |  9321 | `			if( pNs == 0 ){` |
|     3 |  9322 | `				SyBlobRelease(&sVal);` |
|     3 |  9323 | `				*pRc = DomThrowWrite(pCtx,DOM_ERR_NAMESPACE);` |
|     3 |  9324 | `				return DOM_SET_DONE;` |
|     - |  9325 | `			}` |
|    11 |  9326 | `			xmlSetNs(pNode,pNs);` |
|     5 |  9327 | `		}` |
|    15 |  9328 | `		SyBlobRelease(&sVal);` |
|    15 |  9329 | `		return DOM_SET_DONE;` |
|     - |  9330 | `	}` |
|    57 |  9331 | `	return DOM_SET_UNKNOWN;` |
|    86 |  9332 | `}` |
|     - |  9333 | `/* DOMElement adds className and id, both of them ATTRIBUTES under the name. */` |
|   102 |  9334 | `static int DomSetElemProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     1 |  9335 | `{` |
|   103 |  9336 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   103 |  9337 | `	int bClass = DomNameIs(zName,"className");` |
|     - |  9338 | `	SyBlob sVal;` |
|   103 |  9339 | `	if( bClass \|\| DomNameIs(zName,"id") ){` |
|     9 |  9340 | `		if( DomWriteText(pCtx,"DOMElement",bClass ? "className" : "id","string",` |
|     7 |  9341 | `			pVal,&sVal,pRc) == 0 ){` |
|     3 |  9342 | `			return DOM_SET_DONE;` |
|     - |  9343 | `		}` |
|     5 |  9344 | `		if( pNd ){` |
|     7 |  9345 | `			xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)(bClass ? "class" : "id"),` |
|     4 |  9346 | `				(const xmlChar *)SyBlobData(&sVal));` |
|     2 |  9347 | `		}` |
|     5 |  9348 | `		SyBlobRelease(&sVal);` |
|     5 |  9349 | `		return DOM_SET_DONE;` |
|     - |  9350 | `	}` |
|    97 |  9351 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|    52 |  9352 | `}` |
|     - |  9353 | `/* DOMAttr::value and DOMCharacterData::data are the node's own content. The` |
|     - |  9354 | `` * attribute's parses entity references, as its `nodeValue` does -- only`` |
|     - |  9355 | `` * `textContent` takes an attribute's bytes literally; character data has no`` |
|     - |  9356 | ` * parsing write at all, whichever name it is written under. */` |
|    80 |  9357 | `static int DomSetContentProp(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|     - |  9358 | `	const char *zName,ph7_value *pVal,int bParseEntities,int *pRc)` |
|     2 |  9359 | `{` |
|    82 |  9360 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     - |  9361 | `	SyBlob sVal;` |
|    82 |  9362 | `	if( !DomNameIs(zName,zProp) ){` |
|    39 |  9363 | `		return DOM_SET_UNKNOWN;` |
|     - |  9364 | `	}` |
|    44 |  9365 | `	if( DomWriteText(pCtx,zOwner,zProp,"string",pVal,&sVal,pRc) == 0 ){` |
|     5 |  9366 | `		return DOM_SET_DONE;` |
|     - |  9367 | `	}` |
|    40 |  9368 | `	if( pNd ){` |
|    59 |  9369 | `		DomSetContent(pCtx,pNd->pShell,(xmlNodePtr)pNd->pNode,` |
|    38 |  9370 | `			(const char *)SyBlobData(&sVal),bParseEntities);` |
|    19 |  9371 | `	}` |
|    40 |  9372 | `	SyBlobRelease(&sVal);` |
|    40 |  9373 | `	return DOM_SET_DONE;` |
|    42 |  9374 | `}` |
|    40 |  9375 | `static int DomSetAttrProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     2 |  9376 | `{` |
|    42 |  9377 | `	int rc = DomSetContentProp(pCtx,"DOMAttr","value",zName,pVal,TRUE,pRc);` |
|    42 |  9378 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|     2 |  9379 | `}` |
|    30 |  9380 | `static int DomSetCharProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     1 |  9381 | `{` |
|    31 |  9382 | `	int rc = DomSetContentProp(pCtx,"DOMCharacterData","data",zName,pVal,FALSE,pRc);` |
|    31 |  9383 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|     1 |  9384 | `}` |
|    10 |  9385 | `static int DomSetPiProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     1 |  9386 | `{` |
|    11 |  9387 | `	int rc = DomSetContentProp(pCtx,"DOMProcessingInstruction","data",zName,pVal,FALSE,pRc);` |
|    11 |  9388 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|     1 |  9389 | `}` |
|     - |  9390 | `/*` |
|     - |  9391 | ` * The DOCUMENT's writable state: the three declaration slots php lets a program` |
|     - |  9392 | `` * change, plus `documentURI`.`` |
|     - |  9393 | ` *` |
|     - |  9394 | `` * php declares them `?string`/`bool`, so a null goes through the string three as`` |
|     - |  9395 | `` * the EMPTY string (`$d->version = null` writes `<?xml version=""?>`) and is a`` |
|     - |  9396 | ` * TypeError on the bool pair; an array or an object is a TypeError on all of` |
|     - |  9397 | ` * them.  The read-only four are refused HERE rather than through the reader,` |
|     - |  9398 | ` * because two of them are deprecated and php's readonly Error comes without the` |
|     - |  9399 | ` * deprecation notice a read would have raised.` |
|     - |  9400 | ` */` |
|   116 |  9401 | `static int DomSetDocProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     1 |  9402 | `{` |
|   117 |  9403 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   117 |  9404 | `	xmlDocPtr pDoc = pNd ? (xmlDocPtr)pNd->pNode : 0;` |
|   117 |  9405 | `	int bVersion = DomNameIs(zName,"version") \|\| DomNameIs(zName,"xmlVersion");` |
|   117 |  9406 | `	int bUri = DomNameIs(zName,"documentURI");` |
|     - |  9407 | `	SyBlob sVal;` |
|   116 |  9408 | `	if( DomNameIs(zName,"actualEncoding") \|\| DomNameIs(zName,"config")` |
|   114 |  9409 | `	 \|\| DomNameIs(zName,"xmlEncoding") ){` |
|     7 |  9410 | `		*pRc = DomRefuseWrite(pCtx,zName,1);` |
|     7 |  9411 | `		return DOM_SET_DONE;` |
|     - |  9412 | `	}` |
|   111 |  9413 | `	if( bVersion \|\| bUri \|\| DomNameIs(zName,"encoding") ){` |
|     - |  9414 | `		const char *zNew;` |
|    67 |  9415 | `		if( DomWriteText(pCtx,"DOMDocument",zName,"?string",pVal,&sVal,pRc) == 0 ){` |
|    15 |  9416 | `			return DOM_SET_DONE;` |
|     - |  9417 | `		}` |
|    53 |  9418 | `		zNew = (const char *)SyBlobData(&sVal);` |
|    53 |  9419 | `		if( pDoc == 0 ){` |
|   ! 0 |  9420 | `			SyBlobRelease(&sVal);` |
|   ! 0 |  9421 | `			return DOM_SET_DONE;` |
|     - |  9422 | `		}` |
|    53 |  9423 | `		if( bVersion ){` |
|    23 |  9424 | `			if( pDoc->version ){` |
|    23 |  9425 | `				xmlFree((xmlChar *)pDoc->version);` |
|    11 |  9426 | `			}` |
|    23 |  9427 | `			pDoc->version = xmlStrdup((const xmlChar *)zNew);` |
|    42 |  9428 | `		}else if( bUri ){` |
|    15 |  9429 | `			if( pDoc->URL ){` |
|    15 |  9430 | `				xmlFree((xmlChar *)pDoc->URL);` |
|     7 |  9431 | `			}` |
|    15 |  9432 | `			pDoc->URL = xmlStrdup((const xmlChar *)zNew);` |
|     8 |  9433 | `		}else{` |
|     - |  9434 | `			/* php asks libxml for a converter and refuses the name outright when` |
|     - |  9435 | ``			 * there is none -- so `$d->encoding = 'x'` (and the empty string a`` |
|     - |  9436 | `			 * null coerces to) is a ValueError BEFORE anything is written,` |
|     - |  9437 | `			 * rather than a document that cannot be serialized later. */` |
|     - |  9438 | `			/* A null is refused before libxml is asked anything, as php does: the` |
|     - |  9439 | `			 * empty string it coerces to is a name a current libxml (2.15) answers` |
|     - |  9440 | `			 * WITH a converter, so asking would let the null through. */` |
|    17 |  9441 | `			xmlCharEncodingHandlerPtr pEnc = ph7_value_is_null(pVal) ? 0` |
|    15 |  9442 | `				: xmlFindCharEncodingHandler(zNew);` |
|    17 |  9443 | `			if( pEnc == 0 ){` |
|     8 |  9444 | `				SyBlobRelease(&sVal);` |
|     8 |  9445 | `				*pRc = PH7_VmThrowException(pCtx,"ValueError","Invalid document encoding");` |
|     8 |  9446 | `				return DOM_SET_DONE;` |
|     - |  9447 | `			}` |
|    10 |  9448 | `			xmlCharEncCloseFunc(pEnc);` |
|    10 |  9449 | `			if( pDoc->encoding ){` |
|   ! 0 |  9450 | `				xmlFree((xmlChar *)pDoc->encoding);` |
|   ! 0 |  9451 | `			}` |
|    10 |  9452 | `			pDoc->encoding = xmlStrdup((const xmlChar *)zNew);` |
|     - |  9453 | `		}` |
|    46 |  9454 | `		SyBlobRelease(&sVal);` |
|    46 |  9455 | `		return DOM_SET_DONE;` |
|     - |  9456 | `	}` |
|    45 |  9457 | `	if( DomNameIs(zName,"standalone") \|\| DomNameIs(zName,"xmlStandalone") ){` |
|    35 |  9458 | `		if( DomWriteBool(pCtx,"DOMDocument",zName,pVal,pRc) == 0 ){` |
|     9 |  9459 | `			return DOM_SET_DONE;` |
|     - |  9460 | `		}` |
|    27 |  9461 | `		if( pDoc ){` |
|     - |  9462 | `			/* Either way it becomes a DECLARED answer: writing false is` |
|     - |  9463 | ``			 * `standalone="no"` in the output, not the absent attribute. */`` |
|    27 |  9464 | `			pDoc->standalone = ph7_value_to_bool(pVal) ? 1 : 0;` |
|    13 |  9465 | `		}` |
|    27 |  9466 | `		return DOM_SET_DONE;` |
|     - |  9467 | `	}` |
|    11 |  9468 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|    59 |  9469 | `}` |
|     - |  9470 | ``/* The two collections have nothing writable of their own; `length` is read-only. */`` |
|     6 |  9471 | `static int DomSetNothing(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     1 |  9472 | `{` |
|     3 |  9473 | `	SXUNUSED(pCtx); SXUNUSED(zName); SXUNUSED(pVal); SXUNUSED(pRc);` |
|     7 |  9474 | `	return DOM_SET_UNKNOWN;` |
|     1 |  9475 | `}` |
|     - |  9476 | `/* DOMProcessingInstruction adds target (its name) and data (its content) --` |
|     - |  9477 | ` * php declares it under DOMNode, not DOMCharacterData, so the character-data` |
|     - |  9478 | ` * methods are deliberately absent from it. */` |
|    80 |  9479 | `static int DomPiProp(ph7_context *pCtx,const char *zName)` |
|     1 |  9480 | `{` |
|    81 |  9481 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    81 |  9482 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    81 |  9483 | `	if( DomNameIs(zName,"target") ){` |
|    15 |  9484 | `		ph7_result_string(pCtx,(pNode && pNode->name) ? (const char *)pNode->name : "",-1);` |
|    15 |  9485 | `		return 1;` |
|     - |  9486 | `	}` |
|    67 |  9487 | `	if( DomNameIs(zName,"data") ){` |
|    13 |  9488 | `		DomDataValue(pCtx,pNode);` |
|    13 |  9489 | `		return 1;` |
|     - |  9490 | `	}` |
|    55 |  9491 | `	return DomNodeProp(pCtx,zName);` |
|    41 |  9492 | `}` |
|     - |  9493 | `/*` |
|     - |  9494 | ` * DOMNameSpaceNode's ten properties. It is not a DOMNode -- php gives it its` |
|     - |  9495 | ` * own class with no parent -- so it shares none of the readers above: what it` |
|     - |  9496 | ` * carries is the DECLARATION (an xmlNs) and the element that makes it.` |
|     - |  9497 | ` */` |
|   182 |  9498 | `static int DomNsNodeProp(ph7_context *pCtx,const char *zName)` |
|     1 |  9499 | `{` |
|   183 |  9500 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   183 |  9501 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   183 |  9502 | `	xmlNsPtr pNs = pNd ? (xmlNsPtr)pNd->pNode : 0;` |
|   183 |  9503 | `	ph7_class_instance *pOwner = pThis ? PH7_NativeAttrObj(pThis,DOM_NS_OWNER) : 0;` |
|   183 |  9504 | `	phl_domnode *pOwnerNd = DomResOf(pOwner);` |
|   183 |  9505 | `	const char *zPrefix = (pNs && pNs->prefix) ? (const char *)pNs->prefix : 0;` |
|   183 |  9506 | `	if( pNs == 0 ){` |
|   ! 0 |  9507 | `		return 0;` |
|     - |  9508 | `	}` |
|   183 |  9509 | `	if( DomNameIs(zName,"nodeName") ){` |
|    31 |  9510 | `		if( zPrefix ){` |
|    27 |  9511 | `			ph7_result_string_format(pCtx,"%s:%s",DOM_XMLNS_NAME,zPrefix);` |
|    14 |  9512 | `		}else{` |
|     5 |  9513 | `			ph7_result_string(pCtx,DOM_XMLNS_NAME,-1);` |
|     - |  9514 | `		}` |
|    31 |  9515 | `		return 1;` |
|     - |  9516 | `	}` |
|   153 |  9517 | `	if( DomNameIs(zName,"nodeValue") ){` |
|     - |  9518 | `		/* php builds its wrapper as a fake node whose text CHILD carries the` |
|     - |  9519 | `		 * URI, and an EMPTY href writes no child at all -- so the xmlns=""` |
|     - |  9520 | `		 * UNDECLARATION answers null here while namespaceURI below answers` |
|     - |  9521 | `		 * the empty string off the href itself. Both doors (the attribute` |
|     - |  9522 | `		 * lookups and the namespace:: axis) share this recognizer. */` |
|    35 |  9523 | `		if( pNs->href && pNs->href[0] ){` |
|    33 |  9524 | `			ph7_result_string(pCtx,(const char *)pNs->href,-1);` |
|    17 |  9525 | `		}else{` |
|     3 |  9526 | `			ph7_result_null(pCtx);` |
|     - |  9527 | `		}` |
|    35 |  9528 | `		return 1;` |
|     - |  9529 | `	}` |
|   119 |  9530 | `	if( DomNameIs(zName,"namespaceURI") ){` |
|    21 |  9531 | `		ph7_result_string(pCtx,pNs->href ? (const char *)pNs->href : "",-1);` |
|    21 |  9532 | `		return 1;` |
|     - |  9533 | `	}` |
|    99 |  9534 | `	if( DomNameIs(zName,"nodeType") ){` |
|    17 |  9535 | `		ph7_result_int(pCtx,XML_NAMESPACE_DECL);` |
|    17 |  9536 | `		return 1;` |
|     - |  9537 | `	}` |
|     - |  9538 | ``	/* php answers the EMPTY prefix for the default declaration, and `xmlns` as`` |
|     - |  9539 | `	 * its local name -- the two halves of the name it is spelled with. */` |
|    83 |  9540 | `	if( DomNameIs(zName,"prefix") ){` |
|    21 |  9541 | `		ph7_result_string(pCtx,zPrefix ? zPrefix : "",-1);` |
|    21 |  9542 | `		return 1;` |
|     - |  9543 | `	}` |
|    63 |  9544 | `	if( DomNameIs(zName,"localName") ){` |
|    19 |  9545 | `		ph7_result_string(pCtx,zPrefix ? zPrefix : DOM_XMLNS_NAME,-1);` |
|    19 |  9546 | `		return 1;` |
|     - |  9547 | `	}` |
|    45 |  9548 | `	if( DomNameIs(zName,"isConnected") ){` |
|     5 |  9549 | `		ph7_result_bool(pCtx,pOwnerNd != 0` |
|     2 |  9550 | `			&& DomIsConnected((xmlNodePtr)pOwnerNd->pNode));` |
|     3 |  9551 | `		return 1;` |
|     - |  9552 | `	}` |
|    43 |  9553 | `	if( DomNameIs(zName,"ownerDocument") ){` |
|    11 |  9554 | `		DomResultWrap(pCtx,DomThisDoc(pCtx));` |
|    11 |  9555 | `		return 1;` |
|     - |  9556 | `	}` |
|    33 |  9557 | `	if( DomNameIs(zName,"parentNode") \|\| DomNameIs(zName,"parentElement") ){` |
|    29 |  9558 | `		DomResultWrap(pCtx,pOwner);` |
|    29 |  9559 | `		return 1;` |
|     - |  9560 | `	}` |
|     5 |  9561 | `	return 0;` |
|    92 |  9562 | `}` |
|   183 |  9563 | `DOM_PROP_ACCESSORS(DOMNameSpaceNode,DomNsNodeProp,DomSetNothing)` |
|   268 |  9564 | `DOM_PROP_ACCESSORS(DOMNodeList,DomListProp,DomSetNothing)` |
|    27 |  9565 | `DOM_PROP_ACCESSORS(DOMNamedNodeMap,DomMapProp,DomSetNothing)` |
|    49 |  9566 | `DOM_PROP_ACCESSORS(DOMNode,DomNodeProp,DomSetNodeProp)` |
|  2169 |  9567 | `DOM_PROP_ACCESSORS(DOMDocument,DomDocProp,DomSetDocProp)` |
|  2113 |  9568 | `DOM_PROP_ACCESSORS(DOMElement,DomElemProp,DomSetElemProp)` |
|   523 |  9569 | `DOM_PROP_ACCESSORS(DOMAttr,DomAttrProp,DomSetAttrProp)` |
|    73 |  9570 | `DOM_PROP_ACCESSORS(DOMCharacterData,DomCharProp,DomSetCharProp)` |
|   334 |  9571 | `DOM_PROP_ACCESSORS(DOMText,DomTextProp,DomSetCharProp)` |
|    89 |  9572 | `DOM_PROP_ACCESSORS(DOMProcessingInstruction,DomPiProp,DomSetPiProp)` |
|     - |  9573 | `/* The DTD half's OWN properties are all read-only, so the writer states none of` |
|     - |  9574 | ` * them and each lands on DomRefuseWrite's readonly Error. DOMNode's three still` |
|     - |  9575 | `` * write here -- `nodeValue` and `textContent` are accepted and ignored on a`` |
|     - |  9576 | ` * doctype, which is not the same answer as refusing them. */` |
|   178 |  9577 | `DOM_PROP_ACCESSORS(DOMDocumentType,DomDocTypeProp,DomSetNodeProp)` |
|     - |  9578 | `/*` |
|     - |  9579 | ` * Every property DOMEntity adds is read-only, and the refusal is raised HERE` |
|     - |  9580 | ` * rather than by falling through to the reader: the reader is where the three` |
|     - |  9581 | ` * deprecated names raise their notice, and php's write never reaches it -- the` |
|     - |  9582 | ` * readonly Error comes first and says nothing about deprecation.` |
|     - |  9583 | ` */` |
|    12 |  9584 | `static int DomSetEntityProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     1 |  9585 | `{` |
|    12 |  9586 | `	if( DomNameIs(zName,"publicId") \|\| DomNameIs(zName,"systemId")` |
|     9 |  9587 | `	 \|\| DomNameIs(zName,"notationName") \|\| DomNameIs(zName,"actualEncoding")` |
|     6 |  9588 | `	 \|\| DomNameIs(zName,"encoding") \|\| DomNameIs(zName,"version") ){` |
|    13 |  9589 | `		*pRc = DomRefuseWrite(pCtx,zName,1);` |
|    13 |  9590 | `		return DOM_SET_DONE;` |
|     - |  9591 | `	}` |
|   ! 0 |  9592 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|     7 |  9593 | `}` |
|    99 |  9594 | `DOM_PROP_ACCESSORS(DOMEntity,DomEntityProp,DomSetEntityProp)` |
|     - |  9595 | `/*` |
|     - |  9596 | `` * DOMNotation: the two identifiers a `<!NOTATION ...>` declares.`` |
|     - |  9597 | ` *` |
|     - |  9598 | ` * Both are plain strings that answer "" for the half that is absent -- a` |
|     - |  9599 | `` * SYSTEM-only notation reads "" from `publicId` -- where DOMEntity's same-named`` |
|     - |  9600 | `` * pair are `?string`. The node under them is the stand-in DomNotationNode`` |
|     - |  9601 | ` * built, which shares the entity's layout, so both identifiers are read from` |
|     - |  9602 | ` * the same two fields.` |
|     - |  9603 | ` */` |
|    74 |  9604 | `static int DomNotationProp(ph7_context *pCtx,const char *zName)` |
|     2 |  9605 | `{` |
|    76 |  9606 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    76 |  9607 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    76 |  9608 | `	xmlEntityPtr pNot = (pNode && pNode->type == XML_NOTATION_NODE)` |
|   111 |  9609 | `		? (xmlEntityPtr)pNode : 0;` |
|    76 |  9610 | `	if( DomNameIs(zName,"publicId") ){` |
|     9 |  9611 | `		ph7_result_string(pCtx,(pNot && pNot->ExternalID) ? (const char *)pNot->ExternalID : "",-1);` |
|     9 |  9612 | `		return 1;` |
|     - |  9613 | `	}` |
|    68 |  9614 | `	if( DomNameIs(zName,"systemId") ){` |
|    10 |  9615 | `		ph7_result_string(pCtx,(pNot && pNot->SystemID) ? (const char *)pNot->SystemID : "",-1);` |
|    10 |  9616 | `		return 1;` |
|     - |  9617 | `	}` |
|    59 |  9618 | `	return DomNodeProp(pCtx,zName);` |
|    39 |  9619 | `}` |
|     4 |  9620 | `static int DomSetNotationProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|     1 |  9621 | `{` |
|     5 |  9622 | `	if( DomNameIs(zName,"publicId") \|\| DomNameIs(zName,"systemId") ){` |
|     5 |  9623 | `		*pRc = DomRefuseWrite(pCtx,zName,1);` |
|     5 |  9624 | `		return DOM_SET_DONE;` |
|     - |  9625 | `	}` |
|   ! 0 |  9626 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|     3 |  9627 | `}` |
|    80 |  9628 | `DOM_PROP_ACCESSORS(DOMNotation,DomNotationProp,DomSetNotationProp)` |
|     - |  9629 | `/* The fragment writes what DOMNode writes; only its READ set is wider. */` |
|    85 |  9630 | `DOM_PROP_ACCESSORS(DOMDocumentFragment,DomFragProp,DomSetNodeProp)` |
|     - |  9631 | `/*` |
|     - |  9632 | ` * DOMDocumentFragment::appendXML(string $data): bool` |
|     - |  9633 | ` *` |
|     - |  9634 | ` * php parses the chunk as a well-balanced FRAGMENT (no single root required,` |
|     - |  9635 | ` * bare text allowed) and appends what it produced; anything libxml refuses is` |
|     - |  9636 | `` * `false` with nothing appended.`` |
|     - |  9637 | ` */` |
|    44 |  9638 | `DOM_METHOD(vm_builtin_DOMDocumentFragment_appendXML)` |
|     1 |  9639 | `{` |
|    45 |  9640 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    45 |  9641 | `	const char *zXml = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    45 |  9642 | `	xmlNodePtr pFrag,pList = 0;` |
|     - |  9643 | `	sxu32 nMark;` |
|     - |  9644 | `	int rc;` |
|    45 |  9645 | `	if( pNd == 0 ){` |
|   ! 0 |  9646 | `		ph7_result_bool(pCtx,0);` |
|   ! 0 |  9647 | `		return PH7_OK;` |
|     - |  9648 | `	}` |
|    45 |  9649 | `	pFrag = (xmlNodePtr)pNd->pNode;` |
|    45 |  9650 | `	if( DomNodeReadOnly(pFrag) ){` |
|     - |  9651 | ``		/* A CONSTRUCTED fragment -- `new DOMDocumentFragment()` -- refuses`` |
|     - |  9652 | `		 * this door the way every child-list door refuses an ownerless` |
|     - |  9653 | `		 * receiver, where its append() takes the same chunk's nodes. */` |
|     3 |  9654 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|     - |  9655 | `	}` |
|    43 |  9656 | `	nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|    43 |  9657 | `	rc = xmlParseBalancedChunkMemory(pFrag->doc,0,0,0,(const xmlChar *)zXml,&pList);` |
|    43 |  9658 | `	PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,"DOMDocumentFragment::appendXML");` |
|    43 |  9659 | `	if( rc != 0 ){` |
|     3 |  9660 | `		if( pList ){` |
|   ! 0 |  9661 | `			xmlFreeNodeList(pList);` |
|   ! 0 |  9662 | `		}` |
|     3 |  9663 | `		ph7_result_bool(pCtx,0);` |
|     3 |  9664 | `		return PH7_OK;` |
|     - |  9665 | `	}` |
|   101 |  9666 | `	while( pList ){` |
|    61 |  9667 | `		xmlNodePtr pNext = pList->next;` |
|    61 |  9668 | `		pList->next = pList->prev = 0;` |
|    61 |  9669 | `		DomLinkLast(pFrag,pList);` |
|    61 |  9670 | `		pList = pNext;` |
|     1 |  9671 | `	}` |
|    41 |  9672 | `	ph7_result_bool(pCtx,1);` |
|    41 |  9673 | `	return PH7_OK;` |
|    23 |  9674 | `}` |
|     - |  9675 | `/* DOMDocument::getElementsByTagName / DOMElement::getElementsByTagName --` |
|     - |  9676 | ` * php declares it on those two, not on DOMNode, so both specs name it. */` |
|   102 |  9677 | `DOM_METHOD(vm_builtin_Dom_getElementsByTagName)` |
|     1 |  9678 | `{` |
|   103 |  9679 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   103 |  9680 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     - |  9681 | `	ph7_class_instance *pList;` |
|   103 |  9682 | `	if( pThis == 0 ){` |
|   ! 0 |  9683 | `		return PH7_OK;` |
|     - |  9684 | `	}` |
|   103 |  9685 | `	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTN,pThis,zName,0,0);` |
|   103 |  9686 | `	if( pList == 0 ){` |
|   ! 0 |  9687 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9688 | `	}` |
|   103 |  9689 | `	PH7_NativeResultObject(pCtx,pList);` |
|   103 |  9690 | `	return PH7_OK;` |
|    52 |  9691 | `}` |
|     - |  9692 | `/*` |
|     - |  9693 | ` * DOMDocument::getElementsByTagNameNS / DOMElement::getElementsByTagNameNS` |
|     - |  9694 | ` * (?string $namespace, string $localName): DOMNodeList` |
|     - |  9695 | ` *` |
|     - |  9696 | ` * The namespace-aware half of the only two lookups the DOM has, and the one` |
|     - |  9697 | ` * every namespaced format is read with -- an XSLT stylesheet, a SOAP envelope, a` |
|     - |  9698 | ` * sitemap. Undefined here, so the URI could be answered for a node already found` |
|     - |  9699 | ` * and never searched FOR.` |
|     - |  9700 | ` *` |
|     - |  9701 | ` * The list is live and the receiver is never in it, exactly as the name-only` |
|     - |  9702 | ` * one; DomGebtnMatch carries php's asymmetric wildcard rules.` |
|     - |  9703 | ` */` |
|    44 |  9704 | `DOM_METHOD(vm_builtin_Dom_getElementsByTagNameNS)` |
|     1 |  9705 | `{` |
|    45 |  9706 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    45 |  9707 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|    45 |  9708 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     - |  9709 | `	ph7_class_instance *pList;` |
|    45 |  9710 | `	if( pThis == 0 ){` |
|   ! 0 |  9711 | `		return PH7_OK;` |
|     - |  9712 | `	}` |
|    67 |  9713 | `	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTNNS,pThis,` |
|    22 |  9714 | `		zName,zUri ? zUri : "",0);` |
|    45 |  9715 | `	if( pList == 0 ){` |
|   ! 0 |  9716 | `		return PH7_ContextMemoryError(pCtx);` |
|     - |  9717 | `	}` |
|    45 |  9718 | `	PH7_NativeResultObject(pCtx,pList);` |
|    45 |  9719 | `	return PH7_OK;` |
|    23 |  9720 | `}` |
|     - |  9721 |  |
|     - |  9722 | `/*` |
|     - |  9723 | ` * Install the DOM library: every class declared from C, no embedded chunk and` |
|     - |  9724 | ` * no globally visible thunk left.  Called from PH7_VmInit inside the` |
|     - |  9725 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml (the capture plumbing must` |
|     - |  9726 | ` * exist) and after the Reflection install (DOMException needs Exception).` |
|     - |  9727 | ` */` |
|  5740 |  9728 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)` |
|     5 |  9729 | `{` |
|     - |  9730 | `	/* The two slots every wrapper carries. They were public in the chunk and stay` |
|     - |  9731 | `	 * public: hiding them is the per-class debug-info hook's job (§7.4 (e)), which` |
|     - |  9732 | `	 * DateTime, XMLWriter, Fiber, Generator and WeakReference all wait on too. */` |
|     - |  9733 | `	static const PH7_NativePropDef aNodeProp[] = {` |
|     - |  9734 | `		{ DOM_RES, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9735 | `		{ DOM_DOC, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9736 | `		/* The identity cache. Only a DOCUMENT'S is a document's; a CONSTRUCTED` |
|     - |  9737 | `		 * ownerless node is its own holder (its $__doc points at itself) and` |
|     - |  9738 | `		 * caches its tree's wrappers HERE until an insertion adopts them into` |
|     - |  9739 | `		 * a real document's cache. Empty and unread on every owned node. */` |
|     - |  9740 | `		{ DOM_NODES, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9741 | `	};` |
|     - |  9742 | ``	/* php's own signatures. Declaring `DOMNode $node` is what makes`` |
|     - |  9743 | ``	 * `$n->appendChild(1)` the TypeError php raises instead of a warning from`` |
|     - |  9744 | `	 * reading ->__res off an int. */` |
|     - |  9745 | `	static const PH7_NativeMethodDef aNodeMethod[] = {` |
|     - |  9746 | `		{ "appendChild",    PH7_MOD_PUBLIC, "DOMNode $node", "", vm_builtin_DOMNode_appendChild },` |
|     - |  9747 | `		{ "insertBefore",   PH7_MOD_PUBLIC, "DOMNode $node, ?DOMNode $child = null", "",` |
|     - |  9748 | `		  vm_builtin_DOMNode_insertBefore },` |
|     - |  9749 | `		{ "removeChild",    PH7_MOD_PUBLIC, "DOMNode $child", "", vm_builtin_DOMNode_removeChild },` |
|     - |  9750 | `		{ "replaceChild",   PH7_MOD_PUBLIC, "DOMNode $node, DOMNode $child", "",` |
|     - |  9751 | `		  vm_builtin_DOMNode_replaceChild },` |
|     - |  9752 | `		{ "hasChildNodes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasChildNodes },` |
|     - |  9753 | `		{ "hasAttributes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasAttributes },` |
|     - |  9754 | `		{ "isSameNode",     PH7_MOD_PUBLIC, "DOMNode $otherNode", "@bool", vm_builtin_DOMNode_isSameNode },` |
|     - |  9755 | `		/* php declares no return type at all on this one, not even a tentative` |
|     - |  9756 | `		 * one, so the row states none either. */` |
|     - |  9757 | `		{ "cloneNode",      PH7_MOD_PUBLIC, "bool $deep = false", "", vm_builtin_DOMNode_cloneNode },` |
|     - |  9758 | `		/* php runs the same walk normalizeDocument() does, from the receiver. */` |
|     - |  9759 | `		{ "normalize",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },` |
|     - |  9760 | `		{ "getNodePath",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_DOMNode_getNodePath },` |
|     - |  9761 | `		{ "isEqualNode",    PH7_MOD_PUBLIC, "?DOMNode $otherNode", "bool",` |
|     - |  9762 | `		  vm_builtin_DOMNode_isEqualNode },` |
|     - |  9763 | `		{ "isSupported",    PH7_MOD_PUBLIC, "string $feature, string $version", "@bool",` |
|     - |  9764 | `		  vm_builtin_DOMNode_isSupported },` |
|     - |  9765 | `		/* php's declared type names DOMNameSpaceNode, a class PHL does not have;` |
|     - |  9766 | `		 * the row states it anyway so Reflection reports php's, and nothing can` |
|     - |  9767 | `		 * be handed one. (php's own zpp rejects a NON-object here with a` |
|     - |  9768 | `		 * "?object" message instead -- PLAN §7.4, the error-format class.) */` |
|     - |  9769 | `		{ "contains",       PH7_MOD_PUBLIC, "DOMNode\|DOMNameSpaceNode\|null $other", "bool",` |
|     - |  9770 | `		  vm_builtin_DOMNode_contains },` |
|     - |  9771 | `		{ "getRootNode",    PH7_MOD_PUBLIC, "?array $options = null", "DOMNode",` |
|     - |  9772 | `		  vm_builtin_DOMNode_getRootNode },` |
|     - |  9773 | `		{ "compareDocumentPosition", PH7_MOD_PUBLIC, "DOMNode $other", "int",` |
|     - |  9774 | `		  vm_builtin_DOMNode_compareDocumentPosition },` |
|     - |  9775 | `		{ "getLineNo",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNode_getLineNo },` |
|     - |  9776 | `		{ "C14N",           PH7_MOD_PUBLIC,` |
|     - |  9777 | `		  "bool $exclusive = false, bool $withComments = false, ?array $xpath = null, "` |
|     - |  9778 | `		  "?array $nsPrefixes = null", "@string\|false", vm_builtin_DOMNode_C14N },` |
|     - |  9779 | `		{ "C14NFile",       PH7_MOD_PUBLIC,` |
|     - |  9780 | `		  "string $uri, bool $exclusive = false, bool $withComments = false, "` |
|     - |  9781 | `		  "?array $xpath = null, ?array $nsPrefixes = null", "@int\|false",` |
|     - |  9782 | `		  vm_builtin_DOMNode_C14NFile },` |
|     - |  9783 | `		/* The refusal MACHINERY, not decoration: serialize() finds __sleep and` |
|     - |  9784 | `		 * unserialize() calls __wakeup, so a subclass declaring either escapes. */` |
|     - |  9785 | `		{ "__sleep",        PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },` |
|     - |  9786 | `		{ "__wakeup",       PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },` |
|     - |  9787 | `		{ "lookupNamespaceURI", PH7_MOD_PUBLIC, "?string $prefix", "@?string",` |
|     - |  9788 | `		  vm_builtin_DOMNode_lookupNamespaceURI },` |
|     - |  9789 | `		{ "lookupPrefix",   PH7_MOD_PUBLIC, "string $namespace", "@?string",` |
|     - |  9790 | `		  vm_builtin_DOMNode_lookupPrefix },` |
|     - |  9791 | `		{ "isDefaultNamespace", PH7_MOD_PUBLIC, "string $namespace", "@bool",` |
|     - |  9792 | `		  vm_builtin_DOMNode_isDefaultNamespace },` |
|     - |  9793 | `		{ "__get",          PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNode_get },` |
|     - |  9794 | `		{ "__isset",        PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNode_isset },` |
|     - |  9795 | `		{ "__set",          PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMNode_set },` |
|     - |  9796 | `	};` |
|     - |  9797 | `	/* php declares the six on DOMNode; every node class inherits them. */` |
|     - |  9798 | `	static const PH7_NativeConstDef aNodeConst[] = {` |
|     - |  9799 | `		{ "DOCUMENT_POSITION_DISCONNECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  9800 | `		  DOM_POS_DISCONNECTED, 0, 0.0 },` |
|     - |  9801 | `		{ "DOCUMENT_POSITION_PRECEDING", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  9802 | `		  DOM_POS_PRECEDING, 0, 0.0 },` |
|     - |  9803 | `		{ "DOCUMENT_POSITION_FOLLOWING", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  9804 | `		  DOM_POS_FOLLOWING, 0, 0.0 },` |
|     - |  9805 | `		{ "DOCUMENT_POSITION_CONTAINS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  9806 | `		  DOM_POS_CONTAINS, 0, 0.0 },` |
|     - |  9807 | `		{ "DOCUMENT_POSITION_CONTAINED_BY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  9808 | `		  DOM_POS_CONTAINED_BY, 0, 0.0 },` |
|     - |  9809 | `		{ "DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|     - |  9810 | `		  DOM_POS_IMPL_SPEC, 0, 0.0 },` |
|     - |  9811 | `	};` |
|     - |  9812 | `	static const PH7_NativePropDef aDocProp[] = {` |
|     - |  9813 | `		/* php models both as VIRTUAL hooked properties reading libxml state, so it` |
|     - |  9814 | `		 * reports no default; PHL's are real slots and keep theirs, or a read before` |
|     - |  9815 | `		 * the first write would raise where php answers the parser's current value.` |
|     - |  9816 | `		 * The TYPE is what the row can state exactly (PLAN §7.4 for the virtual half). */` |
|     - |  9817 | `		{ "preserveWhiteSpace", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },` |
|     - |  9818 | `		{ "formatOutput",       PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|     - |  9819 | `		/* The four the parse reads (DomParseOptions). php's defaults are all` |
|     - |  9820 | `		 * false: nothing is validated, expanded, defaulted or recovered unless` |
|     - |  9821 | `		 * the program asks. */` |
|     - |  9822 | `		{ "validateOnParse",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|     - |  9823 | `		{ "resolveExternals",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|     - |  9824 | `		{ "substituteEntities", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|     - |  9825 | `		{ "recover",            PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|     - |  9826 | `		/* The only one php defaults to TRUE: refusals are exceptions until a` |
|     - |  9827 | `		 * program asks for warnings (DomThrowAs). */` |
|     - |  9828 | `		{ "strictErrorChecking", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },` |
|     - |  9829 | `		/* The identity cache DomWrap keys by node pointer. */` |
|     - |  9830 | `		{ DOM_NODES,            PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9831 | `		/* ...and the base-class => user-class table registerNodeClass writes. */` |
|     - |  9832 | `		{ DOM_NCLS,             PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - |  9833 | `	};` |
|     - |  9834 | `	static const PH7_NativeMethodDef aDocMethod[] = {` |
|     - |  9835 | `		{ "__construct",          PH7_MOD_PUBLIC, "string $version = '1.0', string $encoding = ''", "",` |
|     - |  9836 | `		  vm_builtin_DOMDocument_construct },` |
|     - |  9837 | `		{ "loadXML",              PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",` |
|     - |  9838 | `		  vm_builtin_DOMDocument_loadXML },` |
|     - |  9839 | `		{ "load",                 PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@bool",` |
|     - |  9840 | `		  vm_builtin_DOMDocument_load },` |
|     - |  9841 | `		{ "save",                 PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@int\|false",` |
|     - |  9842 | `		  vm_builtin_DOMDocument_save },` |
|     - |  9843 | `		{ "loadHTML",             PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",` |
|     - |  9844 | `		  vm_builtin_DOMDocument_loadHTML },` |
|     - |  9845 | `		{ "loadHTMLFile",         PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@bool",` |
|     - |  9846 | `		  vm_builtin_DOMDocument_loadHTMLFile },` |
|     - |  9847 | `		{ "saveHTML",             PH7_MOD_PUBLIC, "?DOMNode $node = null", "@string\|false",` |
|     - |  9848 | `		  vm_builtin_DOMDocument_saveHTML },` |
|     - |  9849 | `		{ "saveHTMLFile",         PH7_MOD_PUBLIC, "string $filename", "@int\|false",` |
|     - |  9850 | `		  vm_builtin_DOMDocument_saveHTMLFile },` |
|     - |  9851 | `		{ "saveXML",              PH7_MOD_PUBLIC, "?DOMNode $node = null, int $options = 0", "@string\|false",` |
|     - |  9852 | `		  vm_builtin_DOMDocument_saveXML },` |
|     - |  9853 | `		{ "createElement",        PH7_MOD_PUBLIC, "string $localName, string $value = ''", "",` |
|     - |  9854 | `		  vm_builtin_DOMDocument_createElement },` |
|     - |  9855 | `		/* php declares no return type at all on this one either -- its answer is` |
|     - |  9856 | ``		 * `DOMElement\|false` and it never wrote that down. */`` |
|     - |  9857 | `		{ "createElementNS",      PH7_MOD_PUBLIC,` |
|     - |  9858 | `		  "?string $namespace, string $qualifiedName, string $value = ''", "",` |
|     - |  9859 | `		  vm_builtin_DOMDocument_createElementNS },` |
|     - |  9860 | ``		/* php declares no return type on this one either: `DOMNode\|false`. */`` |
|     - |  9861 | `		{ "importNode",           PH7_MOD_PUBLIC, "DOMNode $node, bool $deep = false", "",` |
|     - |  9862 | `		  vm_builtin_DOMDocument_importNode },` |
|     - |  9863 | `		{ "adoptNode",            PH7_MOD_PUBLIC, "DOMNode $node", "@DOMNode\|false",` |
|     - |  9864 | `		  vm_builtin_DOMDocument_adoptNode },` |
|     - |  9865 | `		{ "getElementById",       PH7_MOD_PUBLIC, "string $elementId", "@?DOMElement",` |
|     - |  9866 | `		  vm_builtin_DOMDocument_getElementById },` |
|     - |  9867 | `		{ "createAttribute",      PH7_MOD_PUBLIC, "string $localName", "",` |
|     - |  9868 | `		  vm_builtin_DOMDocument_createAttribute },` |
|     - |  9869 | `		{ "createAttributeNS",    PH7_MOD_PUBLIC, "?string $namespace, string $qualifiedName", "",` |
|     - |  9870 | `		  vm_builtin_DOMDocument_createAttributeNS },` |
|     - |  9871 | `		{ "createTextNode",       PH7_MOD_PUBLIC, "string $data", "@DOMText",` |
|     - |  9872 | `		  vm_builtin_DOMDocument_createTextNode },` |
|     - |  9873 | `		{ "createComment",        PH7_MOD_PUBLIC, "string $data", "@DOMComment",` |
|     - |  9874 | `		  vm_builtin_DOMDocument_createComment },` |
|     - |  9875 | `		{ "createCDATASection",   PH7_MOD_PUBLIC, "string $data", "",` |
|     - |  9876 | `		  vm_builtin_DOMDocument_createCDATASection },` |
|     - |  9877 | `		{ "createProcessingInstruction", PH7_MOD_PUBLIC, "string $target, string $data = ''", "",` |
|     - |  9878 | `		  vm_builtin_DOMDocument_createPI },` |
|     - |  9879 | `		{ "createEntityReference", PH7_MOD_PUBLIC, "string $name", "",` |
|     - |  9880 | `		  vm_builtin_DOMDocument_createEntityRef },` |
|     - |  9881 | `		{ "createDocumentFragment", PH7_MOD_PUBLIC, "", "",` |
|     - |  9882 | `		  vm_builtin_DOMDocument_createFragment },` |
|     - |  9883 | `		{ "normalizeDocument",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },` |
|     - |  9884 | `		{ "registerNodeClass",    PH7_MOD_PUBLIC, "string $baseClass, ?string $extendedClass",` |
|     - |  9885 | `		  "true", vm_builtin_DOMDocument_registerNodeClass },` |
|     - |  9886 | `		{ "schemaValidate",       PH7_MOD_PUBLIC, "string $filename, int $flags = 0", "@bool",` |
|     - |  9887 | `		  vm_builtin_DOMDocument_schemaValidate },` |
|     - |  9888 | `		{ "schemaValidateSource", PH7_MOD_PUBLIC, "string $source, int $flags = 0", "@bool",` |
|     - |  9889 | `		  vm_builtin_DOMDocument_schemaValidateSource },` |
|     - |  9890 | `		/* php declares no option word on the RelaxNG pair at all. */` |
|     - |  9891 | `		{ "relaxNGValidate",       PH7_MOD_PUBLIC, "string $filename", "@bool",` |
|     - |  9892 | `		  vm_builtin_DOMDocument_relaxNGValidate },` |
|     - |  9893 | `		{ "relaxNGValidateSource", PH7_MOD_PUBLIC, "string $source", "@bool",` |
|     - |  9894 | `		  vm_builtin_DOMDocument_relaxNGValidateSource },` |
|     - |  9895 | `		{ "validate",             PH7_MOD_PUBLIC, "", "@bool",` |
|     - |  9896 | `		  vm_builtin_DOMDocument_validate },` |
|     - |  9897 | `		{ "xinclude",             PH7_MOD_PUBLIC, "int $options = 0", "@int\|false",` |
|     - |  9898 | `		  vm_builtin_DOMDocument_xinclude },` |
|     - |  9899 | `		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",` |
|     - |  9900 | `		  vm_builtin_Dom_getElementsByTagName },` |
|     - |  9901 | `		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",` |
|     - |  9902 | `		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },` |
|     - |  9903 | `		/* The DOMParentNode three: real (non-tentative) void, one untyped` |
|     - |  9904 | `		 * variadic -- php's own rows, screened inside the body. */` |
|     - |  9905 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|     - |  9906 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|     - |  9907 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|     - |  9908 | `		{ "__get",                PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMDocument_get },` |
|     - |  9909 | `		{ "__isset",              PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMDocument_isset },` |
|     - |  9910 | `		{ "__set",                PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMDocument_set },` |
|     - |  9911 | `	};` |
|     - |  9912 | `	static const PH7_NativeMethodDef aElemMethod[] = {` |
|     - |  9913 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|     - |  9914 | `		  "string $qualifiedName, ?string $value = null, string $namespace = ''", "",` |
|     - |  9915 | `		  vm_builtin_DOMElement_construct },` |
|     - |  9916 | `		{ "getAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@string",` |
|     - |  9917 | `		  vm_builtin_DOMElement_getAttribute },` |
|     - |  9918 | `		{ "hasAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",` |
|     - |  9919 | `		  vm_builtin_DOMElement_hasAttribute },` |
|     - |  9920 | `		{ "setAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName, string $value", "",` |
|     - |  9921 | `		  vm_builtin_DOMElement_setAttribute },` |
|     - |  9922 | `		{ "removeAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",` |
|     - |  9923 | `		  vm_builtin_DOMElement_removeAttribute },` |
|     - |  9924 | `		{ "getAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@string",` |
|     - |  9925 | `		  vm_builtin_DOMElement_getAttributeNS },` |
|     - |  9926 | `		/* php declares no return type at all on the five that hand out a NODE --` |
|     - |  9927 | `		 * not even a tentative one -- because their answer is a union it never` |
|     - |  9928 | `		 * wrote down. The rows state none either. */` |
|     - |  9929 | `		{ "getAttributeNode",     PH7_MOD_PUBLIC, "string $qualifiedName", "",` |
|     - |  9930 | `		  vm_builtin_DOMElement_getAttributeNode },` |
|     - |  9931 | `		{ "getAttributeNodeNS",   PH7_MOD_PUBLIC, "?string $namespace, string $localName", "",` |
|     - |  9932 | `		  vm_builtin_DOMElement_getAttributeNodeNS },` |
|     - |  9933 | `		{ "setAttributeNode",     PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|     - |  9934 | `		  vm_builtin_DOMElement_setAttributeNode },` |
|     - |  9935 | `		{ "setAttributeNodeNS",   PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|     - |  9936 | `		  vm_builtin_DOMElement_setAttributeNodeNS },` |
|     - |  9937 | `		{ "removeAttributeNode",  PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|     - |  9938 | `		  vm_builtin_DOMElement_removeAttributeNode },` |
|     - |  9939 | `		{ "getAttributeNames",    PH7_MOD_PUBLIC, "", "array",` |
|     - |  9940 | `		  vm_builtin_DOMElement_getAttributeNames },` |
|     - |  9941 | `		{ "hasAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@bool",` |
|     - |  9942 | `		  vm_builtin_DOMElement_hasAttributeNS },` |
|     - |  9943 | `		{ "removeAttributeNS",    PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@void",` |
|     - |  9944 | `		  vm_builtin_DOMElement_removeAttributeNS },` |
|     - |  9945 | `		{ "toggleAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName, ?bool $force = null", "bool",` |
|     - |  9946 | `		  vm_builtin_DOMElement_toggleAttribute },` |
|     - |  9947 | `		{ "setIdAttribute",       PH7_MOD_PUBLIC, "string $qualifiedName, bool $isId", "@void",` |
|     - |  9948 | `		  vm_builtin_DOMElement_setIdAttribute },` |
|     - |  9949 | `		{ "setIdAttributeNS",     PH7_MOD_PUBLIC,` |
|     - |  9950 | `		  "string $namespace, string $qualifiedName, bool $isId", "@void",` |
|     - |  9951 | `		  vm_builtin_DOMElement_setIdAttributeNS },` |
|     - |  9952 | `		{ "setIdAttributeNode",   PH7_MOD_PUBLIC, "DOMAttr $attr, bool $isId", "@void",` |
|     - |  9953 | `		  vm_builtin_DOMElement_setIdAttributeNode },` |
|     - |  9954 | `		{ "setAttributeNS",       PH7_MOD_PUBLIC,` |
|     - |  9955 | `		  "?string $namespace, string $qualifiedName, string $value", "@void",` |
|     - |  9956 | `		  vm_builtin_DOMElement_setAttributeNS },` |
|     - |  9957 | `		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",` |
|     - |  9958 | `		  vm_builtin_Dom_getElementsByTagName },` |
|     - |  9959 | `		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",` |
|     - |  9960 | `		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },` |
|     - |  9961 | `		/* The DOMChildNode four, php's order on this class. */` |
|     - |  9962 | `		{ "remove",          PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf },` |
|     - |  9963 | `		{ "before",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_before },` |
|     - |  9964 | `		{ "after",           PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_after },` |
|     - |  9965 | `		{ "replaceWith",     PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceWith },` |
|     - |  9966 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|     - |  9967 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|     - |  9968 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|     - |  9969 | `		/* The 8.3 pair. php declares the first's return as a plain ?DOMElement` |
|     - |  9970 | `		 * and the second's as a real void. */` |
|     - |  9971 | `		{ "insertAdjacentElement", PH7_MOD_PUBLIC, "string $where, DOMElement $element",` |
|     - |  9972 | `		  "?DOMElement", vm_builtin_DOMElement_insertAdjacentElement },` |
|     - |  9973 | `		{ "insertAdjacentText",    PH7_MOD_PUBLIC, "string $where, string $data", "void",` |
|     - |  9974 | `		  vm_builtin_DOMElement_insertAdjacentText },` |
|     - |  9975 | `		{ "__get",                PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMElement_get },` |
|     - |  9976 | `		{ "__isset",              PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMElement_isset },` |
|     - |  9977 | `		{ "__set",                PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMElement_set },` |
|     - |  9978 | `	};` |
|     - |  9979 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|     - |  9980 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name, string $value = ''", "",` |
|     - |  9981 | `		  vm_builtin_DOMAttr_construct },` |
|     - |  9982 | `		{ "isId",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMAttr_isId },` |
|     - |  9983 | `		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMAttr_get },` |
|     - |  9984 | `		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMAttr_isset },` |
|     - |  9985 | `		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMAttr_set },` |
|     - |  9986 | `	};` |
|     - |  9987 | `	static const PH7_NativeMethodDef aCharMethod[] = {` |
|     - |  9988 | `		/* Every offset and count here is in UTF-8 CHARACTERS, php's unit. */` |
|     - |  9989 | `		{ "appendData",    PH7_MOD_PUBLIC, "string $data", "@true",` |
|     - |  9990 | `		  vm_builtin_DOMCharacterData_appendData },` |
|     - |  9991 | `		{ "substringData", PH7_MOD_PUBLIC, "int $offset, int $count", "",` |
|     - |  9992 | `		  vm_builtin_DOMCharacterData_substringData },` |
|     - |  9993 | `		{ "insertData",    PH7_MOD_PUBLIC, "int $offset, string $data", "@bool",` |
|     - |  9994 | `		  vm_builtin_DOMCharacterData_insertData },` |
|     - |  9995 | `		{ "deleteData",    PH7_MOD_PUBLIC, "int $offset, int $count", "@bool",` |
|     - |  9996 | `		  vm_builtin_DOMCharacterData_deleteData },` |
|     - |  9997 | `		{ "replaceData",   PH7_MOD_PUBLIC, "int $offset, int $count, string $data", "@bool",` |
|     - |  9998 | `		  vm_builtin_DOMCharacterData_replaceData },` |
|     - |  9999 | `		/* The DOMChildNode four, php's order on THIS class -- replaceWith` |
|     - | 10000 | `		 * leads here where DOMElement's list starts at remove. */` |
|     - | 10001 | `		{ "replaceWith", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceWith },` |
|     - | 10002 | `		{ "remove",      PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf },` |
|     - | 10003 | `		{ "before",      PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_before },` |
|     - | 10004 | `		{ "after",       PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_after },` |
|     - | 10005 | `		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMCharacterData_get },` |
|     - | 10006 | `		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMCharacterData_isset },` |
|     - | 10007 | `		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMCharacterData_set },` |
|     - | 10008 | `	};` |
|     - | 10009 | `	static const PH7_NativeMethodDef aPiMethod[] = {` |
|     - | 10010 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name, string $value = ''", "",` |
|     - | 10011 | `		  vm_builtin_DOMProcessingInstruction_construct },` |
|     - | 10012 | `		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMProcessingInstruction_get },` |
|     - | 10013 | `		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMProcessingInstruction_isset },` |
|     - | 10014 | `		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|     - | 10015 | `		  vm_builtin_DOMProcessingInstruction_set },` |
|     - | 10016 | `	};` |
|     - | 10017 | `	static const PH7_NativeMethodDef aFragMethod[] = {` |
|     - | 10018 | `		{ "__construct", PH7_MOD_PUBLIC, "", "",` |
|     - | 10019 | `		  vm_builtin_DOMDocumentFragment_construct },` |
|     - | 10020 | `		{ "appendXML", PH7_MOD_PUBLIC, "string $data", "@bool",` |
|     - | 10021 | `		  vm_builtin_DOMDocumentFragment_appendXML },` |
|     - | 10022 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|     - | 10023 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|     - | 10024 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|     - | 10025 | `		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMDocumentFragment_get },` |
|     - | 10026 | `		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMDocumentFragment_isset },` |
|     - | 10027 | `		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|     - | 10028 | `		  vm_builtin_DOMDocumentFragment_set },` |
|     - | 10029 | `	};` |
|     - | 10030 | `	static const PH7_NativeMethodDef aTextMethod[] = {` |
|     - | 10031 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data = ''", "",` |
|     - | 10032 | `		  vm_builtin_DOMText_construct },` |
|     - | 10033 | `		{ "splitText", PH7_MOD_PUBLIC, "int $offset", "", vm_builtin_DOMText_splitText },` |
|     - | 10034 | `		/* php's 8.x rename and the name it renamed, one body. */` |
|     - | 10035 | `		{ "isWhitespaceInElementContent", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 10036 | `		  vm_builtin_DOMText_isWhitespace },` |
|     - | 10037 | `		{ "isElementContentWhitespace",   PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 10038 | `		  vm_builtin_DOMText_isWhitespace },` |
|     - | 10039 | `		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMText_get },` |
|     - | 10040 | `		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMText_isset },` |
|     - | 10041 | `		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMText_set },` |
|     - | 10042 | `	};` |
|     - | 10043 | `	static const PH7_NativeMethodDef aEntRefMethod[] = {` |
|     - | 10044 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|     - | 10045 | `		  vm_builtin_DOMEntityReference_construct },` |
|     - | 10046 | `	};` |
|     - | 10047 | `	/* The DTD half declares no method of its own at all -- php's whole` |
|     - | 10048 | `	 * DOMDocumentType surface is properties over DOMNode's method list. */` |
|     - | 10049 | `	static const PH7_NativeMethodDef aDocTypeMethod[] = {` |
|     - | 10050 | `		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMDocumentType_get },` |
|     - | 10051 | `		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMDocumentType_isset },` |
|     - | 10052 | `		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|     - | 10053 | `		  vm_builtin_DOMDocumentType_set },` |
|     - | 10054 | `	};` |
|     - | 10055 | `	static const PH7_NativeMethodDef aEntityMethod[] = {` |
|     - | 10056 | `		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMEntity_get },` |
|     - | 10057 | `		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMEntity_isset },` |
|     - | 10058 | `		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|     - | 10059 | `		  vm_builtin_DOMEntity_set },` |
|     - | 10060 | `	};` |
|     - | 10061 | `	/* php's factory class: three ordinary instance methods and no state. */` |
|     - | 10062 | `	static const PH7_NativeMethodDef aImplMethod[] = {` |
|     - | 10063 | `		{ "createDocumentType", PH7_MOD_PUBLIC,` |
|     - | 10064 | `		  "string $qualifiedName, string $publicId = '', string $systemId = ''", "",` |
|     - | 10065 | `		  vm_builtin_DOMImplementation_createDocumentType },` |
|     - | 10066 | `		{ "createDocument", PH7_MOD_PUBLIC,` |
|     - | 10067 | `		  "?string $namespace = null, string $qualifiedName = '', "` |
|     - | 10068 | `		  "?DOMDocumentType $doctype = null", "DOMDocument",` |
|     - | 10069 | `		  vm_builtin_DOMImplementation_createDocument },` |
|     - | 10070 | `		{ "hasFeature", PH7_MOD_PUBLIC, "string $feature, string $version", "bool",` |
|     - | 10071 | `		  vm_builtin_DOMImplementation_hasFeature },` |
|     - | 10072 | `	};` |
|     - | 10073 | `	static const PH7_NativeMethodDef aNotationMethod[] = {` |
|     - | 10074 | `		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNotation_get },` |
|     - | 10075 | `		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNotation_isset },` |
|     - | 10076 | `		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|     - | 10077 | `		  vm_builtin_DOMNotation_set },` |
|     - | 10078 | `	};` |
|     - | 10079 | `	/* The comment and CDATA constructors -- the only method either class` |
|     - | 10080 | `	 * declares of its own; php's CDATA data is REQUIRED where the other two` |
|     - | 10081 | `	 * default. */` |
|     - | 10082 | `	static const PH7_NativeMethodDef aCommentMethod[] = {` |
|     - | 10083 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data = ''", "",` |
|     - | 10084 | `		  vm_builtin_DOMComment_construct },` |
|     - | 10085 | `	};` |
|     - | 10086 | `	static const PH7_NativeMethodDef aCdataMethod[] = {` |
|     - | 10087 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data", "",` |
|     - | 10088 | `		  vm_builtin_DOMCdataSection_construct },` |
|     - | 10089 | `	};` |
|     - | 10090 | `	/* DOMNodeList and DOMNamedNodeMap share a slot layout: what a live view is OF` |
|     - | 10091 | `	 * ($__owner), the document to wrap results against ($__doc), and -- for the` |
|     - | 10092 | `	 * two node-list kinds -- the tag name or the frozen snapshot. */` |
|     - | 10093 | `	static const PH7_NativePropDef aListProp[] = {` |
|     - | 10094 | `		{ DNL_KIND,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|     - | 10095 | `		{ DOM_DOC,       PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|     - | 10096 | `		{ DNL_OWNER,     PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|     - | 10097 | `		{ DNL_NAME,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 10098 | `		/* ...and, for the namespace-aware lookup, the URI beside the name. */` |
|     - | 10099 | `		{ DNL_URI,       PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 10100 | `		/* The cached node snapshot, missed by the 2 Aug hidden-slot sweep exactly as` |
|     - | 10101 | `		 * Closure's three were: php presents no property on either class this table` |
|     - | 10102 | ``		 * declares (DOMNodeList, DOMNamedNodeMap), and `__snap` was on var_dump,`` |
|     - | 10103 | `		 * (array), get_object_vars, foreach, json_encode and Reflection. */` |
|     - | 10104 | `		{ DNL_SNAP_SLOT, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|     - | 10105 | `	};` |
|     - | 10106 | `	static const PH7_NativeMethodDef aListMethod[] = {` |
|     - | 10107 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNodeList_count },` |
|     - | 10108 | `		{ "item",        PH7_MOD_PUBLIC, "int $index", "", vm_builtin_DOMNodeList_item },` |
|     - | 10109 | `		{ "getIterator", PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },` |
|     - | 10110 | `		{ "__get",       PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNodeList_get },` |
|     - | 10111 | `		{ "__isset",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNodeList_isset },` |
|     - | 10112 | `		{ "__set",       PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMNodeList_set },` |
|     - | 10113 | `	};` |
|     - | 10114 | `	static const PH7_NativeMethodDef aMapMethod[] = {` |
|     - | 10115 | `		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNamedNodeMap_count },` |
|     - | 10116 | `		{ "item",         PH7_MOD_PUBLIC, "int $index", "@?DOMNode", vm_builtin_DOMNamedNodeMap_item },` |
|     - | 10117 | `		{ "getNamedItem", PH7_MOD_PUBLIC, "string $qualifiedName", "@?DOMNode",` |
|     - | 10118 | `		  vm_builtin_DOMNamedNodeMap_getNamedItem },` |
|     - | 10119 | `		{ "getNamedItemNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@?DOMNode",` |
|     - | 10120 | `		  vm_builtin_DOMNamedNodeMap_getNamedItemNS },` |
|     - | 10121 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },` |
|     - | 10122 | `		{ "__get",        PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNamedNodeMap_get },` |
|     - | 10123 | `		{ "__isset",      PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNamedNodeMap_isset },` |
|     - | 10124 | `		{ "__set",        PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMNamedNodeMap_set },` |
|     - | 10125 | `	};` |
|     - | 10126 | `	/* The declaration itself, the document it belongs to, and the element that` |
|     - | 10127 | `	 * MAKES it -- php's parentNode/parentElement. */` |
|     - | 10128 | `	static const PH7_NativePropDef aNsNodeProp[] = {` |
|     - | 10129 | `		{ DOM_RES,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 10130 | `		{ DOM_DOC,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 10131 | `		{ DOM_NS_OWNER, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 10132 | `	};` |
|     - | 10133 | `	static const PH7_NativeMethodDef aNsNodeMethod[] = {` |
|     - | 10134 | `		{ "__sleep",  PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },` |
|     - | 10135 | `		{ "__wakeup", PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },` |
|     - | 10136 | `		{ "__get",    PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNameSpaceNode_get },` |
|     - | 10137 | `		{ "__isset",  PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNameSpaceNode_isset },` |
|     - | 10138 | `		{ "__set",    PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|     - | 10139 | `		  vm_builtin_DOMNameSpaceNode_set },` |
|     - | 10140 | `	};` |
|     - | 10141 | `	static const PH7_NativePropDef aXPathProp[] = {` |
|     - | 10142 | `		/* Written by the constructor, which is why the slot can carry php's` |
|     - | 10143 | `		 * non-nullable type with no default at all. php models both declared` |
|     - | 10144 | `		 * slots as VIRTUAL (the §7.4 residual); the readonly flag here is what` |
|     - | 10145 | `		 * answers php's write refusal ("Cannot modify readonly property"),` |
|     - | 10146 | `		 * at the price of isReadOnly() reading true where php reads false. */` |
|     - | 10147 | `		{ "document", PH7_MOD_PUBLIC\|PH7_MOD_READONLY,` |
|     - | 10148 | `		  { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "DOMDocument" },` |
|     - | 10149 | `		{ "registerNodeNamespaces", PH7_MOD_PUBLIC,` |
|     - | 10150 | `		  { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },` |
|     - | 10151 | `		{ XP_NSREG,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 10152 | `		{ XP_FNMODE, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN,` |
|     - | 10153 | `		  { 0, 0, PH7_NATIVE_VAL_INT, XP_MODE_NONE, 0, 0.0 }, 0 },` |
|     - | 10154 | `		{ XP_FNREG,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 10155 | `		{ XP_NSFN,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 10156 | `	};` |
|     - | 10157 | `	static const PH7_NativeMethodDef aXPathMethod[] = {` |
|     - | 10158 | `		{ "__construct", PH7_MOD_PUBLIC, "DOMDocument $document, bool $registerNodeNS = true", "",` |
|     - | 10159 | `		  vm_builtin_DOMXPath_construct },` |
|     - | 10160 | `		{ "query",       PH7_MOD_PUBLIC,` |
|     - | 10161 | `		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",` |
|     - | 10162 | `		  vm_builtin_DOMXPath_query },` |
|     - | 10163 | `		{ "evaluate",    PH7_MOD_PUBLIC,` |
|     - | 10164 | `		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",` |
|     - | 10165 | `		  vm_builtin_DOMXPath_evaluate },` |
|     - | 10166 | `		{ "registerNamespace", PH7_MOD_PUBLIC, "string $prefix, string $namespace", "@bool",` |
|     - | 10167 | `		  vm_builtin_DOMXPath_registerNamespace },` |
|     - | 10168 | `		{ "registerPhpFunctions", PH7_MOD_PUBLIC, "array\|string\|null $restrict = null", "@void",` |
|     - | 10169 | `		  vm_builtin_DOMXPath_registerPhpFunctions },` |
|     - | 10170 | `		{ "registerPhpFunctionNS", PH7_MOD_PUBLIC,` |
|     - | 10171 | `		  "string $namespaceURI, string $name, callable $callable", "void",` |
|     - | 10172 | `		  vm_builtin_DOMXPath_registerPhpFunctionNS },` |
|     - | 10173 | `		{ "quote", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $str", "string",` |
|     - | 10174 | `		  vm_builtin_DOMXPath_quote },` |
|     - | 10175 | `	};` |
|     - | 10176 | `	/* Bases before subclasses: PH7_InstallNativeClasses declares the whole table` |
|     - | 10177 | `	 * before touching a method, but PH7_ClassInherit still needs the parent to` |
|     - | 10178 | `	 * exist when the child's row is declared. */` |
|     - | 10179 | `	/* php refuses to serialize a NODE class, and its refusal is the soft kind: the` |
|     - | 10180 | `	 * deny handler sits behind the __serialize()/__sleep() lookup, so a subclass that` |
|     - | 10181 | `	 * declares either one is serialized normally and the sentence says so. DOMXPath's` |
|     - | 10182 | `	 * is the HARD kind — a subclass declaring __serialize() is refused there too — and` |
|     - | 10183 | ``	 * DOMNodeList/DOMNamedNodeMap are not refused at all (`0:{}`), which is what they`` |
|     - | 10184 | `	 * became once serialize() stopped emitting the hidden slot. Restating the flag on` |
|     - | 10185 | `	 * every row is rule 29: a native subclass does not inherit its parent's. */` |
|     - | 10186 | `	/* php's 8.0 insertion interfaces: three untyped-variadic void methods on` |
|     - | 10187 | `	 * the parent side (the child side is DOMChildNode below).  A class row` |
|     - | 10188 | `	 * declares its methods before the implement phase runs, so nothing is` |
|     - | 10189 | `	 * stubbed abstract. */` |
|     - | 10190 | `	static const PH7_NativeMethodDef aParentNodeIf[] = {` |
|     - | 10191 | `		{ "append",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|     - | 10192 | `		{ "prepend",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|     - | 10193 | `		{ "replaceChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|     - | 10194 | `	};` |
|     - | 10195 | `	static const PH7_NativeMethodDef aChildNodeIf[] = {` |
|     - | 10196 | `		{ "remove",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "void", 0 },` |
|     - | 10197 | `		{ "before",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|     - | 10198 | `		{ "after",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|     - | 10199 | `		{ "replaceWith", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|     - | 10200 | `	};` |
|     - | 10201 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 10202 | `		{ "DOMException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10203 | `		{ "DOMParentNode", 0, 0, PH7_CLASS_INTERFACE,` |
|     - | 10204 | `		  aParentNodeIf, SX_ARRAYSIZE(aParentNodeIf), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10205 | `		{ "DOMChildNode", 0, 0, PH7_CLASS_INTERFACE,` |
|     - | 10206 | `		  aChildNodeIf, SX_ARRAYSIZE(aChildNodeIf), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10207 | `		{ "DOMNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10208 | `		  aNodeMethod, SX_ARRAYSIZE(aNodeMethod), aNodeConst, SX_ARRAYSIZE(aNodeConst),` |
|     - | 10209 | `		  aNodeProp, SX_ARRAYSIZE(aNodeProp), 0, 0, 0 },` |
|     - | 10210 | `		{ "DOMDocument", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10211 | `		  aDocMethod, SX_ARRAYSIZE(aDocMethod), 0, 0, aDocProp, SX_ARRAYSIZE(aDocProp), 0, 0, 0 },` |
|     - | 10212 | `		{ "DOMElement", "DOMNode", "DOMParentNode,DOMChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10213 | `		  aElemMethod, SX_ARRAYSIZE(aElemMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10214 | `		{ "DOMAttr", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10215 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10216 | `		{ "DOMCharacterData", "DOMNode", "DOMChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10217 | `		  aCharMethod, SX_ARRAYSIZE(aCharMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10218 | `		{ "DOMText", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10219 | `		  aTextMethod, SX_ARRAYSIZE(aTextMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10220 | `		{ "DOMComment", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10221 | `		  aCommentMethod, SX_ARRAYSIZE(aCommentMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10222 | `		{ "DOMCdataSection", "DOMText", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10223 | `		  aCdataMethod, SX_ARRAYSIZE(aCdataMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10224 | ``		/* php declares the PI under DOMNode (its `data` is its own property, not`` |
|     - | 10225 | `		 * DOMCharacterData's), the fragment and the entity reference plainly. */` |
|     - | 10226 | `		{ "DOMProcessingInstruction", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10227 | `		  aPiMethod, SX_ARRAYSIZE(aPiMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10228 | `		{ "DOMDocumentFragment", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10229 | `		  aFragMethod, SX_ARRAYSIZE(aFragMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10230 | `		{ "DOMEntityReference", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10231 | `		  aEntRefMethod, SX_ARRAYSIZE(aEntRefMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10232 | `		{ "DOMDocumentType", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10233 | `		  aDocTypeMethod, SX_ARRAYSIZE(aDocTypeMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10234 | `		{ "DOMEntity", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10235 | `		  aEntityMethod, SX_ARRAYSIZE(aEntityMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10236 | `		{ "DOMImplementation", 0, 0, 0,` |
|     - | 10237 | `		  aImplMethod, SX_ARRAYSIZE(aImplMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10238 | `		{ "DOMNotation", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10239 | `		  aNotationMethod, SX_ARRAYSIZE(aNotationMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 10240 | `		/* php's own two: IteratorAggregate (NOT Iterator -- the chunk had the` |
|     - | 10241 | `		 * list carry its own cursor) and Countable. */` |
|     - | 10242 | `		{ "DOMNodeList", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,` |
|     - | 10243 | `		  aListMethod, SX_ARRAYSIZE(aListMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),` |
|     - | 10244 | `		  0, &sDomListIterVtab, 0 },` |
|     - | 10245 | `		{ "DOMNamedNodeMap", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,` |
|     - | 10246 | `		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),` |
|     - | 10247 | `		  0, &sDomMapIterVtab, 0 },` |
|     - | 10248 | `		/* php's own: a class of its OWN, with no parent at all -- a namespace` |
|     - | 10249 | ``		 * declaration is not a DOMNode there, and `$ns instanceof DOMNode` is`` |
|     - | 10250 | `		 * false. Its refusal to serialize is the same soft kind the node` |
|     - | 10251 | `		 * classes carry. */` |
|     - | 10252 | `		{ "DOMNameSpaceNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|     - | 10253 | `		  aNsNodeMethod, SX_ARRAYSIZE(aNsNodeMethod), 0, 0,` |
|     - | 10254 | `		  aNsNodeProp, SX_ARRAYSIZE(aNsNodeProp), 0, 0, 0 },` |
|     - | 10255 | `		{ "DOMXPath", 0, 0, PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|     - | 10256 | `		  aXPathMethod, SX_ARRAYSIZE(aXPathMethod), 0, 0, aXPathProp, SX_ARRAYSIZE(aXPathProp), 0, 0, 0 },` |
|     - | 10257 | `	};` |
|  5745 | 10258 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|  5745 | 10259 | `	if( rc == SXRET_OK ){` |
|     - | 10260 | `		/* The clone hook (ph7_class::xClone, php's clone_obj): stated on every` |
|     - | 10261 | `		 * node class -- rule 29, a hook is per-row and never inherited between` |
|     - | 10262 | `		 * native rows -- and assigned HERE because PH7_NativeClassSpec carries` |
|     - | 10263 | `		 * no field for it. The document's copies the whole document; a user` |
|     - | 10264 | `		 * subclass reaches the nearest ancestor's hook through the engine's` |
|     - | 10265 | `		 * chain walk, php's handler inheritance. */` |
|     - | 10266 | `		static const char * const azNodeClone[] = {` |
|     - | 10267 | `			"DOMNode", "DOMElement", "DOMAttr", "DOMCharacterData", "DOMText",` |
|     - | 10268 | `			"DOMComment", "DOMCdataSection", "DOMProcessingInstruction",` |
|     - | 10269 | `			"DOMDocumentFragment", "DOMEntityReference", "DOMDocumentType"` |
|     - | 10270 | `		};` |
|     - | 10271 | `		sxu32 n;` |
|     - | 10272 | `		ph7_class *pClass;` |
| 68885 | 10273 | `		for( n = 0 ; n < SX_ARRAYSIZE(azNodeClone) ; ++n ){` |
| 94715 | 10274 | `			pClass = PH7_VmExtractClass(&(*pVm),azNodeClone[n],` |
| 63140 | 10275 | `				(sxu32)SyStrlen(azNodeClone[n]),FALSE,0);` |
| 63145 | 10276 | `			if( pClass ){` |
| 63145 | 10277 | `				pClass->xClone = DomInstanceClone;` |
| 31570 | 10278 | `			}` |
| 31575 | 10279 | `		}` |
|  5745 | 10280 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMDocument",sizeof("DOMDocument")-1,FALSE,0);` |
|  5745 | 10281 | `		if( pClass ){` |
|  5745 | 10282 | `			pClass->xClone = DomInstanceCloneDoc;` |
|  2870 | 10283 | `		}` |
|     - | 10284 | `		/* The dimension handlers (ph7_class::xDim, php's read_dimension /` |
|     - | 10285 | `		 * has_dimension), assigned here for the same reason the clone hook is:` |
|     - | 10286 | `		 * PH7_NativeClassSpec carries no field for them, and php's own two` |
|     - | 10287 | `		 * classes wear them without declaring ArrayAccess. */` |
|  5745 | 10288 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMNodeList",sizeof("DOMNodeList")-1,FALSE,0);` |
|  5745 | 10289 | `		if( pClass ){` |
|  5745 | 10290 | `			pClass->xDim = DomListDim;` |
|  2870 | 10291 | `		}` |
|  5745 | 10292 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMNamedNodeMap",sizeof("DOMNamedNodeMap")-1,FALSE,0);` |
|  5745 | 10293 | `		if( pClass ){` |
|  5745 | 10294 | `			pClass->xDim = DomMapDim;` |
|  2870 | 10295 | `		}` |
|  2870 | 10296 | `	}` |
|  5745 | 10297 | `	return rc;` |
|     5 | 10298 | `}` |
|     - | 10299 |  |
|     - | 10300 | `#else` |
|     - | 10301 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|     - | 10302 | `typedef int vm_dom_unused;` |
|     - | 10303 | `#endif /* PH7_ENABLE_LIBXML */` |
|     - | 10304 |  |
