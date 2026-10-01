# src/ph7/vm_dom.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5720/6174 lines (92.65%)

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
|      - |    34 | ` * underlying node always yields the same object.  The cache owns the` |
|      - |    35 | ` * wrappers, which is why DomWrap hands back a BORROWED instance: it stays` |
|      - |    36 | ` * alive as long as its document does.  (The old shape allocated a fresh` |
|      - |    37 | ` * phl_domnode on every navigation step even when the cache then threw the` |
|      - |    38 | ` * result away; only a genuine cache MISS allocates one now.)` |
|      - |    39 | ` *` |
|      - |    40 | ` * Tree surgery (append/insert/replace/remove) is done with manual pointer` |
|      - |    41 | ` * splicing instead of xmlAddChild: xmlAddChild MERGES adjacent text nodes` |
|      - |    42 | ` * and frees the merged-away node, which would dangle any PHP wrapper (and` |
|      - |    43 | ` * violates DOM semantics, which php follows -- appendChild never merges).` |
|      - |    44 | ` * Unlinked nodes are parked on the owning phl_xmldoc's orphan set so they` |
|      - |    45 | ` * are freed with the document at VM reset/release.` |
|      - |    46 | ` */` |
|      - |    47 |  |
|      - |    48 | `/* One native method body. Its receiver's node is DomThisNode(pCtx); apArg is` |
|      - |    49 | ` * php's own argument list, already screened against the declared signature. */` |
|      - |    50 | `#define DOM_METHOD(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |    51 |  |
|      - |    52 | `/* The two slots every wrapper carries, and the document's identity cache. */` |
|      - |    53 | `#define DOM_RES   "__res"` |
|      - |    54 | `#define DOM_DOC   "__doc"` |
|      - |    55 | `#define DOM_NODES "__nodes"` |
|      - |    56 | `/* The base-class => user-class table registerNodeClass writes (defined here` |
|      - |    57 | ` * because the document CLONE, far above it, carries the table across). */` |
|      - |    58 | `#define DOM_NCLS  "__ncls"` |
|      - |    59 |  |
|      - |    60 | `/*` |
|      - |    61 | ` * The DOCUMENT's own directives: php's seven boolean properties.  Their value is` |
|      - |    62 | ` * the extension's own state and not a question about the tree, and php keeps no` |
|      - |    63 | ` * property SLOT for any of them -- each is a read_property/write_property handler,` |
|      - |    64 | `` * which is why php's `(array)` cast and `get_object_vars()` show a DOMDocument as`` |
|      - |    65 | ` * empty.  So the object carries ONE hidden integer here and the class declares the` |
|      - |    66 | ` * seven as VIRTUAL names (PH7_MOD_VIRTUAL) that DomDocProp/DomSetDocProp answer.` |
|      - |    67 | ` * Four of them are read by every parse and one by every refusal, and a clone of a` |
|      - |    68 | ` * document carries the whole word across with the slot itself.` |
|      - |    69 | ` */` |
|      - |    70 | `#define DOM_DFLAGS "__dflags"` |
|      - |    71 | `#define DOM_F_PRESERVE_WS    0x01` |
|      - |    72 | `#define DOM_F_FORMAT_OUTPUT  0x02` |
|      - |    73 | `#define DOM_F_VALIDATE       0x04` |
|      - |    74 | `#define DOM_F_RESOLVE_EXT    0x08` |
|      - |    75 | `#define DOM_F_SUBST_ENT      0x10` |
|      - |    76 | `#define DOM_F_RECOVER        0x20` |
|      - |    77 | `#define DOM_F_STRICT_ERR     0x40` |
|      - |    78 | `/* php's defaults: nothing is validated, expanded, defaulted or recovered unless` |
|      - |    79 | ` * the program asks, whitespace is kept, and a refusal is an exception. */` |
|      - |    80 | `#define DOM_F_DEFAULT (DOM_F_PRESERVE_WS\|DOM_F_STRICT_ERR)` |
|      - |    81 | `static const struct { const char *zName; int iBit; } aDomDocFlag[] = {` |
|      - |    82 | `	{ "preserveWhiteSpace",  DOM_F_PRESERVE_WS   },` |
|      - |    83 | `	{ "formatOutput",        DOM_F_FORMAT_OUTPUT },` |
|      - |    84 | `	{ "validateOnParse",     DOM_F_VALIDATE      },` |
|      - |    85 | `	{ "resolveExternals",    DOM_F_RESOLVE_EXT   },` |
|      - |    86 | `	{ "substituteEntities",  DOM_F_SUBST_ENT     },` |
|      - |    87 | `	{ "recover",             DOM_F_RECOVER       },` |
|      - |    88 | `	{ "strictErrorChecking", DOM_F_STRICT_ERR    }` |
|      - |    89 | `};` |
|      - |    90 | `/* Is this directive on for this document object? Answers false for anything that` |
|      - |    91 | ` * is not one (a node's $__doc points at its own holder when it has no document). */` |
|   9548 |    92 | `static int DomDocFlag(ph7_class_instance *pDoc,int iBit)` |
|      5 |    93 | `{` |
|   9553 |    94 | `	return pDoc != 0 && (PH7_NativeAttrInt(pDoc,DOM_DFLAGS) & iBit) != 0;` |
|      5 |    95 | `}` |
|      - |    96 |  |
|      - |    97 | `/*` |
|      - |    98 | ` * php's DOMException carries the DOM level-2 error CODE beside its sentence --` |
|      - |    99 | `` * `catch (DOMException $e) { if ($e->getCode() === DOM_NOT_FOUND_ERR) ... }` is`` |
|      - |   100 | ` * how a caller tells one refusal from another, and the sentence is only a` |
|      - |   101 | ` * sentence.  Every throw below states its code; DOM_PHP_ERR (0) is php's own` |
|      - |   102 | ` * "not a DOM error" and no refusal here uses it.` |
|      - |   103 | ` */` |
|      - |   104 | `#define DOM_ERR_INDEX_SIZE     1` |
|      - |   105 | `#define DOM_ERR_HIERARCHY      3` |
|      - |   106 | `#define DOM_ERR_WRONG_DOC      4` |
|      - |   107 | `#define DOM_ERR_INVALID_CHAR   5` |
|      - |   108 | `#define DOM_ERR_NO_MOD         7` |
|      - |   109 | `#define DOM_ERR_NOT_FOUND      8` |
|      - |   110 | `#define DOM_ERR_NOT_SUPPORTED  9` |
|      - |   111 | `#define DOM_ERR_INVALID_STATE 11` |
|      - |   112 | `#define DOM_ERR_SYNTAX        12` |
|      - |   113 | `#define DOM_ERR_NAMESPACE     14` |
|      - |   114 | `/* The sentence php prints for each -- so a refusal that travels as a code can` |
|      - |   115 | ` * be raised from one place. */` |
|    906 |   116 | `static const char * DomErrText(int iCode)` |
|      1 |   117 | `{` |
|    907 |   118 | `	switch( iCode ){` |
|     61 |   119 | `	case DOM_ERR_INDEX_SIZE:   return "Index Size Error";` |
|    119 |   120 | `	case DOM_ERR_HIERARCHY:    return "Hierarchy Request Error";` |
|     91 |   121 | `	case DOM_ERR_WRONG_DOC:    return "Wrong Document Error";` |
|    169 |   122 | `	case DOM_ERR_INVALID_CHAR: return "Invalid Character Error";` |
|     37 |   123 | `	case DOM_ERR_NO_MOD:       return "No Modification Allowed Error";` |
|     17 |   124 | `	case DOM_ERR_NOT_SUPPORTED: return "Not Supported Error";` |
|     43 |   125 | `	case DOM_ERR_INVALID_STATE: return "Invalid State Error";` |
|      9 |   126 | `	case DOM_ERR_SYNTAX:       return "Syntax Error";` |
|    261 |   127 | `	case DOM_ERR_NAMESPACE:    return "Namespace Error";` |
|    109 |   128 | `	default:                   return "Not Found Error";` |
|      - |   129 | `	}` |
|    454 |   130 | `}` |
|      - |   131 | `/* Forward: the refusal has to ask the receiver's document for its mode --` |
|      - |   132 | ` * and check that what the slot holds IS a document. */` |
|      - |   133 | `static ph7_class_instance * DomThisDoc(ph7_context *pCtx);` |
|      - |   134 | `static phl_domnode * DomResOf(ph7_class_instance *pObj);` |
|      - |   135 | `/*` |
|      - |   136 | ` * A DOM refusal, in whichever of php's TWO modes the document is in.` |
|      - |   137 | ` *` |
|      - |   138 | `` * `$doc->strictErrorChecking` (true by default) decides whether a refusal is an`` |
|      - |   139 | ` * exception or a warning: with it off, php raises the SAME sentence as an` |
|      - |   140 | ` * E_WARNING under the method's own name and the method answers instead of` |
|      - |   141 | ``  * unwinding. The two answers it gives are the two this file needs -- `false` `` |
|      - |   142 | ` * from a method that returns something, and NOTHING from one php declares` |
|      - |   143 | `` * `void` -- so the mode is one call with the answer as its argument.`` |
|      - |   144 | ` *` |
|      - |   145 | ``  * The flag is document state rather than tree state: it survives a `loadXML()` `` |
|      - |   146 | ` * onto the same object, and a clone carries it. It is read off the RECEIVER's` |
|      - |   147 | ` * document -- the argument's own is not consulted even when the refusal is` |
|      - |   148 | `` * about that argument -- with exactly one exception, `adoptNode`, which reads`` |
|      - |   149 | ` * the argument's and is passed it explicitly.` |
|      - |   150 | ` *` |
|      - |   151 | ` * And not every refusal consults it at all: php passes a hardcoded "strict" at` |
|      - |   152 | `` * `setAttribute` and `toggleAttribute`, which throw whatever the flag says.`` |
|      - |   153 | ` * Those call DomThrowAlways.` |
|      - |   154 | ` */` |
|      - |   155 | `#define DOM_REFUSE_FALSE 0   /* the method answers false */` |
|      - |   156 | `#define DOM_REFUSE_VOID  1   /* the method answers nothing (php declares it void) */` |
|      - |   157 | `/*` |
|      - |   158 | ` * A refusal made while the PROPERTY hook is running.` |
|      - |   159 | ` *` |
|      - |   160 | ``  * The readers and writers below are shared: the same body answers `$el->tagName` `` |
|      - |   161 | ` * and the debug walk, and under the hook it runs on a scratch context inside the` |
|      - |   162 | ` * member opcode. A throw raised there would run the enclosing catch mid-access,` |
|      - |   163 | ` * before the opcode has settled its stack -- which is why PH7_NativePropCtx has` |
|      - |   164 | ` * a refusal channel of its own. Answers 1 when the refusal was RECORDED (the` |
|      - |   165 | ` * opcode raises it where the access lands) and 0 when the caller must throw the` |
|      - |   166 | ` * ordinary way, which is every call made from a method body.` |
|      - |   167 | ` */` |
|    599 |   168 | `static int DomPropRefuse(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zMsg)` |
|      1 |   169 | `{` |
|    600 |   170 | `	PH7_NativePropCtx *pProp = pCtx ? pCtx->pPropCtx : 0;` |
|    600 |   171 | `	if( pProp == 0 ){` |
|    403 |   172 | `		return 0;` |
|      - |   173 | `	}` |
|    198 |   174 | `	pProp->bAnswered = 1;` |
|    198 |   175 | `	pProp->zThrowClass = zClass;` |
|    198 |   176 | `	pProp->iThrowCode = iCode;` |
|    198 |   177 | `	SyBufferFormat(pProp->zThrowMsg,sizeof(pProp->zThrowMsg),"%s",zMsg);` |
|    198 |   178 | `	return 1;` |
|    300 |   179 | `}` |
|      - |   180 | `/* The same, for a refusal whose message is formatted. */` |
|    153 |   181 | `static sxi32 DomPropThrow(ph7_context *pCtx,const char *zClass,sxi32 iCode,` |
|      - |   182 | `	const char *zFormat,...)` |
|      1 |   183 | `{` |
|      - |   184 | `	SyBlob sMsg;` |
|      - |   185 | `	va_list ap;` |
|      - |   186 | `	sxi32 rc;` |
|    154 |   187 | `	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);` |
|    154 |   188 | `	va_start(ap,zFormat);` |
|    154 |   189 | `	SyBlobFormatAp(&sMsg,zFormat,ap);` |
|    154 |   190 | `	va_end(ap);` |
|    154 |   191 | `	SyBlobNullAppend(&sMsg);` |
|    154 |   192 | `	if( DomPropRefuse(pCtx,zClass,iCode,(const char *)SyBlobData(&sMsg)) ){` |
|    154 |   193 | `		rc = PH7_OK;` |
|     77 |   194 | `	}else{` |
|    ! 0 |   195 | `		rc = PH7_VmThrowExceptionCode(pCtx,zClass,iCode,"%s",(const char *)SyBlobData(&sMsg));` |
|      - |   196 | `	}` |
|    154 |   197 | `	SyBlobRelease(&sMsg);` |
|    154 |   198 | `	return rc;` |
|      1 |   199 | `}` |
|    408 |   200 | `static int DomThrowAlways(ph7_context *pCtx,int iCode)` |
|      1 |   201 | `{` |
|    409 |   202 | `	if( DomPropRefuse(pCtx,"DOMException",(sxi32)iCode,DomErrText(iCode)) ){` |
|      7 |   203 | `		return PH7_OK;` |
|      - |   204 | `	}` |
|    403 |   205 | `	return PH7_VmThrowExceptionCode(pCtx,"DOMException",(sxi32)iCode,"%s",DomErrText(iCode));` |
|    205 |   206 | `}` |
|    432 |   207 | `static int DomThrowFor(ph7_context *pCtx,ph7_class_instance *pDoc,int iCode,int iAnswer)` |
|      1 |   208 | `{` |
|      - |   209 | `	/* Only a DOCUMENT carries the flag: a constructed ownerless node's $__doc` |
|      - |   210 | `	 * slot points at its own holder object, and php is always strict there --` |
|      - |   211 | `	 * there is no document to have said otherwise. */` |
|    433 |   212 | `	phl_domnode *pDocNd = pDoc ? DomResOf(pDoc) : 0;` |
|    433 |   213 | `	xmlNodePtr pDocNode = pDocNd ? (xmlNodePtr)pDocNd->pNode : 0;` |
|    432 |   214 | `	if( pDocNode` |
|    406 |   215 | `	 && (pDocNode->type != XML_DOCUMENT_NODE && pDocNode->type != XML_HTML_DOCUMENT_NODE) ){` |
|     15 |   216 | `		pDoc = 0;` |
|      7 |   217 | `	}` |
|    433 |   218 | `	if( pDoc && !DomDocFlag(pDoc,DOM_F_STRICT_ERR) ){` |
|      - |   219 | ``		/* The context prints php's own `Class::method(): ` in front of it. */`` |
|     55 |   220 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,DomErrText(iCode));` |
|     55 |   221 | `		if( iAnswer == DOM_REFUSE_FALSE ){` |
|     37 |   222 | `			ph7_result_bool(pCtx,0);` |
|     18 |   223 | `		}` |
|     55 |   224 | `		return PH7_OK;` |
|      - |   225 | `	}` |
|    379 |   226 | `	return DomThrowAlways(pCtx,iCode);` |
|    217 |   227 | `}` |
|    324 |   228 | `static int DomThrow(ph7_context *pCtx,int iCode)` |
|      1 |   229 | `{` |
|    325 |   230 | `	return DomThrowFor(pCtx,DomThisDoc(pCtx),iCode,DOM_REFUSE_FALSE);` |
|      1 |   231 | `}` |
|     98 |   232 | `static int DomThrowVoid(ph7_context *pCtx,int iCode)` |
|      1 |   233 | `{` |
|     99 |   234 | `	return DomThrowFor(pCtx,DomThisDoc(pCtx),iCode,DOM_REFUSE_VOID);` |
|      1 |   235 | `}` |
|      - |   236 | `/* Property names are byte-exact in php, and every name that reaches here is` |
|      - |   237 | ` * NUL-terminated (ph7_value_to_string null-appends). */` |
|  66464 |   238 | `static int DomNameIs(const char *zName,const char *zWant)` |
|      5 |   239 | `{` |
|  66469 |   240 | `	sxu32 n = (sxu32)SyStrlen(zWant);` |
|  66469 |   241 | `	return SyStrlen(zName) == n && SyStrncmp(zName,zWant,n) == 0;` |
|      5 |   242 | `}` |
|      - |   243 | `/* ...and the ONE name the DOM matches case-insensitively: insertAdjacent*'s` |
|      - |   244 | `` * `$where` word ("BeforeBegin" works), php's zend_string_equals_literal_ci. */`` |
|    130 |   245 | `static int DomNameIsCi(const char *zName,const char *zWant)` |
|      1 |   246 | `{` |
|    131 |   247 | `	sxu32 n = (sxu32)SyStrlen(zWant);` |
|    131 |   248 | `	return SyStrlen(zName) == n && SyStrnicmp(zName,zWant,n) == 0;` |
|      1 |   249 | `}` |
|      - |   250 | `/* The handle behind an instance's $__res, or NULL for anything else. */` |
|  22775 |   251 | `static phl_domnode * DomResOf(ph7_class_instance *pObj)` |
|      5 |   252 | `{` |
|  22780 |   253 | `	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,DOM_RES) : 0;` |
|  22780 |   254 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|    235 |   255 | `		return 0;` |
|      - |   256 | `	}` |
|  22546 |   257 | `	return (phl_domnode *)pVal->x.pOther;` |
|  11394 |   258 | `}` |
|      - |   259 | `/* The receiver of a native method, and the two things every body wants from it. */` |
|  12385 |   260 | `static phl_domnode * DomThisNode(ph7_context *pCtx)` |
|      5 |   261 | `{` |
|  12390 |   262 | `	return DomResOf(PH7_ContextThis(pCtx));` |
|      5 |   263 | `}` |
|   7856 |   264 | `static ph7_class_instance * DomThisDoc(ph7_context *pCtx)` |
|      5 |   265 | `{` |
|   7861 |   266 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   7861 |   267 | `	return pThis ? PH7_NativeAttrObj(pThis,DOM_DOC) : 0;` |
|      5 |   268 | `}` |
|      - |   269 | `/* A fresh handle onto one node of pShell's tree. Freed with the VM allocator. */` |
|   6404 |   270 | `static phl_domnode * DomNewRes(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode)` |
|      5 |   271 | `{` |
|   6409 |   272 | `	phl_domnode *pWrap = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|   6409 |   273 | `	if( pWrap ){` |
|   6409 |   274 | `		pWrap->pShell = pShell;` |
|   6409 |   275 | `		pWrap->pNode = pNode;` |
|   3202 |   276 | `	}` |
|   6409 |   277 | `	return pWrap;` |
|      5 |   278 | `}` |
|      - |   279 | `/* Store a handle in an instance's $__res slot. */` |
|   6340 |   280 | `static void DomSetRes(ph7_vm *pVm,ph7_class_instance *pObj,phl_domnode *pRes)` |
|      5 |   281 | `{` |
|      - |   282 | `	ph7_value sVal;` |
|   6345 |   283 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|   6345 |   284 | `	sVal.x.pOther = pRes;` |
|   6345 |   285 | `	sVal.iFlags = MEMOBJ_RES;` |
|   6345 |   286 | `	PH7_NativeSetProp(&(*pVm),pObj,DOM_RES,sizeof(DOM_RES)-1,&sVal);` |
|      - |   287 | `	/* A handle that IS the document names this object as the tree's document` |
|      - |   288 | `	 * wrapper, so anything holding only the SHELL -- ext/simplexml's` |
|      - |   289 | `	 * dom_import_simplexml() -- can reach the cache the identity rule lives in.` |
|      - |   290 | `	 * Borrowed: DomDocRelease clears it when the object goes. */` |
|   6340 |   291 | `	if( pRes && pRes->pShell && pRes->pNode` |
|   6345 |   292 | `	 && (((xmlNodePtr)pRes->pNode)->type == XML_DOCUMENT_NODE` |
|   4703 |   293 | `	  \|\| ((xmlNodePtr)pRes->pNode)->type == XML_HTML_DOCUMENT_NODE) ){` |
|   3299 |   294 | `		pRes->pShell->pDocObj = (void *)pObj;` |
|   1647 |   295 | `	}` |
|   6345 |   296 | `}` |
|      - |   297 | `/* ph7_class::xRelease for DOMDocument: forget a document object its tree still` |
|      - |   298 | ` * points at. Not a __destruct -- php declares none. */` |
|   1488 |   299 | `static void DomDocRelease(ph7_vm *pVm,ph7_class_instance *pThis)` |
|      1 |   300 | `{` |
|   1489 |   301 | `	ph7_value *pVal = PH7_NativeAttr(pThis,DOM_RES);` |
|   1489 |   302 | `	phl_domnode *pNd = pVal && (pVal->iFlags & MEMOBJ_RES)` |
|   2231 |   303 | `		? (phl_domnode *)pVal->x.pOther : 0;` |
|    744 |   304 | `	(void)pVm;` |
|   1489 |   305 | `	if( pNd && pNd->pShell && pNd->pShell->pDocObj == (void *)pThis ){` |
|   1487 |   306 | `		pNd->pShell->pDocObj = 0;` |
|    743 |   307 | `	}` |
|   1489 |   308 | `}` |
|      - |   309 | `/*` |
|      - |   310 | ` * The document's identity cache, materialized and separated from any copy that` |
|      - |   311 | ` * shares it. Same three moves a native class always needs to own an array slot` |
|      - |   312 | ` * (WeakMap's WmStore is the other one).` |
|      - |   313 | ` */` |
|   5012 |   314 | `static ph7_hashmap * DomCache(ph7_vm *pVm,ph7_class_instance *pDoc)` |
|      5 |   315 | `{` |
|   5017 |   316 | `	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NODES) : 0;` |
|   5017 |   317 | `	if( pSlot == 0 ){` |
|    ! 0 |   318 | `		return 0;` |
|      - |   319 | `	}` |
|   5017 |   320 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    270 |   321 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|    ! 0 |   322 | `			return 0;` |
|      - |   323 | `		}` |
|    134 |   324 | `	}` |
|   5017 |   325 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|   2511 |   326 | `}` |
|      - |   327 | `/* php's class for a node type. Anything else is a plain DOMNode, as before. */` |
|   2752 |   328 | `static const char * DomClassOfKind(int iKind)` |
|      5 |   329 | `{` |
|   2757 |   330 | `	switch( iKind ){` |
|   1891 |   331 | `	case XML_ELEMENT_NODE:       return "DOMElement";` |
|    297 |   332 | `	case XML_ATTRIBUTE_NODE:     return "DOMAttr";` |
|    234 |   333 | `	case XML_TEXT_NODE:          return "DOMText";` |
|     41 |   334 | `	case XML_CDATA_SECTION_NODE: return "DOMCdataSection";` |
|     47 |   335 | `	case XML_COMMENT_NODE:       return "DOMComment";` |
|     45 |   336 | `	case XML_PI_NODE:            return "DOMProcessingInstruction";` |
|     93 |   337 | `	case XML_DOCUMENT_FRAG_NODE: return "DOMDocumentFragment";` |
|     35 |   338 | `	case XML_ENTITY_REF_NODE:    return "DOMEntityReference";` |
|     25 |   339 | `	case XML_DTD_NODE:` |
|     52 |   340 | `	case XML_DOCUMENT_TYPE_NODE: return "DOMDocumentType";` |
|      - |   341 | `	/* php has one class for the whole declaration half of a DTD and hands an` |
|      - |   342 | ``	 * ELEMENT declaration the entity's, which is the class a `$doctype->`` |
|      - |   343 | ``	 * childNodes` walk meets. DOMEntity's own readers ask the node's real`` |
|      - |   344 | `	 * type before touching a field, so an element declaration answers null` |
|      - |   345 | `	 * from each of them rather than reading an xmlElement as an xmlEntity. */` |
|     12 |   346 | `	case XML_ENTITY_DECL:` |
|     25 |   347 | `	case XML_ELEMENT_DECL:       return "DOMEntity";` |
|     12 |   348 | `	case XML_NOTATION_NODE:      return "DOMNotation";` |
|    ! 0 |   349 | `	default:                     return "DOMNode";` |
|      - |   350 | `	}` |
|   1381 |   351 | `}` |
|      - |   352 | `/*` |
|      - |   353 | ` * The nodeType php reports, which is not always libxml's own.` |
|      - |   354 | ` *` |
|      - |   355 | ` * The two numberings were built to agree -- a text node is 3 in both -- but` |
|      - |   356 | ` * libxml parses a DOCTYPE into an XML_DTD_NODE (14) where the DOM's number for` |
|      - |   357 | ` * one is DOCUMENT_TYPE_NODE (10), and php reports the DOM's.  So` |
|      - |   358 | `` * `$n->nodeType === XML_DOCUMENT_TYPE_NODE` -- the way a walk tells the doctype`` |
|      - |   359 | `` * from an element without a `get_class` -- was FALSE here for every document`` |
|      - |   360 | ` * carrying one.` |
|      - |   361 | ` */` |
|    106 |   362 | `static int DomNodeTypeOf(xmlNodePtr pNode)` |
|      1 |   363 | `{` |
|    107 |   364 | `	if( pNode == 0 ){` |
|    ! 0 |   365 | `		return 0;` |
|      - |   366 | `	}` |
|    107 |   367 | `	return pNode->type == XML_DTD_NODE ? (int)XML_DOCUMENT_TYPE_NODE : (int)pNode->type;` |
|     54 |   368 | `}` |
|      - |   369 | `/* Defined with registerNodeClass below, which is the only thing that makes the` |
|      - |   370 | ` * answer anything other than DomClassOfKind's. */` |
|      - |   371 | `static const char * DomWrapClassName(ph7_vm *pVm,ph7_class_instance *pDoc,int iKind,` |
|      - |   372 | `	SyBlob *pOut);` |
|      - |   373 | `/*` |
|      - |   374 | ` * The wrapper object for one node of pDoc's tree -- the same one every time,` |
|      - |   375 | `` * which is what makes `$doc->documentElement === $doc->documentElement` true.`` |
|      - |   376 | ` *` |
|      - |   377 | ` * BORROWED: the cache owns the returned instance. A caller that hands it to PHP` |
|      - |   378 | ` * goes through DomResultWrap (ph7_result_value takes its own reference); a` |
|      - |   379 | ` * caller that stores it uses PH7_NativeSetAttrObj, which does the same. Neither` |
|      - |   380 | ` * unrefs.` |
|      - |   381 | ` */` |
|   4526 |   382 | `static ph7_class_instance * DomWrap(ph7_vm *pVm,ph7_class_instance *pDoc,` |
|      - |   383 | `	phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      5 |   384 | `{` |
|      - |   385 | `	ph7_hashmap *pCache;` |
|   4531 |   386 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |   387 | `	ph7_class_instance *pObj;` |
|      - |   388 | `	ph7_class *pClass;` |
|      - |   389 | `	phl_domnode *pRes;` |
|      - |   390 | `	const char *zClass;` |
|      - |   391 | `	ph7_value sKey,sVal;` |
|      - |   392 | `	SyBlob sName;` |
|   4531 |   393 | `	if( pNode == 0 \|\| pDoc == 0 ){` |
|    141 |   394 | `		return 0;` |
|      - |   395 | `	}` |
|   4391 |   396 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|      - |   397 | `		/* The document is its own wrapper: php answers the SAME DOMDocument. */` |
|     19 |   398 | `		return pDoc;` |
|      - |   399 | `	}` |
|   4373 |   400 | `	pCache = DomCache(&(*pVm),pDoc);` |
|   4373 |   401 | `	if( pCache == 0 ){` |
|    ! 0 |   402 | `		return 0;` |
|      - |   403 | `	}` |
|   4373 |   404 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|   4373 |   405 | `	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|   1641 |   406 | `		ph7_value *pHit = HashmapExtractNodeValue(pEntry);` |
|   1641 |   407 | `		if( pHit && (pHit->iFlags & MEMOBJ_OBJ) ){` |
|   1641 |   408 | `			PH7_MemObjRelease(&sKey);` |
|   1641 |   409 | `			return (ph7_class_instance *)pHit->x.pOther;` |
|      - |   410 | `		}` |
|    ! 0 |   411 | `	}` |
|      - |   412 | `	/* php's class for the kind, unless this document has REGISTERED another` |
|      - |   413 | `	 * one for it (registerNodeClass). The name may live in sName's buffer, so` |
|      - |   414 | `	 * the blob outlives the lookup. */` |
|   2737 |   415 | `	SyBlobInit(&sName,&pVm->sAllocator);` |
|   2737 |   416 | `	zClass = DomWrapClassName(&(*pVm),pDoc,(int)pNode->type,&sName);` |
|   2737 |   417 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|   2737 |   418 | `	SyBlobRelease(&sName);` |
|   2737 |   419 | `	pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|   2737 |   420 | `	pRes = pObj ? DomNewRes(&(*pVm),pShell,pNode) : 0;` |
|   2737 |   421 | `	if( pRes == 0 ){` |
|    ! 0 |   422 | `		if( pObj ){` |
|    ! 0 |   423 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |   424 | `		}` |
|    ! 0 |   425 | `		PH7_MemObjRelease(&sKey);` |
|    ! 0 |   426 | `		return 0;` |
|      - |   427 | `	}` |
|   2737 |   428 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|   2737 |   429 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|   2737 |   430 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|   2737 |   431 | `	sVal.x.pOther = pObj;` |
|   2737 |   432 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|   2737 |   433 | `	PH7_HashmapInsert(pCache,&sKey,&sVal);   /* takes the cache's reference */` |
|   2737 |   434 | `	PH7_MemObjRelease(&sKey);` |
|   2737 |   435 | `	PH7_ClassInstanceUnref(pObj);            /* ...and the cache is now the owner */` |
|   2737 |   436 | `	return pObj;` |
|   2268 |   437 | `}` |
|      - |   438 | `/*` |
|      - |   439 | ` * The wrapper for one node of a tree whose DOCUMENT OBJECT the caller does not` |
|      - |   440 | `` * have -- ext/simplexml's `dom_import_simplexml()`, which holds a shell and a`` |
|      - |   441 | ` * node and nothing else.` |
|      - |   442 | ` *` |
|      - |   443 | `` * The identity rule (`$doc->documentElement === $doc->documentElement`) lives`` |
|      - |   444 | ` * in a cache keyed on the document object, so a tree that has none yet gets one` |
|      - |   445 | ` * built here and remembered on the shell; a tree that came from a DOMDocument` |
|      - |   446 | ` * already names it, which is what makes an import back out of a SimpleXML made` |
|      - |   447 | ` * from that document answer the document's own nodes. BORROWED, like DomWrap's.` |
|      - |   448 | ` */` |
|      8 |   449 | `PH7_PRIVATE ph7_class_instance * PH7_DomWrapForeign(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode)` |
|      1 |   450 | `{` |
|      - |   451 | `	ph7_class_instance *pDoc;` |
|      9 |   452 | `	if( pShell == 0 \|\| pShell->pDoc == 0 ){` |
|    ! 0 |   453 | `		return 0;` |
|      - |   454 | `	}` |
|      9 |   455 | `	pDoc = (ph7_class_instance *)pShell->pDocObj;` |
|      9 |   456 | `	if( pDoc == 0 ){` |
|      3 |   457 | `		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DOMDocument",` |
|      - |   458 | `			sizeof("DOMDocument")-1,FALSE,0);` |
|      - |   459 | `		phl_domnode *pRes;` |
|      3 |   460 | `		pDoc = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|      3 |   461 | `		pRes = pDoc ? DomNewRes(&(*pVm),pShell,pShell->pDoc) : 0;` |
|      3 |   462 | `		if( pRes == 0 ){` |
|    ! 0 |   463 | `			if( pDoc ){` |
|    ! 0 |   464 | `				PH7_ClassInstanceUnref(pDoc);` |
|    ! 0 |   465 | `			}` |
|    ! 0 |   466 | `			return 0;` |
|      - |   467 | `		}` |
|      3 |   468 | `		PH7_NativeSetAttrInt(&(*pVm),pDoc,DOM_DFLAGS,DOM_F_DEFAULT);` |
|      3 |   469 | `		DomSetRes(&(*pVm),pDoc,pRes);          /* ...which records pShell->pDocObj */` |
|      3 |   470 | `		PH7_NativeSetAttrObj(&(*pVm),pDoc,DOM_DOC,pDoc);` |
|      1 |   471 | `	}` |
|      9 |   472 | `	return DomWrap(&(*pVm),pDoc,pShell,(xmlNodePtr)pNode);` |
|      5 |   473 | `}` |
|      - |   474 | `/* Answer a borrowed instance (or NULL) from a native method. */` |
|   4092 |   475 | `static int DomResultWrap(ph7_context *pCtx,ph7_class_instance *pObj)` |
|      5 |   476 | `{` |
|      - |   477 | `	ph7_value sRes;` |
|   4097 |   478 | `	if( pObj == 0 ){` |
|     77 |   479 | `		ph7_result_null(pCtx);` |
|     77 |   480 | `		return PH7_OK;` |
|      - |   481 | `	}` |
|   4021 |   482 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|   4021 |   483 | `	sRes.x.pOther = pObj;` |
|   4021 |   484 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|   4021 |   485 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|   4021 |   486 | `	return PH7_OK;` |
|   2051 |   487 | `}` |
|      - |   488 | `/* The common tail: wrap a node of the RECEIVER's document and answer it. */` |
|   3754 |   489 | `static int DomResultNodeOf(ph7_context *pCtx,phl_domnode *pNd,xmlNodePtr pNode)` |
|      5 |   490 | `{` |
|   3759 |   491 | `	if( pNd == 0 \|\| pNode == 0 ){` |
|    270 |   492 | `		ph7_result_null(pCtx);` |
|    270 |   493 | `		return PH7_OK;` |
|      - |   494 | `	}` |
|   3491 |   495 | `	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNode));` |
|   1882 |   496 | `}` |
|      - |   497 | `/* The phl_domnode behind a DOMNode-typed ARGUMENT (already screened by ZPP). */` |
|   1850 |   498 | `static phl_domnode * DomObjArg(ph7_value *pVal)` |
|      3 |   499 | `{` |
|   1853 |   500 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      5 |   501 | `		return 0;` |
|      - |   502 | `	}` |
|   1849 |   503 | `	return DomResOf((ph7_class_instance *)pVal->x.pOther);` |
|    928 |   504 | `}` |
|      - |   505 | `/* ...and the DOCUMENT object it belongs to, which is where its wrapper is` |
|      - |   506 | `` * cached and what its `ownerDocument` answers. */`` |
|    598 |   507 | `static ph7_class_instance * DomObjArgDoc(ph7_value *pVal)` |
|      3 |   508 | `{` |
|    601 |   509 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |   510 | `		return 0;` |
|      - |   511 | `	}` |
|    601 |   512 | `	return PH7_NativeAttrObj((ph7_class_instance *)pVal->x.pOther,DOM_DOC);` |
|    302 |   513 | `}` |
|      - |   514 | `/* The slot a DOMNameSpaceNode carries beside its own two: the element that` |
|      - |   515 | ` * MAKES the declaration, which is php's parentNode for one. */` |
|      - |   516 | `#define DOM_NS_OWNER "__owner"` |
|      - |   517 |  |
|      - |   518 | `/* The element a DOMNameSpaceNode argument was found on, or NULL for anything` |
|      - |   519 | ` * else -- the slot exists on that class alone. */` |
|     16 |   520 | `static phl_domnode * DomNsNodeOwner(ph7_value *pVal)` |
|      1 |   521 | `{` |
|      - |   522 | `	ph7_class_instance *pObj;` |
|     17 |   523 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |   524 | `		return 0;` |
|      - |   525 | `	}` |
|     17 |   526 | `	pObj = (ph7_class_instance *)pVal->x.pOther;` |
|     17 |   527 | `	return DomResOf(PH7_NativeAttrObj(pObj,DOM_NS_OWNER));` |
|      9 |   528 | `}` |
|      - |   529 | `/* Orphan bookkeeping: nodes not linked into their tree but still owned */` |
|   1358 |   530 | `static void DomOrphanAdd(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      3 |   531 | `{` |
|   1361 |   532 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|      - |   533 | `	sxu32 n;` |
|  15685 |   534 | `	for( n = 0 ; n < SySetUsed(&pShell->aOrphans) ; ++n ){` |
|  14327 |   535 | `		if( apOrphan[n] == pNode ){` |
|    ! 0 |   536 | `			return;` |
|      - |   537 | `		}` |
|   7165 |   538 | `	}` |
|   1361 |   539 | `	SySetPut(&pShell->aOrphans,(const void *)&pNode);` |
|    682 |   540 | `}` |
|    516 |   541 | `static void DomOrphanRemove(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      3 |   542 | `{` |
|    519 |   543 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|    519 |   544 | `	sxu32 n,nUsed = SySetUsed(&pShell->aOrphans);` |
|   4353 |   545 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|   4353 |   546 | `		if( apOrphan[n] == pNode ){` |
|    519 |   547 | `			apOrphan[n] = apOrphan[nUsed-1];` |
|    519 |   548 | `			SySetTruncate(&pShell->aOrphans,nUsed-1);` |
|    519 |   549 | `			return;` |
|      - |   550 | `		}` |
|   1918 |   551 | `	}` |
|    261 |   552 | `}` |
|      - |   553 | `/* Detach a node from wherever it is (tree or orphan set) prior to linking */` |
|    598 |   554 | `static void DomDetach(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      3 |   555 | `{` |
|    601 |   556 | `	if( pNode->parent ){` |
|    171 |   557 | `		xmlUnlinkNode(pNode);` |
|     86 |   558 | `	}else{` |
|    431 |   559 | `		DomOrphanRemove(pShell,pNode);` |
|      - |   560 | `	}` |
|    601 |   561 | `}` |
|      - |   562 | `/* Raw child-list splicing (no text-node merging -- DOM/php semantics) */` |
|    494 |   563 | `static void DomLinkLast(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      2 |   564 | `{` |
|    496 |   565 | `	pChild->parent = pParent;` |
|    496 |   566 | `	pChild->next = 0;` |
|    496 |   567 | `	if( pParent->last ){` |
|    153 |   568 | `		pParent->last->next = pChild;` |
|    153 |   569 | `		pChild->prev = pParent->last;` |
|     77 |   570 | `	}else{` |
|    344 |   571 | `		pParent->children = pChild;` |
|    344 |   572 | `		pChild->prev = 0;` |
|      - |   573 | `	}` |
|    496 |   574 | `	pParent->last = pChild;` |
|    496 |   575 | `}` |
|     78 |   576 | `static void DomLinkBefore(xmlNodePtr pParent,xmlNodePtr pChild,xmlNodePtr pRef)` |
|      1 |   577 | `{` |
|     79 |   578 | `	pChild->parent = pParent;` |
|     79 |   579 | `	pChild->next = pRef;` |
|     79 |   580 | `	pChild->prev = pRef->prev;` |
|     79 |   581 | `	if( pRef->prev ){` |
|     41 |   582 | `		pRef->prev->next = pChild;` |
|     21 |   583 | `	}else{` |
|     39 |   584 | `		pParent->children = pChild;` |
|      - |   585 | `	}` |
|     79 |   586 | `	pRef->prev = pChild;` |
|     79 |   587 | `}` |
|      - |   588 |  |
|      - |   589 | `/* ===== Node introspection: the readers behind __get ===== */` |
|      - |   590 |  |
|      - |   591 | `/* php's nodeName rules */` |
|    668 |   592 | `static void DomNodeName(ph7_context *pCtx,xmlNodePtr pNode)` |
|      4 |   593 | `{` |
|    672 |   594 | `	if( pNode == 0 ){` |
|      3 |   595 | `		ph7_result_string(pCtx,"",0);` |
|      3 |   596 | `		return;` |
|      - |   597 | `	}` |
|    670 |   598 | `	switch( pNode->type ){` |
|     13 |   599 | `	case XML_TEXT_NODE:          ph7_result_string(pCtx,"#text",(int)sizeof("#text")-1); break;` |
|      7 |   600 | `	case XML_CDATA_SECTION_NODE: ph7_result_string(pCtx,"#cdata-section",(int)sizeof("#cdata-section")-1); break;` |
|      5 |   601 | `	case XML_COMMENT_NODE:       ph7_result_string(pCtx,"#comment",(int)sizeof("#comment")-1); break;` |
|      8 |   602 | `	case XML_HTML_DOCUMENT_NODE:` |
|     17 |   603 | `	case XML_DOCUMENT_NODE:      ph7_result_string(pCtx,"#document",(int)sizeof("#document")-1); break;` |
|      7 |   604 | `	case XML_DOCUMENT_FRAG_NODE: ph7_result_string(pCtx,"#document-fragment",(int)sizeof("#document-fragment")-1); break;` |
|    311 |   605 | `	default:` |
|    622 |   606 | `		if( (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE)` |
|    609 |   607 | `			&& pNode->ns && pNode->ns->prefix ){` |
|    229 |   608 | `			ph7_result_string_format(pCtx,"%s:%s",(const char *)pNode->ns->prefix,(const char *)pNode->name);` |
|    115 |   609 | `		}else{` |
|    398 |   610 | `			ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|      - |   611 | `		}` |
|    622 |   612 | `		break;` |
|      - |   613 | `	}` |
|    338 |   614 | `}` |
|      - |   615 | `/*` |
|      - |   616 | ` * The three names a namespaced document reads on every node.` |
|      - |   617 | ` *` |
|      - |   618 | `` * `namespaceURI` and `localName` are php's `?string` -- null for a node that`` |
|      - |   619 | ` * cannot carry a name in a namespace at all (a text node, a comment, a PI, the` |
|      - |   620 | `` * document itself) -- while `prefix` is a plain `string` that answers "" there,`` |
|      - |   621 | ` * which is why one of the three cannot be derived from the other two.` |
|      - |   622 | ` *` |
|      - |   623 | ` * All three are gated on the node KIND before anything is read, which is not` |
|      - |   624 | ` * only php's rule but a memory-safety one: only element and attribute nodes are` |
|      - |   625 | ``  * xmlNode/xmlAttr-shaped, and `->ns` on an xmlDoc aliases its `compression` `` |
|      - |   626 | ` * int, on an xmlDtd its notation table.  Reading it there and dereferencing the` |
|      - |   627 | `` * result is a SIGSEGV out of `$doc->namespaceURI` -- an ordinary property read.`` |
|      - |   628 | ` */` |
|    602 |   629 | `static int DomHasNsSlot(xmlNodePtr pNode)` |
|      1 |   630 | `{` |
|    603 |   631 | `	return pNode != 0` |
|    903 |   632 | `		&& (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE);` |
|      1 |   633 | `}` |
|    328 |   634 | `static void DomNamespaceUri(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |   635 | `{` |
|    329 |   636 | `	if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->href ){` |
|    227 |   637 | `		ph7_result_string(pCtx,(const char *)pNode->ns->href,-1);` |
|    114 |   638 | `	}else{` |
|    103 |   639 | `		ph7_result_null(pCtx);` |
|      - |   640 | `	}` |
|    329 |   641 | `}` |
|    126 |   642 | `static void DomPrefix(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |   643 | `{` |
|    127 |   644 | `	if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->prefix ){` |
|     67 |   645 | `		ph7_result_string(pCtx,(const char *)pNode->ns->prefix,-1);` |
|     34 |   646 | `	}else{` |
|     61 |   647 | `		ph7_result_string(pCtx,"",0);` |
|      - |   648 | `	}` |
|    127 |   649 | `}` |
|    124 |   650 | `static void DomLocalName(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |   651 | `{` |
|    125 |   652 | `	if( DomHasNsSlot(pNode) ){` |
|    107 |   653 | `		ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|     54 |   654 | `	}else{` |
|     19 |   655 | `		ph7_result_null(pCtx);` |
|      - |   656 | `	}` |
|    125 |   657 | `}` |
|      - |   658 | ``/* php's `isConnected`: is the node's root the DOCUMENT? A node built by a`` |
|      - |   659 | `` * create* factory carries the document as its `ownerDocument` from birth, so`` |
|      - |   660 | ` * that property cannot answer this and a program testing it reads true for a` |
|      - |   661 | ` * node it has not appended yet. */` |
|    102 |   662 | `static int DomIsConnected(xmlNodePtr pNode)` |
|      1 |   663 | `{` |
|    103 |   664 | `	xmlNodePtr pRoot = pNode;` |
|    103 |   665 | `	if( pRoot == 0 ){` |
|    ! 0 |   666 | `		return 0;` |
|      - |   667 | `	}` |
|    191 |   668 | `	while( pRoot->parent ){` |
|     89 |   669 | `		pRoot = pRoot->parent;` |
|      1 |   670 | `	}` |
|    103 |   671 | `	return pRoot->type == XML_DOCUMENT_NODE \|\| pRoot->type == XML_HTML_DOCUMENT_NODE;` |
|     52 |   672 | `}` |
|      - |   673 | `/*` |
|      - |   674 | ` * php's nodeValue, which is null for every node kind that has no value of its` |
|      - |   675 | ` * own -- the document, a doctype, a fragment, an entity DECLARATION and an` |
|      - |   676 | `` * entity REFERENCE all answer null, where `textContent` on the same node walks`` |
|      - |   677 | ` * its children and answers a string. (The element case is php's own` |
|      - |   678 | ` * convenience: DOM says an element has no node value.)` |
|      - |   679 | ` */` |
|    274 |   680 | `static void DomNodeValue(ph7_context *pCtx,xmlNodePtr pNode)` |
|      3 |   681 | `{` |
|      - |   682 | `	xmlChar *zContent;` |
|    277 |   683 | `	if( pNode == 0 ){` |
|    ! 0 |   684 | `		ph7_result_null(pCtx);` |
|    ! 0 |   685 | `		return;` |
|      - |   686 | `	}` |
|    277 |   687 | `	switch( pNode->type ){` |
|     83 |   688 | `	case XML_TEXT_NODE:` |
|      - |   689 | `	case XML_COMMENT_NODE:` |
|      - |   690 | `	case XML_CDATA_SECTION_NODE:` |
|      - |   691 | `	case XML_PI_NODE:` |
|      - |   692 | `		/* The CONTENT POINTER itself, not xmlNodeGetContent's copy: a null` |
|      - |   693 | `		 * pointer -- the omitted-argument constructor's state -- reads NULL` |
|      - |   694 | `		 * where an empty string reads "", and newer libxml's` |
|      - |   695 | `		 * xmlNodeGetContent papers over exactly that difference (2.13 answers` |
|      - |   696 | `		 * "" for both, 2.9 answers NULL for the pointer). */` |
|    168 |   697 | `		if( pNode->content == 0 ){` |
|     15 |   698 | `			ph7_result_null(pCtx);` |
|      8 |   699 | `		}else{` |
|    154 |   700 | `			ph7_result_string(pCtx,(const char *)pNode->content,-1);` |
|      - |   701 | `		}` |
|    168 |   702 | `		return;` |
|     38 |   703 | `	case XML_ATTRIBUTE_NODE:` |
|      - |   704 | `	case XML_ELEMENT_NODE:` |
|     78 |   705 | `		break;` |
|     16 |   706 | `	default:` |
|     33 |   707 | `		ph7_result_null(pCtx);` |
|     33 |   708 | `		return;` |
|      - |   709 | `	}` |
|     78 |   710 | `	zContent = xmlNodeGetContent(pNode);` |
|     78 |   711 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|     78 |   712 | `	if( zContent ){` |
|     78 |   713 | `		xmlFree(zContent);` |
|     38 |   714 | `	}` |
|    140 |   715 | `}` |
|      - |   716 | ``/* The `data` property's reading of the same content: php COERCES there, so a`` |
|      - |   717 | ` * NULL content pointer -- the omitted-argument constructors' state -- reads ""` |
|      - |   718 | `` * from `$node->data` and null from `$node->nodeValue`, one node, two answers. */`` |
|    126 |   719 | `static void DomDataValue(ph7_context *pCtx,xmlNodePtr pNode)` |
|      2 |   720 | `{` |
|    128 |   721 | `	DomNodeValue(pCtx,pNode);` |
|    128 |   722 | `	if( pCtx->pRet->iFlags & MEMOBJ_NULL ){` |
|      7 |   723 | `		ph7_result_string(pCtx,"",0);` |
|      3 |   724 | `	}` |
|    128 |   725 | `}` |
|      - |   726 | `/* php's textContent: the same walk, but a document answers its text too */` |
|     56 |   727 | `static void DomTextContent(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |   728 | `{` |
|     57 |   729 | `	xmlChar *zContent = pNode ? xmlNodeGetContent(pNode) : 0;` |
|     57 |   730 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|     57 |   731 | `	if( zContent ){` |
|     49 |   732 | `		xmlFree(zContent);` |
|     24 |   733 | `	}` |
|     57 |   734 | `}` |
|      - |   735 | `/* The two child counts childNodes->length and childElementCount read. */` |
|      - |   736 | `static xmlNodePtr DomRefChildren(xmlNodePtr pNode);` |
|    158 |   737 | `static int DomChildCount(xmlNodePtr pNode,int bElementsOnly)` |
|      2 |   738 | `{` |
|    160 |   739 | `	xmlNodePtr pChild = DomRefChildren(pNode);` |
|    160 |   740 | `	int iCount = 0;` |
|    396 |   741 | `	for( ; pChild ; pChild = pChild->next ){` |
|    238 |   742 | `		if( !bElementsOnly \|\| pChild->type == XML_ELEMENT_NODE ){` |
|    216 |   743 | `			iCount++;` |
|    107 |   744 | `		}` |
|    120 |   745 | `	}` |
|    160 |   746 | `	return iCount;` |
|      2 |   747 | `}` |
|    330 |   748 | `static xmlNodePtr DomChildAt(xmlNodePtr pNode,int iWant)` |
|      2 |   749 | `{` |
|    332 |   750 | `	xmlNodePtr pChild = DomRefChildren(pNode);` |
|    886 |   751 | `	for( ; pChild && iWant > 0 ; pChild = pChild->next ){` |
|    556 |   752 | `		iWant--;` |
|    279 |   753 | `	}` |
|    332 |   754 | `	return pChild;` |
|      2 |   755 | `}` |
|      - |   756 |  |
|      - |   757 | `/* ===== Tree surgery: DOMNode's four mutators ===== */` |
|      - |   758 |  |
|      - |   759 | `/*` |
|      - |   760 | ` * Defined with the namespace machinery below, and declared here because the` |
|      - |   761 | ` * surgery runs it: a node LINKED into a tree loses the declarations its new` |
|      - |   762 | ` * scope already makes, and gains the ones its new scope no longer makes.` |
|      - |   763 | ` * (The namespace section cannot move up because it reads the attribute` |
|      - |   764 | ` * walker; DomDropChildren below is with the property-write machinery it was` |
|      - |   765 | ` * built for, and replaceChildren() runs the same wrapper-preserving drop.)` |
|      - |   766 | ` */` |
|      - |   767 | `static void DomNsOnInsertEx(xmlNodePtr pNode,int bDeep);` |
|      - |   768 | `static void DomDropChildren(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode);` |
|      - |   769 | `/* The attribute machinery, defined with the attribute surface below: the four` |
|      - |   770 | ` * mutators reach it because php's appendChild/insertBefore ATTACH an attribute` |
|      - |   771 | ` * argument as a property rather than splicing it among the children. */` |
|      - |   772 | `static xmlAttrPtr DomAttrByLocal(xmlNodePtr pElem,const char *zName);` |
|      - |   773 | `static xmlAttrPtr DomAttrByNs(xmlNodePtr pElem,const xmlChar *zUri,const char *zLocal);` |
|      - |   774 | `static void DomAttrLinkLast(xmlNodePtr pElem,xmlAttrPtr pAttr);` |
|      - |   775 | `static void DomAttrLinkBefore(xmlNodePtr pElem,xmlAttrPtr pAttr,xmlAttrPtr pRef);` |
|      - |   776 | `static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr);` |
|      - |   777 | `static void DomNsAttrArrive(xmlNodePtr pElem,xmlAttrPtr pAttr);` |
|      - |   778 | `/* The adoptNode wrapper machinery, defined with it below: the insertion doors` |
|      - |   779 | ` * run it too, because php ADOPTS a constructed, ownerless argument -- doc,` |
|      - |   780 | ` * identity-cache home and handle shell all move on the first insertion. */` |
|      - |   781 | `static void DomAdoptWrappers(ph7_vm *pVm,ph7_class_instance *pSrcDoc,` |
|      - |   782 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode);` |
|      - |   783 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot);` |
|      - |   784 | `/*` |
|      - |   785 | ` * An entity REFERENCE's children as php answers them. libxml's re-homing` |
|      - |   786 | ` * CLEARS the raw link when a constructed reference is adopted -- and php's` |
|      - |   787 | ` * raw state stays cleared, which replaceChild's childless-false cell measures` |
|      - |   788 | `` * -- but php's READERS still resolve: `$ref->firstChild` answers the NEW`` |
|      - |   789 | ` * document's declaration for the name, or the predefined five, or nothing.` |
|      - |   790 | ` */` |
|   1080 |   791 | `static xmlNodePtr DomRefChildren(xmlNodePtr pNode)` |
|      4 |   792 | `{` |
|   1080 |   793 | `	if( pNode && pNode->type == XML_ENTITY_REF_NODE` |
|    557 |   794 | `	 && pNode->children == 0 && pNode->doc ){` |
|      6 |   795 | `		return (xmlNodePtr)xmlGetDocEntity(pNode->doc,pNode->name);` |
|      - |   796 | `	}` |
|   1079 |   797 | `	return pNode ? pNode->children : 0;` |
|    544 |   798 | `}` |
|      - |   799 | `/*` |
|      - |   800 | ` * Take an OWNERLESS subtree into the receiver's world, php's constructed-node` |
|      - |   801 | ` * adoption: the libxml nodes get the receiver's document (none of their` |
|      - |   802 | ` * strings are dict-interned -- a constructed node's are plain allocations, so` |
|      - |   803 | ` * xmlSetTreeDoc is the whole move), the orphan entry crosses from the limbo` |
|      - |   804 | ` * shell to the receiver's, and every wrapper PHP holds re-homes into the` |
|      - |   805 | ` * receiver's identity cache.  Also the OWNERLESS-to-OWNERLESS merge, where no` |
|      - |   806 | ` * document changes hands but the wrappers still need ONE holder for` |
|      - |   807 | `` * `$a->firstChild === $b` to hold.  The caller has already screened documents:`` |
|      - |   808 | ` * a mismatch here means the argument's is NULL.` |
|      - |   809 | ` */` |
|    462 |   810 | `static void DomAdoptIntoRecv(ph7_context *pCtx,ph7_value *pArgVal,phl_domnode *pArgNd)` |
|      3 |   811 | `{` |
|    465 |   812 | `	ph7_vm *pVm = pCtx->pVm;` |
|    465 |   813 | `	ph7_class_instance *pSrcHolder = DomObjArgDoc(pArgVal);` |
|    465 |   814 | `	ph7_class_instance *pDstHolder = DomThisDoc(pCtx);` |
|    465 |   815 | `	phl_domnode *pRecv = DomThisNode(pCtx);` |
|    465 |   816 | `	xmlNodePtr pNode = (xmlNodePtr)pArgNd->pNode;` |
|    465 |   817 | `	xmlNodePtr pRecvNode = pRecv ? (xmlNodePtr)pRecv->pNode : 0;` |
|    465 |   818 | `	if( pSrcHolder == pDstHolder \|\| pRecvNode == 0 ){` |
|    433 |   819 | `		return;` |
|      - |   820 | `	}` |
|     33 |   821 | `	if( pNode->doc == 0 && pRecvNode->doc ){` |
|     29 |   822 | `		xmlSetTreeDoc(pNode,pRecvNode->doc);` |
|     14 |   823 | `	}` |
|     33 |   824 | `	if( pArgNd->pShell != pRecv->pShell ){` |
|     29 |   825 | `		DomOrphanRemove(pArgNd->pShell,pNode);` |
|     29 |   826 | `		DomOrphanAdd(pRecv->pShell,pNode);` |
|     29 |   827 | `		pArgNd->pShell = pRecv->pShell;` |
|     14 |   828 | `	}` |
|     33 |   829 | `	DomAdoptWrappers(pVm,pSrcHolder,pDstHolder,pRecv->pShell,pNode);` |
|    234 |   830 | `}` |
|      - |   831 |  |
|      - |   832 | `/*` |
|      - |   833 | ` * php's dom_node_children_valid: the node kinds that can never have children.` |
|      - |   834 | ` * A level-2 mutator on such a receiver answers FALSE with nothing said at all` |
|      - |   835 | ` * -- no warning, no exception -- and answers it BEFORE any other screen, so` |
|      - |   836 | `` * `$text->appendChild($nodeFromAnotherDocument)` is false, not Wrong Document.`` |
|      - |   837 | ` */` |
|   1148 |   838 | `static int DomChildrenValid(xmlNodePtr pNode)` |
|      4 |   839 | `{` |
|   1152 |   840 | `	switch( pNode->type ){` |
|     14 |   841 | `	case XML_TEXT_NODE:` |
|      - |   842 | `	case XML_CDATA_SECTION_NODE:` |
|      - |   843 | `	case XML_PI_NODE:` |
|      - |   844 | `	case XML_COMMENT_NODE:` |
|      - |   845 | `	case XML_DOCUMENT_TYPE_NODE:` |
|      - |   846 | `	case XML_DTD_NODE:` |
|      - |   847 | `	case XML_NOTATION_NODE:` |
|     29 |   848 | `		return 0;` |
|    560 |   849 | `	default:` |
|   1124 |   850 | `		return 1;` |
|      - |   851 | `	}` |
|    578 |   852 | `}` |
|      - |   853 | `/*` |
|      - |   854 | `` * The two ends of the child list php's `firstChild`/`lastChild` answer, and`` |
|      - |   855 | `` * what `hasChildNodes()` asks -- all three through the same screen, so the`` |
|      - |   856 | ` * DOCTYPE (whose declarations libxml really does link as children) answers` |
|      - |   857 | ` * null, null and false the way php's do.` |
|      - |   858 | ` */` |
|    592 |   859 | `static xmlNodePtr DomNodeChildFirst(xmlNodePtr pNode)` |
|      3 |   860 | `{` |
|    595 |   861 | `	if( pNode == 0 \|\| !DomChildrenValid(pNode) ){` |
|      7 |   862 | `		return 0;` |
|      - |   863 | `	}` |
|    589 |   864 | `	return DomRefChildren(pNode);` |
|    299 |   865 | `}` |
|     86 |   866 | `static xmlNodePtr DomNodeChildLast(xmlNodePtr pNode)` |
|      1 |   867 | `{` |
|     87 |   868 | `	if( pNode == 0 \|\| !DomChildrenValid(pNode) ){` |
|      3 |   869 | `		return 0;` |
|      - |   870 | `	}` |
|     85 |   871 | `	return pNode->last ? pNode->last : DomRefChildren(pNode);` |
|     44 |   872 | `}` |
|      - |   873 | `/*` |
|      - |   874 | ` * php's dom_node_is_read_only: the DTD-owned kinds -- an entity reference's` |
|      - |   875 | ` * subtree is the entity's, shared by every reference to it -- and, one clause` |
|      - |   876 | `` * later, a node with NO document: a constructed `new DOMText('t')` that was`` |
|      - |   877 | ` * never adopted refuses the level-2 child-list doors with No Modification` |
|      - |   878 | ` * Allowed where the modern variadic family compares documents instead.` |
|      - |   879 | ` */` |
|    610 |   880 | `static int DomNodeReadOnly(xmlNodePtr pNode)` |
|      3 |   881 | `{` |
|    613 |   882 | `	switch( pNode->type ){` |
|      5 |   883 | `	case XML_ENTITY_REF_NODE:` |
|      - |   884 | `	case XML_ENTITY_NODE:` |
|      - |   885 | `	case XML_DOCUMENT_TYPE_NODE:` |
|      - |   886 | `	case XML_NOTATION_NODE:` |
|      - |   887 | `	case XML_DTD_NODE:` |
|      - |   888 | `	case XML_ELEMENT_DECL:` |
|      - |   889 | `	case XML_ATTRIBUTE_DECL:` |
|      - |   890 | `	case XML_ENTITY_DECL:` |
|     11 |   891 | `		return 1;` |
|    300 |   892 | `	default:` |
|    603 |   893 | `		return pNode->doc == 0;` |
|      - |   894 | `	}` |
|    308 |   895 | `}` |
|      - |   896 | `/* The two screens every level-2 mutator opens with, in php's order: an` |
|      - |   897 | ` * invalid-children receiver answers false in silence, then the read-only` |
|      - |   898 | ` * refusal -- the receiver's own, or that of the parent the CHILD would be` |
|      - |   899 | ` * taken from. Returns non-zero when the caller must stop (result already` |
|      - |   900 | ` * set). */` |
|    428 |   901 | `static int DomMutatorScreen(ph7_context *pCtx,xmlNodePtr pParent,xmlNodePtr pChild,int *pRc)` |
|      3 |   902 | `{` |
|    431 |   903 | `	if( !DomChildrenValid(pParent) ){` |
|     19 |   904 | `		ph7_result_bool(pCtx,0);` |
|     19 |   905 | `		*pRc = PH7_OK;` |
|     19 |   906 | `		return 1;` |
|      - |   907 | `	}` |
|    410 |   908 | `	if( DomNodeReadOnly(pParent)` |
|    408 |   909 | `	 \|\| (pChild->parent && DomNodeReadOnly(pChild->parent)) ){` |
|     11 |   910 | `		*pRc = DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|     11 |   911 | `		return 1;` |
|      - |   912 | `	}` |
|    403 |   913 | `	return 0;` |
|    217 |   914 | `}` |
|      - |   915 | `/*` |
|      - |   916 | ` * The attribute HALF of appendChild/insertBefore: php hands an attribute` |
|      - |   917 | ` * argument to xmlAddChild, which attaches it as a PROPERTY -- so` |
|      - |   918 | `` * `$el->appendChild($attr)` is a spelling of setAttributeNode, not a child`` |
|      - |   919 | `` * splice (the chunk spliced it among the children and serialized `<r> k=""`,`` |
|      - |   920 | ` * bytes that are not XML). The receiver must be an ELEMENT: a document, a` |
|      - |   921 | ` * fragment or an attribute answers the Hierarchy refusal. An existing` |
|      - |   922 | ``  * attribute of the same name (libxml's name-only match, so a plain `k` `` |
|      - |   923 | ` * displaces a namespaced one -- the setAttributeNode rule) is displaced` |
|      - |   924 | ` * UNLESS it is the argument itself, and the argument always (re)enters at the` |
|      - |   925 | ` * tail of the property list, which is observable: appending an element's own` |
|      - |   926 | ` * first attribute moves it last.` |
|      - |   927 | ` *` |
|      - |   928 | ` * One deliberate divergence, recorded in §7.4: php FREES the displaced` |
|      - |   929 | ` * attribute, so a wrapper held across the call answers Invalid State from` |
|      - |   930 | ` * every later read ("Couldn't fetch DOMAttr" from a method). PHL parks it` |
|      - |   931 | ` * detached and alive -- the same after-state setAttributeNode leaves.` |
|      - |   932 | ` */` |
|     16 |   933 | `static int DomMutatorAttrAttach(ph7_context *pCtx,phl_domnode *pPar,phl_domnode *pChd,` |
|      - |   934 | `	ph7_value *pArg)` |
|      2 |   935 | `{` |
|     18 |   936 | `	xmlNodePtr pElem = (xmlNodePtr)pPar->pNode;` |
|     18 |   937 | `	xmlAttrPtr pAttr = (xmlAttrPtr)pChd->pNode;` |
|      - |   938 | `	xmlAttrPtr pOld;` |
|     18 |   939 | `	if( pElem->type != XML_ELEMENT_NODE ){` |
|    ! 0 |   940 | `		return DomThrow(pCtx,DOM_ERR_HIERARCHY);` |
|      - |   941 | `	}` |
|     11 |   942 | `	pOld = pAttr->ns ? DomAttrByNs(pElem,pAttr->ns->href,(const char *)pAttr->name)` |
|     15 |   943 | `	                 : DomAttrByLocal(pElem,(const char *)pAttr->name);` |
|     18 |   944 | `	if( pOld && pOld != pAttr ){` |
|     10 |   945 | `		xmlUnlinkNode((xmlNodePtr)pOld);` |
|     10 |   946 | `		DomOrphanAdd(pPar->pShell,(xmlNodePtr)pOld);` |
|      4 |   947 | `	}` |
|     18 |   948 | `	DomAttrDetach(pChd->pShell,pAttr);` |
|     18 |   949 | `	DomAttrLinkLast(pElem,pAttr);` |
|     18 |   950 | `	DomNsAttrArrive(pElem,pAttr);` |
|     18 |   951 | `	ph7_result_value(pCtx,pArg);` |
|     18 |   952 | `	return PH7_OK;` |
|     10 |   953 | `}` |
|      - |   954 |  |
|      - |   955 | `/*` |
|      - |   956 | ` * php's refusal taxonomy for linking pChild under pParent, or NULL when the` |
|      - |   957 | ` * link is allowed. The chunk collapsed all of it into one message per method,` |
|      - |   958 | ` * which cost more than a wording: nothing rejected making a node its own` |
|      - |   959 | `` * DESCENDANT, so `$a->firstChild->appendChild($a)` spliced a CYCLE into the`` |
|      - |   960 | ` * tree and every later walk of it ran away.` |
|      - |   961 | ` */` |
|      - |   962 | `/*` |
|      - |   963 | ` * The refusal is in TWO halves because php's empty-fragment answer sits` |
|      - |   964 | ` * between them: a foreign empty fragment is Wrong Document, an empty fragment` |
|      - |   965 | ` * on an ATTRIBUTE receiver is the "Document Fragment is empty" warning plus` |
|      - |   966 | ` * false -- so the document screen runs before the fragment check and the` |
|      - |   967 | ` * receiver-kind screen after it.` |
|      - |   968 | ` */` |
|    400 |   969 | `static int DomLinkRefusalPre(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      3 |   970 | `{` |
|      - |   971 | `	/* A child with NO document is exempt: it is a constructed node, and the` |
|      - |   972 | `	 * level-2 doors ADOPT it -- where the modern variadic family refuses it` |
|      - |   973 | `	 * with this same code. */` |
|    403 |   974 | `	if( pParent->doc != pChild->doc && pChild->doc != 0 ){` |
|     17 |   975 | `		return DOM_ERR_WRONG_DOC;` |
|      - |   976 | `	}` |
|      - |   977 | `	/* A DOCUMENT is never a child, stated outright: the ancestor walk below` |
|      - |   978 | `	 * only sees it from an ATTACHED receiver, and a detached one --` |
|      - |   979 | ``	 * `$d->createElement('x')->appendChild($d)` -- spliced the document node`` |
|      - |   980 | `	 * into its own orphan's child list, which teardown then freed twice. */` |
|    387 |   981 | `	if( pChild->type == XML_DOCUMENT_NODE \|\| pChild->type == XML_HTML_DOCUMENT_NODE ){` |
|      3 |   982 | `		return DOM_ERR_HIERARCHY;` |
|      - |   983 | `	}` |
|    385 |   984 | `	return 0;` |
|    203 |   985 | `}` |
|      - |   986 | `/* The ancestor-cycle walk. Walking UP from the parent also catches` |
|      - |   987 | `` * pChild == pParent, so `$frag->appendChild($frag)` is Hierarchy even`` |
|      - |   988 | ` * for an EMPTY fragment -- the cycle answers before the empty warning. */` |
|    416 |   989 | `static int DomLinkCycle(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      3 |   990 | `{` |
|      - |   991 | `	xmlNodePtr p;` |
|   1237 |   992 | `	for( p = pParent ; p ; p = p->parent ){` |
|    837 |   993 | `		if( p == pChild ){` |
|     17 |   994 | `			return DOM_ERR_HIERARCHY;` |
|      - |   995 | `		}` |
|    412 |   996 | `	}` |
|    403 |   997 | `	return 0;` |
|    211 |   998 | `}` |
|      - |   999 | `/* An ATTRIBUTE takes text and entity references, nothing else -- not even a` |
|      - |  1000 | ` * fragment whose every child is text (though the EMPTY fragment's warning` |
|      - |  1001 | ` * answers before this). php's Hierarchy refusal. */` |
|    394 |  1002 | `static int DomAttrRecvKind(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      3 |  1003 | `{` |
|    394 |  1004 | `	if( pParent->type == XML_ATTRIBUTE_NODE` |
|    208 |  1005 | `	 && pChild->type != XML_TEXT_NODE && pChild->type != XML_ENTITY_REF_NODE ){` |
|      7 |  1006 | `		return DOM_ERR_HIERARCHY;` |
|      - |  1007 | `	}` |
|    391 |  1008 | `	return 0;` |
|    200 |  1009 | `}` |
|      - |  1010 | `/*` |
|      - |  1011 | ` * A DOCUMENT FRAGMENT is not linked, it is EMPTIED: php moves its children into` |
|      - |  1012 | ` * the target and answers the FIRST of them (the fragment itself is never a` |
|      - |  1013 | ` * child of anything, which is the whole point of the type -- it is how a` |
|      - |  1014 | ` * program builds a run of nodes and inserts it in one call). An EMPTY one is` |
|      - |  1015 | `` * php's warning plus `false`, not an exception.`` |
|      - |  1016 | ` *` |
|      - |  1017 | ` * *ppFirst takes the first node moved, or NULL when the argument was not a` |
|      - |  1018 | ` * fragment at all; the caller then links the node itself.` |
|      - |  1019 | ` */` |
|    792 |  1020 | `static int DomIsFragment(xmlNodePtr pNode)` |
|      3 |  1021 | `{` |
|    795 |  1022 | `	return pNode && pNode->type == XML_DOCUMENT_FRAG_NODE;` |
|      3 |  1023 | `}` |
|      6 |  1024 | `static int DomFragEmpty(ph7_context *pCtx)` |
|      1 |  1025 | `{` |
|      - |  1026 | ``	/* The context already qualifies the message with php's `DOMNode::method(): `. */`` |
|      7 |  1027 | `	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Document Fragment is empty");` |
|      7 |  1028 | `	ph7_result_bool(pCtx,0);` |
|      7 |  1029 | `	return PH7_OK;` |
|      1 |  1030 | `}` |
|      - |  1031 | `/* Move every child of pFrag into pParent, before pRef or at the end. */` |
|     36 |  1032 | `static xmlNodePtr DomFragMove(phl_xmldoc *pShell,xmlNodePtr pParent,xmlNodePtr pFrag,xmlNodePtr pRef)` |
|      1 |  1033 | `{` |
|     37 |  1034 | `	xmlNodePtr pFirst = pFrag->children;` |
|     37 |  1035 | `	xmlNodePtr pChild = pFirst;` |
|     83 |  1036 | `	while( pChild ){` |
|     47 |  1037 | `		xmlNodePtr pNext = pChild->next;` |
|     47 |  1038 | `		DomDetach(pShell,pChild);` |
|     47 |  1039 | `		if( pRef ){` |
|      9 |  1040 | `			DomLinkBefore(pParent,pChild,pRef);` |
|      5 |  1041 | `		}else{` |
|     39 |  1042 | `			DomLinkLast(pParent,pChild);` |
|      - |  1043 | `		}` |
|      - |  1044 | `		/* php reconciles each node it MOVED, not the fragment they came from --` |
|      - |  1045 | `		 * and through this path it does so DEEPLY (see DomNsOnInsertEx). */` |
|     47 |  1046 | `		DomNsOnInsertEx(pChild,1);` |
|     47 |  1047 | `		pChild = pNext;` |
|      1 |  1048 | `	}` |
|     37 |  1049 | `	pFrag->children = pFrag->last = 0;` |
|     37 |  1050 | `	return pFirst;` |
|      1 |  1051 | `}` |
|      - |  1052 | `/*` |
|      - |  1053 | ` * DOMNode::appendChild(DOMNode $node): DOMNode` |
|      - |  1054 | ` *` |
|      - |  1055 | `` * Note the argument reaches C already screened -- `$n->appendChild(1)` is a`` |
|      - |  1056 | ``  * TypeError from the declared `DOMNode $node`, where the chunk read `->__res` `` |
|      - |  1057 | ` * off an int and warned.` |
|      - |  1058 | ` */` |
|    384 |  1059 | `DOM_METHOD(vm_builtin_DOMNode_appendChild)` |
|      3 |  1060 | `{` |
|    387 |  1061 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    387 |  1062 | `	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|      - |  1063 | `	int iErr,rc;` |
|    387 |  1064 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|    ! 0 |  1065 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  1066 | `	}` |
|    387 |  1067 | `	if( DomMutatorScreen(pCtx,(xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode,&rc) ){` |
|     27 |  1068 | `		return rc;` |
|      - |  1069 | `	}` |
|    361 |  1070 | `	iErr = DomLinkRefusalPre((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|    361 |  1071 | `	if( iErr == 0 ){` |
|    345 |  1072 | `		iErr = DomLinkCycle((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|    171 |  1073 | `	}` |
|      - |  1074 | `	/* The empty-fragment answer sits between the screens: a foreign empty` |
|      - |  1075 | `	 * fragment is Wrong Document, appending a fragment to ITSELF is the cycle's` |
|      - |  1076 | `	 * Hierarchy, and only an empty one on an attribute receiver reaches the` |
|      - |  1077 | `	 * warning plus false. */` |
|    358 |  1078 | `	if( iErr == 0 && DomIsFragment((xmlNodePtr)pChd->pNode)` |
|    183 |  1079 | `	 && ((xmlNodePtr)pChd->pNode)->children == 0 ){` |
|      5 |  1080 | `		return DomFragEmpty(pCtx);` |
|      - |  1081 | `	}` |
|    357 |  1082 | `	if( iErr == 0 ){` |
|    329 |  1083 | `		iErr = DomAttrRecvKind((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|    163 |  1084 | `	}` |
|      - |  1085 | `	/* An attribute lands on an ELEMENT or nowhere -- and BEFORE the adoption,` |
|      - |  1086 | ``	 * which is measurable: `$doc->appendChild(new DOMAttr('k'))` refuses with`` |
|      - |  1087 | `	 * the argument still ownerless. */` |
|    354 |  1088 | `	if( iErr == 0 && ((xmlNodePtr)pChd->pNode)->type == XML_ATTRIBUTE_NODE` |
|    172 |  1089 | `	 && ((xmlNodePtr)pPar->pNode)->type != XML_ELEMENT_NODE ){` |
|      3 |  1090 | `		iErr = DOM_ERR_HIERARCHY;` |
|      1 |  1091 | `	}` |
|    357 |  1092 | `	if( iErr ){` |
|     35 |  1093 | `		return DomThrow(pCtx,iErr);` |
|      - |  1094 | `	}` |
|      - |  1095 | `	/* Every screen passed: a document-less argument is ADOPTED here, php's` |
|      - |  1096 | `	 * constructed-node door -- wrappers, orphan entry and (for an owned` |
|      - |  1097 | `	 * receiver) the document itself all move before the link. */` |
|    323 |  1098 | `	DomAdoptIntoRecv(pCtx,apArg[0],pChd);` |
|    323 |  1099 | `	if( ((xmlNodePtr)pChd->pNode)->type == XML_ATTRIBUTE_NODE ){` |
|     16 |  1100 | `		return DomMutatorAttrAttach(pCtx,pPar,pChd,apArg[0]);` |
|      - |  1101 | `	}` |
|    308 |  1102 | `	if( DomIsFragment((xmlNodePtr)pChd->pNode) ){` |
|      - |  1103 | `		xmlNodePtr pFirst;` |
|     25 |  1104 | `		pFirst = DomFragMove(pChd->pShell,(xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode,0);` |
|     25 |  1105 | `		return DomResultNodeOf(pCtx,pPar,pFirst);` |
|      - |  1106 | `	}` |
|    284 |  1107 | `	DomDetach(pChd->pShell,(xmlNodePtr)pChd->pNode);` |
|    284 |  1108 | `	DomLinkLast((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|    284 |  1109 | `	DomNsOnInsertEx((xmlNodePtr)pChd->pNode,0);` |
|    284 |  1110 | `	ph7_result_value(pCtx,apArg[0]);` |
|    284 |  1111 | `	return PH7_OK;` |
|    195 |  1112 | `}` |
|      - |  1113 | `/* DOMNode::insertBefore(DOMNode $node, ?DOMNode $child = null): DOMNode --` |
|      - |  1114 | ` * a reference node that is not a child of the receiver is Not Found. */` |
|     44 |  1115 | `DOM_METHOD(vm_builtin_DOMNode_insertBefore)` |
|      2 |  1116 | `{` |
|     46 |  1117 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|     46 |  1118 | `	phl_domnode *pNew = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     46 |  1119 | `	phl_domnode *pRef = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;` |
|      - |  1120 | `	xmlNodePtr pParent,pChild,pAnchor;` |
|      - |  1121 | `	int iErr,rc;` |
|     46 |  1122 | `	if( pPar == 0 \|\| pNew == 0 ){` |
|    ! 0 |  1123 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1124 | `	}` |
|     46 |  1125 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|     46 |  1126 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|     46 |  1127 | `	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;` |
|     46 |  1128 | `	if( DomMutatorScreen(pCtx,pParent,pChild,&rc) ){` |
|      3 |  1129 | `		return rc;` |
|      - |  1130 | `	}` |
|     44 |  1131 | `	iErr = DomLinkRefusalPre(pParent,pChild);` |
|     44 |  1132 | `	if( iErr == 0 ){` |
|     42 |  1133 | `		iErr = DomLinkCycle(pParent,pChild);` |
|     20 |  1134 | `	}` |
|      - |  1135 | `	/* Between the screens, and BEFORE the reference-membership refusal: an` |
|      - |  1136 | `	 * empty fragment answers its warning even against a reference node that` |
|      - |  1137 | `	 * is no child of the receiver. */` |
|     44 |  1138 | `	if( iErr == 0 && DomIsFragment(pChild) && pChild->children == 0 ){` |
|      3 |  1139 | `		return DomFragEmpty(pCtx);` |
|      - |  1140 | `	}` |
|     42 |  1141 | `	if( iErr == 0 ){` |
|     40 |  1142 | `		iErr = DomAttrRecvKind(pParent,pChild);` |
|     19 |  1143 | `	}` |
|      - |  1144 | `	/* An attribute lands on an ELEMENT or nowhere, and php answers that` |
|      - |  1145 | `	 * Hierarchy refusal BEFORE the reference-membership one -- a fragment` |
|      - |  1146 | `	 * receiver with an attribute argument and a foreign reference is` |
|      - |  1147 | `	 * Hierarchy, not Not Found. */` |
|     40 |  1148 | `	if( iErr == 0 && pChild->type == XML_ATTRIBUTE_NODE` |
|     28 |  1149 | `	 && pParent->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  1150 | `		iErr = DOM_ERR_HIERARCHY;` |
|    ! 0 |  1151 | `	}` |
|     42 |  1152 | `	if( iErr == 0 && pAnchor && pAnchor->parent != pParent ){` |
|     11 |  1153 | `		iErr = DOM_ERR_NOT_FOUND;` |
|      5 |  1154 | `	}` |
|     42 |  1155 | `	if( iErr ){` |
|     13 |  1156 | `		return DomThrow(pCtx,iErr);` |
|      - |  1157 | `	}` |
|      - |  1158 | `	/* The constructed-node adoption, before ANY of the insertion tails --` |
|      - |  1159 | `	 * php's order, so even an argument the sibling Error is about to strand` |
|      - |  1160 | `	 * detached comes out of the call owned by this document. */` |
|     30 |  1161 | `	DomAdoptIntoRecv(pCtx,apArg[0],pNew);` |
|     30 |  1162 | `	if( pChild->type == XML_ATTRIBUTE_NODE ){` |
|      - |  1163 | `		/*` |
|      - |  1164 | `		 * The attribute half, with insertBefore's own tails. A NULL reference` |
|      - |  1165 | `		 * is the append spelling and attaches (the same-name displacement` |
|      - |  1166 | `		 * included). A reference that is itself an ATTRIBUTE of the receiver` |
|      - |  1167 | `		 * really does mean "before": the argument enters the property list at` |
|      - |  1168 | `		 * the reference's position. Any other reference runs the DISPLACEMENT` |
|      - |  1169 | `		 * and then fails the sibling link, php's own order, so` |
|      - |  1170 | `` 		 * `$el->insertBefore($attr, $child)` on an element carrying `k="old"` `` |
|      - |  1171 | `		 * LOSES the old attribute, attaches nothing, and raises the plain` |
|      - |  1172 | `		 * Error the self-sibling splice raises.` |
|      - |  1173 | `		 */` |
|     13 |  1174 | `		xmlAttrPtr pAttr = (xmlAttrPtr)pChild;` |
|      - |  1175 | `		xmlAttrPtr pOld;` |
|     13 |  1176 | `		if( pAnchor == 0 ){` |
|      3 |  1177 | `			return DomMutatorAttrAttach(pCtx,pPar,pNew,apArg[0]);` |
|      - |  1178 | `		}` |
|     16 |  1179 | `		pOld = pAttr->ns` |
|    ! 0 |  1180 | `			? DomAttrByNs(pParent,pAttr->ns->href,(const char *)pAttr->name)` |
|     10 |  1181 | `			: DomAttrByLocal(pParent,(const char *)pAttr->name);` |
|     11 |  1182 | `		if( pOld && pOld != pAttr ){` |
|      5 |  1183 | `			xmlUnlinkNode((xmlNodePtr)pOld);` |
|      5 |  1184 | `			DomOrphanAdd(pPar->pShell,(xmlNodePtr)pOld);` |
|      2 |  1185 | `		}` |
|     10 |  1186 | `		if( pAnchor->type != XML_ATTRIBUTE_NODE` |
|      9 |  1187 | `		 \|\| pAnchor == (xmlNodePtr)pAttr \|\| pAnchor == (xmlNodePtr)pOld ){` |
|      - |  1188 | `			/* The argument is UNLINKED before the sibling link fails -- php's` |
|      - |  1189 | ``			 * own order, so `$r->insertBefore($cAttr, $child)` costs the other`` |
|      - |  1190 | `			 * element its attribute and attaches nothing here. The link fails` |
|      - |  1191 | `			 * for a non-attribute reference, for the argument AS its own` |
|      - |  1192 | `			 * reference, and for a reference the displacement just took --` |
|      - |  1193 | `			 * php frees it and the sibling link then refuses. */` |
|      9 |  1194 | `			DomAttrDetach(pNew->pShell,pAttr);` |
|      9 |  1195 | `			DomOrphanAdd(pNew->pShell,(xmlNodePtr)pAttr);` |
|      9 |  1196 | `			return PH7_VmThrowException(pCtx,"Error",` |
|      - |  1197 | `				"Cannot add newnode as the previous sibling of refnode");` |
|      - |  1198 | `		}` |
|      3 |  1199 | `		DomAttrDetach(pNew->pShell,pAttr);` |
|      3 |  1200 | `		DomAttrLinkBefore(pParent,pAttr,(xmlAttrPtr)pAnchor);` |
|      3 |  1201 | `		DomNsAttrArrive(pParent,pAttr);` |
|      3 |  1202 | `		ph7_result_value(pCtx,apArg[0]);` |
|      3 |  1203 | `		return PH7_OK;` |
|      - |  1204 | `	}` |
|     18 |  1205 | `	if( pAnchor && pAnchor->type == XML_ATTRIBUTE_NODE ){` |
|      - |  1206 | `		/*` |
|      - |  1207 | `		 * A non-attribute argument against an ATTRIBUTE reference: php hands` |
|      - |  1208 | `		 * the pair to xmlAddPrevSibling, which UNLINKS the argument and then` |
|      - |  1209 | `		 * splices it into the PROPERTY chain -- state no serializer or` |
|      - |  1210 | `		 * childNodes walk ever shows, whose exact shape is libxml's version's.` |
|      - |  1211 | `		 * The bytes agree when PHL simply DETACHES the argument and answers` |
|      - |  1212 | `		 * it; the one detail php answers differently afterwards is recorded in` |
|      - |  1213 | ``		 * §7.4 (the argument's `parentNode` reads the receiver there).`` |
|      - |  1214 | `		 */` |
|      3 |  1215 | `		DomDetach(pNew->pShell,pChild);` |
|      3 |  1216 | `		DomOrphanAdd(pNew->pShell,pChild);` |
|      3 |  1217 | `		ph7_result_value(pCtx,apArg[0]);` |
|      3 |  1218 | `		return PH7_OK;` |
|      - |  1219 | `	}` |
|     15 |  1220 | `	if( pAnchor == pChild ){` |
|      - |  1221 | `		/*` |
|      - |  1222 | `		 * A node cannot be inserted before ITSELF, and linking it anyway made` |
|      - |  1223 | ``		 * it its own sibling: `$p->insertBefore($x,$x)` spliced a cycle into`` |
|      - |  1224 | `		 * the child list, and the next walk of the tree -- saveXML, a` |
|      - |  1225 | `		 * childNodes count, getNodePath -- never returned.` |
|      - |  1226 | `		 *` |
|      - |  1227 | `		 * php's refusal here is a plain Error with no DOM code, because there` |
|      - |  1228 | `		 * is no DOM error for it, and it comes AFTER the node is detached: the` |
|      - |  1229 | `		 * tree loses the node and the caller is told nothing more.` |
|      - |  1230 | `		 */` |
|      5 |  1231 | `		DomDetach(pNew->pShell,pChild);` |
|      5 |  1232 | `		DomOrphanAdd(pNew->pShell,pChild);` |
|      5 |  1233 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  1234 | `			"Cannot add newnode as the previous sibling of refnode");` |
|      - |  1235 | `	}` |
|     11 |  1236 | `	if( DomIsFragment(pChild) ){` |
|      - |  1237 | `		/* An EMPTY fragment answered its warning above, before the reference` |
|      - |  1238 | `		 * screen -- php's order. */` |
|      - |  1239 | `		xmlNodePtr pFirst;` |
|      3 |  1240 | `		pFirst = DomFragMove(pNew->pShell,pParent,pChild,pAnchor);` |
|      3 |  1241 | `		return DomResultNodeOf(pCtx,pPar,pFirst);` |
|      - |  1242 | `	}` |
|      9 |  1243 | `	DomDetach(pNew->pShell,pChild);` |
|      9 |  1244 | `	if( pAnchor ){` |
|      9 |  1245 | `		DomLinkBefore(pParent,pChild,pAnchor);` |
|      5 |  1246 | `	}else{` |
|    ! 0 |  1247 | `		DomLinkLast(pParent,pChild);` |
|      - |  1248 | `	}` |
|      9 |  1249 | `	DomNsOnInsertEx(pChild,0);` |
|      9 |  1250 | `	ph7_result_value(pCtx,apArg[0]);` |
|      9 |  1251 | `	return PH7_OK;` |
|     24 |  1252 | `}` |
|      - |  1253 | `/* DOMNode::removeChild(DOMNode $child): DOMNode */` |
|     36 |  1254 | `DOM_METHOD(vm_builtin_DOMNode_removeChild)` |
|      1 |  1255 | `{` |
|     37 |  1256 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|     37 |  1257 | `	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|      - |  1258 | `	xmlNodePtr pChild;` |
|     37 |  1259 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|    ! 0 |  1260 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1261 | `	}` |
|     37 |  1262 | `	pChild = (xmlNodePtr)pChd->pNode;` |
|      - |  1263 | ``	/* php's membership test is `no children at all, or the parent pointer`` |
|      - |  1264 | ``	 * disagrees` -- which lets an ATTACHED ATTRIBUTE through (its libxml`` |
|      - |  1265 | `	 * parent IS the element), so removeChild really does remove an attribute` |
|      - |  1266 | `	 * -- but only from an element that has at least one real child; on a` |
|      - |  1267 | `	 * childless one the same attribute is Not Found. (An entity reference's` |
|      - |  1268 | `	 * child fails the parent test: its parent is the DTD.) */` |
|     36 |  1269 | `	if( ((xmlNodePtr)pPar->pNode)->children == 0` |
|     36 |  1270 | `	 \|\| pChild->parent != (xmlNodePtr)pPar->pNode ){` |
|     17 |  1271 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1272 | `	}` |
|      - |  1273 | `	/* ...and only THEN the read-only refusal, php's order: an entity` |
|      - |  1274 | `	 * reference's child is Not Found territory never reached, while` |
|      - |  1275 | ``	 * `$ownerless->removeChild($its->child)` is the No Modification`` |
|      - |  1276 | `	 * refusal. */` |
|     21 |  1277 | `	if( DomNodeReadOnly((xmlNodePtr)pPar->pNode) ){` |
|      3 |  1278 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|      - |  1279 | `	}` |
|     19 |  1280 | `	xmlUnlinkNode(pChild);` |
|     19 |  1281 | `	DomOrphanAdd(pChd->pShell,pChild);` |
|     19 |  1282 | `	ph7_result_value(pCtx,apArg[0]);` |
|     19 |  1283 | `	return PH7_OK;` |
|     19 |  1284 | `}` |
|      - |  1285 | `/* DOMNode::replaceChild(DOMNode $node, DOMNode $child): DOMNode -- answers the` |
|      - |  1286 | ` * node it replaced, which is the SECOND argument. */` |
|     44 |  1287 | `DOM_METHOD(vm_builtin_DOMNode_replaceChild)` |
|      1 |  1288 | `{` |
|     45 |  1289 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|     45 |  1290 | `	phl_domnode *pNew = nArg > 1 ? DomObjArg(apArg[0]) : 0;` |
|     45 |  1291 | `	phl_domnode *pOld = nArg > 1 ? DomObjArg(apArg[1]) : 0;` |
|      - |  1292 | `	xmlNodePtr pParent,pChild,pVictim;` |
|      - |  1293 | `	int iErr;` |
|     45 |  1294 | `	if( pPar == 0 \|\| pNew == 0 \|\| pOld == 0 ){` |
|    ! 0 |  1295 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1296 | `	}` |
|     45 |  1297 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|     45 |  1298 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|     45 |  1299 | `	pVictim = (xmlNodePtr)pOld->pNode;` |
|      - |  1300 | `	/*` |
|      - |  1301 | `	 * php's replaceChild, in its own order (dom_node_replace_child) -- and it` |
|      - |  1302 | `	 * disagrees with appendChild's twice. The document screen answers FIRST (a` |
|      - |  1303 | `	 * text receiver or a read-only receiver with a foreign argument is Wrong` |
|      - |  1304 | `	 * Document here, where appendChild answers false and No Modification).` |
|      - |  1305 | `	 * Then the two silent-false answers: the invalid-children receiver and the` |
|      - |  1306 | `	 * CHILDLESS one -- nothing to replace, and php says nothing at all, even` |
|      - |  1307 | `	 * for an attribute or a document argument. Then the shared insertion` |
|      - |  1308 | `	 * validity: read-only, the ancestor cycle, the attribute receiver's` |
|      - |  1309 | `	 * child-kind rule, an attribute argument's element-only rule, the` |
|      - |  1310 | `	 * document-as-child rule. Then a rule of replaceChild's OWN: old and new` |
|      - |  1311 | `	 * must be attributes TOGETHER or not at all -- so an attribute argument` |
|      - |  1312 | `	 * against a foreign ATTRIBUTE victim reads Not Found from the membership` |
|      - |  1313 | `	 * check (both are attributes, the pair passes) while an element argument` |
|      - |  1314 | `	 * against the same victim is Hierarchy. The victim's membership answers` |
|      - |  1315 | `	 * last.` |
|      - |  1316 | `	 */` |
|     45 |  1317 | `	if( pChild->doc != pParent->doc && pChild->doc != 0 ){` |
|      3 |  1318 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  1319 | `	}` |
|     43 |  1320 | `	if( !DomChildrenValid(pParent) \|\| pParent->children == 0 ){` |
|      7 |  1321 | `		ph7_result_bool(pCtx,0);` |
|      7 |  1322 | `		return PH7_OK;` |
|      - |  1323 | `	}` |
|     36 |  1324 | `	if( DomNodeReadOnly(pParent)` |
|     36 |  1325 | `	 \|\| (pChild->parent && DomNodeReadOnly(pChild->parent)) ){` |
|      3 |  1326 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|      - |  1327 | `	}` |
|     35 |  1328 | `	iErr = DomLinkCycle(pParent,pChild);` |
|     35 |  1329 | `	if( iErr == 0 ){` |
|     31 |  1330 | `		iErr = DomAttrRecvKind(pParent,pChild);` |
|     15 |  1331 | `	}` |
|     34 |  1332 | `	if( iErr == 0 && pChild->type == XML_ATTRIBUTE_NODE` |
|     18 |  1333 | `	 && pParent->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  1334 | `		iErr = DOM_ERR_HIERARCHY;` |
|    ! 0 |  1335 | `	}` |
|     34 |  1336 | `	if( iErr == 0` |
|     32 |  1337 | `	 && (pChild->type == XML_DOCUMENT_NODE \|\| pChild->type == XML_HTML_DOCUMENT_NODE) ){` |
|    ! 0 |  1338 | `		iErr = DOM_ERR_HIERARCHY;` |
|    ! 0 |  1339 | `	}` |
|     34 |  1340 | `	if( iErr == 0` |
|     32 |  1341 | `	 && (pChild->type == XML_ATTRIBUTE_NODE) != (pVictim->type == XML_ATTRIBUTE_NODE) ){` |
|      5 |  1342 | `		iErr = DOM_ERR_HIERARCHY;` |
|      2 |  1343 | `	}` |
|     35 |  1344 | `	if( iErr == 0 && pVictim->parent != pParent ){` |
|      7 |  1345 | `		iErr = DOM_ERR_NOT_FOUND;` |
|      3 |  1346 | `	}` |
|     35 |  1347 | `	if( iErr ){` |
|     17 |  1348 | `		return DomThrow(pCtx,iErr);` |
|      - |  1349 | `	}` |
|      - |  1350 | `	/* The constructed-node adoption, php's "document assignment" step. */` |
|     19 |  1351 | `	DomAdoptIntoRecv(pCtx,apArg[0],pNew);` |
|     19 |  1352 | `	if( DomIsFragment(pChild) ){` |
|      - |  1353 | `		/* No empty-fragment refusal here, unlike the other two: php REMOVES the` |
|      - |  1354 | `		 * old child and inserts nothing, and answers it as any replaceChild` |
|      - |  1355 | `		 * does. */` |
|      5 |  1356 | `		DomFragMove(pNew->pShell,pParent,pChild,pVictim);` |
|      5 |  1357 | `		xmlUnlinkNode(pVictim);` |
|      5 |  1358 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|      5 |  1359 | `		ph7_result_value(pCtx,apArg[1]);` |
|      5 |  1360 | `		return PH7_OK;` |
|      - |  1361 | `	}` |
|     15 |  1362 | `	if( pChild != pVictim && pChild->type == XML_ATTRIBUTE_NODE ){` |
|      - |  1363 | `		/* Both sides are attributes (the XOR above let them through): the swap` |
|      - |  1364 | `		 * happens in the PROPERTY list, at the victim's position, with NO` |
|      - |  1365 | `		 * same-name displacement -- php hands the pair to xmlReplaceNode` |
|      - |  1366 | `		 * as-is, so a duplicate name is the caller's to answer for. */` |
|      5 |  1367 | `		DomAttrDetach(pNew->pShell,(xmlAttrPtr)pChild);` |
|      5 |  1368 | `		DomAttrLinkBefore(pParent,(xmlAttrPtr)pChild,(xmlAttrPtr)pVictim);` |
|      5 |  1369 | `		xmlUnlinkNode(pVictim);` |
|      5 |  1370 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|      5 |  1371 | `		DomNsAttrArrive(pParent,(xmlAttrPtr)pChild);` |
|     13 |  1372 | `	}else if( pChild != pVictim ){` |
|      9 |  1373 | `		DomDetach(pNew->pShell,pChild);` |
|      9 |  1374 | `		DomLinkBefore(pParent,pChild,pVictim);` |
|      9 |  1375 | `		xmlUnlinkNode(pVictim);` |
|      9 |  1376 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|      9 |  1377 | `		DomNsOnInsertEx(pChild,0);` |
|      4 |  1378 | `	}` |
|     15 |  1379 | `	ph7_result_value(pCtx,apArg[1]);` |
|     15 |  1380 | `	return PH7_OK;` |
|     23 |  1381 | `}` |
|      - |  1382 | `/* ===== The 8.3 parent/child-node family (DOMParentNode / DOMChildNode) ===== */` |
|      - |  1383 |  |
|      - |  1384 | `/*` |
|      - |  1385 | ` * The name a TypeError prints for a value that is neither a DOMNode nor a` |
|      - |  1386 | ` * string: the CLASS of an object, php's type keyword for anything else --` |
|      - |  1387 | ` * the same rendering DomWriteText uses for a typed property store.` |
|      - |  1388 | ` */` |
|     24 |  1389 | `static const char * DomGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)` |
|      1 |  1390 | `{` |
|     25 |  1391 | `	if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|      5 |  1392 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|      5 |  1393 | `		SyBufferFormat(zBuf,nBuf,"%z",&pObj->pClass->sName);` |
|      5 |  1394 | `		return zBuf;` |
|      - |  1395 | `	}` |
|     21 |  1396 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      3 |  1397 | `		return "null";` |
|      - |  1398 | `	}` |
|     19 |  1399 | `	return ph7_type_name(pVal);` |
|     13 |  1400 | `}` |
|      - |  1401 | `/*` |
|      - |  1402 | ` * php's variadic screen for the 8.0 insertion methods: every argument must be` |
|      - |  1403 | ` * a DOMNode or a STRING (nothing coerces -- an int is refused where an` |
|      - |  1404 | `` * ordinary `string $data` parameter would take it), the WHOLE list is checked`` |
|      - |  1405 | ` * before anything else runs, and the TypeError names the position with no` |
|      - |  1406 | ` * parameter name, because many values share the one variadic formal.  The` |
|      - |  1407 | ``  * method name it prints is the DECLARING class's (`DOMCharacterData::before()` `` |
|      - |  1408 | ` * for a comment), which is what pCtx->pFunc->sName already carries.` |
|      - |  1409 | ` * Answers 0 when every argument passed, non-zero after raising.` |
|      - |  1410 | ` */` |
|    160 |  1411 | `static int DomNodesScreen(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      1 |  1412 | `{` |
|    161 |  1413 | `	ph7_class *pNodeCls = PH7_VmExtractClass(pCtx->pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);` |
|      - |  1414 | `	int i;` |
|    345 |  1415 | `	for( i = 0 ; i < nArg ; i++ ){` |
|    209 |  1416 | `		ph7_value *pVal = apArg[i];` |
|      - |  1417 | `		char zBuf[128];` |
|    209 |  1418 | `		if( pVal->iFlags & MEMOBJ_OBJ ){` |
|    139 |  1419 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|    139 |  1420 | `			if( pNodeCls && PH7_VmInstanceOf(pObj->pClass,pNodeCls) ){` |
|    160 |  1421 | `				continue;` |
|      1 |  1422 | `			}` |
|     73 |  1423 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     51 |  1424 | `			continue;` |
|      - |  1425 | `		}` |
|     37 |  1426 | `		PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  1427 | `			"%z(): Argument #%d must be of type DOMNode\|string, %s given",` |
|     24 |  1428 | `			&pCtx->pFunc->sName,i+1,DomGivenName(pVal,zBuf,sizeof(zBuf)));` |
|     25 |  1429 | `		return -1;` |
|    ! 0 |  1430 | `	}` |
|    137 |  1431 | `	return 0;` |
|     81 |  1432 | `}` |
|      - |  1433 | `/*` |
|      - |  1434 | ` * php's "convert nodes into a node" (dom_zvals_to_single_node), transcribed` |
|      - |  1435 | ` * with its ONE-argument shortcut: a single node argument is handed through` |
|      - |  1436 | ` * whole -- nothing is unlinked, every check waits for the insertion -- while` |
|      - |  1437 | ` * two or more arguments really are appended one by one into an internal` |
|      - |  1438 | ` * fragment.  The difference is observable twice over.  A refusal DURING that` |
|      - |  1439 | ` * conversion (another document's node, a document, an attribute) leaves every` |
|      - |  1440 | `` * argument already converted DETACHED -- `$b->append($a, $attr)` costs the`` |
|      - |  1441 | ` * tree its $a -- and a refusal at the final insertion (the receiver was in` |
|      - |  1442 | ` * the converted set) leaves ALL of them detached, which is how` |
|      - |  1443 | `` * `$b->append($a, $b)` empties <r> of both children where `$r->append($r)`,`` |
|      - |  1444 | ` * one argument, moves nothing at all.  The CYCLE is checked only against the` |
|      - |  1445 | ` * conversion fragment (i.e. never fails there), NOT against the receiver --` |
|      - |  1446 | ` * that waits for the insertion step.` |
|      - |  1447 | ` *` |
|      - |  1448 | ` * The converted list is built in pList (xmlNodePtr entries, in order).  Every` |
|      - |  1449 | ` * node it takes is detached and parked in the receiver's orphan set, where a` |
|      - |  1450 | ` * failure leaves it alive for whatever PHP variable still wraps it -- php` |
|      - |  1451 | ` * frees the unwrapped ones instead, which no program can see.  A fragment` |
|      - |  1452 | ` * argument is emptied INTO the list (php unpacks it), so it stays empty even` |
|      - |  1453 | ` * when a later argument is refused.  Answers 0, or non-zero after raising.` |
|      - |  1454 | ` */` |
|     58 |  1455 | `static int DomNodesConvert(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pParent,` |
|      - |  1456 | `	int nArg,ph7_value **apArg,SySet *pList,int *pRc)` |
|      1 |  1457 | `{` |
|     59 |  1458 | `	ph7_class_instance *pDstHolder = DomThisDoc(pCtx);` |
|      - |  1459 | `	int i;` |
|    149 |  1460 | `	for( i = 0 ; i < nArg ; i++ ){` |
|      - |  1461 | `		phl_domnode *pNd;` |
|      - |  1462 | `		xmlNodePtr pNode;` |
|      - |  1463 | `		ph7_class_instance *pSrcHolder;` |
|    101 |  1464 | `		if( (apArg[i]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     41 |  1465 | `			int nLen = 0;` |
|     41 |  1466 | `			const char *zText = ph7_value_to_string(apArg[i],&nLen);` |
|     41 |  1467 | `			pNode = xmlNewDocTextLen(pParent->doc,(const xmlChar *)zText,nLen);` |
|     41 |  1468 | `			if( pNode ){` |
|     41 |  1469 | `				DomOrphanAdd(pShell,pNode);` |
|     41 |  1470 | `				SySetPut(pList,(const void *)&pNode);` |
|     20 |  1471 | `			}` |
|     41 |  1472 | `			continue;` |
|      - |  1473 | `		}` |
|     61 |  1474 | `		pNd = DomObjArg(apArg[i]);` |
|     61 |  1475 | `		if( pNd == 0 ){` |
|      - |  1476 | `			/* A DOMNode-classed object with no node behind it. php's refusal` |
|      - |  1477 | `			 * ignores strictErrorChecking. */` |
|      3 |  1478 | `			*pRc = DomThrowAlways(pCtx,DOM_ERR_INVALID_STATE);` |
|      7 |  1479 | `			return -1;` |
|      - |  1480 | `		}` |
|     59 |  1481 | `		pNode = (xmlNodePtr)pNd->pNode;` |
|     59 |  1482 | `		if( pNode->doc != pParent->doc ){` |
|      - |  1483 | `			/* No adoption in the modern family: a constructed node's NULL` |
|      - |  1484 | `			 * document is a mismatch like any other and refuses -- only the` |
|      - |  1485 | `			 * OWNERLESS-to-OWNERLESS pair (both NULL) passes. */` |
|      5 |  1486 | `			*pRc = DomThrowVoid(pCtx,DOM_ERR_WRONG_DOC);` |
|      5 |  1487 | `			return -1;` |
|      - |  1488 | `		}` |
|      - |  1489 | `		/* Same document, possibly different HOLDER: two constructed trees` |
|      - |  1490 | `		 * merging. The wrappers move to the receiver's cache so identity` |
|      - |  1491 | `		 * keeps answering. */` |
|     55 |  1492 | `		pSrcHolder = DomObjArgDoc(apArg[i]);` |
|     55 |  1493 | `		if( pSrcHolder != pDstHolder ){` |
|      9 |  1494 | `			DomAdoptWrappers(pCtx->pVm,pSrcHolder,pDstHolder,pShell,pNode);` |
|      9 |  1495 | `			pNd->pShell = pShell;` |
|      4 |  1496 | `		}` |
|     54 |  1497 | `		if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE` |
|     55 |  1498 | `		 \|\| pNode->type == XML_ATTRIBUTE_NODE ){` |
|      5 |  1499 | `			*pRc = DomThrowVoid(pCtx,DOM_ERR_HIERARCHY);` |
|      5 |  1500 | `			return -1;` |
|      - |  1501 | `		}` |
|     51 |  1502 | `		if( DomIsFragment(pNode) ){` |
|    ! 0 |  1503 | `			xmlNodePtr pChild = pNode->children;` |
|    ! 0 |  1504 | `			while( pChild ){` |
|    ! 0 |  1505 | `				xmlNodePtr pNext = pChild->next;` |
|    ! 0 |  1506 | `				xmlUnlinkNode(pChild);` |
|    ! 0 |  1507 | `				DomOrphanAdd(pShell,pChild);` |
|    ! 0 |  1508 | `				SySetPut(pList,(const void *)&pChild);` |
|    ! 0 |  1509 | `				pChild = pNext;` |
|    ! 0 |  1510 | `			}` |
|    ! 0 |  1511 | `			pNode->children = pNode->last = 0;` |
|    ! 0 |  1512 | `			continue;` |
|      - |  1513 | `		}` |
|     51 |  1514 | `		DomDetach(pNd->pShell,pNode);` |
|     51 |  1515 | `		DomOrphanAdd(pShell,pNode);` |
|     51 |  1516 | `		SySetPut(pList,(const void *)&pNode);` |
|     26 |  1517 | `	}` |
|     49 |  1518 | `	return 0;` |
|     30 |  1519 | `}` |
|     84 |  1520 | `static int DomListHas(SySet *pList,xmlNodePtr pNode)` |
|      1 |  1521 | `{` |
|     85 |  1522 | `	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);` |
|      - |  1523 | `	sxu32 n;` |
|    211 |  1524 | `	for( n = 0 ; n < SySetUsed(pList) ; ++n ){` |
|    133 |  1525 | `		if( apNode[n] == pNode ){` |
|      7 |  1526 | `			return 1;` |
|      - |  1527 | `		}` |
|     64 |  1528 | `	}` |
|     79 |  1529 | `	return 0;` |
|     43 |  1530 | `}` |
|      - |  1531 | `/*` |
|      - |  1532 | ` * php's pre-insertion validity for what conversion produced, against the REAL` |
|      - |  1533 | ` * parent this time.  For a single node argument this is where every check` |
|      - |  1534 | ` * runs -- another document before the kind-or-ancestor Hierarchy refusal, the` |
|      - |  1535 | ` * same order the conversion pass uses -- and for a converted list the only` |
|      - |  1536 | ` * question left is whether the receiver is now INSIDE the set (its ancestor` |
|      - |  1537 | ` * chain passes through a detached argument).  Note what php never checks on` |
|      - |  1538 | ` * this path: a document receiver takes a second root element and bare text` |
|      - |  1539 | ` * without complaint, so the document it writes may not be well-formed XML --` |
|      - |  1540 | ` * measured, and matched.` |
|      - |  1541 | ` */` |
|    146 |  1542 | `static int DomInsertValidity(xmlNodePtr pParent,xmlNodePtr pSingle,SySet *pList)` |
|      1 |  1543 | `{` |
|      - |  1544 | `	xmlNodePtr p;` |
|    147 |  1545 | `	if( pSingle ){` |
|     99 |  1546 | `		if( pSingle->doc != pParent->doc ){` |
|     13 |  1547 | `			return DOM_ERR_WRONG_DOC;` |
|      - |  1548 | `		}` |
|     86 |  1549 | `		if( pSingle->type == XML_DOCUMENT_NODE \|\| pSingle->type == XML_HTML_DOCUMENT_NODE` |
|     85 |  1550 | `		 \|\| pSingle->type == XML_ATTRIBUTE_NODE ){` |
|      7 |  1551 | `			return DOM_ERR_HIERARCHY;` |
|      - |  1552 | `		}` |
|    215 |  1553 | `		for( p = pParent ; p ; p = p->parent ){` |
|    151 |  1554 | `			if( p == pSingle ){` |
|     17 |  1555 | `				return DOM_ERR_HIERARCHY;` |
|      - |  1556 | `			}` |
|     68 |  1557 | `		}` |
|     65 |  1558 | `		return 0;` |
|      - |  1559 | `	}` |
|    121 |  1560 | `	for( p = pParent ; p ; p = p->parent ){` |
|     79 |  1561 | `		if( DomListHas(pList,p) ){` |
|      7 |  1562 | `			return DOM_ERR_HIERARCHY;` |
|      - |  1563 | `		}` |
|     37 |  1564 | `	}` |
|     43 |  1565 | `	return 0;` |
|     74 |  1566 | `}` |
|      - |  1567 | `/*` |
|      - |  1568 | ` * The insertion itself (php's dom_insert_node_list_unchecked): everything in` |
|      - |  1569 | ` * pList goes before pRef -- at the end when NULL -- in order.  A list node` |
|      - |  1570 | ` * came through the conversion fragment, so its namespace reconcile is the` |
|      - |  1571 | ` * DEEP one (dom_reconcile_ns_list); a single node is php's dom_reconcile_ns,` |
|      - |  1572 | ` * the shallow appendChild rule.  A single node inserted before ITSELF slides` |
|      - |  1573 | ` * the reference to its next sibling first (the spec's step 3), which is what` |
|      - |  1574 | `` * makes `$r->prepend($r->firstChild)` a no-op instead of a cycle.`` |
|      - |  1575 | ` */` |
|     42 |  1576 | `static void DomNodesPlace(phl_xmldoc *pShell,xmlNodePtr pParent,xmlNodePtr pRef,SySet *pList)` |
|      1 |  1577 | `{` |
|     43 |  1578 | `	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);` |
|      - |  1579 | `	sxu32 n;` |
|    111 |  1580 | `	for( n = 0 ; n < SySetUsed(pList) ; ++n ){` |
|     69 |  1581 | `		xmlNodePtr pNode = apNode[n];` |
|     69 |  1582 | `		DomDetach(pShell,pNode);` |
|     69 |  1583 | `		if( pRef ){` |
|     23 |  1584 | `			DomLinkBefore(pParent,pNode,pRef);` |
|     12 |  1585 | `		}else{` |
|     47 |  1586 | `			DomLinkLast(pParent,pNode);` |
|      - |  1587 | `		}` |
|     69 |  1588 | `		DomNsOnInsertEx(pNode,1);` |
|     35 |  1589 | `	}` |
|     43 |  1590 | `}` |
|      - |  1591 | `/*` |
|      - |  1592 | ` * DOMParentNode::append / prepend / replaceChildren -- one body, three` |
|      - |  1593 | `` * insertion points.  php declares all three `void`, so a refusal in the`` |
|      - |  1594 | ` * non-strict mode is a warning and NOTHING is answered (DomThrowVoid).` |
|      - |  1595 | ` */` |
|      - |  1596 | `#define DOM_PN_APPEND   0` |
|      - |  1597 | `#define DOM_PN_PREPEND  1` |
|      - |  1598 | `#define DOM_PN_REPLACE  2` |
|    108 |  1599 | `static int DomParentNodeInsert(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)` |
|      1 |  1600 | `{` |
|    109 |  1601 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    109 |  1602 | `	phl_domnode *pOne = 0;` |
|    109 |  1603 | `	xmlNodePtr pParent,pSingle = 0,pRef = 0;` |
|      - |  1604 | `	SySet sList;` |
|    109 |  1605 | `	int iErr,rc = PH7_OK;` |
|    109 |  1606 | `	if( DomNodesScreen(pCtx,nArg,apArg) \|\| pPar == 0 ){` |
|     21 |  1607 | `		return PH7_OK;` |
|      - |  1608 | `	}` |
|     89 |  1609 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|     89 |  1610 | `	SySetInit(&sList,&pCtx->pVm->sAllocator,sizeof(xmlNodePtr));` |
|     89 |  1611 | `	if( nArg == 1 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){` |
|      - |  1612 | `		/* The one-argument shortcut: the node itself, unconverted.  A shell` |
|      - |  1613 | `		 * with no node behind it is php's SILENT no-op here (the pre-insert` |
|      - |  1614 | `		 * NULL guard), where the multi-argument conversion raises Invalid` |
|      - |  1615 | `		 * State -- one more face of the shortcut. */` |
|     53 |  1616 | `		pOne = DomObjArg(apArg[0]);` |
|     53 |  1617 | `		if( pOne == 0 ){` |
|      3 |  1618 | `			return PH7_OK;` |
|      - |  1619 | `		}` |
|     51 |  1620 | `		pSingle = (xmlNodePtr)pOne->pNode;` |
|     62 |  1621 | `	}else if( DomNodesConvert(pCtx,pPar->pShell,pParent,nArg,apArg,&sList,&rc) ){` |
|      7 |  1622 | `		SySetRelease(&sList);` |
|      7 |  1623 | `		return rc;` |
|      - |  1624 | `	}` |
|     81 |  1625 | `	iErr = DomInsertValidity(pParent,pSingle,&sList);` |
|     81 |  1626 | `	if( iErr ){` |
|     31 |  1627 | `		SySetRelease(&sList);` |
|     31 |  1628 | `		return DomThrowVoid(pCtx,iErr);` |
|      - |  1629 | `	}` |
|     51 |  1630 | `	if( pOne ){` |
|      - |  1631 | `		/* The single-node shortcut skipped the conversion, so it re-homes its` |
|      - |  1632 | `		 * wrappers here: an ownerless argument merging into an ownerless` |
|      - |  1633 | `		 * receiver (the only mismatch the validity lets through). */` |
|     23 |  1634 | `		DomAdoptIntoRecv(pCtx,apArg[0],pOne);` |
|     11 |  1635 | `	}` |
|     51 |  1636 | `	if( iMode == DOM_PN_REPLACE ){` |
|      - |  1637 | `		/* Every remaining child goes -- through the wrapper-preserving drop a` |
|      - |  1638 | `		 * content write uses, so a PHP variable holding one keeps a live` |
|      - |  1639 | `		 * detached node rather than a dangling pointer.  After the validity` |
|      - |  1640 | `		 * check, as php orders it. */` |
|     11 |  1641 | `		DomDropChildren(pCtx,pPar->pShell,pParent);` |
|     46 |  1642 | `	}else if( iMode == DOM_PN_PREPEND ){` |
|      - |  1643 | `		/* The first child AFTER conversion has emptied the set out of the` |
|      - |  1644 | `		 * tree -- and never a member of the set. */` |
|      7 |  1645 | `		pRef = pParent->children;` |
|      3 |  1646 | `	}` |
|     51 |  1647 | `	if( pSingle ){` |
|     23 |  1648 | `		if( DomIsFragment(pSingle) ){` |
|      - |  1649 | `			/* A single fragment splices -- silently even when EMPTY, unlike` |
|      - |  1650 | `			 * appendChild's warning. */` |
|      5 |  1651 | `			DomFragMove(pOne->pShell,pParent,pSingle,pRef);` |
|      3 |  1652 | `		}else{` |
|     19 |  1653 | `			if( pRef == pSingle ){` |
|    ! 0 |  1654 | `				pRef = pSingle->next;` |
|    ! 0 |  1655 | `			}` |
|     19 |  1656 | `			DomDetach(pOne->pShell,pSingle);` |
|     19 |  1657 | `			if( pRef ){` |
|      3 |  1658 | `				DomLinkBefore(pParent,pSingle,pRef);` |
|      2 |  1659 | `			}else{` |
|     17 |  1660 | `				DomLinkLast(pParent,pSingle);` |
|      - |  1661 | `			}` |
|     19 |  1662 | `			DomNsOnInsertEx(pSingle,0);` |
|      - |  1663 | `		}` |
|     12 |  1664 | `	}else{` |
|     29 |  1665 | `		DomNodesPlace(pPar->pShell,pParent,pRef,&sList);` |
|      - |  1666 | `	}` |
|     51 |  1667 | `	SySetRelease(&sList);` |
|     51 |  1668 | `	return PH7_OK;` |
|     55 |  1669 | `}` |
|     86 |  1670 | `DOM_METHOD(vm_builtin_Dom_append)` |
|      1 |  1671 | `{` |
|     87 |  1672 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_APPEND);` |
|      1 |  1673 | `}` |
|      8 |  1674 | `DOM_METHOD(vm_builtin_Dom_prepend)` |
|      1 |  1675 | `{` |
|      9 |  1676 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_PREPEND);` |
|      1 |  1677 | `}` |
|     14 |  1678 | `DOM_METHOD(vm_builtin_Dom_replaceChildren)` |
|      1 |  1679 | `{` |
|     15 |  1680 | `	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_REPLACE);` |
|      1 |  1681 | `}` |
|      - |  1682 | `/*` |
|      - |  1683 | ` * Is this xmlNodePtr one of the ARGUMENT nodes?  The viable-sibling walks ask` |
|      - |  1684 | ` * it about tree nodes, so only object arguments can match -- php's` |
|      - |  1685 | ` * dom_is_node_in_list does the same walk over the zval list.` |
|      - |  1686 | ` */` |
|     24 |  1687 | `static int DomArgListHasNode(int nArg,ph7_value **apArg,xmlNodePtr pNode)` |
|      1 |  1688 | `{` |
|      - |  1689 | `	int i;` |
|     47 |  1690 | `	for( i = 0 ; i < nArg ; i++ ){` |
|     29 |  1691 | `		phl_domnode *pNd = DomObjArg(apArg[i]);` |
|     29 |  1692 | `		if( pNd && (xmlNodePtr)pNd->pNode == pNode ){` |
|      7 |  1693 | `			return 1;` |
|      - |  1694 | `		}` |
|     12 |  1695 | `	}` |
|     19 |  1696 | `	return 0;` |
|     13 |  1697 | `}` |
|      - |  1698 | `/*` |
|      - |  1699 | ` * DOMChildNode::before / after / replaceWith -- php's WHATWG transcription` |
|      - |  1700 | ` * (dom_parent_node_before/after, dom_child_replace_with), sharing the parent` |
|      - |  1701 | ` * side's conversion machinery.  The order is the measurable part: the TYPE` |
|      - |  1702 | ` * screen runs first even for a node with no parent; a parentless receiver` |
|      - |  1703 | ` * then returns in SILENCE -- around an argument that could never be inserted` |
|      - |  1704 | ` * -- and only then does conversion run, with the same mid-list detachment the` |
|      - |  1705 | ` * parent side has.  The reference sibling ("viable") is the nearest sibling` |
|      - |  1706 | ` * NOT in the argument set, read before anything moves; the insertion point is` |
|      - |  1707 | ` * derived from it after conversion, so a set member that was also the first` |
|      - |  1708 | ` * child no longer counts.` |
|      - |  1709 | ` */` |
|      - |  1710 | `#define DOM_CN_BEFORE   0` |
|      - |  1711 | `#define DOM_CN_AFTER    1` |
|      - |  1712 | `#define DOM_CN_REPLACE  2` |
|     52 |  1713 | `static int DomChildNodeOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)` |
|      1 |  1714 | `{` |
|     53 |  1715 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     53 |  1716 | `	phl_domnode *pOne = 0;` |
|     53 |  1717 | `	xmlNodePtr pThis,pParent,pViable,pRef,pSingle = 0;` |
|      - |  1718 | `	SySet sList;` |
|     53 |  1719 | `	int iErr,rc = PH7_OK;` |
|     53 |  1720 | `	if( DomNodesScreen(pCtx,nArg,apArg) \|\| pNd == 0 ){` |
|      5 |  1721 | `		return PH7_OK;` |
|      - |  1722 | `	}` |
|     49 |  1723 | `	pThis = (xmlNodePtr)pNd->pNode;` |
|     49 |  1724 | `	pParent = pThis->parent;` |
|     49 |  1725 | `	if( pParent == 0 ){` |
|     11 |  1726 | `		return PH7_OK;` |
|      - |  1727 | `	}` |
|     38 |  1728 | `	if( iMode == DOM_CN_REPLACE` |
|     25 |  1729 | `	 && (DomNodeReadOnly(pThis) \|\| DomNodeReadOnly(pParent)) ){` |
|      - |  1730 | `		/* replaceWith carries the read-only refusal (it REMOVES the receiver)` |
|      - |  1731 | `		 * where before/after do not: a text child of a constructed ownerless` |
|      - |  1732 | `		 * element takes before() and refuses replaceWith(). The parentless` |
|      - |  1733 | `		 * silence above still answers first -- a constructed ROOT is a silent` |
|      - |  1734 | `		 * no-op, not this refusal. */` |
|    ! 0 |  1735 | `		return DomThrowVoid(pCtx,DOM_ERR_NO_MOD);` |
|      - |  1736 | `	}` |
|     39 |  1737 | `	if( iMode == DOM_CN_BEFORE ){` |
|     19 |  1738 | `		pViable = pThis->prev;` |
|     21 |  1739 | `		while( pViable && DomArgListHasNode(nArg,apArg,pViable) ){` |
|      3 |  1740 | `			pViable = pViable->prev;` |
|      1 |  1741 | `		}` |
|     10 |  1742 | `	}else{` |
|     21 |  1743 | `		pViable = pThis->next;` |
|     25 |  1744 | `		while( pViable && DomArgListHasNode(nArg,apArg,pViable) ){` |
|      5 |  1745 | `			pViable = pViable->next;` |
|      1 |  1746 | `		}` |
|      - |  1747 | `	}` |
|     39 |  1748 | `	SySetInit(&sList,&pCtx->pVm->sAllocator,sizeof(xmlNodePtr));` |
|     39 |  1749 | `	if( nArg == 1 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){` |
|     17 |  1750 | `		pOne = DomObjArg(apArg[0]);` |
|     17 |  1751 | `		if( pOne == 0 ){` |
|    ! 0 |  1752 | `			return PH7_OK;` |
|      - |  1753 | `		}` |
|     17 |  1754 | `		pSingle = (xmlNodePtr)pOne->pNode;` |
|     31 |  1755 | `	}else if( DomNodesConvert(pCtx,pNd->pShell,pParent,nArg,apArg,&sList,&rc) ){` |
|      5 |  1756 | `		SySetRelease(&sList);` |
|      5 |  1757 | `		return rc;` |
|      - |  1758 | `	}` |
|     35 |  1759 | `	iErr = DomInsertValidity(pParent,pSingle,&sList);` |
|     35 |  1760 | `	if( iErr ){` |
|      5 |  1761 | `		SySetRelease(&sList);` |
|      5 |  1762 | `		return DomThrowVoid(pCtx,iErr);` |
|      - |  1763 | `	}` |
|     31 |  1764 | `	if( pOne ){` |
|      - |  1765 | `		/* The single-node shortcut skipped the conversion's wrapper re-home:` |
|      - |  1766 | `		 * an ownerless argument merging into an ownerless receiver's tree, the` |
|      - |  1767 | `		 * only mismatch the validity lets through. */` |
|     17 |  1768 | `		DomAdoptIntoRecv(pCtx,apArg[0],pOne);` |
|      8 |  1769 | `	}` |
|     31 |  1770 | `	if( iMode == DOM_CN_BEFORE ){` |
|      - |  1771 | `		/* Step 5: the viable previous sibling's NEXT -- the parent's first` |
|      - |  1772 | `		 * child when there is none -- both read after conversion. */` |
|     15 |  1773 | `		pRef = pViable ? pViable->next : pParent->children;` |
|      8 |  1774 | `	}else{` |
|     17 |  1775 | `		pRef = pViable;` |
|      - |  1776 | `	}` |
|     31 |  1777 | `	if( iMode == DOM_CN_REPLACE ){` |
|      - |  1778 | `		/* php unlinks the receiver unless conversion already took it. */` |
|      9 |  1779 | `		if( pThis != pSingle && !DomListHas(&sList,pThis) ){` |
|      7 |  1780 | `			xmlUnlinkNode(pThis);` |
|      7 |  1781 | `			DomOrphanAdd(pNd->pShell,pThis);` |
|      3 |  1782 | `		}` |
|      4 |  1783 | `	}` |
|     31 |  1784 | `	if( pSingle ){` |
|     17 |  1785 | `		if( DomIsFragment(pSingle) ){` |
|      3 |  1786 | `			DomFragMove(pOne->pShell,pParent,pSingle,pRef);` |
|      2 |  1787 | `		}else{` |
|     15 |  1788 | `			if( pRef == pSingle ){` |
|      3 |  1789 | `				pRef = pSingle->next;` |
|      1 |  1790 | `			}` |
|     15 |  1791 | `			DomDetach(pOne->pShell,pSingle);` |
|     15 |  1792 | `			if( pRef ){` |
|     13 |  1793 | `				DomLinkBefore(pParent,pSingle,pRef);` |
|      7 |  1794 | `			}else{` |
|      3 |  1795 | `				DomLinkLast(pParent,pSingle);` |
|      - |  1796 | `			}` |
|     15 |  1797 | `			DomNsOnInsertEx(pSingle,0);` |
|      - |  1798 | `		}` |
|      9 |  1799 | `	}else{` |
|     15 |  1800 | `		DomNodesPlace(pNd->pShell,pParent,pRef,&sList);` |
|      - |  1801 | `	}` |
|     31 |  1802 | `	SySetRelease(&sList);` |
|     31 |  1803 | `	return PH7_OK;` |
|     27 |  1804 | `}` |
|     26 |  1805 | `DOM_METHOD(vm_builtin_Dom_before)` |
|      1 |  1806 | `{` |
|     27 |  1807 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_BEFORE);` |
|      1 |  1808 | `}` |
|     12 |  1809 | `DOM_METHOD(vm_builtin_Dom_after)` |
|      1 |  1810 | `{` |
|     13 |  1811 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_AFTER);` |
|      1 |  1812 | `}` |
|     14 |  1813 | `DOM_METHOD(vm_builtin_Dom_replaceWith)` |
|      1 |  1814 | `{` |
|     15 |  1815 | `	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_REPLACE);` |
|      1 |  1816 | `}` |
|      - |  1817 | `/*` |
|      - |  1818 | ` * DOMChildNode::remove(): void -- and php's asymmetry: where before() on a` |
|      - |  1819 | ` * parentless node is a silent no-op, remove() is the Not Found refusal, in` |
|      - |  1820 | ` * whichever mode the document is in.` |
|      - |  1821 | ` */` |
|     10 |  1822 | `DOM_METHOD(vm_builtin_Dom_removeSelf)` |
|      1 |  1823 | `{` |
|     11 |  1824 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      - |  1825 | `	xmlNodePtr pNode;` |
|      5 |  1826 | `	SXUNUSED(nArg);` |
|      5 |  1827 | `	SXUNUSED(apArg);` |
|     11 |  1828 | `	if( pNd == 0 ){` |
|    ! 0 |  1829 | `		return PH7_OK;` |
|      - |  1830 | `	}` |
|     11 |  1831 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|      - |  1832 | `	/* The read-only refusal answers BEFORE the parentless one: a constructed` |
|      - |  1833 | `	 * ownerless node -- necessarily parentless -- is No Modification Allowed` |
|      - |  1834 | `	 * here, where an owned parentless node is Not Found. */` |
|     11 |  1835 | `	if( DomNodeReadOnly(pNode) \|\| (pNode->parent && DomNodeReadOnly(pNode->parent)) ){` |
|      3 |  1836 | `		return DomThrowVoid(pCtx,DOM_ERR_NO_MOD);` |
|      - |  1837 | `	}` |
|      9 |  1838 | `	if( pNode->parent == 0 ){` |
|      5 |  1839 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  1840 | `	}` |
|      5 |  1841 | `	xmlUnlinkNode(pNode);` |
|      5 |  1842 | `	DomOrphanAdd(pNd->pShell,pNode);` |
|      5 |  1843 | `	return PH7_OK;` |
|      6 |  1844 | `}` |
|      - |  1845 |  |
|      - |  1846 | `/* DOMNode::hasChildNodes(): bool / hasAttributes(): bool / getLineNo(): int */` |
|      4 |  1847 | `DOM_METHOD(vm_builtin_DOMNode_hasChildNodes)` |
|      1 |  1848 | `{` |
|      5 |  1849 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      2 |  1850 | `	SXUNUSED(nArg);` |
|      2 |  1851 | `	SXUNUSED(apArg);` |
|      5 |  1852 | `	ph7_result_bool(pCtx,pNd && DomNodeChildFirst((xmlNodePtr)pNd->pNode) != 0);` |
|      5 |  1853 | `	return PH7_OK;` |
|      1 |  1854 | `}` |
|      - |  1855 | `/* The attribute list of an element (empty for anything else). */` |
|    242 |  1856 | `static xmlAttrPtr DomAttrList(xmlNodePtr pNode)` |
|      2 |  1857 | `{` |
|    244 |  1858 | `	return (pNode && pNode->type == XML_ELEMENT_NODE) ? pNode->properties : 0;` |
|      2 |  1859 | `}` |
|     16 |  1860 | `static int DomAttrCount(xmlNodePtr pNode)` |
|      1 |  1861 | `{` |
|     17 |  1862 | `	xmlAttrPtr pAttr = DomAttrList(pNode);` |
|     17 |  1863 | `	int iCount = 0;` |
|     41 |  1864 | `	for( ; pAttr ; pAttr = pAttr->next ){` |
|     25 |  1865 | `		iCount++;` |
|     13 |  1866 | `	}` |
|     17 |  1867 | `	return iCount;` |
|      1 |  1868 | `}` |
|    208 |  1869 | `static xmlAttrPtr DomAttrAt(xmlNodePtr pNode,int iWant)` |
|      2 |  1870 | `{` |
|    210 |  1871 | `	xmlAttrPtr pAttr = DomAttrList(pNode);` |
|    330 |  1872 | `	for( ; pAttr && iWant > 0 ; pAttr = pAttr->next ){` |
|    122 |  1873 | `		iWant--;` |
|     62 |  1874 | `	}` |
|    210 |  1875 | `	return pAttr;` |
|      2 |  1876 | `}` |
|    ! 0 |  1877 | `DOM_METHOD(vm_builtin_DOMNode_hasAttributes)` |
|    ! 0 |  1878 | `{` |
|    ! 0 |  1879 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    ! 0 |  1880 | `	SXUNUSED(nArg);` |
|    ! 0 |  1881 | `	SXUNUSED(apArg);` |
|    ! 0 |  1882 | `	ph7_result_bool(pCtx,pNd && DomAttrCount((xmlNodePtr)pNd->pNode) > 0);` |
|    ! 0 |  1883 | `	return PH7_OK;` |
|    ! 0 |  1884 | `}` |
|      8 |  1885 | `DOM_METHOD(vm_builtin_DOMNode_getLineNo)` |
|      1 |  1886 | `{` |
|      9 |  1887 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      4 |  1888 | `	SXUNUSED(nArg);` |
|      4 |  1889 | `	SXUNUSED(apArg);` |
|      9 |  1890 | `	ph7_result_int64(pCtx,pNd ? (ph7_int64)xmlGetLineNo((xmlNodePtr)pNd->pNode) : 0);` |
|      9 |  1891 | `	return PH7_OK;` |
|      1 |  1892 | `}` |
|      - |  1893 | `/* DOMNode::isSameNode(DOMNode $otherNode): bool -- pointer identity, which is` |
|      - |  1894 | ` * also the identity the wrapper cache keys on. */` |
|      6 |  1895 | `DOM_METHOD(vm_builtin_DOMNode_isSameNode)` |
|      1 |  1896 | `{` |
|      7 |  1897 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  1898 | `	phl_domnode *pOther = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|      7 |  1899 | `	ph7_result_bool(pCtx,pNd != 0 && pOther != 0 && pNd->pNode == pOther->pNode);` |
|      7 |  1900 | `	return PH7_OK;` |
|      1 |  1901 | `}` |
|      - |  1902 |  |
|      - |  1903 | `/* ===== Position, containment and structural equality ===== */` |
|      - |  1904 |  |
|      - |  1905 | `/* php's DOMNode::DOCUMENT_POSITION_* -- the DOM's own bit values. */` |
|      - |  1906 | `#define DOM_POS_DISCONNECTED 1` |
|      - |  1907 | `#define DOM_POS_PRECEDING    2` |
|      - |  1908 | `#define DOM_POS_FOLLOWING    4` |
|      - |  1909 | `#define DOM_POS_CONTAINS     8` |
|      - |  1910 | `#define DOM_POS_CONTAINED_BY 16` |
|      - |  1911 | `#define DOM_POS_IMPL_SPEC    32` |
|      - |  1912 |  |
|      - |  1913 | `/* The topmost node reachable by parent links -- the DOCUMENT for a node in a` |
|      - |  1914 | ` * tree, and the outermost detached node otherwise. */` |
|     66 |  1915 | `static xmlNodePtr DomRootOf(xmlNodePtr pNode)` |
|      1 |  1916 | `{` |
|    155 |  1917 | `	while( pNode && pNode->parent ){` |
|     89 |  1918 | `		pNode = pNode->parent;` |
|      1 |  1919 | `	}` |
|     67 |  1920 | `	return pNode;` |
|      1 |  1921 | `}` |
|      - |  1922 | `/* Is pAnc a STRICT ancestor of pNode? libxml parents an attribute at its` |
|      - |  1923 | `` * element, which is how php answers true for `$el->contains($el->attr)`. */`` |
|     38 |  1924 | `static int DomIsAncestorOf(xmlNodePtr pAnc,xmlNodePtr pNode)` |
|      1 |  1925 | `{` |
|     39 |  1926 | `	xmlNodePtr p = pNode ? pNode->parent : 0;` |
|     75 |  1927 | `	for( ; p ; p = p->parent ){` |
|     51 |  1928 | `		if( p == pAnc ){` |
|     15 |  1929 | `			return 1;` |
|      - |  1930 | `		}` |
|     19 |  1931 | `	}` |
|     25 |  1932 | `	return 0;` |
|     20 |  1933 | `}` |
|     24 |  1934 | `static int DomDepthOf(xmlNodePtr pNode)` |
|      1 |  1935 | `{` |
|     25 |  1936 | `	int n = 0;` |
|     93 |  1937 | `	for( ; pNode ; pNode = pNode->parent ){` |
|     69 |  1938 | `		n++;` |
|     35 |  1939 | `	}` |
|     25 |  1940 | `	return n;` |
|      1 |  1941 | `}` |
|      - |  1942 | `/*` |
|      - |  1943 | ` * Does pA come before pB in document order? Both are distinct nodes of one` |
|      - |  1944 | ` * tree. Lifting each to the depth of the other either lands on the SAME node --` |
|      - |  1945 | ` * one is an ancestor of the other, and an ancestor comes first in a preorder` |
|      - |  1946 | ` * walk -- or, after stepping up in lockstep, on two distinct children of one` |
|      - |  1947 | ` * parent, whose child-list order is the answer.` |
|      - |  1948 | ` *` |
|      - |  1949 | ` * The ancestor case is reachable even though compareDocumentPosition answers` |
|      - |  1950 | ` * CONTAINS/CONTAINED_BY for it: an ATTRIBUTE folds onto its element first, so` |
|      - |  1951 | `` * `$root->attr` against `$child->attr` arrives here as the element PAIR with`` |
|      - |  1952 | ` * one of them an ancestor of the other.` |
|      - |  1953 | ` */` |
|     12 |  1954 | `static int DomPrecedesInTree(xmlNodePtr pA,xmlNodePtr pB)` |
|      1 |  1955 | `{` |
|     13 |  1956 | `	int nA = DomDepthOf(pA),nB = DomDepthOf(pB);` |
|     13 |  1957 | `	xmlNodePtr pUpA = pA,pUpB = pB,p;` |
|     15 |  1958 | `	while( nA > nB ){ pUpA = pUpA->parent; nA--; }` |
|     15 |  1959 | `	while( nB > nA ){ pUpB = pUpB->parent; nB--; }` |
|     13 |  1960 | `	if( pUpA == pUpB ){` |
|      5 |  1961 | `		return pUpA == pA;   /* pA was not lifted: it is the ancestor */` |
|      - |  1962 | `	}` |
|      9 |  1963 | `	while( pUpA && pUpB && pUpA->parent != pUpB->parent ){` |
|    ! 0 |  1964 | `		pUpA = pUpA->parent;` |
|    ! 0 |  1965 | `		pUpB = pUpB->parent;` |
|    ! 0 |  1966 | `	}` |
|      9 |  1967 | `	for( p = pUpA ? pUpA->prev : 0 ; p ; p = p->prev ){` |
|      5 |  1968 | `		if( p == pUpB ){` |
|      5 |  1969 | `			return 0;   /* pB is an earlier sibling */` |
|      - |  1970 | `		}` |
|    ! 0 |  1971 | `	}` |
|      5 |  1972 | `	return 1;` |
|      7 |  1973 | `}` |
|      - |  1974 | `/*` |
|      - |  1975 | ` * DOMNode::compareDocumentPosition(DOMNode $other): int` |
|      - |  1976 | ` *` |
|      - |  1977 | `` * The DOM's own algorithm, run with `other` as node1 and the receiver as node2.`` |
|      - |  1978 | ` * An ATTRIBUTE is folded onto its element first, which is what makes an` |
|      - |  1979 | `` * attribute answer `CONTAINED_BY\|FOLLOWING` against its own element and`` |
|      - |  1980 | `` * `IMPLEMENTATION_SPECIFIC` plus the attribute-list order against a sibling`` |
|      - |  1981 | ` * attribute -- and what makes it compare as its element against everything else.` |
|      - |  1982 | ` *` |
|      - |  1983 | ` * Two nodes in different trees are DISCONNECTED, and the direction bit there is` |
|      - |  1984 | ` * php's own: the raw node POINTERS, which is the only thing available and which` |
|      - |  1985 | ` * php marks IMPLEMENTATION_SPECIFIC for exactly that reason. The bit is stable` |
|      - |  1986 | ` * and antisymmetric within one process; it is not comparable ACROSS engines,` |
|      - |  1987 | ` * so no test pins it.` |
|      - |  1988 | ` */` |
|     32 |  1989 | `DOM_METHOD(vm_builtin_DOMNode_compareDocumentPosition)` |
|      1 |  1990 | `{` |
|     33 |  1991 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     33 |  1992 | `	phl_domnode *pOtherNd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     33 |  1993 | `	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     33 |  1994 | `	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;` |
|     33 |  1995 | `	xmlNodePtr pNode1,pNode2,pAttr1 = 0,pAttr2 = 0;` |
|     33 |  1996 | `	if( pThisNode == 0 \|\| pOther == 0 ){` |
|     10 |  1997 | `		ph7_result_int(pCtx,0);` |
|     10 |  1998 | `		return PH7_OK;` |
|      - |  1999 | `	}` |
|     43 |  2000 | `	if( pThisNode == pOther ){` |
|      3 |  2001 | `		ph7_result_int(pCtx,0);` |
|      3 |  2002 | `		return PH7_OK;` |
|      - |  2003 | `	}` |
|     41 |  2004 | `	pNode1 = pOther;` |
|     41 |  2005 | `	pNode2 = pThisNode;` |
|     41 |  2006 | `	if( pNode1->type == XML_ATTRIBUTE_NODE ){` |
|     13 |  2007 | `		pAttr1 = pNode1;` |
|     13 |  2008 | `		pNode1 = pNode1->parent;` |
|      6 |  2009 | `	}` |
|     41 |  2010 | `	if( pNode2->type == XML_ATTRIBUTE_NODE ){` |
|     13 |  2011 | `		pAttr2 = pNode2;` |
|     13 |  2012 | `		pNode2 = pNode2->parent;` |
|     13 |  2013 | `		if( pAttr1 && pNode1 && pNode1 == pNode2 ){` |
|      - |  2014 | `			xmlAttrPtr pAttr;` |
|      5 |  2015 | `			for( pAttr = pNode2->properties ; pAttr ; pAttr = pAttr->next ){` |
|      5 |  2016 | `				if( (xmlNodePtr)pAttr == pAttr1 ){` |
|      3 |  2017 | `					ph7_result_int(pCtx,DOM_POS_IMPL_SPEC\|DOM_POS_PRECEDING);` |
|      3 |  2018 | `					return PH7_OK;` |
|      - |  2019 | `				}` |
|      3 |  2020 | `				if( (xmlNodePtr)pAttr == pAttr2 ){` |
|      3 |  2021 | `					ph7_result_int(pCtx,DOM_POS_IMPL_SPEC\|DOM_POS_FOLLOWING);` |
|      3 |  2022 | `					return PH7_OK;` |
|      - |  2023 | `				}` |
|    ! 0 |  2024 | `			}` |
|    ! 0 |  2025 | `		}` |
|      4 |  2026 | `	}` |
|     37 |  2027 | `	if( pNode1 == 0 \|\| pNode2 == 0 \|\| DomRootOf(pNode1) != DomRootOf(pNode2) ){` |
|     47 |  2028 | `		ph7_result_int(pCtx,DOM_POS_DISCONNECTED\|DOM_POS_IMPL_SPEC` |
|     24 |  2029 | `			\|((sxuptr)pThisNode > (sxuptr)pOther ? DOM_POS_PRECEDING : DOM_POS_FOLLOWING));` |
|     25 |  2030 | `		return PH7_OK;` |
|      - |  2031 | `	}` |
|     36 |  2032 | `	if( (pAttr1 == 0 && DomIsAncestorOf(pNode1,pNode2))` |
|     36 |  2033 | `	 \|\| (pAttr2 != 0 && pNode1 == pNode2) ){` |
|     15 |  2034 | `		ph7_result_int(pCtx,DOM_POS_CONTAINS\|DOM_POS_PRECEDING);` |
|     15 |  2035 | `		return PH7_OK;` |
|      - |  2036 | `	}` |
|     24 |  2037 | `	if( (pAttr2 == 0 && DomIsAncestorOf(pNode2,pNode1))` |
|     23 |  2038 | `	 \|\| (pAttr1 != 0 && pNode1 == pNode2) ){` |
|     13 |  2039 | `		ph7_result_int(pCtx,DOM_POS_CONTAINED_BY\|DOM_POS_FOLLOWING);` |
|     13 |  2040 | `		return PH7_OK;` |
|      - |  2041 | `	}` |
|     13 |  2042 | `	ph7_result_int(pCtx,DomPrecedesInTree(pNode1,pNode2)` |
|      - |  2043 | `		? DOM_POS_PRECEDING : DOM_POS_FOLLOWING);` |
|     13 |  2044 | `	return PH7_OK;` |
|     27 |  2045 | `}` |
|      - |  2046 | `/* DOMNode::contains(DOMNode\|DOMNameSpaceNode\|null $other): bool -- INCLUSIVE` |
|      - |  2047 | ` * descendant, so a node contains itself, and (libxml parenting attributes) an` |
|      - |  2048 | ` * element contains its own attributes. A namespace DECLARATION is asked about` |
|      - |  2049 | ` * through the element that MAKES it: its own pointer is an xmlNs, which is in` |
|      - |  2050 | ` * no tree at all. */` |
|     18 |  2051 | `DOM_METHOD(vm_builtin_DOMNode_contains)` |
|      1 |  2052 | `{` |
|     19 |  2053 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     19 |  2054 | `	ph7_value *pArg = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? apArg[0] : 0;` |
|     19 |  2055 | `	phl_domnode *pOtherNd = pArg ? DomObjArg(pArg) : 0;` |
|     19 |  2056 | `	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     19 |  2057 | `	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;` |
|     19 |  2058 | `	phl_domnode *pOwnerNd = pArg ? DomNsNodeOwner(pArg) : 0;` |
|     19 |  2059 | `	if( pOwnerNd ){` |
|      3 |  2060 | `		pOther = (xmlNodePtr)pOwnerNd->pNode;` |
|      1 |  2061 | `	}` |
|     33 |  2062 | `	ph7_result_bool(pCtx,pThisNode != 0 && pOther != 0` |
|     23 |  2063 | `		&& (pThisNode == pOther \|\| DomIsAncestorOf(pThisNode,pOther)));` |
|     19 |  2064 | `	return PH7_OK;` |
|      1 |  2065 | `}` |
|      - |  2066 | `/* DOMNode::getRootNode(?array $options = null): DOMNode -- php declares the` |
|      - |  2067 | `` * options array (the shadow-DOM `composed` key) and reads nothing from it. */`` |
|     14 |  2068 | `DOM_METHOD(vm_builtin_DOMNode_getRootNode)` |
|      1 |  2069 | `{` |
|     15 |  2070 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  2071 | `	SXUNUSED(nArg);` |
|      7 |  2072 | `	SXUNUSED(apArg);` |
|     15 |  2073 | `	return DomResultNodeOf(pCtx,pNd,DomRootOf(pNd ? (xmlNodePtr)pNd->pNode : 0));` |
|      1 |  2074 | `}` |
|      - |  2075 | `/*` |
|      - |  2076 | ` * DOMNode::isSupported(string $feature, string $version): bool -- the DOM Level` |
|      - |  2077 | `` * 1 feature test, and php's whole table is two rows: `XML` at 1.0 or 2.0 and`` |
|      - |  2078 | `` * `Core` at 1.0 (never `Core` at 2.0). The feature name folds case, the version`` |
|      - |  2079 | ` * does not.` |
|      - |  2080 | ` */` |
|     20 |  2081 | `DOM_METHOD(vm_builtin_DOMNode_isSupported)` |
|      1 |  2082 | `{` |
|     21 |  2083 | `	const char *zFeature = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|     21 |  2084 | `	const char *zVersion = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     21 |  2085 | `	int bOne = DomNameIs(zVersion,"1.0");` |
|     21 |  2086 | `	int bTwo = DomNameIs(zVersion,"2.0");` |
|     21 |  2087 | `	int bXml = SyStrlen(zFeature) == 3 && SyStrnicmp(zFeature,"XML",3) == 0;` |
|     21 |  2088 | `	int bCore = SyStrlen(zFeature) == 4 && SyStrnicmp(zFeature,"Core",4) == 0;` |
|     21 |  2089 | `	ph7_result_bool(pCtx,(bXml && (bOne \|\| bTwo)) \|\| (bCore && bOne));` |
|     21 |  2090 | `	return PH7_OK;` |
|      1 |  2091 | `}` |
|      - |  2092 | `/*` |
|      - |  2093 | ` * php's structural equality, which is NOT the DOM spec's to the letter.` |
|      - |  2094 | ` *` |
|      - |  2095 | ` * The type has to match, then the per-kind identity: an ELEMENT compares its` |
|      - |  2096 | `` * namespace URI, its PREFIX and its local name (so `p:m` and `q:m` bound to the`` |
|      - |  2097 | ` * one URI are NOT equal) plus its attributes as a SET -- same count, and every` |
|      - |  2098 | ` * attribute matched by namespace, local name and value regardless of order. An` |
|      - |  2099 | ` * ATTRIBUTE compares its namespace URI, its name and its value and NOT its` |
|      - |  2100 | ` * prefix, which is the asymmetry no reading of the spec predicts. A PI compares` |
|      - |  2101 | ` * target and data, character data its content, an entity REFERENCE its name.` |
|      - |  2102 | ` * Then the children, in order and in the same number.` |
|      - |  2103 | ` */` |
|    228 |  2104 | `static int DomStrEqOrBothNull(const xmlChar *zA,const xmlChar *zB)` |
|      1 |  2105 | `{` |
|    229 |  2106 | `	if( zA == 0 \|\| zB == 0 ){` |
|     91 |  2107 | `		return zA == zB;` |
|      - |  2108 | `	}` |
|    139 |  2109 | `	return xmlStrEqual(zA,zB) != 0;` |
|    115 |  2110 | `}` |
|    128 |  2111 | `static void DomNsHrefOf(xmlNodePtr pNode,const xmlChar **pzHref,const xmlChar **pzPrefix)` |
|      1 |  2112 | `{` |
|    129 |  2113 | `	*pzHref = (pNode->ns && pNode->ns->href) ? pNode->ns->href : 0;` |
|    129 |  2114 | `	*pzPrefix = (pNode->ns && pNode->ns->prefix) ? pNode->ns->prefix : 0;` |
|    129 |  2115 | `}` |
|     26 |  2116 | `static int DomAttrValueEq(xmlNodePtr pA,xmlNodePtr pB)` |
|      1 |  2117 | `{` |
|     27 |  2118 | `	xmlChar *zA = xmlNodeGetContent(pA);` |
|     27 |  2119 | `	xmlChar *zB = xmlNodeGetContent(pB);` |
|     27 |  2120 | `	int bEq = DomStrEqOrBothNull(zA,zB);` |
|     27 |  2121 | `	if( zA ){ xmlFree(zA); }` |
|     27 |  2122 | `	if( zB ){ xmlFree(zB); }` |
|     27 |  2123 | `	return bEq;` |
|      1 |  2124 | `}` |
|     28 |  2125 | `static int DomAttrSetEqual(xmlNodePtr pA,xmlNodePtr pB)` |
|      1 |  2126 | `{` |
|      - |  2127 | `	xmlAttrPtr pOne,pTwo;` |
|     29 |  2128 | `	int nA = 0,nB = 0;` |
|     63 |  2129 | `	for( pOne = pA->properties ; pOne ; pOne = pOne->next ){ nA++; }` |
|     57 |  2130 | `	for( pTwo = pB->properties ; pTwo ; pTwo = pTwo->next ){ nB++; }` |
|     29 |  2131 | `	if( nA != nB ){` |
|      5 |  2132 | `		return 0;` |
|      - |  2133 | `	}` |
|     47 |  2134 | `	for( pOne = pA->properties ; pOne ; pOne = pOne->next ){` |
|      - |  2135 | `		const xmlChar *zHrefA,*zPfxA,*zHrefB,*zPfxB;` |
|     27 |  2136 | `		DomNsHrefOf((xmlNodePtr)pOne,&zHrefA,&zPfxA);` |
|     35 |  2137 | `		for( pTwo = pB->properties ; pTwo ; pTwo = pTwo->next ){` |
|     31 |  2138 | `			DomNsHrefOf((xmlNodePtr)pTwo,&zHrefB,&zPfxB);` |
|     30 |  2139 | `			if( DomStrEqOrBothNull(zHrefA,zHrefB)` |
|     29 |  2140 | `			 && DomStrEqOrBothNull(pOne->name,pTwo->name)` |
|     27 |  2141 | `			 && DomAttrValueEq((xmlNodePtr)pOne,(xmlNodePtr)pTwo) ){` |
|     23 |  2142 | `				break;` |
|      - |  2143 | `			}` |
|      5 |  2144 | `		}` |
|     27 |  2145 | `		if( pTwo == 0 ){` |
|      5 |  2146 | `			return 0;` |
|      - |  2147 | `		}` |
|     12 |  2148 | `	}` |
|     21 |  2149 | `	return 1;` |
|     15 |  2150 | `}` |
|     86 |  2151 | `static int DomNodesEqual(xmlNodePtr pA,xmlNodePtr pB)` |
|      1 |  2152 | `{` |
|      - |  2153 | `	xmlNodePtr pKidA,pKidB;` |
|      - |  2154 | `	const xmlChar *zHrefA,*zPfxA,*zHrefB,*zPfxB;` |
|     87 |  2155 | `	if( pA == 0 \|\| pB == 0 ){` |
|    ! 0 |  2156 | `		return pA == pB;` |
|      - |  2157 | `	}` |
|     87 |  2158 | `	if( pA->type != pB->type ){` |
|      3 |  2159 | `		return 0;` |
|      - |  2160 | `	}` |
|     85 |  2161 | `	switch( pA->type ){` |
|     17 |  2162 | `	case XML_ELEMENT_NODE:` |
|     35 |  2163 | `		DomNsHrefOf(pA,&zHrefA,&zPfxA);` |
|     35 |  2164 | `		DomNsHrefOf(pB,&zHrefB,&zPfxB);` |
|     34 |  2165 | `		if( !DomStrEqOrBothNull(zHrefA,zHrefB) \|\| !DomStrEqOrBothNull(zPfxA,zPfxB)` |
|     33 |  2166 | `		 \|\| !DomStrEqOrBothNull(pA->name,pB->name) \|\| !DomAttrSetEqual(pA,pB) ){` |
|     15 |  2167 | `			return 0;` |
|      - |  2168 | `		}` |
|     21 |  2169 | `		break;` |
|      1 |  2170 | `	case XML_ATTRIBUTE_NODE:` |
|      3 |  2171 | `		DomNsHrefOf(pA,&zHrefA,&zPfxA);` |
|      3 |  2172 | `		DomNsHrefOf(pB,&zHrefB,&zPfxB);` |
|      2 |  2173 | `		if( !DomStrEqOrBothNull(zHrefA,zHrefB)` |
|      2 |  2174 | `		 \|\| !DomStrEqOrBothNull(pA->name,pB->name)` |
|      3 |  2175 | `		 \|\| !DomAttrValueEq(pA,pB) ){` |
|    ! 0 |  2176 | `			return 0;` |
|      - |  2177 | `		}` |
|      - |  2178 | `		/* An attribute's value IS its child list; comparing it twice would only` |
|      - |  2179 | `		 * refuse a value split across nodes that reads the same. */` |
|      3 |  2180 | `		return 1;` |
|      4 |  2181 | `	case XML_PI_NODE:` |
|      8 |  2182 | `		if( !DomStrEqOrBothNull(pA->name,pB->name)` |
|      8 |  2183 | `		 \|\| !DomStrEqOrBothNull(pA->content,pB->content) ){` |
|      5 |  2184 | `			return 0;` |
|      - |  2185 | `		}` |
|      5 |  2186 | `		break;` |
|     12 |  2187 | `	case XML_TEXT_NODE:` |
|      - |  2188 | `	case XML_CDATA_SECTION_NODE:` |
|      - |  2189 | `	case XML_COMMENT_NODE:` |
|     25 |  2190 | `		if( !DomStrEqOrBothNull(pA->content,pB->content) ){` |
|      3 |  2191 | `			return 0;` |
|      - |  2192 | `		}` |
|     23 |  2193 | `		break;` |
|      2 |  2194 | `	case XML_ENTITY_REF_NODE:` |
|      - |  2195 | `		/* The reference's NAME is what a program wrote; its children are the` |
|      - |  2196 | `		 * DECLARATION libxml resolved it to, which is not part of the node. */` |
|      5 |  2197 | `		return DomStrEqOrBothNull(pA->name,pB->name);` |
|      6 |  2198 | `	default:` |
|     12 |  2199 | `		break;` |
|      - |  2200 | `	}` |
|     59 |  2201 | `	pKidA = pA->children;` |
|     59 |  2202 | `	pKidB = pB->children;` |
|     91 |  2203 | `	while( pKidA && pKidB ){` |
|     35 |  2204 | `		if( !DomNodesEqual(pKidA,pKidB) ){` |
|      3 |  2205 | `			return 0;` |
|      - |  2206 | `		}` |
|     33 |  2207 | `		pKidA = pKidA->next;` |
|     33 |  2208 | `		pKidB = pKidB->next;` |
|      1 |  2209 | `	}` |
|     57 |  2210 | `	return pKidA == 0 && pKidB == 0;` |
|     44 |  2211 | `}` |
|      - |  2212 | `/* DOMNode::isEqualNode(?DOMNode $otherNode): bool */` |
|     54 |  2213 | `DOM_METHOD(vm_builtin_DOMNode_isEqualNode)` |
|      1 |  2214 | `{` |
|     55 |  2215 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     55 |  2216 | `	phl_domnode *pOtherNd = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|    107 |  2217 | `	ph7_result_bool(pCtx,pNd != 0 && pOtherNd != 0` |
|     53 |  2218 | `		&& DomNodesEqual((xmlNodePtr)pNd->pNode,(xmlNodePtr)pOtherNd->pNode));` |
|     55 |  2219 | `	return PH7_OK;` |
|      1 |  2220 | `}` |
|      - |  2221 |  |
|      - |  2222 | `/* ===== Copying: cloneNode ===== */` |
|      - |  2223 |  |
|      - |  2224 | `/*` |
|      - |  2225 | ` * One node copied the way php copies it.` |
|      - |  2226 | ` *` |
|      - |  2227 | ` * libxml's generic copier has no case for a DTD node and answers NULL there, so` |
|      - |  2228 | `` * `$doc->doctype->cloneNode()` was `false` -- php reaches for xmlCopyDtd`` |
|      - |  2229 | ` * instead, which carries the whole internal subset (its declarations, entities` |
|      - |  2230 | ``  * and notations) across. The copy keeps the SOURCE's document in its `doc` `` |
|      - |  2231 | ` * slot without being linked into it, which is what makes php's cloned doctype` |
|      - |  2232 | `` * still answer an `internalSubset` while its `parentNode` is null.`` |
|      - |  2233 | ` */` |
|     50 |  2234 | `static xmlNodePtr DomCopyNode(xmlNodePtr pNode,xmlDocPtr pDoc,int iExtended)` |
|      1 |  2235 | `{` |
|      - |  2236 | `	xmlNodePtr pCopy;` |
|     51 |  2237 | `	if( pNode->type == XML_DTD_NODE \|\| pNode->type == XML_DOCUMENT_TYPE_NODE ){` |
|      5 |  2238 | `		pCopy = (xmlNodePtr)xmlCopyDtd((xmlDtdPtr)pNode);` |
|      5 |  2239 | `		if( pCopy ){` |
|      5 |  2240 | `			pCopy->doc = pNode->doc;` |
|      2 |  2241 | `		}` |
|      5 |  2242 | `		return pCopy;` |
|      - |  2243 | `	}` |
|     47 |  2244 | `	return xmlDocCopyNode(pNode,pDoc,iExtended);` |
|     26 |  2245 | `}` |
|      - |  2246 |  |
|      - |  2247 | `/*` |
|      - |  2248 | ` * Cloning a DOCUMENT is not cloning a node: php builds a SECOND document --` |
|      - |  2249 | ` * its own tree, its own wrapper, its own identity cache -- so the copy's` |
|      - |  2250 | `` * `documentElement` answers the copy as its `ownerDocument` and appending a`` |
|      - |  2251 | ` * node of the ORIGINAL into it is the Wrong Document Error it would be between` |
|      - |  2252 | ` * any two documents. Everything else is one xmlDocCopyNode into the SAME tree,` |
|      - |  2253 | ` * parked as an orphan like every other node this file creates.` |
|      - |  2254 | ` *` |
|      - |  2255 | ` * The parser directives ride along: php's copy answers the receiver's whole` |
|      - |  2256 | ` * flag block, not the class defaults.` |
|      - |  2257 | ` */` |
|     12 |  2258 | `static int DomCloneDocument(ph7_context *pCtx,phl_domnode *pNd,int bDeep)` |
|      1 |  2259 | `{` |
|     13 |  2260 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  2261 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  2262 | `	ph7_class *pClass;` |
|      - |  2263 | `	ph7_class_instance *pObj;` |
|      - |  2264 | `	phl_xmldoc *pShell;` |
|      - |  2265 | `	phl_domnode *pRes;` |
|      - |  2266 | `	xmlDocPtr pCopy;` |
|     13 |  2267 | `	sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     13 |  2268 | `	pCopy = xmlCopyDoc((xmlDocPtr)pNd->pNode,bDeep ? 1 : 0);` |
|     13 |  2269 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::cloneNode");` |
|     13 |  2270 | `	if( pCopy == 0 ){` |
|    ! 0 |  2271 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  2272 | `		return PH7_OK;` |
|      - |  2273 | `	}` |
|      - |  2274 | `	/* php answers a plain DOMDocument even when the receiver is a subclass of` |
|      - |  2275 | ``	 * one: the copy is built by the extension, not by `new static`. */`` |
|     13 |  2276 | `	pClass = PH7_VmExtractClass(pVm,"DOMDocument",sizeof("DOMDocument")-1,FALSE,0);` |
|     13 |  2277 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|     13 |  2278 | `	pShell = pObj ? PH7_LibxmlNewDoc(pVm,pCopy) : 0;` |
|     13 |  2279 | `	pRes = pShell ? DomNewRes(pVm,pShell,pCopy) : 0;` |
|     13 |  2280 | `	if( pRes == 0 ){` |
|    ! 0 |  2281 | `		if( pShell == 0 ){` |
|    ! 0 |  2282 | `			xmlFreeDoc(pCopy);   /* not registered: nothing else will free it */` |
|    ! 0 |  2283 | `		}` |
|    ! 0 |  2284 | `		if( pObj ){` |
|    ! 0 |  2285 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  2286 | `		}` |
|    ! 0 |  2287 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2288 | `	}` |
|     13 |  2289 | `	DomSetRes(pVm,pObj,pRes);` |
|     13 |  2290 | `	PH7_NativeSetAttrObj(pVm,pObj,DOM_DOC,pObj);` |
|     13 |  2291 | `	if( pThis ){` |
|      - |  2292 | `		ph7_value *pFrom,*pTo;` |
|      - |  2293 | `		/* The seven directives travel as the one word they are stored in. */` |
|     13 |  2294 | `		PH7_NativeSetAttrInt(pVm,pObj,DOM_DFLAGS,PH7_NativeAttrInt(pThis,DOM_DFLAGS));` |
|      - |  2295 | `		/* ...and the registerNodeClass table, which php's copy answers too --` |
|      - |  2296 | `		 * shared copy-on-write, which the map's own writer separates. */` |
|     13 |  2297 | `		pFrom = PH7_NativeAttr(pThis,DOM_NCLS);` |
|     13 |  2298 | `		pTo = PH7_NativeAttr(pObj,DOM_NCLS);` |
|     13 |  2299 | `		if( pFrom && pTo && (pFrom->iFlags & MEMOBJ_HASHMAP) ){` |
|      3 |  2300 | `			PH7_MemObjStore(pFrom,pTo);` |
|      1 |  2301 | `		}` |
|      6 |  2302 | `	}` |
|     13 |  2303 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     13 |  2304 | `	return PH7_OK;` |
|      7 |  2305 | `}` |
|      - |  2306 | `/*` |
|      - |  2307 | ` * DOMNode::cloneNode(bool $deep = false): DOMNode\|false` |
|      - |  2308 | ` *` |
|      - |  2309 | `` * The SHALLOW copy is not libxml's shallow copy: php asks for `extended = 2`,`` |
|      - |  2310 | `` * which carries an element's attributes and its own `xmlns` declarations across`` |
|      - |  2311 | `` * while leaving the children behind -- so `$el->cloneNode()` is a usable`` |
|      - |  2312 | `` * template row, not a bare tag. A deep one is `extended = 1`, and libxml then`` |
|      - |  2313 | ` * reconciles whatever namespace the descendants were using onto the copy.` |
|      - |  2314 | ` */` |
|     44 |  2315 | `DOM_METHOD(vm_builtin_DOMNode_cloneNode)` |
|      1 |  2316 | `{` |
|     45 |  2317 | `	ph7_vm *pVm = pCtx->pVm;` |
|     45 |  2318 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     45 |  2319 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     45 |  2320 | `	int bDeep = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;` |
|      - |  2321 | `	xmlNodePtr pCopy;` |
|      - |  2322 | `	sxu32 nMark;` |
|     45 |  2323 | `	if( pNode == 0 ){` |
|    ! 0 |  2324 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  2325 | `		return PH7_OK;` |
|      - |  2326 | `	}` |
|     45 |  2327 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|     13 |  2328 | `		return DomCloneDocument(pCtx,pNd,bDeep);` |
|      - |  2329 | `	}` |
|     33 |  2330 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     33 |  2331 | `	pCopy = DomCopyNode(pNode,pNode->doc,bDeep ? 1 : 2);` |
|     33 |  2332 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::cloneNode");` |
|     33 |  2333 | `	if( pCopy == 0 ){` |
|      5 |  2334 | `		ph7_result_bool(pCtx,0);` |
|      5 |  2335 | `		return PH7_OK;` |
|      - |  2336 | `	}` |
|      - |  2337 | `	/* An ATTRIBUTE copy comes back in NO namespace: libxml resolves an` |
|      - |  2338 | `	 * attribute's prefix against the element it is being copied ONTO, and there` |
|      - |  2339 | ``	 * is no element here. php's clone keeps the namespace, so `p:at="1"` cloned`` |
|      - |  2340 | ``	 * stays `p:at="1"` rather than turning into `at="1"` -- a silent rename of`` |
|      - |  2341 | `	 * the very attribute a namespaced document is keyed on. The copy borrows the` |
|      - |  2342 | `	 * SOURCE's declaration, which is the only thing it can do: an attribute` |
|      - |  2343 | ``	 * carries no `nsDef` of its own, and the declaration outlives it (nothing in`` |
|      - |  2344 | `	 * this file frees a node before its document). */` |
|     29 |  2345 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pCopy->ns == 0 ){` |
|      3 |  2346 | `		pCopy->ns = pNode->ns;` |
|      1 |  2347 | `	}` |
|     29 |  2348 | `	DomOrphanAdd(pNd->pShell,pCopy);` |
|     29 |  2349 | `	return DomResultNodeOf(pCtx,pNd,pCopy);` |
|     23 |  2350 | `}` |
|      - |  2351 | `/* Enter one wrapper into a holder's identity cache, keyed by the node pointer.` |
|      - |  2352 | ` * The cache takes its OWN reference; the caller keeps whatever it holds. */` |
|    254 |  2353 | `static void DomCacheStore(ph7_vm *pVm,ph7_class_instance *pDoc,xmlNodePtr pNode,` |
|      - |  2354 | `	ph7_class_instance *pObj)` |
|      1 |  2355 | `{` |
|    255 |  2356 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|      - |  2357 | `	ph7_value sKey,sVal;` |
|    255 |  2358 | `	if( pCache == 0 ){` |
|    ! 0 |  2359 | `		return;` |
|      - |  2360 | `	}` |
|    255 |  2361 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|    255 |  2362 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    255 |  2363 | `	sVal.x.pOther = pObj;` |
|    255 |  2364 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    255 |  2365 | `	PH7_HashmapInsert(pCache,&sKey,&sVal);` |
|    255 |  2366 | `	PH7_MemObjRelease(&sKey);` |
|    128 |  2367 | `}` |
|      - |  2368 | `/* Empty a slot the instance copied from its clone source: the null value. */` |
|     12 |  2369 | `static void DomSetSlotNull(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxu32 nName)` |
|      1 |  2370 | `{` |
|      - |  2371 | `	ph7_value sNull;` |
|     13 |  2372 | `	PH7_MemObjInit(&(*pVm),&sNull);` |
|     13 |  2373 | `	PH7_NativeSetProp(&(*pVm),pObj,zName,nName,&sNull);` |
|     13 |  2374 | `}` |
|      - |  2375 | `/*` |
|      - |  2376 | `` * `clone $node` / `clone $doc` -- ph7_class::xClone for the DOM classes.`` |
|      - |  2377 | ` *` |
|      - |  2378 | ` * php's clone_obj handler copies the NODE, so the clone is a second SUBTREE and` |
|      - |  2379 | ` * not a second object over the same one.  The slot-by-slot copy that runs` |
|      - |  2380 | ` * before this hook duplicated $__res, and stopping there is the XMLWriter clone` |
|      - |  2381 | ` * bug one family later: a write through either object shows through both.` |
|      - |  2382 | ` *` |
|      - |  2383 | `` * php's rules, measured: the copy is always DEEP (`clone $el` carries the whole`` |
|      - |  2384 | ` * subtree where cloneNode() defaults shallow), always DETACHED, and stays in` |
|      - |  2385 | `` * the SAME document -- `$c->ownerDocument === $d` -- while a DOCUMENT is copied`` |
|      - |  2386 | ` * whole into a second document, directives, declaration and URI included, so` |
|      - |  2387 | ` * mutating the copy's tree leaves the original's bytes alone.  A user subclass` |
|      - |  2388 | ` * clones through the inherited hook and keeps its class and its own properties,` |
|      - |  2389 | ` * php's handler inheritance (the ENGINE's chain walk serves that).` |
|      - |  2390 | ` */` |
|     18 |  2391 | `static void DomInstanceClone(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)` |
|      1 |  2392 | `{` |
|     19 |  2393 | `	phl_domnode *pNd = DomResOf(pSrc);` |
|     19 |  2394 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     19 |  2395 | `	ph7_class_instance *pDoc = PH7_NativeAttrObj(pClone,DOM_DOC);` |
|      - |  2396 | `	xmlNodePtr pCopy;` |
|      - |  2397 | `	phl_domnode *pRes;` |
|     19 |  2398 | `	if( pNode == 0 ){` |
|    ! 0 |  2399 | `		return;   /* no node behind the source: the copy has none either */` |
|      - |  2400 | `	}` |
|     19 |  2401 | `	pCopy = DomCopyNode(pNode,pNode->doc,1);` |
|     19 |  2402 | `	pRes = pCopy ? DomNewRes(&(*pVm),pNd->pShell,pCopy) : 0;` |
|     19 |  2403 | `	if( pRes == 0 ){` |
|      - |  2404 | `		/* Never leave the slot-copied handle in place: two objects over one` |
|      - |  2405 | `		 * node is the exact aliasing this hook exists to prevent. */` |
|      3 |  2406 | `		if( pCopy ){` |
|    ! 0 |  2407 | `			xmlFreeNode(pCopy);` |
|    ! 0 |  2408 | `		}` |
|      3 |  2409 | `		DomSetSlotNull(&(*pVm),pClone,DOM_RES,sizeof(DOM_RES)-1);` |
|      3 |  2410 | `		return;` |
|      - |  2411 | `	}` |
|      - |  2412 | `	/* The same namespace borrow cloneNode() does: an attribute copied with no` |
|      - |  2413 | `	 * element to resolve against comes back in NO namespace. */` |
|     17 |  2414 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pCopy->ns == 0 ){` |
|      3 |  2415 | `		pCopy->ns = pNode->ns;` |
|      1 |  2416 | `	}` |
|     17 |  2417 | `	DomOrphanAdd(pNd->pShell,pCopy);` |
|     17 |  2418 | `	DomSetRes(&(*pVm),pClone,pRes);` |
|      - |  2419 | `	/* The clone IS the copy's wrapper: enter it into the identity cache so` |
|      - |  2420 | ``	 * `$c->firstChild->parentNode === $c` holds. ($__doc rode the slot copy.) */`` |
|     17 |  2421 | `	DomCacheStore(&(*pVm),pDoc,pCopy,pClone);` |
|     10 |  2422 | `}` |
|     10 |  2423 | `static void DomInstanceCloneDoc(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)` |
|      1 |  2424 | `{` |
|     11 |  2425 | `	phl_domnode *pNd = DomResOf(pSrc);` |
|      - |  2426 | `	xmlDocPtr pCopy;` |
|      - |  2427 | `	phl_xmldoc *pShell;` |
|      - |  2428 | `	phl_domnode *pRes;` |
|     11 |  2429 | `	if( pNd == 0 \|\| pNd->pNode == 0 ){` |
|    ! 0 |  2430 | `		return;` |
|      - |  2431 | `	}` |
|     11 |  2432 | `	pCopy = xmlCopyDoc((xmlDocPtr)pNd->pNode,1);` |
|     11 |  2433 | `	pShell = pCopy ? PH7_LibxmlNewDoc(&(*pVm),pCopy) : 0;` |
|     11 |  2434 | `	pRes = pShell ? DomNewRes(&(*pVm),pShell,pCopy) : 0;` |
|     11 |  2435 | `	if( pRes == 0 ){` |
|    ! 0 |  2436 | `		if( pCopy && pShell == 0 ){` |
|    ! 0 |  2437 | `			xmlFreeDoc(pCopy);   /* not registered: nothing else will free it */` |
|    ! 0 |  2438 | `		}` |
|    ! 0 |  2439 | `		DomSetSlotNull(&(*pVm),pClone,DOM_RES,sizeof(DOM_RES)-1);` |
|    ! 0 |  2440 | `		return;` |
|      - |  2441 | `	}` |
|     11 |  2442 | `	DomSetRes(&(*pVm),pClone,pRes);` |
|      - |  2443 | `	/* Its own document, its own identity cache: the slot copy pointed both at` |
|      - |  2444 | `	 * the SOURCE's, so the copy's documentElement would have answered the` |
|      - |  2445 | `	 * original document as its owner. (The directive slots the copy carried` |
|      - |  2446 | `	 * across are php's answer and stay.) */` |
|     11 |  2447 | `	PH7_NativeSetAttrObj(&(*pVm),pClone,DOM_DOC,pClone);` |
|     11 |  2448 | `	DomSetSlotNull(&(*pVm),pClone,DOM_NODES,sizeof(DOM_NODES)-1);` |
|      6 |  2449 | `}` |
|      - |  2450 |  |
|      - |  2451 | `/* ===== The node CONSTRUCTORS: php's ownerless nodes ===== */` |
|      - |  2452 |  |
|      - |  2453 | `/*` |
|      - |  2454 | `` * php gives a constructed node NO document at all -- `(new DOMText('t'))->`` |
|      - |  2455 | `` * ownerDocument` is null and the libxml node's doc is NULL -- and adopts it on`` |
|      - |  2456 | ` * the first insertion.  Until then the node has to be OWNED by something that` |
|      - |  2457 | ` * frees it: the limbo shell, one per VM, a phl_xmldoc with no xmlDoc whose` |
|      - |  2458 | ` * orphan set carries every constructed-and-never-adopted node to teardown.` |
|      - |  2459 | ` */` |
|    238 |  2460 | `static phl_xmldoc * DomLimboShell(ph7_vm *pVm)` |
|      1 |  2461 | `{` |
|    239 |  2462 | `	if( pVm->pXmlLimbo == 0 ){` |
|      3 |  2463 | `		pVm->pXmlLimbo = PH7_LibxmlNewDoc(&(*pVm),0);` |
|      1 |  2464 | `	}` |
|    239 |  2465 | `	return (phl_xmldoc *)pVm->pXmlLimbo;` |
|      1 |  2466 | `}` |
|      - |  2467 | `/*` |
|      - |  2468 | ` * The shared constructor tail: park the fresh node on the limbo shell, wire` |
|      - |  2469 | ` * the instance's two slots, and make the instance its OWN holder -- $__doc` |
|      - |  2470 | ` * points at itself and the identity cache lives on it, exactly the document's` |
|      - |  2471 | `` * own arrangement, so `$e->firstChild->parentNode === $e` holds for a tree`` |
|      - |  2472 | ` * that belongs to no document.  (ownerDocument still answers null: the getter` |
|      - |  2473 | ` * reads the NODE's document, not the slot.)  Takes ownership of pNode either` |
|      - |  2474 | `` * way; a re-run constructor -- `$t->__construct('b')`, which php allows --`` |
|      - |  2475 | ` * simply re-points the slots and leaves the old node parked.` |
|      - |  2476 | ` */` |
|    218 |  2477 | `static int DomCtorInstall(ph7_context *pCtx,xmlNodePtr pNode)` |
|      1 |  2478 | `{` |
|    219 |  2479 | `	ph7_vm *pVm = pCtx->pVm;` |
|    219 |  2480 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  2481 | `	phl_xmldoc *pShell;` |
|      - |  2482 | `	phl_domnode *pRes;` |
|    219 |  2483 | `	if( pNode == 0 ){` |
|    ! 0 |  2484 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2485 | `	}` |
|    219 |  2486 | `	pShell = pThis ? DomLimboShell(pVm) : 0;` |
|    219 |  2487 | `	pRes = pShell ? DomNewRes(pVm,pShell,pNode) : 0;` |
|    219 |  2488 | `	if( pRes == 0 ){` |
|    ! 0 |  2489 | `		xmlFreeNode(pNode);` |
|    ! 0 |  2490 | `		return pThis ? PH7_ContextMemoryError(pCtx) : PH7_OK;` |
|      - |  2491 | `	}` |
|    219 |  2492 | `	DomOrphanAdd(pShell,pNode);` |
|    219 |  2493 | `	DomSetRes(pVm,pThis,pRes);` |
|    219 |  2494 | `	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);` |
|    219 |  2495 | `	DomCacheStore(pVm,pThis,pNode,pThis);` |
|    219 |  2496 | `	return PH7_OK;` |
|    110 |  2497 | `}` |
|      - |  2498 | `/* The content of the character-data three: php passes NULL for an OMITTED` |
|      - |  2499 | ` * argument and the string -- even the empty one -- for a given one, which is` |
|      - |  2500 | `` * why `new DOMText()` has a NULL nodeValue where `new DOMText('')` reads "". */`` |
|     66 |  2501 | `DOM_METHOD(vm_builtin_DOMText_construct)` |
|      1 |  2502 | `{` |
|     67 |  2503 | `	int nData = 0;` |
|     67 |  2504 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : 0;` |
|     67 |  2505 | `	xmlNodePtr pNode = zData` |
|     56 |  2506 | `		? xmlNewDocTextLen(0,(const xmlChar *)zData,nData)` |
|     38 |  2507 | `		: xmlNewDocText(0,0);` |
|     67 |  2508 | `	return DomCtorInstall(pCtx,pNode);` |
|      1 |  2509 | `}` |
|     16 |  2510 | `DOM_METHOD(vm_builtin_DOMComment_construct)` |
|      1 |  2511 | `{` |
|     17 |  2512 | `	int nData = 0;` |
|     17 |  2513 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : 0;` |
|      - |  2514 | `	xmlNodePtr pNode;` |
|     17 |  2515 | `	if( zData ){` |
|      - |  2516 | `		/* libxml has no length-taking comment constructor and` |
|      - |  2517 | `		 * xmlNewDocComment measures with strlen, so a NUL-carrying PHP string` |
|      - |  2518 | `		 * goes through a bounded copy. */` |
|     13 |  2519 | `		xmlChar *zCopy = xmlStrndup((const xmlChar *)zData,nData);` |
|     13 |  2520 | `		pNode = zCopy ? xmlNewDocComment(0,zCopy) : 0;` |
|     13 |  2521 | `		if( zCopy ){` |
|     13 |  2522 | `			xmlFree(zCopy);` |
|      6 |  2523 | `		}` |
|      7 |  2524 | `	}else{` |
|      5 |  2525 | `		pNode = xmlNewDocComment(0,0);` |
|      - |  2526 | `	}` |
|     17 |  2527 | `	return DomCtorInstall(pCtx,pNode);` |
|      1 |  2528 | `}` |
|     10 |  2529 | `DOM_METHOD(vm_builtin_DOMCdataSection_construct)` |
|      1 |  2530 | `{` |
|     11 |  2531 | `	int nData = 0;` |
|     11 |  2532 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : "";` |
|     16 |  2533 | `	return DomCtorInstall(pCtx,` |
|      5 |  2534 | `		xmlNewCDataBlock(0,(const xmlChar *)zData,nData));` |
|      1 |  2535 | `}` |
|      - |  2536 | `/*` |
|      - |  2537 | ` * DOMElement::__construct(string $qualifiedName, ?string $value = null,` |
|      - |  2538 | ` *                         string $namespace = '')` |
|      - |  2539 | ` *` |
|      - |  2540 | ` * The constructor's name grammar is its OWN, not createElementNS's, each cell` |
|      - |  2541 | `` * measured: the whole name must be an XML Name first (so `1:a` is Invalid`` |
|      - |  2542 | ` * Character where createElementNS answers Namespace), a prefix without a` |
|      - |  2543 | ` * namespace is the Namespace refusal, and WITH one the name must be a QName` |
|      - |  2544 | `` * whose prefix is neither `xml` nor `xmlns` -- php refuses `xml:a` here even`` |
|      - |  2545 | ` * against the xml namespace's own URI, where createElementNS allows it. A` |
|      - |  2546 | `` * plain `xmlns` passes as an ordinary name and binds the DEFAULT namespace.`` |
|      - |  2547 | ` *` |
|      - |  2548 | ``  * The $value rides libxml's entity parser, createElement's own quirk: `&amp;` `` |
|      - |  2549 | `` * becomes `&`, and an unterminated reference warns (under this constructor's`` |
|      - |  2550 | ` * name) and drops the whole value. An attribute's value -- the constructor` |
|      - |  2551 | `` * below -- is LITERAL instead: `&amp;` stays five characters.`` |
|      - |  2552 | ` */` |
|     96 |  2553 | `DOM_METHOD(vm_builtin_DOMElement_construct)` |
|      1 |  2554 | `{` |
|     97 |  2555 | `	ph7_vm *pVm = pCtx->pVm;` |
|     97 |  2556 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     97 |  2557 | `	int nVal = 0;` |
|     68 |  2558 | `	const char *zVal = (nArg > 1 && !ph7_value_is_null(apArg[1]))` |
|     73 |  2559 | `		? ph7_value_to_string(apArg[1],&nVal) : 0;` |
|     97 |  2560 | `	const char *zUri = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|     97 |  2561 | `	int bHasUri = zUri[0] != 0;` |
|     97 |  2562 | `	xmlChar *zPrefix = 0;` |
|      - |  2563 | `	xmlChar *zLocal;` |
|      - |  2564 | `	xmlNodePtr pNode;` |
|      - |  2565 | `	sxu32 nMark;` |
|     97 |  2566 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     13 |  2567 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  2568 | `	}` |
|      - |  2569 | `	/* The split is BY HAND, at the first colon, with a leading colon meaning` |
|      - |  2570 | `	 * no prefix at all: libxml's xmlSplitQName2 changed its answer for a name` |
|      - |  2571 | `	 * that ENDS in the colon between 2.9 and 2.13 (the Windows gate caught` |
|      - |  2572 | ``	 * `new DOMElement('a:')` constructing there), and the grammar must answer`` |
|      - |  2573 | `	 * the same on every platform. */` |
|      - |  2574 | `	{` |
|     85 |  2575 | `		const xmlChar *zColon = xmlStrchr((const xmlChar *)zName,':');` |
|     85 |  2576 | `		if( zColon && zColon != (const xmlChar *)zName ){` |
|     43 |  2577 | `			zPrefix = xmlStrndup((const xmlChar *)zName,` |
|     28 |  2578 | `				(int)(zColon - (const xmlChar *)zName));` |
|     29 |  2579 | `			zLocal = xmlStrdup(zColon + 1);` |
|     15 |  2580 | `		}else{` |
|     57 |  2581 | `			zLocal = 0;` |
|      - |  2582 | `		}` |
|      - |  2583 | `	}` |
|     85 |  2584 | `	if( !bHasUri ){` |
|     61 |  2585 | `		if( zPrefix ){` |
|      - |  2586 | `			/* A prefix names a namespace, and none came. */` |
|     13 |  2587 | `			xmlFree(zPrefix);` |
|     13 |  2588 | `			xmlFree(zLocal);` |
|     13 |  2589 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  2590 | `		}` |
|     49 |  2591 | `		if( zLocal ){` |
|    ! 0 |  2592 | `			xmlFree(zLocal);` |
|    ! 0 |  2593 | `		}` |
|     49 |  2594 | ``		zLocal = 0;   /* the whole name, `:a` included */`` |
|     25 |  2595 | `	}else{` |
|     24 |  2596 | `		if( xmlValidateQName((const xmlChar *)zName,0) != 0` |
|     21 |  2597 | `		 \|\| (zPrefix && (xmlStrEqual(zPrefix,(const xmlChar *)"xml")` |
|      9 |  2598 | `		              \|\| xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){` |
|     13 |  2599 | `			if( zPrefix ){` |
|     11 |  2600 | `				xmlFree(zPrefix);` |
|      5 |  2601 | `			}` |
|     13 |  2602 | `			if( zLocal ){` |
|     11 |  2603 | `				xmlFree(zLocal);` |
|      5 |  2604 | `			}` |
|     13 |  2605 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  2606 | `		}` |
|      - |  2607 | `	}` |
|     61 |  2608 | `	pNode = xmlNewNode(0,zLocal ? zLocal : (const xmlChar *)zName);` |
|     61 |  2609 | `	if( zLocal ){` |
|      7 |  2610 | `		xmlFree(zLocal);` |
|      3 |  2611 | `	}` |
|     61 |  2612 | `	if( pNode == 0 ){` |
|    ! 0 |  2613 | `		if( zPrefix ){` |
|    ! 0 |  2614 | `			xmlFree(zPrefix);` |
|    ! 0 |  2615 | `		}` |
|    ! 0 |  2616 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2617 | `	}` |
|     61 |  2618 | `	if( bHasUri ){` |
|      - |  2619 | `		/* On the node's OWN nsDef, so the declaration serializes here once an` |
|      - |  2620 | `		 * insertion adopts the element and lookupNamespaceURI answers it` |
|      - |  2621 | `		 * meanwhile; the reconcile strips it wherever an ancestor already` |
|      - |  2622 | `		 * declares the binding. */` |
|     13 |  2623 | `		xmlNsPtr pNs = xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);` |
|     13 |  2624 | `		if( pNs ){` |
|     13 |  2625 | `			xmlSetNs(pNode,pNs);` |
|      6 |  2626 | `		}` |
|      6 |  2627 | `	}` |
|     61 |  2628 | `	if( zPrefix ){` |
|      7 |  2629 | `		xmlFree(zPrefix);` |
|      3 |  2630 | `	}` |
|     61 |  2631 | `	if( zVal && nVal > 0 ){` |
|      - |  2632 | `` 		/* The EMPTY value is skipped whole -- php's `new DOMElement('a','')` `` |
|      - |  2633 | `		 * has no text child at all, where libxml's setter would leave one. */` |
|     11 |  2634 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     11 |  2635 | `		xmlNodeSetContentLen(pNode,(const xmlChar *)zVal,nVal);` |
|     11 |  2636 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMElement::__construct");` |
|      5 |  2637 | `	}` |
|     61 |  2638 | `	return DomCtorInstall(pCtx,pNode);` |
|     49 |  2639 | `}` |
|      - |  2640 | `/*` |
|      - |  2641 | ` * The last three: a FRAGMENT takes nothing at all; a PROCESSING INSTRUCTION` |
|      - |  2642 | `` * validates its target as a plain XML Name (`xml`, `XML` and `p:a` all pass --`` |
|      - |  2643 | ` * php never asks whether the target is reserved) and stores its data` |
|      - |  2644 | ` * literally, NULL when omitted like the character-data three; an ENTITY` |
|      - |  2645 | ` * REFERENCE validates its name and takes libxml's answer for the content: a` |
|      - |  2646 | `` * PREDEFINED name (`amp`) arrives with the shared entity declaration as its`` |
|      - |  2647 | ` * child -- a STATIC libxml global, wrapped but never owned, which is why the` |
|      - |  2648 | ` * constructor parks only the reference node itself on the limbo shell.` |
|      - |  2649 | ` */` |
|     14 |  2650 | `DOM_METHOD(vm_builtin_DOMDocumentFragment_construct)` |
|      1 |  2651 | `{` |
|      7 |  2652 | `	SXUNUSED(nArg);` |
|      7 |  2653 | `	SXUNUSED(apArg);` |
|     15 |  2654 | `	return DomCtorInstall(pCtx,xmlNewDocFragment(0));` |
|      1 |  2655 | `}` |
|     22 |  2656 | `DOM_METHOD(vm_builtin_DOMProcessingInstruction_construct)` |
|      1 |  2657 | `{` |
|     23 |  2658 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     23 |  2659 | `	const char *zData = nArg > 1 ? ph7_value_to_string(apArg[1],0) : 0;` |
|     23 |  2660 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  2661 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  2662 | `	}` |
|     25 |  2663 | `	return DomCtorInstall(pCtx,` |
|      8 |  2664 | `		xmlNewPI((const xmlChar *)zName,(const xmlChar *)zData));` |
|     12 |  2665 | `}` |
|     24 |  2666 | `DOM_METHOD(vm_builtin_DOMEntityReference_construct)` |
|      1 |  2667 | `{` |
|     25 |  2668 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     25 |  2669 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  2670 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  2671 | `	}` |
|     19 |  2672 | `	return DomCtorInstall(pCtx,xmlNewReference(0,(const xmlChar *)zName));` |
|     13 |  2673 | `}` |
|      - |  2674 | `/*` |
|      - |  2675 | ` * DOMAttr::__construct(string $name, string $value = '')` |
|      - |  2676 | ` *` |
|      - |  2677 | `` * The name is a plain XML Name -- NO QName split at all, so `p:a` and even`` |
|      - |  2678 | `` * `xmlns:x` pass whole and carry no namespace (`prefix` reads "" and`` |
|      - |  2679 | `` * `localName` the full spelling). The value is LITERAL: php builds the text`` |
|      - |  2680 | ` * child directly rather than through the entity parser, which is what keeps` |
|      - |  2681 | `` * `&amp;` five characters where the element constructor's value collapses it.`` |
|      - |  2682 | ` */` |
|     24 |  2683 | `DOM_METHOD(vm_builtin_DOMAttr_construct)` |
|      1 |  2684 | `{` |
|     25 |  2685 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     25 |  2686 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     25 |  2687 | `	if( zName[0] == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  2688 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  2689 | `	}` |
|     28 |  2690 | `	return DomCtorInstall(pCtx,` |
|     18 |  2691 | `		(xmlNodePtr)xmlNewProp(0,(const xmlChar *)zName,(const xmlChar *)zVal));` |
|     13 |  2692 | `}` |
|      - |  2693 |  |
|      - |  2694 | `/* ===== Namespaces ===== */` |
|      - |  2695 |  |
|      - |  2696 | `/*` |
|      - |  2697 | ` * Where a namespace lookup starts. php resolves a DOCUMENT to its root element` |
|      - |  2698 | `` * first -- so `$doc->lookupPrefix($uri)` answers what the document element`` |
|      - |  2699 | ` * would, and an empty document answers nothing at all -- and starts from the` |
|      - |  2700 | ` * node itself for everything else, because libxml's own search walks up the` |
|      - |  2701 | ` * parent chain (which is how a text node or a PI reaches its element's` |
|      - |  2702 | ` * declarations, and how a detached one reaches none).` |
|      - |  2703 | ` */` |
|    130 |  2704 | `static xmlNodePtr DomNsAnchor(xmlNodePtr pNode)` |
|      1 |  2705 | `{` |
|    131 |  2706 | `	if( pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE) ){` |
|     29 |  2707 | `		return (xmlNodePtr)xmlDocGetRootElement((xmlDocPtr)pNode);` |
|      - |  2708 | `	}` |
|    103 |  2709 | `	return pNode;` |
|     66 |  2710 | `}` |
|      - |  2711 | ``/* A `?string` argument: its bytes, or NULL for a null one. */`` |
|    508 |  2712 | `static const char * DomArgStrOrNull(int nArg,ph7_value **apArg,int iArg)` |
|      1 |  2713 | `{` |
|    509 |  2714 | `	if( iArg >= nArg \|\| ph7_value_is_null(apArg[iArg]) ){` |
|     93 |  2715 | `		return 0;` |
|      - |  2716 | `	}` |
|    417 |  2717 | `	return ph7_value_to_string(apArg[iArg],0);` |
|    255 |  2718 | `}` |
|      - |  2719 | `/* DOMNode::lookupNamespaceURI(?string $prefix): ?string */` |
|     62 |  2720 | `DOM_METHOD(vm_builtin_DOMNode_lookupNamespaceURI)` |
|      1 |  2721 | `{` |
|     63 |  2722 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     63 |  2723 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|     63 |  2724 | `	const char *zPrefix = DomArgStrOrNull(nArg,apArg,0);` |
|     63 |  2725 | `	xmlNsPtr pNs = pNode ? xmlSearchNs(pNode->doc,pNode,(const xmlChar *)zPrefix) : 0;` |
|     63 |  2726 | `	if( pNs && pNs->href ){` |
|     35 |  2727 | `		ph7_result_string(pCtx,(const char *)pNs->href,-1);` |
|     18 |  2728 | `	}else{` |
|     29 |  2729 | `		ph7_result_null(pCtx);` |
|      - |  2730 | `	}` |
|     63 |  2731 | `	return PH7_OK;` |
|      1 |  2732 | `}` |
|      - |  2733 | `/* DOMNode::lookupPrefix(string $namespace): ?string -- the DEFAULT namespace has` |
|      - |  2734 | `` * no prefix, so a document whose only declaration is `xmlns="..."` answers null`` |
|      - |  2735 | ` * for the very URI lookupNamespaceURI(null) hands back. */` |
|     36 |  2736 | `DOM_METHOD(vm_builtin_DOMNode_lookupPrefix)` |
|      1 |  2737 | `{` |
|     37 |  2738 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     37 |  2739 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|     37 |  2740 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     37 |  2741 | `	xmlNsPtr pNs = (pNode && zUri[0]) ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;` |
|     37 |  2742 | `	if( pNs && pNs->prefix ){` |
|     15 |  2743 | `		ph7_result_string(pCtx,(const char *)pNs->prefix,-1);` |
|      8 |  2744 | `	}else{` |
|     23 |  2745 | `		ph7_result_null(pCtx);` |
|      - |  2746 | `	}` |
|     37 |  2747 | `	return PH7_OK;` |
|      1 |  2748 | `}` |
|      - |  2749 | `/* DOMNode::isDefaultNamespace(string $namespace): bool -- php tests the URI` |
|      - |  2750 | ` * against the default declaration in scope, and answers FALSE for the empty` |
|      - |  2751 | ` * string rather than "this node is in no namespace". */` |
|     32 |  2752 | `DOM_METHOD(vm_builtin_DOMNode_isDefaultNamespace)` |
|      1 |  2753 | `{` |
|     33 |  2754 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     33 |  2755 | `	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|     33 |  2756 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     33 |  2757 | `	xmlNsPtr pNs = (pNode && zUri[0]) ? xmlSearchNs(pNode->doc,pNode,0) : 0;` |
|     45 |  2758 | `	ph7_result_bool(pCtx,pNs != 0 && pNs->href != 0` |
|     22 |  2759 | `		&& xmlStrEqual(pNs->href,(const xmlChar *)zUri));` |
|     33 |  2760 | `	return PH7_OK;` |
|      1 |  2761 | `}` |
|      - |  2762 |  |
|      - |  2763 | `/* ===== Element attributes ===== */` |
|      - |  2764 |  |
|      - |  2765 | `/* Document-order successor within pRoot's subtree (pRoot excluded) */` |
|   1576 |  2766 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot)` |
|      1 |  2767 | `{` |
|   1577 |  2768 | `	if( pCur->children ){` |
|    660 |  2769 | `		return pCur->children;` |
|      - |  2770 | `	}` |
|   1453 |  2771 | `	while( pCur && pCur != pRoot ){` |
|   1133 |  2772 | `		if( pCur->next ){` |
|    598 |  2773 | `			return pCur->next;` |
|      - |  2774 | `		}` |
|    536 |  2775 | `		pCur = pCur->parent;` |
|      1 |  2776 | `	}` |
|    321 |  2777 | `	return 0;` |
|    790 |  2778 | `}` |
|      - |  2779 | `/*` |
|      - |  2780 | ` * ===== Qualified names and the namespaces they need =====` |
|      - |  2781 | ` *` |
|      - |  2782 | ` * The grammar php screens a created name against, and the rule by which it` |
|      - |  2783 | ` * finds or declares the namespace behind it.  Both were missing here, and what` |
|      - |  2784 | `` * stood in for them wrote documents that are not XML: `setAttributeNS('urn:b',`` |
|      - |  2785 | ``  * '1:x', 'v')` emitted `xmlns:1="urn:b" 1:x="v"`, `('urn:b','a:b:c','v')` `` |
|      - |  2786 | ` * emitted an attribute with two colons in its name, and a prefixed name with a` |
|      - |  2787 | `` * NULL namespace emitted `xmlns:q=""`.  php refuses all three with a Namespace`` |
|      - |  2788 | ` * Error before the element is touched.` |
|      - |  2789 | ` */` |
|      - |  2790 | `#define DOM_XML_NS_URI   "http://www.w3.org/XML/1998/namespace"` |
|      - |  2791 | `#define DOM_XMLNS_NS_URI "http://www.w3.org/2000/xmlns/"` |
|      - |  2792 |  |
|      - |  2793 | `typedef struct dom_qname dom_qname;` |
|      - |  2794 | `struct dom_qname {` |
|      - |  2795 | `	xmlChar *zPrefix;   /* NULL when the name carries none */` |
|      - |  2796 | `	xmlChar *zLocal;    /* always allocated */` |
|      - |  2797 | `};` |
|    240 |  2798 | `static void DomQNameRelease(dom_qname *pQ)` |
|      1 |  2799 | `{` |
|    241 |  2800 | `	if( pQ->zPrefix ){` |
|    148 |  2801 | `		xmlFree(pQ->zPrefix);` |
|     73 |  2802 | `	}` |
|    241 |  2803 | `	if( pQ->zLocal ){` |
|    241 |  2804 | `		xmlFree(pQ->zLocal);` |
|    120 |  2805 | `	}` |
|    241 |  2806 | `	pQ->zPrefix = pQ->zLocal = 0;` |
|    241 |  2807 | `}` |
|    272 |  2808 | `static int DomUriIs(const char *zUri,const char *zWant)` |
|      1 |  2809 | `{` |
|    273 |  2810 | `	return zUri != 0 && DomNameIs(zUri,zWant);` |
|      1 |  2811 | `}` |
|      - |  2812 | `/*` |
|      - |  2813 | `` * php's `dom_check_qname`: the name has to be a QName, and a prefix demands a`` |
|      - |  2814 | ` * namespace.  Three callers ask three different questions of the same name, so` |
|      - |  2815 | ` * iMode says which:` |
|      - |  2816 | ` *` |
|      - |  2817 | ` *   DOM_QN_SET   setAttributeNS -- the loosest. A prefixed name is judged as two` |
|      - |  2818 | ` *                NCNames and every failure is the Namespace Error; an unprefixed` |
|      - |  2819 | ` *                one is a plain Name, where a character libxml will not take is` |
|      - |  2820 | `` *                the Invalid Character Error. The unprefixed `xmlns` is how a`` |
|      - |  2821 | ` *                program writes a namespace DECLARATION, so nothing about the` |
|      - |  2822 | ` *                xmlns namespace is checked here.` |
|      - |  2823 | ` *   DOM_QN_ATTR  createAttributeNS -- a QName and nothing else, plus the DOM` |
|      - |  2824 | ` *                spec's pairing (the xmlns namespace may only be spelled by an` |
|      - |  2825 | ` *                xmlns name and an xmlns name may name nothing else) and the` |
|      - |  2826 | `` *                `xml` prefix's own URI.`` |
|      - |  2827 | ` *   DOM_QN_ELEM  createElementNS -- a QName when a namespace came with it, and` |
|      - |  2828 | `` *                the SET side's split when none did (so `createElementNS(null,`` |
|      - |  2829 | `` *                'x y')` is the Invalid Character Error where the attribute`` |
|      - |  2830 | `` *                factory says Namespace Error, and `:x` is an element named`` |
|      - |  2831 | `` *                `:x` there and a refusal here). No reserved rule at all: php`` |
|      - |  2832 | ` *                checks those where it RESOLVES the namespace, which is after` |
|      - |  2833 | ` *                any binding the document already has, so` |
|      - |  2834 | `` *                `createElementNS($XML_NS, 'xmlns:x')` is an `xml:x` element`` |
|      - |  2835 | ` *                rather than a refusal.` |
|      - |  2836 | ` *` |
|      - |  2837 | ` * Answers 0, or the DOM error code to raise.` |
|      - |  2838 | ` */` |
|      - |  2839 | `#define DOM_QN_SET  0` |
|      - |  2840 | `#define DOM_QN_ATTR 1` |
|      - |  2841 | `#define DOM_QN_ELEM 2` |
|    268 |  2842 | `static int DomQNameParse(const char *zQname,const char *zUri,int iMode,dom_qname *pOut)` |
|      1 |  2843 | `{` |
|    269 |  2844 | `	int bHasUri = zUri != 0 && zUri[0] != 0;` |
|      - |  2845 | `	int bXmlnsName;` |
|    269 |  2846 | `	pOut->zPrefix = pOut->zLocal = 0;` |
|    269 |  2847 | `	if( zQname == 0 \|\| zQname[0] == 0 ){` |
|    ! 0 |  2848 | `		return DOM_ERR_NAMESPACE;` |
|      - |  2849 | `	}` |
|    269 |  2850 | `	if( iMode == DOM_QN_ATTR \|\| (iMode == DOM_QN_ELEM && bHasUri) ){` |
|      - |  2851 | `		/* A created name that names a namespace has to be a QName, and every` |
|      - |  2852 | `		 * failure there is the Namespace Error. */` |
|    157 |  2853 | `		if( xmlValidateQName((const xmlChar *)zQname,0) != 0 ){` |
|     29 |  2854 | `			return DOM_ERR_NAMESPACE;` |
|      - |  2855 | `		}` |
|     64 |  2856 | `	}` |
|    241 |  2857 | `	pOut->zLocal = xmlSplitQName2((const xmlChar *)zQname,&pOut->zPrefix);` |
|    241 |  2858 | `	if( pOut->zLocal == 0 ){` |
|      - |  2859 | `		/* No prefix -- or a name that BEGINS with the colon, which libxml hands` |
|      - |  2860 | ``		 * back whole and php then writes literally (`:x`) as long as no`` |
|      - |  2861 | `		 * namespace came with it. */` |
|     94 |  2862 | `		pOut->zLocal = xmlStrdup((const xmlChar *)zQname);` |
|     94 |  2863 | `		if( pOut->zLocal == 0 ){` |
|    ! 0 |  2864 | `			return DOM_ERR_NAMESPACE;` |
|      - |  2865 | `		}` |
|     47 |  2866 | `	}` |
|    241 |  2867 | `	if( iMode == DOM_QN_SET \|\| (iMode == DOM_QN_ELEM && !bHasUri) ){` |
|      - |  2868 | `		/* The SET side separates the two failures php separates. A name with a` |
|      - |  2869 | `		 * PREFIX is judged as two NCNames and every failure there is the` |
|      - |  2870 | `		 * Namespace Error; an unprefixed one is judged as a plain Name, and a` |
|      - |  2871 | `		 * name libxml will not take at all is the Invalid Character Error. A` |
|      - |  2872 | `		 * namespace then demands that the local part be an NCName too, which is` |
|      - |  2873 | ``		 * what refuses `:x` once a URI comes with it. */`` |
|    113 |  2874 | `		if( pOut->zPrefix ){` |
|     63 |  2875 | `			if( xmlValidateNCName(pOut->zPrefix,0) != 0` |
|     61 |  2876 | `			 \|\| xmlValidateNCName(pOut->zLocal,0) != 0 ){` |
|     22 |  2877 | `				DomQNameRelease(pOut);` |
|     22 |  2878 | `				return DOM_ERR_NAMESPACE;` |
|      1 |  2879 | `			}` |
|     71 |  2880 | `		}else if( xmlValidateName((const xmlChar *)zQname,0) != 0 ){` |
|     15 |  2881 | `			DomQNameRelease(pOut);` |
|     15 |  2882 | `			return DOM_ERR_INVALID_CHAR;` |
|      - |  2883 | `		}` |
|     78 |  2884 | `		if( bHasUri && xmlValidateNCName(pOut->zLocal,0) != 0 ){` |
|      4 |  2885 | `			DomQNameRelease(pOut);` |
|      4 |  2886 | `			return DOM_ERR_NAMESPACE;` |
|      - |  2887 | `		}` |
|     37 |  2888 | `	}` |
|    203 |  2889 | `	bXmlnsName = pOut->zPrefix == 0 && xmlStrEqual(pOut->zLocal,(const xmlChar *)"xmlns");` |
|    203 |  2890 | `	if( pOut->zPrefix && !bHasUri ){` |
|      - |  2891 | `		/* A prefix names a namespace, so there has to be one. (Whether the` |
|      - |  2892 | `		 * prefix may be USED is the resolution's question, not the grammar's:` |
|      - |  2893 | `		 * php reuses a binding the document already has whatever prefix was` |
|      - |  2894 | `		 * asked for, and only refuses when it would have to declare one.) */` |
|     21 |  2895 | `		DomQNameRelease(pOut);` |
|     21 |  2896 | `		return DOM_ERR_NAMESPACE;` |
|      - |  2897 | `	}` |
|    182 |  2898 | `	if( iMode == DOM_QN_ATTR && pOut->zPrefix` |
|     24 |  2899 | `	 && xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xml")` |
|     11 |  2900 | `	 && !DomUriIs(zUri,DOM_XML_NS_URI) ){` |
|    ! 0 |  2901 | `		DomQNameRelease(pOut);` |
|    ! 0 |  2902 | `		return DOM_ERR_NAMESPACE;` |
|      - |  2903 | `	}` |
|    183 |  2904 | `	if( iMode == DOM_QN_ATTR ){` |
|      - |  2905 | `		/* The DOM spec's pairing, which php applies to a created ATTRIBUTE: the` |
|      - |  2906 | `		 * xmlns namespace may only be spelled by an xmlns name, and an xmlns` |
|      - |  2907 | `		 * name may name nothing else. */` |
|     55 |  2908 | `		int bXmlnsPrefix = pOut->zPrefix != 0` |
|     30 |  2909 | `			&& xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xmlns");` |
|     31 |  2910 | `		if( (bXmlnsName \|\| bXmlnsPrefix) != DomUriIs(zUri,DOM_XMLNS_NS_URI) ){` |
|      5 |  2911 | `			DomQNameRelease(pOut);` |
|      5 |  2912 | `			return DOM_ERR_NAMESPACE;` |
|      - |  2913 | `		}` |
|     13 |  2914 | `	}` |
|    179 |  2915 | `	return 0;` |
|    135 |  2916 | `}` |
|      - |  2917 | `/*` |
|      - |  2918 | ` * The namespace a node in zUri should carry, declared on pAnchor when the` |
|      - |  2919 | ` * document has none.  php REUSES a binding it can find by URI as long as that` |
|      - |  2920 | ` * binding has a prefix, takes the caller's prefix when it has to declare and` |
|      - |  2921 | `` * the prefix is free, and otherwise generates `default`, `default1`, ... --`` |
|      - |  2922 | ` * which is why asking for a prefix another URI already owns quietly answers` |
|      - |  2923 | `` * `default:x` rather than refusing.`` |
|      - |  2924 | ` *` |
|      - |  2925 | `` * bNeedPrefix is the CREATE side (`createAttributeNS`), where an unprefixed`` |
|      - |  2926 | ` * name still gets a generated prefix; the SET side may declare the DEFAULT` |
|      - |  2927 | ` * namespace instead.` |
|      - |  2928 | ` */` |
|      2 |  2929 | `static xmlNsPtr DomFindPrefixedNs(xmlNodePtr pNode,const char *zUri)` |
|      1 |  2930 | `{` |
|      - |  2931 | `	xmlNodePtr p;` |
|      5 |  2932 | `	for( p = pNode ; p ; p = p->parent ){` |
|      - |  2933 | `		xmlNsPtr pNs;` |
|      5 |  2934 | `		if( p->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  2935 | `			continue;` |
|      - |  2936 | `		}` |
|      7 |  2937 | `		for( pNs = p->nsDef ; pNs ; pNs = pNs->next ){` |
|      4 |  2938 | `			if( pNs->prefix == 0 \|\| pNs->href == 0` |
|      3 |  2939 | `			 \|\| !xmlStrEqual(pNs->href,(const xmlChar *)zUri) ){` |
|      3 |  2940 | `				continue;` |
|      - |  2941 | `			}` |
|      - |  2942 | `			/* ...and only if a nearer declaration has not taken the prefix. */` |
|      3 |  2943 | `			if( xmlSearchNs(pNode->doc,pNode,pNs->prefix) == pNs ){` |
|      3 |  2944 | `				return pNs;` |
|      - |  2945 | `			}` |
|    ! 0 |  2946 | `		}` |
|      2 |  2947 | `	}` |
|    ! 0 |  2948 | `	return 0;` |
|      2 |  2949 | `}` |
|      - |  2950 | `/*` |
|      - |  2951 | ` * Declare a binding of zUri on pAnchor under a prefix nothing there has taken:` |
|      - |  2952 | `` * zBase, then zBase1, zBase2...  php starts from `default` for a namespace`` |
|      - |  2953 | ` * with no prefix of its own and from the prefix ITSELF when it is re-spelling` |
|      - |  2954 | `` * one an inner declaration has shadowed (which is where `p1` comes from).`` |
|      - |  2955 | ` *` |
|      - |  2956 | ` * bScope is what "nothing there has taken" means. xmlNewNs only refuses a second` |
|      - |  2957 | ` * declaration on the SAME element, which is the whole test for a re-spelling` |
|      - |  2958 | ` * (the shadowing declaration is the one being written). An attribute ARRIVING` |
|      - |  2959 | ` * needs the stronger one -- a prefix bound anywhere in scope is taken, or the` |
|      - |  2960 | ` * declaration written here would shadow it and re-point every node under it.` |
|      - |  2961 | ` */` |
|     28 |  2962 | `static xmlNsPtr DomNsGenerateEx(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase,` |
|      - |  2963 | `	int bScope)` |
|      1 |  2964 | `{` |
|     29 |  2965 | `	xmlNsPtr pNs = 0;` |
|      - |  2966 | `	int i;` |
|     35 |  2967 | `	for( i = 0 ; i < 1000 ; i++ ){` |
|      - |  2968 | `		char zGen[256];` |
|     35 |  2969 | `		const char *zB = zBase ? (const char *)zBase : "default";` |
|     35 |  2970 | `		if( SyStrlen(zB) > sizeof(zGen)-16 ){` |
|    ! 0 |  2971 | `			zB = "default";` |
|    ! 0 |  2972 | `		}` |
|     35 |  2973 | `		if( i == 0 ){` |
|     29 |  2974 | `			SyBufferFormat(zGen,sizeof(zGen),"%s",zB);` |
|     15 |  2975 | `		}else{` |
|      7 |  2976 | `			SyBufferFormat(zGen,sizeof(zGen),"%s%d",zB,i);` |
|      - |  2977 | `		}` |
|     35 |  2978 | `		if( bScope && xmlSearchNs(pAnchor->doc,pAnchor,(const xmlChar *)zGen) != 0 ){` |
|      - |  2979 | `			/* Taken -- by a declaration IN SCOPE, which xmlNewNs does not see:` |
|      - |  2980 | `			 * it only refuses a second one on the same element. */` |
|      5 |  2981 | `			continue;` |
|      - |  2982 | `		}` |
|     31 |  2983 | `		pNs = xmlNewNs(pAnchor,(const xmlChar *)zUri,(const xmlChar *)zGen);` |
|     31 |  2984 | `		if( pNs ){` |
|     29 |  2985 | `			return pNs;` |
|      - |  2986 | `		}` |
|      2 |  2987 | `	}` |
|    ! 0 |  2988 | `	return 0;` |
|     15 |  2989 | `}` |
|     14 |  2990 | `static xmlNsPtr DomNsGenerate(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase)` |
|      1 |  2991 | `{` |
|     15 |  2992 | `	return DomNsGenerateEx(pAnchor,zUri,zBase,0);` |
|      1 |  2993 | `}` |
|      - |  2994 | `/* A binding of this URI an ATTRIBUTE can use: one that carries a prefix. */` |
|     44 |  2995 | `static xmlNsPtr DomNsReuse(xmlNodePtr pAnchor,const char *zUri)` |
|      1 |  2996 | `{` |
|     45 |  2997 | `	xmlNsPtr pNs = xmlSearchNsByHref(pAnchor->doc,pAnchor,(const xmlChar *)zUri);` |
|     45 |  2998 | `	if( pNs && pNs->prefix ){` |
|     19 |  2999 | ``		return pNs;   /* including libxml's implicit `xml` binding */`` |
|      - |  3000 | `	}` |
|      - |  3001 | `	/* Bound, but only WITHOUT a prefix, which does not serve an attribute: a` |
|      - |  3002 | `	 * prefixed binding of the same URI further out still does. */` |
|     27 |  3003 | `	return pNs ? DomFindPrefixedNs(pAnchor,zUri) : 0;` |
|     23 |  3004 | `}` |
|     62 |  3005 | `static xmlNsPtr DomNsResolve(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zPrefix,int bNeedPrefix)` |
|      1 |  3006 | `{` |
|      - |  3007 | `	xmlNsPtr pNs;` |
|     63 |  3008 | `	if( !bNeedPrefix ){` |
|     33 |  3009 | `		pNs = DomNsReuse(pAnchor,zUri);` |
|     33 |  3010 | `		if( pNs ){` |
|     15 |  3011 | `			return pNs;` |
|      - |  3012 | `		}` |
|      9 |  3013 | `	}` |
|      - |  3014 | `	/* The CREATE side asks libxml's own question and no more: a document that` |
|      - |  3015 | `	 * binds this URI to the default namespace AND to a prefix answers the` |
|      - |  3016 | `	 * default one there, and php then declares its own rather than looking for` |
|      - |  3017 | `	 * the prefixed binding the SET side would have found. */` |
|     49 |  3018 | `	pNs = xmlSearchNsByHref(pAnchor->doc,pAnchor,(const xmlChar *)zUri);` |
|     49 |  3019 | `	if( bNeedPrefix ){` |
|     31 |  3020 | `		if( pNs && pNs->prefix ){` |
|     11 |  3021 | `			return pNs;` |
|      - |  3022 | `		}` |
|     21 |  3023 | `		pNs = 0;   /* a prefix-less binding is no use to an attribute */` |
|     10 |  3024 | `	}` |
|      - |  3025 | `	/* A prefix-less binding stops php from declaring another one under the` |
|      - |  3026 | `	 * caller's prefix -- what happens then is a generated one. */` |
|     39 |  3027 | `	if( pNs == 0 && (zPrefix != 0 \|\| !bNeedPrefix) ){` |
|     34 |  3028 | `		if( !bNeedPrefix && zPrefix` |
|     16 |  3029 | `		 && (xmlStrEqual(zPrefix,(const xmlChar *)"xml")` |
|     11 |  3030 | `		  \|\| xmlStrEqual(zPrefix,(const xmlChar *)"xmlns")) ){` |
|      - |  3031 | `			/* A RESERVED prefix cannot be declared, and php does not paper over` |
|      - |  3032 | `			 * that with a generated one: it refuses. (Nothing is refused when` |
|      - |  3033 | `			 * the URI already had a binding -- the prefix is never consulted` |
|      - |  3034 | ``			 * then, which is why `setAttributeNS($uri,'xml:id',..)` succeeds on`` |
|      - |  3035 | `			 * a document that binds $uri and fails on one that does not.) */` |
|      5 |  3036 | `			return 0;` |
|      - |  3037 | `		}` |
|     31 |  3038 | `		pNs = xmlNewNs(pAnchor,(const xmlChar *)zUri,zPrefix);` |
|     31 |  3039 | `		if( pNs ){` |
|     27 |  3040 | `			return pNs;` |
|      - |  3041 | `		}` |
|      2 |  3042 | `	}` |
|      9 |  3043 | `	return DomNsGenerate(pAnchor,zUri,0);` |
|     32 |  3044 | `}` |
|      - |  3045 | `/*` |
|      - |  3046 | ` * The namespace a node CREATED in zUri carries, which is a different rule from` |
|      - |  3047 | ` * either side above and php's smallest one: a binding already in scope is used` |
|      - |  3048 | ` * whatever prefix was asked for -- for a fresh node that means only libxml's own` |
|      - |  3049 | `` * `xml` declaration, which is why every `createElementNS($XML_NS, ...)` comes`` |
|      - |  3050 | `` * back spelled `xml:` -- and otherwise the node declares zUri on ITSELF under`` |
|      - |  3051 | ` * the caller's prefix, with no generated prefix and no fallback: the three` |
|      - |  3052 | `` * reserved-name rules php checks here (`dom_get_ns`) are a refusal, not a`` |
|      - |  3053 | ` * rename. NULL means Namespace Error.` |
|      - |  3054 | ` */` |
|     92 |  3055 | `static xmlNsPtr DomNsForCreate(xmlNodePtr pNode,const char *zUri,const xmlChar *zPrefix)` |
|      1 |  3056 | `{` |
|     93 |  3057 | `	xmlNsPtr pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);` |
|     93 |  3058 | `	if( pNs ){` |
|     17 |  3059 | `		return pNs;` |
|      - |  3060 | `	}` |
|     76 |  3061 | `	if( zPrefix != 0` |
|     64 |  3062 | `	 && ((xmlStrEqual(zPrefix,(const xmlChar *)"xml") && !DomUriIs(zUri,DOM_XML_NS_URI))` |
|     46 |  3063 | `	  \|\| (xmlStrEqual(zPrefix,(const xmlChar *)"xmlns") && !DomUriIs(zUri,DOM_XMLNS_NS_URI))` |
|     43 |  3064 | `	  \|\| (DomUriIs(zUri,DOM_XMLNS_NS_URI)` |
|     27 |  3065 | `	   && !xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){` |
|     15 |  3066 | `		return 0;` |
|      - |  3067 | `	}` |
|     69 |  3068 | `	return xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);` |
|     50 |  3069 | `}` |
|      - |  3070 | `/*` |
|      - |  3071 | ` * A declaration that lands on pElem takes the SPELLING away from every node` |
|      - |  3072 | ` * under it that reached its namespace through a declaration this one now` |
|      - |  3073 | ` * shadows -- the node still points at a binding nothing can name from there, so` |
|      - |  3074 | ` * a re-parse of the serialized document reads it in the wrong namespace (or in` |
|      - |  3075 | ` * none).  php re-points those nodes at a binding of their OWN URI: one still in` |
|      - |  3076 | ` * scope when there is one, and otherwise a fresh declaration on pElem under` |
|      - |  3077 | `` * their own prefix numbered up (`p` -> `p1`), or `default` when they had none.`` |
|      - |  3078 | ` *` |
|      - |  3079 | ` * Only called when a write actually declared something, which is what keeps it` |
|      - |  3080 | ` * off the ordinary path.` |
|      - |  3081 | ` */` |
|     76 |  3082 | `static void DomNsRespell(xmlNodePtr pNode,int bAttr)` |
|      1 |  3083 | `{` |
|     77 |  3084 | `	xmlNsPtr pNs = pNode->ns,pAlt;` |
|      - |  3085 | `	/* php declares what it needs on the node that NEEDS it -- the element` |
|      - |  3086 | `	 * itself, or the element an attribute belongs to. */` |
|     77 |  3087 | `	xmlNodePtr pSite = bAttr ? pNode->parent : pNode;` |
|     77 |  3088 | `	if( pNs == 0 \|\| pNs->href == 0 \|\| pSite == 0 ){` |
|     33 |  3089 | `		return;` |
|      - |  3090 | `	}` |
|     45 |  3091 | `	if( xmlSearchNs(pNode->doc,pNode,pNs->prefix) == pNs ){` |
|     29 |  3092 | `		return;   /* the prefix still names this very binding */` |
|      - |  3093 | `	}` |
|      - |  3094 | `	/* An attribute needs a PREFIXED binding; an element is happy with the` |
|      - |  3095 | `	 * default one. */` |
|     13 |  3096 | `	pAlt = bAttr ? DomNsReuse(pNode,(const char *)pNs->href)` |
|     12 |  3097 | `	             : xmlSearchNsByHref(pNode->doc,pNode,pNs->href);` |
|     17 |  3098 | `	if( pAlt == 0 ){` |
|      - |  3099 | `		/* Its own prefix first -- a declaration that was REMOVED leaves that` |
|      - |  3100 | `		 * prefix free again, and php re-declares it unchanged there. */` |
|     11 |  3101 | `		pAlt = xmlNewNs(pSite,pNs->href,pNs->prefix);` |
|      5 |  3102 | `	}` |
|     17 |  3103 | `	if( pAlt == 0 ){` |
|      7 |  3104 | `		pAlt = DomNsGenerate(pSite,(const char *)pNs->href,pNs->prefix);` |
|      3 |  3105 | `	}` |
|     17 |  3106 | `	if( pAlt ){` |
|     17 |  3107 | `		pNode->ns = pAlt;` |
|      8 |  3108 | `	}` |
|     39 |  3109 | `}` |
|     78 |  3110 | `static int DomNsDefCount(xmlNodePtr pElem)` |
|      1 |  3111 | `{` |
|      - |  3112 | `	xmlNsPtr pNs;` |
|     79 |  3113 | `	int n = 0;` |
|     99 |  3114 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|     21 |  3115 | `		n++;` |
|     11 |  3116 | `	}` |
|     79 |  3117 | `	return n;` |
|      1 |  3118 | `}` |
|     34 |  3119 | `static void DomNsReconcile(xmlNodePtr pElem)` |
|      1 |  3120 | `{` |
|     35 |  3121 | `	xmlNodePtr pCur = pElem;` |
|     77 |  3122 | `	while( pCur ){` |
|      - |  3123 | `		xmlAttrPtr pAttr;` |
|     43 |  3124 | `		if( pCur->type == XML_ELEMENT_NODE ){` |
|     43 |  3125 | `			DomNsRespell(pCur,0);` |
|     77 |  3126 | `			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){` |
|     35 |  3127 | `				if( pAttr->type == XML_ATTRIBUTE_NODE ){` |
|     35 |  3128 | `					DomNsRespell((xmlNodePtr)pAttr,1);` |
|     17 |  3129 | `				}` |
|     18 |  3130 | `			}` |
|     21 |  3131 | `		}` |
|     43 |  3132 | `		pCur = DomWalkNext(pCur,pElem);` |
|      1 |  3133 | `	}` |
|     35 |  3134 | `}` |
|      - |  3135 | `/*` |
|      - |  3136 | ` * A declaration is freed with the element that carries it, so one REMOVED from` |
|      - |  3137 | ` * an element cannot simply be dropped: a node further down may still point at` |
|      - |  3138 | ` * it. It goes where libxml's own document teardown will free it and nothing` |
|      - |  3139 | ``  * resolves through it -- `doc->oldNs`, which is what php's `dom_set_old_ns` `` |
|      - |  3140 | ` * writes to.` |
|      - |  3141 | ` */` |
|     32 |  3142 | `static void DomNsPark(xmlNodePtr pOwner,xmlNsPtr pNs)` |
|      1 |  3143 | `{` |
|     33 |  3144 | `	xmlDocPtr pDoc = pOwner->doc;` |
|      - |  3145 | `	xmlNsPtr pTail;` |
|     33 |  3146 | `	pNs->next = 0;` |
|      - |  3147 | ``	/* The list's HEAD must stay libxml's own `xml` declaration, because`` |
|      - |  3148 | ``	 * xmlSearchNs answers doc->oldNs DIRECTLY for the `xml` prefix. Asking for`` |
|      - |  3149 | `	 * it is what builds it. */` |
|     33 |  3150 | `	xmlSearchNs(pDoc,pOwner,(const xmlChar *)"xml");` |
|     33 |  3151 | `	if( pDoc->oldNs == 0 ){` |
|    ! 0 |  3152 | `		pDoc->oldNs = pNs;` |
|    ! 0 |  3153 | `		return;` |
|      - |  3154 | `	}` |
|     37 |  3155 | `	for( pTail = pDoc->oldNs ; pTail->next ; pTail = pTail->next ){}` |
|     33 |  3156 | `	pTail->next = pNs;` |
|     17 |  3157 | `}` |
|      - |  3158 | `/*` |
|      - |  3159 | `` * php's `dom_reconcile_ns`, which every mutator runs on the node it LINKED.`` |
|      - |  3160 | ` * Without it a move wrote documents that are not XML in both directions:` |
|      - |  3161 | ` * appending a node whose namespace was declared on the ancestor it just left` |
|      - |  3162 | `` * emitted `<p:b k="1"/>` with the prefix bound nowhere, and appending one that`` |
|      - |  3163 | `` * carries its own declaration (`createElementNS`, or a chunk `appendXML` built)`` |
|      - |  3164 | ` * emitted a second copy of a declaration the new parent already makes.` |
|      - |  3165 | ` *` |
|      - |  3166 | ` * The strip is php's own test: same URI, and either the node's declaration` |
|      - |  3167 | ` * carries NO prefix -- then any binding of that URI in scope replaces it, even` |
|      - |  3168 | `` * a prefixed one, which is how an appended `createElementNS($uri,'y')` comes`` |
|      - |  3169 | `` * out spelled `p:y` -- or the in-scope binding spells it the same way.`` |
|      - |  3170 | ` *` |
|      - |  3171 | `` * The re-pointing after it is libxml's own `xmlReconciliateNs`, called here`` |
|      - |  3172 | ` * rather than paraphrased: it re-points EVERY node of the subtree at the first` |
|      - |  3173 | ` * in-scope binding of its URI found from the inserted node -- so a URI two` |
|      - |  3174 | ` * prefixes bind is respelled to the first of them, which the setAttributeNS` |
|      - |  3175 | ` * respeller (DomNsReconcile, which only touches a node whose spelling BROKE)` |
|      - |  3176 | ` * does not do -- and declares one on the inserted node for a URI nothing in` |
|      - |  3177 | ` * scope binds any more (including one a DESCENDANT declares, since the search` |
|      - |  3178 | ` * only ever looks up).` |
|      - |  3179 | ` */` |
|    300 |  3180 | `static void DomNsStrip(xmlNodePtr pNode,xmlNodePtr pScopeAt)` |
|      1 |  3181 | `{` |
|    301 |  3182 | `	xmlNsPtr pCur = pNode->nsDef,pPrev = 0;` |
|    301 |  3183 | `	if( pNode->doc == 0 ){` |
|      - |  3184 | `		/* Nothing would own a removed declaration, and a node under it may` |
|      - |  3185 | `		 * still point at one: leave the element's list alone. */` |
|     15 |  3186 | `		return;` |
|      - |  3187 | `	}` |
|    407 |  3188 | `	while( pCur ){` |
|    121 |  3189 | `		xmlNsPtr pNext = pCur->next;` |
|    181 |  3190 | `		xmlNsPtr pScope = pCur->href` |
|    120 |  3191 | `			? xmlSearchNsByHref(pNode->doc,pScopeAt,pCur->href) : 0;` |
|    120 |  3192 | `		if( pScope != 0` |
|     96 |  3193 | `		 && (pCur->prefix == 0` |
|     38 |  3194 | `		  \|\| (pScope->prefix != 0 && xmlStrEqual(pScope->prefix,pCur->prefix))) ){` |
|     31 |  3195 | `			if( pPrev ){` |
|    ! 0 |  3196 | `				pPrev->next = pNext;` |
|    ! 0 |  3197 | `			}else{` |
|     31 |  3198 | `				pNode->nsDef = pNext;` |
|      - |  3199 | `			}` |
|     31 |  3200 | `			DomNsPark(pNode,pCur);` |
|     16 |  3201 | `		}else{` |
|     91 |  3202 | `			pPrev = pCur;` |
|      - |  3203 | `		}` |
|    121 |  3204 | `		pCur = pNext;` |
|      1 |  3205 | `	}` |
|    151 |  3206 | `}` |
|      - |  3207 | `/*` |
|      - |  3208 | ` * php runs the strip on the node it linked and NO deeper -- a redundant` |
|      - |  3209 | `` * declaration one level down survives an `appendChild` -- but a FRAGMENT is`` |
|      - |  3210 | ` * spliced by a second function that walks each moved child WHOLE, so the same` |
|      - |  3211 | ` * subtree arriving that way comes out stripped at every depth. bDeep is that` |
|      - |  3212 | ` * difference, and both halves are measurable.` |
|      - |  3213 | ` *` |
|      - |  3214 | ` * The deep walk judges every node against the same scope -- the INSERTION` |
|      - |  3215 | ` * POINT, not each node's own parent -- so a declaration duplicated inside the` |
|      - |  3216 | ` * moved subtree survives when the new parent does not make it too.` |
|      - |  3217 | ` */` |
|    470 |  3218 | `static void DomNsOnInsertEx(xmlNodePtr pNode,int bDeep)` |
|      2 |  3219 | `{` |
|    472 |  3220 | `	if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|    186 |  3221 | `		return;` |
|      - |  3222 | `	}` |
|    287 |  3223 | `	if( bDeep ){` |
|     73 |  3224 | `		xmlNodePtr pCur = pNode,pAt = pNode->parent;` |
|    163 |  3225 | `		while( pCur ){` |
|     91 |  3226 | `			if( pCur->type == XML_ELEMENT_NODE ){` |
|     87 |  3227 | `				DomNsStrip(pCur,pAt);` |
|     43 |  3228 | `			}` |
|     91 |  3229 | `			pCur = DomWalkNext(pCur,pNode);` |
|      1 |  3230 | `		}` |
|     37 |  3231 | `	}else{` |
|    215 |  3232 | `		DomNsStrip(pNode,pNode->parent);` |
|      - |  3233 | `	}` |
|    287 |  3234 | `	xmlReconciliateNs(pNode->doc,pNode);` |
|    237 |  3235 | `}` |
|      - |  3236 | `/*` |
|      - |  3237 | ` * The namespace an attribute NODE carries once it is linked onto pElem. Its own` |
|      - |  3238 | ` * ns struct is a declaration of wherever it came FROM, and moving the node does` |
|      - |  3239 | `` * not move that: written unchanged it emitted `p:k="1"` with the prefix bound`` |
|      - |  3240 | ` * NOWHERE, and -- when the target's scope binds that prefix to something else --` |
|      - |  3241 | ` * bound to the WRONG URI, which is the worse half, because those bytes parse` |
|      - |  3242 | ` * back cleanly as an attribute in a namespace the program never wrote.` |
|      - |  3243 | ` *` |
|      - |  3244 | ` * php keeps the spelling when the attribute's own declaration is still in scope` |
|      - |  3245 | ` * here, even if a nearer one binds the same URI under another prefix. Otherwise` |
|      - |  3246 | ` * it takes any binding of the URI in scope -- INCLUDING a prefix-less one, where` |
|      - |  3247 | ` * the attribute then serializes with no prefix at all and still answers the URI,` |
|      - |  3248 | ` * which is NOT how the by-NAME writes resolve (there an attribute always wants a` |
|      - |  3249 | ` * prefixed binding, DomNsReuse) -- and otherwise declares one here under the` |
|      - |  3250 | `` * attribute's own prefix, numbered up when that prefix is taken (`p` -> `p1`).`` |
|      - |  3251 | ` */` |
|     78 |  3252 | `static void DomNsAttrArrive(xmlNodePtr pElem,xmlAttrPtr pAttr)` |
|      2 |  3253 | `{` |
|     80 |  3254 | `	xmlNsPtr pNs = pAttr->ns;` |
|     80 |  3255 | `	if( pNs == 0 \|\| pNs->href == 0 ){` |
|     32 |  3256 | `		return;` |
|      - |  3257 | `	}` |
|     49 |  3258 | `	if( DomUriIs((const char *)pNs->href,DOM_XMLNS_NS_URI) ){` |
|      - |  3259 | `		/* An attribute in the xmlns namespace IS a declaration, and php resolves` |
|      - |  3260 | ``		 * nothing for it: `xmlns="urn:z"` stays spelled that way wherever it is`` |
|      - |  3261 | `		 * written, and never acquires a declaration of the xmlns namespace. */` |
|      7 |  3262 | `		return;` |
|      - |  3263 | `	}` |
|     43 |  3264 | `	if( xmlSearchNs(pElem->doc,pElem,pNs->prefix) == pNs ){` |
|     21 |  3265 | `		return;` |
|      - |  3266 | `	}` |
|     23 |  3267 | `	pNs = xmlSearchNsByHref(pElem->doc,pElem,pAttr->ns->href);` |
|     23 |  3268 | `	if( pNs ){` |
|      9 |  3269 | `		pAttr->ns = pNs;` |
|      9 |  3270 | `		return;` |
|      - |  3271 | `	}` |
|     15 |  3272 | `	pNs = DomNsGenerateEx(pElem,(const char *)pAttr->ns->href,pAttr->ns->prefix,1);` |
|     15 |  3273 | `	if( pNs == 0 ){` |
|    ! 0 |  3274 | `		return;` |
|      - |  3275 | `	}` |
|     15 |  3276 | `	pAttr->ns = pNs;` |
|      - |  3277 | `	/*` |
|      - |  3278 | `	 * A declaration LANDED on this element, and php then judges its whole` |
|      - |  3279 | `	 * subtree from HERE: a descendant whose namespace is declared further down` |
|      - |  3280 | `	 * is re-pointed at a fresh declaration on this element -- even though the` |
|      - |  3281 | `	 * one it had is still in scope where it stands. That is libxml's` |
|      - |  3282 | `	 * xmlReconciliateNs, and it runs ONLY on this path: an arriving attribute` |
|      - |  3283 | `	 * that needed no declaration leaves the subtree exactly as it was, which is` |
|      - |  3284 | `	 * measurable both ways.` |
|      - |  3285 | `	 */` |
|     15 |  3286 | `	xmlReconciliateNs(pElem->doc,pElem);` |
|     41 |  3287 | `}` |
|      - |  3288 | `/* A namespace DECLARATION on this element: php's setAttributeNS writes one` |
|      - |  3289 | `` * when the name is `xmlns` or its prefix is, and REBINDS the one already`` |
|      - |  3290 | ` * there rather than adding a second. */` |
|     14 |  3291 | `static void DomNsDeclare(xmlNodePtr pElem,const xmlChar *zPrefix,const char *zHref)` |
|      1 |  3292 | `{` |
|      - |  3293 | `	xmlNsPtr pNs;` |
|     17 |  3294 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|      7 |  3295 | `		int bSame = zPrefix == 0 ? pNs->prefix == 0` |
|      3 |  3296 | `			: (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix));` |
|      5 |  3297 | `		if( bSame ){` |
|      3 |  3298 | `			xmlChar *zNew = xmlStrdup((const xmlChar *)zHref);` |
|      3 |  3299 | `			if( zNew == 0 ){` |
|    ! 0 |  3300 | `				return;` |
|      - |  3301 | `			}` |
|      3 |  3302 | `			if( pNs->href ){` |
|      3 |  3303 | `				xmlFree((xmlChar *)pNs->href);` |
|      1 |  3304 | `			}` |
|      3 |  3305 | `			pNs->href = zNew;` |
|      3 |  3306 | `			return;` |
|      - |  3307 | `		}` |
|      2 |  3308 | `	}` |
|     13 |  3309 | `	xmlNewNs(pElem,(const xmlChar *)zHref,zPrefix);` |
|      8 |  3310 | `}` |
|      - |  3311 |  |
|      - |  3312 | `/*` |
|      - |  3313 | ` * ===== Namespace DECLARATIONS, which php answers from the attribute surface =====` |
|      - |  3314 | ` *` |
|      - |  3315 | `` * `xmlns:x="urn:x"` is not an attribute in libxml -- it is an xmlNs on the`` |
|      - |  3316 | ``  * element's nsDef chain -- but php answers it from `getAttributeNode('xmlns:x')` `` |
|      - |  3317 | `` * (as a DOMNameSpaceNode), from `getAttribute`, `hasAttribute`,`` |
|      - |  3318 | `` * `removeAttribute`, `toggleAttribute` and `getAttributeNames`, which is how a`` |
|      - |  3319 | ` * program reads or drops one.  Here every one of those said the declaration was` |
|      - |  3320 | ``  * not there: `hasAttribute('xmlns:x')` was false and `getAttribute('xmlns:x')` `` |
|      - |  3321 | ` * was "" on a document whose root declares it.` |
|      - |  3322 | ` *` |
|      - |  3323 | ` * The lookup is the element's OWN declarations, not the ones in scope: a child` |
|      - |  3324 | ` * answers false for a prefix its parent declared.` |
|      - |  3325 | ` */` |
|      - |  3326 | `#define DOM_XMLNS_NAME "xmlns"` |
|      - |  3327 |  |
|      - |  3328 | `/* The declaration this element makes for zPrefix (NULL for the DEFAULT one). */` |
|     68 |  3329 | `static xmlNsPtr DomNsDeclOf(xmlNodePtr pElem,const xmlChar *zPrefix)` |
|      1 |  3330 | `{` |
|      - |  3331 | `	xmlNsPtr pNs;` |
|     69 |  3332 | `	if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  3333 | `		return 0;` |
|      - |  3334 | `	}` |
|    111 |  3335 | `	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){` |
|    148 |  3336 | `		if( zPrefix == 0 ? pNs->prefix == 0` |
|     70 |  3337 | `		                 : (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix)) ){` |
|     45 |  3338 | `			return pNs;` |
|      - |  3339 | `		}` |
|     22 |  3340 | `	}` |
|     25 |  3341 | `	return 0;` |
|     35 |  3342 | `}` |
|      - |  3343 | ``/* ...under the NAME php spells it with: `xmlns` or `xmlns:<prefix>`. */`` |
|    112 |  3344 | `static xmlNsPtr DomNsDeclByName(xmlNodePtr pElem,const char *zName)` |
|      2 |  3345 | `{` |
|    114 |  3346 | `	sxu32 n = (sxu32)SyStrlen(DOM_XMLNS_NAME);` |
|    114 |  3347 | `	if( SyStrlen(zName) < n \|\| SyStrncmp(zName,DOM_XMLNS_NAME,n) != 0 ){` |
|     70 |  3348 | `		return 0;` |
|      - |  3349 | `	}` |
|     45 |  3350 | `	if( zName[n] == 0 ){` |
|     15 |  3351 | `		return DomNsDeclOf(pElem,0);` |
|      - |  3352 | `	}` |
|     31 |  3353 | `	if( zName[n] != ':' ){` |
|    ! 0 |  3354 | `		return 0;` |
|      - |  3355 | `	}` |
|     31 |  3356 | `	return DomNsDeclOf(pElem,(const xmlChar *)(zName+n+1));` |
|     58 |  3357 | `}` |
|      - |  3358 | `/*` |
|      - |  3359 | ` * Dropping one: the declaration leaves the element's chain, but the xmlNs` |
|      - |  3360 | ` * itself must NOT be freed -- nodes below can still point at it, and php's own` |
|      - |  3361 | ` * answer for that case is a document that keeps saying what it said. The` |
|      - |  3362 | ` * document's oldNs chain owns it from here, so it dies with the document.` |
|      - |  3363 | ` */` |
|      8 |  3364 | `static void DomNsDeclRemove(xmlNodePtr pElem,xmlNsPtr pNs)` |
|      1 |  3365 | `{` |
|      9 |  3366 | `	xmlNsPtr pPrev = 0,pCur;` |
|      9 |  3367 | `	xmlDocPtr pDoc = pElem->doc;` |
|      9 |  3368 | `	for( pCur = pElem->nsDef ; pCur ; pPrev = pCur,pCur = pCur->next ){` |
|      9 |  3369 | `		if( pCur != pNs ){` |
|    ! 0 |  3370 | `			continue;` |
|      - |  3371 | `		}` |
|      9 |  3372 | `		if( pPrev ){` |
|    ! 0 |  3373 | `			pPrev->next = pCur->next;` |
|    ! 0 |  3374 | `		}else{` |
|      9 |  3375 | `			pElem->nsDef = pCur->next;` |
|      - |  3376 | `		}` |
|      9 |  3377 | `		pCur->next = 0;` |
|      9 |  3378 | `		if( pDoc == 0 ){` |
|    ! 0 |  3379 | `			return;` |
|      - |  3380 | `		}` |
|      9 |  3381 | `		if( pDoc->oldNs == 0 ){` |
|      9 |  3382 | `			pDoc->oldNs = pCur;` |
|      5 |  3383 | `		}else{` |
|    ! 0 |  3384 | `			xmlNsPtr pTail = pDoc->oldNs;` |
|    ! 0 |  3385 | `			while( pTail->next ){` |
|    ! 0 |  3386 | `				pTail = pTail->next;` |
|    ! 0 |  3387 | `			}` |
|    ! 0 |  3388 | `			pTail->next = pCur;` |
|      - |  3389 | `		}` |
|      9 |  3390 | `		return;` |
|    ! 0 |  3391 | `	}` |
|      5 |  3392 | `}` |
|      - |  3393 |  |
|      - |  3394 | `/*` |
|      - |  3395 | ` * php's wrapper for one: DOMNameSpaceNode, a class of its own that does NOT` |
|      - |  3396 | ` * extend DOMNode and answers ten properties.  A FRESH object every time (php's` |
|      - |  3397 | `` * two calls are never `===`), so it needs none of the identity cache.`` |
|      - |  3398 | ` */` |
|      - |  3399 | `/*` |
|      - |  3400 | ` * The handle for one declaration, kept in the document's identity cache under` |
|      - |  3401 | `` * the xmlNs pointer.  php's two lookups answer two OBJECTS (`===` is false) but`` |
|      - |  3402 | `` * the same declaration, and a shared handle is what makes them `==` -- and what`` |
|      - |  3403 | `` * stops a loop over `getAttributeNode('xmlns:x')` from allocating one per call.`` |
|      - |  3404 | ` */` |
|     60 |  3405 | `static phl_domnode * DomNsRes(ph7_vm *pVm,ph7_class_instance *pDoc,phl_xmldoc *pShell,xmlNsPtr pNs)` |
|      1 |  3406 | `{` |
|     61 |  3407 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|     61 |  3408 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  3409 | `	phl_domnode *pRes;` |
|      - |  3410 | `	ph7_value sKey,sVal;` |
|     61 |  3411 | `	if( pCache == 0 ){` |
|    ! 0 |  3412 | `		return DomNewRes(&(*pVm),pShell,pNs);` |
|      - |  3413 | `	}` |
|     61 |  3414 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNs);` |
|     61 |  3415 | `	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|     33 |  3416 | `		ph7_value *pHit = HashmapExtractNodeValue(pEntry);` |
|     33 |  3417 | `		if( pHit && (pHit->iFlags & MEMOBJ_RES) ){` |
|     33 |  3418 | `			PH7_MemObjRelease(&sKey);` |
|     33 |  3419 | `			return (phl_domnode *)pHit->x.pOther;` |
|      - |  3420 | `		}` |
|    ! 0 |  3421 | `	}` |
|     29 |  3422 | `	pRes = DomNewRes(&(*pVm),pShell,pNs);` |
|     29 |  3423 | `	if( pRes ){` |
|     29 |  3424 | `		PH7_MemObjInit(&(*pVm),&sVal);` |
|     29 |  3425 | `		sVal.x.pOther = pRes;` |
|     29 |  3426 | `		sVal.iFlags = MEMOBJ_RES;` |
|     29 |  3427 | `		PH7_HashmapInsert(pCache,&sKey,&sVal);` |
|     14 |  3428 | `	}` |
|     29 |  3429 | `	PH7_MemObjRelease(&sKey);` |
|     29 |  3430 | `	return pRes;` |
|     31 |  3431 | `}` |
|     60 |  3432 | `static ph7_class_instance * DomNewNsNode(ph7_vm *pVm,ph7_class_instance *pDoc,` |
|      - |  3433 | `	phl_xmldoc *pShell,xmlNsPtr pNs,xmlNodePtr pElem)` |
|      1 |  3434 | `{` |
|     91 |  3435 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DOMNameSpaceNode",` |
|     60 |  3436 | `		(sxu32)SyStrlen("DOMNameSpaceNode"),FALSE,0);` |
|     61 |  3437 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|     61 |  3438 | `	phl_domnode *pRes = pObj ? DomNsRes(&(*pVm),pDoc,pShell,pNs) : 0;` |
|     61 |  3439 | `	if( pRes == 0 ){` |
|    ! 0 |  3440 | `		if( pObj ){` |
|    ! 0 |  3441 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  3442 | `		}` |
|    ! 0 |  3443 | `		return 0;` |
|      - |  3444 | `	}` |
|     61 |  3445 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|     61 |  3446 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|     61 |  3447 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_NS_OWNER,DomWrap(&(*pVm),pDoc,pShell,pElem));` |
|     61 |  3448 | `	return pObj;   /* the CALLER owns this reference */` |
|     31 |  3449 | `}` |
|      - |  3450 | `/* Answer one from a native method (php hands back a fresh object every time). */` |
|     18 |  3451 | `static int DomResultNsNode(ph7_context *pCtx,phl_domnode *pNd,xmlNsPtr pNs,xmlNodePtr pElem)` |
|      1 |  3452 | `{` |
|     19 |  3453 | `	ph7_class_instance *pObj = DomNewNsNode(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNs,pElem);` |
|     19 |  3454 | `	if( pObj == 0 ){` |
|    ! 0 |  3455 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3456 | `	}` |
|     19 |  3457 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     19 |  3458 | `	return PH7_OK;` |
|     10 |  3459 | `}` |
|      - |  3460 |  |
|      - |  3461 | `/*` |
|      - |  3462 | ` * An attribute of pElem by QUALIFIED name, php's own two-step lookup.` |
|      - |  3463 | ` *` |
|      - |  3464 | ` * libxml's xmlHasProp() matches the stored name and ignores namespaces` |
|      - |  3465 | ` * entirely, which is wrong in both directions and was the answer the whole` |
|      - |  3466 | `` * by-name surface gave: `getAttribute('x:b')` found NOTHING (the stored name is`` |
|      - |  3467 | `` * `b`, the prefix lives in the node's ns) while `getAttribute('b')` found the`` |
|      - |  3468 | `` * NAMESPACED one and `removeAttribute('b')` deleted it.  php looks for an`` |
|      - |  3469 | ` * attribute in NO namespace under the whole name first -- which is what finds` |
|      - |  3470 | ` * one written under an unresolvable prefix, stored with the colon in its name --` |
|      - |  3471 | ` * and only then splits the prefix, resolves it in the element's scope and asks` |
|      - |  3472 | ` * again by (local name, URI).  An unresolvable prefix finds nothing.` |
|      - |  3473 | ` *` |
|      - |  3474 | ` * A DTD-declared DEFAULT is not an attribute here: xmlHasNsProp answers the` |
|      - |  3475 | ` * DECLARATION node for those, which is a different node kind entirely.` |
|      - |  3476 | ` */` |
|    236 |  3477 | `static xmlAttrPtr DomAttrNoNs(xmlNodePtr pElem,const char *zName)` |
|      2 |  3478 | `{` |
|      - |  3479 | `	xmlAttrPtr pAttr;` |
|    238 |  3480 | `	if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  3481 | `		return 0;` |
|      - |  3482 | `	}` |
|    238 |  3483 | `	pAttr = xmlHasNsProp(pElem,(const xmlChar *)zName,0);` |
|    238 |  3484 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|    120 |  3485 | `}` |
|    210 |  3486 | `static xmlAttrPtr DomAttrByName(xmlNodePtr pElem,const char *zName)` |
|      2 |  3487 | `{` |
|    212 |  3488 | `	xmlAttrPtr pAttr = DomAttrNoNs(pElem,zName);` |
|    212 |  3489 | `	if( pAttr == 0 && pElem ){` |
|     69 |  3490 | `		xmlChar *zPrefix = 0;` |
|     69 |  3491 | `		xmlChar *zLocal = xmlSplitQName2((const xmlChar *)zName,&zPrefix);` |
|     69 |  3492 | `		if( zLocal ){` |
|     41 |  3493 | `			if( zPrefix ){` |
|     41 |  3494 | `				xmlNsPtr pNs = xmlSearchNs(pElem->doc,pElem,zPrefix);` |
|     41 |  3495 | `				if( pNs ){` |
|     15 |  3496 | `					pAttr = xmlHasNsProp(pElem,zLocal,pNs->href);` |
|      7 |  3497 | `				}` |
|     41 |  3498 | `				xmlFree(zPrefix);` |
|     20 |  3499 | `			}` |
|     41 |  3500 | `			xmlFree(zLocal);` |
|     20 |  3501 | `		}` |
|     34 |  3502 | `	}` |
|    212 |  3503 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|      2 |  3504 | `}` |
|    122 |  3505 | `static xmlAttrPtr DomAttrByNs(xmlNodePtr pElem,const xmlChar *zUri,const char *zLocal)` |
|      1 |  3506 | `{` |
|    123 |  3507 | `	xmlAttrPtr pAttr = pElem ? xmlHasNsProp(pElem,(const xmlChar *)zLocal,zUri) : 0;` |
|    123 |  3508 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|      1 |  3509 | `}` |
|      - |  3510 |  |
|      - |  3511 | `/* DOMElement::getAttribute(string $qualifiedName): string -- "" when absent */` |
|     44 |  3512 | `DOM_METHOD(vm_builtin_DOMElement_getAttribute)` |
|      1 |  3513 | `{` |
|     45 |  3514 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     45 |  3515 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     45 |  3516 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     45 |  3517 | `	xmlNsPtr pDecl = DomNsDeclByName(pElem,zName);` |
|     45 |  3518 | `	xmlAttrPtr pAttr = pDecl ? 0 : DomAttrByName(pElem,zName);` |
|     45 |  3519 | `	xmlChar *zVal = pAttr ? xmlNodeListGetString(pAttr->doc,pAttr->children,1) : 0;` |
|     45 |  3520 | `	if( pDecl ){` |
|      - |  3521 | `		/* A declaration's "value" is the URI it binds. */` |
|      7 |  3522 | `		ph7_result_string(pCtx,pDecl->href ? (const char *)pDecl->href : "",-1);` |
|      7 |  3523 | `		return PH7_OK;` |
|      - |  3524 | `	}` |
|     39 |  3525 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|     39 |  3526 | `	if( zVal ){` |
|     33 |  3527 | `		xmlFree(zVal);` |
|     16 |  3528 | `	}` |
|     39 |  3529 | `	return PH7_OK;` |
|     23 |  3530 | `}` |
|      - |  3531 | `/* DOMElement::hasAttribute(string $qualifiedName): bool */` |
|     18 |  3532 | `DOM_METHOD(vm_builtin_DOMElement_hasAttribute)` |
|      1 |  3533 | `{` |
|     19 |  3534 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     19 |  3535 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     43 |  3536 | `	ph7_result_bool(pCtx,pNd != 0` |
|     27 |  3537 | `		&& (DomAttrByName((xmlNodePtr)pNd->pNode,zName) != 0` |
|     15 |  3538 | `		 \|\| DomNsDeclByName((xmlNodePtr)pNd->pNode,zName) != 0));` |
|     19 |  3539 | `	return PH7_OK;` |
|      1 |  3540 | `}` |
|      - |  3541 | `/* DOMElement::setAttribute(string $qualifiedName, string $value): DOMAttr -- php` |
|      - |  3542 | ` * answers the attribute NODE it wrote, so the write is followed by a wrap. */` |
|     26 |  3543 | `DOM_METHOD(vm_builtin_DOMElement_setAttribute)` |
|      2 |  3544 | `{` |
|     28 |  3545 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     28 |  3546 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|     28 |  3547 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  3548 | `	xmlAttrPtr pAttr;` |
|     28 |  3549 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  3550 | `		return DomThrowAlways(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  3551 | `	}` |
|     22 |  3552 | `	if( DomNsDeclByName((xmlNodePtr)pNd->pNode,zName) ){` |
|      - |  3553 | `		/* php will not write THROUGH a declaration this element already makes:` |
|      - |  3554 | `		 * the write is dropped and the answer is false. (A name that is not one` |
|      - |  3555 | `		 * yet becomes an ordinary attribute, colon and all.) */` |
|      3 |  3556 | `		ph7_result_bool(pCtx,0);` |
|      3 |  3557 | `		return PH7_OK;` |
|      - |  3558 | `	}` |
|     20 |  3559 | `	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);` |
|     20 |  3560 | `	pAttr = DomAttrByName((xmlNodePtr)pNd->pNode,zName);` |
|     20 |  3561 | `	if( pAttr == 0 ){` |
|    ! 0 |  3562 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3563 | `		return PH7_OK;` |
|      - |  3564 | `	}` |
|     20 |  3565 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     15 |  3566 | `}` |
|      - |  3567 | `/* DOMElement::removeAttribute(string $qualifiedName): bool */` |
|     10 |  3568 | `DOM_METHOD(vm_builtin_DOMElement_removeAttribute)` |
|      1 |  3569 | `{` |
|     11 |  3570 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     11 |  3571 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     11 |  3572 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     11 |  3573 | `	xmlAttrPtr pAttr = DomAttrByName(pElem,zName);` |
|     11 |  3574 | `	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|     11 |  3575 | `	if( pDecl ){` |
|      - |  3576 | `		/* The declaration goes, and whatever still needs it gets it back: php` |
|      - |  3577 | `		 * answers TRUE either way, and a binding nothing uses simply vanishes. */` |
|      5 |  3578 | `		DomNsDeclRemove(pElem,pDecl);` |
|      5 |  3579 | `		DomNsReconcile(pElem);` |
|      5 |  3580 | `		ph7_result_bool(pCtx,1);` |
|      5 |  3581 | `		return PH7_OK;` |
|      - |  3582 | `	}` |
|      7 |  3583 | `	if( pAttr == 0 ){` |
|      - |  3584 | `		/* Absent (or a DTD default): php returns false */` |
|      3 |  3585 | `		ph7_result_bool(pCtx,0);` |
|      3 |  3586 | `		return PH7_OK;` |
|      - |  3587 | `	}` |
|      5 |  3588 | `	xmlRemoveProp(pAttr);` |
|      5 |  3589 | `	ph7_result_bool(pCtx,1);` |
|      5 |  3590 | `	return PH7_OK;` |
|      6 |  3591 | `}` |
|      - |  3592 | `/*` |
|      - |  3593 | `` * A `?string $namespace` argument as libxml wants it: NULL for php's null,`` |
|      - |  3594 | ` * which is how a caller asks for the attribute in NO namespace, and the bytes` |
|      - |  3595 | ` * otherwise.  Handing libxml "" for a null instead is not the same question --` |
|      - |  3596 | ` * "" matches a namespace whose URI is the empty string, which no document has,` |
|      - |  3597 | `` * so `getAttributeNS(null, 'href')` answered "" for every plain attribute.`` |
|      - |  3598 | ` */` |
|    132 |  3599 | `static const xmlChar * DomArgUri(int nArg,ph7_value **apArg,int iArg)` |
|      1 |  3600 | `{` |
|    133 |  3601 | `	const char *z = DomArgStrOrNull(nArg,apArg,iArg);` |
|    133 |  3602 | `	return (const xmlChar *)z;` |
|      1 |  3603 | `}` |
|      - |  3604 | `/* DOMElement::getAttributeNS(?string $namespace, string $localName): string */` |
|     20 |  3605 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNS)` |
|      1 |  3606 | `{` |
|     21 |  3607 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     21 |  3608 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     21 |  3609 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     21 |  3610 | `	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,zUri) : 0;` |
|     21 |  3611 | `	if( zVal == 0 && pNd && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|      - |  3612 | `		/* The other door, the one hasAttributeNS already knew about: a` |
|      - |  3613 | ``		 * DECLARATION answers its URI here. `getAttributeNS($XMLNS, 'p')` was ""`` |
|      - |  3614 | ``		 * on an element declaring `xmlns:p`, where php answers the namespace. */`` |
|      9 |  3615 | `		xmlNsPtr pDecl = DomNsDeclOf((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal);` |
|      9 |  3616 | `		if( pDecl && pDecl->href ){` |
|      3 |  3617 | `			ph7_result_string(pCtx,(const char *)pDecl->href,-1);` |
|      3 |  3618 | `			return PH7_OK;` |
|      - |  3619 | `		}` |
|      3 |  3620 | `	}` |
|     19 |  3621 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|     19 |  3622 | `	if( zVal ){` |
|     11 |  3623 | `		xmlFree(zVal);` |
|      5 |  3624 | `	}` |
|     19 |  3625 | `	return PH7_OK;` |
|     11 |  3626 | `}` |
|      - |  3627 | `/* DOMElement::setAttributeNS(?string $namespace, string $qualifiedName, string $value): void */` |
|     66 |  3628 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNS)` |
|      1 |  3629 | `{` |
|     67 |  3630 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     67 |  3631 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|     67 |  3632 | `	const char *zQname = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|     67 |  3633 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|     67 |  3634 | `	int bHasUri = zUri != 0 && zUri[0] != 0;` |
|      - |  3635 | `	xmlNodePtr pNode;` |
|     67 |  3636 | `	xmlNsPtr pNs = 0;` |
|      - |  3637 | `	dom_qname sQ;` |
|      - |  3638 | `	int rc,bDecl,nOldDefs;` |
|     67 |  3639 | `	if( pNd == 0 ){` |
|    ! 0 |  3640 | `		return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  3641 | `	}` |
|     67 |  3642 | `	if( zQname[0] == 0 ){` |
|      - |  3643 | `		/* php screens the EMPTY name at the parameter, before the DOM sees it. */` |
|      3 |  3644 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  3645 | `			"DOMElement::setAttributeNS(): Argument #2 ($qualifiedName) must not be empty");` |
|      - |  3646 | `	}` |
|     65 |  3647 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_SET,&sQ);` |
|     65 |  3648 | `	if( rc ){` |
|     17 |  3649 | `		return DomThrowVoid(pCtx,rc);` |
|      - |  3650 | `	}` |
|     49 |  3651 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|     49 |  3652 | `	nOldDefs = DomNsDefCount(pNode);` |
|      - |  3653 | `	/* The DECLARATION spelling is the xmlns NAMESPACE plus an xmlns name --` |
|      - |  3654 | ``	 * `xmlns:z` binds z, plain `xmlns` binds the default one. Everything else`` |
|      - |  3655 | ``	 * is an ordinary attribute, including a bare `xmlns` under some other URI:`` |
|      - |  3656 | ``	 * php writes it as an ATTRIBUTE whose name happens to be `xmlns`, which`` |
|      - |  3657 | `	 * serializes beside the declaration already there. */` |
|     57 |  3658 | `	bDecl = DomUriIs(zUri,DOM_XMLNS_NS_URI)` |
|     53 |  3659 | `		&& (sQ.zPrefix ? xmlStrEqual(sQ.zPrefix,(const xmlChar *)"xmlns")` |
|     10 |  3660 | `		               : xmlStrEqual(sQ.zLocal,(const xmlChar *)"xmlns"));` |
|     49 |  3661 | `	if( bHasUri && !bDecl ){` |
|      - |  3662 | `		/* An ordinary attribute: find or declare the namespace it names. The` |
|      - |  3663 | `		 * xmlns URI is not special here -- php declares it like any other,` |
|      - |  3664 | `		 * EXCEPT under a prefix, which is the one binding it will not write.` |
|      - |  3665 | ``		 * The same goes for the prefix `xmlns` itself: php will REUSE a binding`` |
|      - |  3666 | ``		 * for it (which is how `setAttributeNS(XML_NS,'xmlns:z')` ends up as`` |
|      - |  3667 | ``		 * `xml:z`) and refuses to declare one. */`` |
|     35 |  3668 | `		if( sQ.zPrefix && DomUriIs(zUri,DOM_XMLNS_NS_URI) ){` |
|      3 |  3669 | `			DomQNameRelease(&sQ);` |
|      3 |  3670 | `			return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  3671 | `		}` |
|     33 |  3672 | `		pNs = DomNsResolve(pNode,zUri,sQ.zPrefix,0);` |
|     33 |  3673 | `		if( pNs == 0 ){` |
|      5 |  3674 | `			DomQNameRelease(&sQ);` |
|      5 |  3675 | `			return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  3676 | `		}` |
|     14 |  3677 | `	}` |
|     43 |  3678 | `	if( bDecl ){` |
|      - |  3679 | ``		/* The declaration spelling: `xmlns:z` binds z, plain `xmlns` binds the`` |
|      - |  3680 | `		 * default namespace, and the VALUE is the URI being bound. */` |
|     13 |  3681 | `		DomNsDeclare(pNode,sQ.zPrefix ? sQ.zLocal : 0,zVal);` |
|      7 |  3682 | `	}else{` |
|     31 |  3683 | `		xmlSetNsProp(pNode,pNs,sQ.zLocal,(const xmlChar *)zVal);` |
|      - |  3684 | `	}` |
|      - |  3685 | `	/* Either path may have declared something here -- and a declaration, new or` |
|      - |  3686 | `	 * rebound, can take the spelling away from what is already below it. */` |
|     43 |  3687 | `	if( bDecl \|\| DomNsDefCount(pNode) != nOldDefs ){` |
|     27 |  3688 | `		DomNsReconcile(pNode);` |
|     13 |  3689 | `	}` |
|     43 |  3690 | `	DomQNameRelease(&sQ);` |
|     43 |  3691 | `	return PH7_OK;` |
|     34 |  3692 | `}` |
|      - |  3693 |  |
|      - |  3694 | `/* ===== Attribute NODES ===== */` |
|      - |  3695 |  |
|      - |  3696 | `/*` |
|      - |  3697 | ` * The half of the attribute surface that hands out (and takes) the attribute` |
|      - |  3698 | ` * NODE rather than its string.  It is how a program moves an attribute between` |
|      - |  3699 | ` * elements, reads one it has held across an edit, or asks which attributes an` |
|      - |  3700 | ``  * element carries at all -- and none of it existed here, so `getAttributeNode` `` |
|      - |  3701 | `` * was a `Call to undefined method` and every idiom built on it stopped at the`` |
|      - |  3702 | ` * first line.` |
|      - |  3703 | ` */` |
|      - |  3704 |  |
|      - |  3705 | `/* The xmlAttr a DOMAttr-typed argument stands for (already screened by ZPP:` |
|      - |  3706 | ` * anything that is not a DOMAttr never reaches the body). */` |
|     84 |  3707 | `static xmlAttrPtr DomAttrArg(ph7_value *pVal)` |
|      1 |  3708 | `{` |
|     85 |  3709 | `	phl_domnode *pNd = DomObjArg(pVal);` |
|     85 |  3710 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     85 |  3711 | `	return (pNode && pNode->type == XML_ATTRIBUTE_NODE) ? (xmlAttrPtr)pNode : 0;` |
|      1 |  3712 | `}` |
|      - |  3713 | `/* The plain setAttributeNode spelling asks libxml's own name-only question --` |
|      - |  3714 | ``  * php's does too, so an attribute named `b` displaces a namespaced `x:b` `` |
|      - |  3715 | ` * there where the NS spelling would not. */` |
|     84 |  3716 | `static xmlAttrPtr DomAttrByLocal(xmlNodePtr pElem,const char *zName)` |
|      2 |  3717 | `{` |
|     86 |  3718 | `	xmlAttrPtr pAttr = pElem ? xmlHasProp(pElem,(const xmlChar *)zName) : 0;` |
|     86 |  3719 | `	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;` |
|      2 |  3720 | `}` |
|      - |  3721 | `/* Append an attribute to an element's property list. The list is its own chain` |
|      - |  3722 | ` * (pElem->properties), not the child chain, which is why DomLinkLast will not` |
|      - |  3723 | ` * do: an attribute spliced among the CHILDREN serializes inside the tag body. */` |
|     72 |  3724 | `static void DomAttrLinkLast(xmlNodePtr pElem,xmlAttrPtr pAttr)` |
|      2 |  3725 | `{` |
|     74 |  3726 | `	xmlAttrPtr pLast = pElem->properties;` |
|     74 |  3727 | `	pAttr->parent = pElem;` |
|     74 |  3728 | `	pAttr->doc = pElem->doc;` |
|     74 |  3729 | `	pAttr->next = 0;` |
|     74 |  3730 | `	if( pLast == 0 ){` |
|     60 |  3731 | `		pElem->properties = pAttr;` |
|     60 |  3732 | `		pAttr->prev = 0;` |
|     60 |  3733 | `		return;` |
|      - |  3734 | `	}` |
|     21 |  3735 | `	while( pLast->next ){` |
|      7 |  3736 | `		pLast = pLast->next;` |
|      1 |  3737 | `	}` |
|     15 |  3738 | `	pLast->next = pAttr;` |
|     15 |  3739 | `	pAttr->prev = pLast;` |
|     38 |  3740 | `}` |
|      - |  3741 | `/* ...and at a POSITION: before pRef, which must be one of pElem's own --` |
|      - |  3742 | ` * insertBefore against an attribute reference, and replaceChild's` |
|      - |  3743 | ` * attribute-for-attribute swap, the two places php lets a caller state the` |
|      - |  3744 | ` * property list's order. */` |
|      6 |  3745 | `static void DomAttrLinkBefore(xmlNodePtr pElem,xmlAttrPtr pAttr,xmlAttrPtr pRef)` |
|      1 |  3746 | `{` |
|      7 |  3747 | `	pAttr->parent = pElem;` |
|      7 |  3748 | `	pAttr->doc = pElem->doc;` |
|      7 |  3749 | `	pAttr->next = pRef;` |
|      7 |  3750 | `	pAttr->prev = pRef->prev;` |
|      7 |  3751 | `	if( pRef->prev ){` |
|    ! 0 |  3752 | `		pRef->prev->next = pAttr;` |
|    ! 0 |  3753 | `	}else{` |
|      7 |  3754 | `		pElem->properties = pAttr;` |
|      - |  3755 | `	}` |
|      7 |  3756 | `	pRef->prev = pAttr;` |
|      7 |  3757 | `}` |
|      - |  3758 | `/* Detach an attribute from its element (or the orphan set) without freeing it:` |
|      - |  3759 | ` * php hands the caller back the node it displaced, alive. */` |
|     86 |  3760 | `static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr)` |
|      2 |  3761 | `{` |
|     88 |  3762 | `	if( pAttr->parent ){` |
|     31 |  3763 | `		xmlUnlinkNode((xmlNodePtr)pAttr);` |
|     16 |  3764 | `	}else{` |
|     58 |  3765 | `		DomOrphanRemove(pShell,(xmlNodePtr)pAttr);` |
|      - |  3766 | `	}` |
|     88 |  3767 | `}` |
|      - |  3768 | `/* DOMElement::getAttributeNode(string $qualifiedName): DOMAttr\|false -- FALSE` |
|      - |  3769 | ` * for an absent one, where the NS spelling below answers null. */` |
|    112 |  3770 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNode)` |
|      2 |  3771 | `{` |
|    114 |  3772 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    114 |  3773 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    114 |  3774 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    114 |  3775 | `	xmlAttrPtr pAttr = DomAttrByName(pElem,zName);` |
|    114 |  3776 | `	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|    114 |  3777 | `	if( pDecl ){` |
|     17 |  3778 | `		return DomResultNsNode(pCtx,pNd,pDecl,pElem);` |
|      - |  3779 | `	}` |
|     98 |  3780 | `	if( pAttr == 0 ){` |
|      7 |  3781 | `		ph7_result_bool(pCtx,0);` |
|      7 |  3782 | `		return PH7_OK;` |
|      - |  3783 | `	}` |
|     92 |  3784 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     58 |  3785 | `}` |
|      - |  3786 | `/* DOMElement::getAttributeNodeNS(?string $namespace, string $localName): ?DOMAttr */` |
|     34 |  3787 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNodeNS)` |
|      1 |  3788 | `{` |
|     35 |  3789 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     35 |  3790 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     35 |  3791 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     35 |  3792 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     35 |  3793 | `	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|      - |  3794 | `		/* Here the LOCAL name is the prefix being declared -- and the DEFAULT` |
|      - |  3795 | ``		 * declaration, whose local name would be `xmlns`, is not reachable this`` |
|      - |  3796 | `		 * way at all. */` |
|      5 |  3797 | `		xmlNsPtr pDecl = DomNsDeclOf(pElem,(const xmlChar *)zLocal);` |
|      5 |  3798 | `		if( pDecl == 0 ){` |
|      3 |  3799 | `			ph7_result_null(pCtx);` |
|      3 |  3800 | `			return PH7_OK;` |
|      - |  3801 | `		}` |
|      3 |  3802 | `		return DomResultNsNode(pCtx,pNd,pDecl,pElem);` |
|      - |  3803 | `	}` |
|     31 |  3804 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)DomAttrByNs(pElem,zUri,zLocal));` |
|     18 |  3805 | `}` |
|      - |  3806 | `/*` |
|      - |  3807 | ` * DOMElement::setAttributeNode(DOMAttr $attr): ?DOMAttr and its NS spelling.` |
|      - |  3808 | ` *` |
|      - |  3809 | ` * php answers the attribute it DISPLACED (alive and ownerless) or null, and` |
|      - |  3810 | ` * the two spellings differ only in how they decide what "the same attribute"` |
|      - |  3811 | ` * is: the plain one matches on the local name alone, the NS one on the name` |
|      - |  3812 | ` * and the namespace URI together.  An attribute that already belongs to` |
|      - |  3813 | ` * another element of the same document is MOVED, not copied.` |
|      - |  3814 | ` */` |
|     64 |  3815 | `static int DomSetAttrNode(ph7_context *pCtx,int nArg,ph7_value **apArg,int bNS)` |
|      1 |  3816 | `{` |
|     65 |  3817 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     65 |  3818 | `	xmlAttrPtr pAttr = nArg > 0 ? DomAttrArg(apArg[0]) : 0;` |
|     65 |  3819 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  3820 | `	xmlAttrPtr pOld;` |
|     65 |  3821 | `	if( pElem == 0 \|\| pAttr == 0 ){` |
|    ! 0 |  3822 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  3823 | `	}` |
|     65 |  3824 | `	if( pAttr->doc != pElem->doc && pAttr->doc != 0 ){` |
|      7 |  3825 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  3826 | `	}` |
|      - |  3827 | `	/* A constructed, document-less attribute is ADOPTED, like every insertion` |
|      - |  3828 | ``	 * door -- `$el->setAttributeNode(new DOMAttr('k','v'))` is how a built`` |
|      - |  3829 | `	 * attribute reaches a real document. */` |
|     59 |  3830 | `	DomAdoptIntoRecv(pCtx,apArg[0],DomObjArg(apArg[0]));` |
|     53 |  3831 | `	pOld = bNS ? DomAttrByNs(pElem,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name)` |
|     35 |  3832 | `	           : DomAttrByLocal(pElem,(const char *)pAttr->name);` |
|     59 |  3833 | `	if( pOld == pAttr ){` |
|      - |  3834 | `		/* Already this element's, under this spelling: php does nothing at all` |
|      - |  3835 | `		 * and answers null rather than handing the node back to itself. */` |
|      3 |  3836 | `		ph7_result_null(pCtx);` |
|      3 |  3837 | `		return PH7_OK;` |
|      - |  3838 | `	}` |
|     57 |  3839 | `	if( pOld ){` |
|      5 |  3840 | `		xmlUnlinkNode((xmlNodePtr)pOld);` |
|      5 |  3841 | `		DomOrphanAdd(pNd->pShell,(xmlNodePtr)pOld);` |
|      2 |  3842 | `	}` |
|     57 |  3843 | `	DomAttrDetach(pNd->pShell,pAttr);` |
|     57 |  3844 | `	DomAttrLinkLast(pElem,pAttr);` |
|     57 |  3845 | `	DomNsAttrArrive(pElem,pAttr);` |
|     57 |  3846 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pOld);` |
|     33 |  3847 | `}` |
|     18 |  3848 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNode)` |
|      1 |  3849 | `{` |
|     19 |  3850 | `	return DomSetAttrNode(pCtx,nArg,apArg,0);` |
|      1 |  3851 | `}` |
|     46 |  3852 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNodeNS)` |
|      1 |  3853 | `{` |
|     47 |  3854 | `	return DomSetAttrNode(pCtx,nArg,apArg,1);` |
|      1 |  3855 | `}` |
|      - |  3856 | `/* DOMElement::removeAttributeNode(DOMAttr $attr): DOMAttr -- an attribute that` |
|      - |  3857 | ` * is not THIS element's is php's Not Found, whichever element owns it. */` |
|     10 |  3858 | `DOM_METHOD(vm_builtin_DOMElement_removeAttributeNode)` |
|      1 |  3859 | `{` |
|     11 |  3860 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     11 |  3861 | `	xmlAttrPtr pAttr = nArg > 0 ? DomAttrArg(apArg[0]) : 0;` |
|     11 |  3862 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     11 |  3863 | `	if( pElem == 0 \|\| pAttr == 0 \|\| pAttr->parent != pElem ){` |
|      9 |  3864 | `		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  3865 | `	}` |
|      3 |  3866 | `	xmlUnlinkNode((xmlNodePtr)pAttr);` |
|      3 |  3867 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|      3 |  3868 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|      6 |  3869 | `}` |
|      - |  3870 | `/* The qualified name a serializer would write for an attribute. */` |
|     20 |  3871 | `static int DomAttrQName(xmlAttrPtr pAttr,SyBlob *pOut)` |
|      1 |  3872 | `{` |
|     21 |  3873 | `	if( pAttr->ns && pAttr->ns->prefix ){` |
|      3 |  3874 | `		SyBlobAppend(pOut,(const void *)pAttr->ns->prefix,SyStrlen((const char *)pAttr->ns->prefix));` |
|      3 |  3875 | `		SyBlobAppend(pOut,(const void *)":",sizeof(char));` |
|      1 |  3876 | `	}` |
|     21 |  3877 | `	SyBlobAppend(pOut,(const void *)pAttr->name,SyStrlen((const char *)pAttr->name));` |
|     21 |  3878 | `	return PH7_OK;` |
|      1 |  3879 | `}` |
|      - |  3880 | `/* DOMElement::getAttributeNames(): array -- the qualified names, in document` |
|      - |  3881 | ` * order, as a LIST (php re-keys from zero). */` |
|     18 |  3882 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNames)` |
|      1 |  3883 | `{` |
|     19 |  3884 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     19 |  3885 | `	ph7_value *pArray = ph7_context_new_array(pCtx);` |
|     19 |  3886 | `	ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|      - |  3887 | `	xmlAttrPtr pAttr;` |
|      - |  3888 | `	xmlNsPtr pNs;` |
|      9 |  3889 | `	SXUNUSED(nArg);` |
|      9 |  3890 | `	SXUNUSED(apArg);` |
|     19 |  3891 | `	if( pArray == 0 \|\| pVal == 0 ){` |
|    ! 0 |  3892 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3893 | `	}` |
|      - |  3894 | `	/* php lists the element's own DECLARATIONS first, in the order it makes` |
|      - |  3895 | `	 * them, and the attributes after. */` |
|     35 |  3896 | `	for( pNs = pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE` |
|     60 |  3897 | `			? ((xmlNodePtr)pNd->pNode)->nsDef : 0 ; pNs ; pNs = pNs->next ){` |
|      - |  3898 | `		SyBlob sName;` |
|     15 |  3899 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|     15 |  3900 | `		SyBlobAppend(&sName,(const void *)DOM_XMLNS_NAME,SyStrlen(DOM_XMLNS_NAME));` |
|     15 |  3901 | `		if( pNs->prefix ){` |
|      9 |  3902 | `			SyBlobAppend(&sName,(const void *)":",sizeof(char));` |
|      9 |  3903 | `			SyBlobAppend(&sName,(const void *)pNs->prefix,SyStrlen((const char *)pNs->prefix));` |
|      4 |  3904 | `		}` |
|     15 |  3905 | `		ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|     15 |  3906 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     15 |  3907 | `		ph7_value_reset_string_cursor(pVal);` |
|     15 |  3908 | `		SyBlobRelease(&sName);` |
|      8 |  3909 | `	}` |
|     39 |  3910 | `	for( pAttr = DomAttrList(pNd ? (xmlNodePtr)pNd->pNode : 0) ; pAttr ; pAttr = pAttr->next ){` |
|      - |  3911 | `		SyBlob sName;` |
|     21 |  3912 | `		if( pAttr->type != XML_ATTRIBUTE_NODE ){` |
|    ! 0 |  3913 | `			continue;` |
|      - |  3914 | `		}` |
|     21 |  3915 | `		SyBlobInit(&sName,&pCtx->pVm->sAllocator);` |
|     21 |  3916 | `		DomAttrQName(pAttr,&sName);` |
|     21 |  3917 | `		ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));` |
|     21 |  3918 | `		ph7_array_add_elem(pArray,0,pVal);` |
|     21 |  3919 | `		ph7_value_reset_string_cursor(pVal);` |
|     21 |  3920 | `		SyBlobRelease(&sName);` |
|     11 |  3921 | `	}` |
|     19 |  3922 | `	ph7_result_value(pCtx,pArray);` |
|     19 |  3923 | `	return PH7_OK;` |
|     10 |  3924 | `}` |
|      - |  3925 | `/* DOMElement::hasAttributeNS(?string $namespace, string $localName): bool */` |
|     22 |  3926 | `DOM_METHOD(vm_builtin_DOMElement_hasAttributeNS)` |
|      1 |  3927 | `{` |
|     23 |  3928 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     23 |  3929 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     23 |  3930 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     23 |  3931 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     23 |  3932 | `	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){` |
|      - |  3933 | `		/*` |
|      - |  3934 | `		 * Two doors onto that namespace, and php answers about EITHER: a` |
|      - |  3935 | `		 * DECLARATION, which is not an attribute in libxml at all, and a real` |
|      - |  3936 | `` 		 * attribute in it -- which is what `createAttributeNS($XMLNS, ...)` `` |
|      - |  3937 | `		 * makes, and which this only asked the first door about. (A DEFAULT` |
|      - |  3938 | `		 * declaration is not one of them: php answers false for the local name` |
|      - |  3939 | ``		 * `xmlns`, and the prefix comparison below never matches it.)`` |
|      - |  3940 | `		 */` |
|     29 |  3941 | `		ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0` |
|     14 |  3942 | `			\|\| DomNsDeclOf(pElem,(const xmlChar *)zLocal) != 0);` |
|     17 |  3943 | `		return PH7_OK;` |
|      - |  3944 | `	}` |
|      7 |  3945 | `	ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0);` |
|      7 |  3946 | `	return PH7_OK;` |
|     12 |  3947 | `}` |
|      - |  3948 | `/* DOMElement::removeAttributeNS(?string $namespace, string $localName): void --` |
|      - |  3949 | ` * an absent one is silence, as php's is. */` |
|      6 |  3950 | `DOM_METHOD(vm_builtin_DOMElement_removeAttributeNS)` |
|      1 |  3951 | `{` |
|      7 |  3952 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  3953 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      7 |  3954 | `	xmlAttrPtr pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,DomArgUri(nArg,apArg,0),zLocal) : 0;` |
|      7 |  3955 | `	if( pAttr ){` |
|      5 |  3956 | `		xmlRemoveProp(pAttr);` |
|      2 |  3957 | `	}` |
|      7 |  3958 | `	return PH7_OK;` |
|      1 |  3959 | `}` |
|      - |  3960 | `/*` |
|      - |  3961 | ` * DOMElement::toggleAttribute(string $qualifiedName, ?bool $force = null): bool` |
|      - |  3962 | ` *` |
|      - |  3963 | ` * php's 8.3 verb: with no $force it flips (removing answers false, adding a` |
|      - |  3964 | ` * value-less attribute answers true), and with one it only ADDS or only` |
|      - |  3965 | ` * REMOVES -- an add that finds the attribute already there leaves its value` |
|      - |  3966 | ` * alone.  The name is validated first, so a bad one is refused before the` |
|      - |  3967 | ` * element is touched.` |
|      - |  3968 | ` */` |
|     20 |  3969 | `DOM_METHOD(vm_builtin_DOMElement_toggleAttribute)` |
|      1 |  3970 | `{` |
|     21 |  3971 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     21 |  3972 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     21 |  3973 | `	int bForceGiven = nArg > 1 && !ph7_value_is_null(apArg[1]);` |
|     21 |  3974 | `	int bForce = bForceGiven && ph7_value_to_bool(apArg[1]);` |
|     21 |  3975 | `	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  3976 | `	xmlAttrPtr pAttr;` |
|      - |  3977 | `	xmlNsPtr pDecl;` |
|     21 |  3978 | `	if( pElem == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      5 |  3979 | `		return DomThrowAlways(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  3980 | `	}` |
|     17 |  3981 | `	pAttr = DomAttrByName(pElem,zName);` |
|     17 |  3982 | `	pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);` |
|     17 |  3983 | `	if( bForceGiven ? bForce : (pAttr == 0 && pDecl == 0) ){` |
|      7 |  3984 | `		if( pAttr == 0 && pDecl == 0 ){` |
|      5 |  3985 | `			sxu32 nXmlns = (sxu32)SyStrlen(DOM_XMLNS_NAME);` |
|      6 |  3986 | `			int bXmlnsName = SyStrlen(zName) >= nXmlns` |
|      3 |  3987 | `				&& SyStrncmp(zName,DOM_XMLNS_NAME,nXmlns) == 0` |
|      6 |  3988 | `				&& (zName[nXmlns] == 0 \|\| zName[nXmlns] == ':');` |
|      5 |  3989 | `			if( bXmlnsName ){` |
|      - |  3990 | `				/* An xmlns name toggled ON becomes a DECLARATION bound to the` |
|      - |  3991 | `				 * empty URI, not an attribute -- which is why it comes out` |
|      - |  3992 | `				 * before the attributes rather than after them. */` |
|      3 |  3993 | `				DomNsDeclare(pElem,zName[nXmlns] == ':' ? (const xmlChar *)(zName+nXmlns+1) : 0,"");` |
|      2 |  3994 | `			}else{` |
|      3 |  3995 | `				xmlSetProp(pElem,(const xmlChar *)zName,(const xmlChar *)"");` |
|      - |  3996 | `			}` |
|      2 |  3997 | `		}` |
|      7 |  3998 | `		ph7_result_bool(pCtx,1);` |
|      7 |  3999 | `		return PH7_OK;` |
|      - |  4000 | `	}` |
|     11 |  4001 | `	if( pAttr ){` |
|      5 |  4002 | `		xmlRemoveProp(pAttr);` |
|      7 |  4003 | `	}else if( pDecl ){` |
|      5 |  4004 | `		DomNsDeclRemove(pElem,pDecl);` |
|      5 |  4005 | `		DomNsReconcile(pElem);` |
|      2 |  4006 | `	}` |
|      9 |  4007 | `	ph7_result_bool(pCtx,0);` |
|      9 |  4008 | `	return PH7_OK;` |
|      9 |  4009 | `}` |
|      - |  4010 | `/*` |
|      - |  4011 | `` * The ID three.  php's `setIdAttribute*` is what makes an attribute the one`` |
|      - |  4012 | `` * `getElementById()` answers by, in a document with no DTD to say so, and`` |
|      - |  4013 | `` * `DOMAttr::isId()` is how a caller reads the flag back.  All three refuse a`` |
|      - |  4014 | ` * name the element does not carry with Not Found -- including an attribute that` |
|      - |  4015 | ` * belongs to a DIFFERENT element, which is why the node spelling checks the` |
|      - |  4016 | ` * owner rather than trusting the argument.` |
|      - |  4017 | ` */` |
|     26 |  4018 | `static int DomMarkId(xmlAttrPtr pAttr,int bIsId)` |
|      1 |  4019 | `{` |
|     27 |  4020 | `	if( bIsId ){` |
|     23 |  4021 | `		if( pAttr->atype != XML_ATTRIBUTE_ID ){` |
|     23 |  4022 | `			xmlChar *zVal = xmlNodeListGetString(pAttr->doc,pAttr->children,1);` |
|     23 |  4023 | `			if( zVal ){` |
|     23 |  4024 | `				xmlAddID(0,pAttr->doc,zVal,pAttr);` |
|     23 |  4025 | `				xmlFree(zVal);` |
|     11 |  4026 | `			}` |
|     11 |  4027 | `		}` |
|     23 |  4028 | `		pAttr->atype = XML_ATTRIBUTE_ID;` |
|     12 |  4029 | `	}else{` |
|      5 |  4030 | `		if( pAttr->atype == XML_ATTRIBUTE_ID ){` |
|      5 |  4031 | `			xmlRemoveID(pAttr->doc,pAttr);` |
|      2 |  4032 | `		}` |
|      5 |  4033 | `		pAttr->atype = (xmlAttributeType)0;` |
|      - |  4034 | `	}` |
|     27 |  4035 | `	return PH7_OK;` |
|      1 |  4036 | `}` |
|     26 |  4037 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttribute)` |
|      1 |  4038 | `{` |
|     27 |  4039 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     27 |  4040 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|      - |  4041 | `	/* php's ID lookup is the STRICT one -- the whole name in NO namespace, with` |
|      - |  4042 | ``	 * no prefix resolution, so `setIdAttribute('p:k')` is Not Found even on an`` |
|      - |  4043 | ``	 * element that carries `p:k`. */`` |
|     27 |  4044 | `	xmlAttrPtr pAttr = pNd ? DomAttrNoNs((xmlNodePtr)pNd->pNode,zName) : 0;` |
|     27 |  4045 | `	if( pAttr == 0 ){` |
|      9 |  4046 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  4047 | `	}` |
|     19 |  4048 | `	return DomMarkId(pAttr,nArg > 1 && ph7_value_to_bool(apArg[1]));` |
|     14 |  4049 | `}` |
|      - |  4050 | `/* php's second parameter is spelled $qualifiedName and matched as a LOCAL one:` |
|      - |  4051 | ` * the namespace decides the rest. */` |
|      8 |  4052 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttributeNS)` |
|      1 |  4053 | `{` |
|      9 |  4054 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      9 |  4055 | `	const char *zLocal = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|      9 |  4056 | `	xmlAttrPtr pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,DomArgUri(nArg,apArg,0),zLocal) : 0;` |
|      9 |  4057 | `	if( pAttr == 0 ){` |
|      5 |  4058 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  4059 | `	}` |
|      5 |  4060 | `	return DomMarkId(pAttr,nArg > 2 && ph7_value_to_bool(apArg[2]));` |
|      5 |  4061 | `}` |
|     10 |  4062 | `DOM_METHOD(vm_builtin_DOMElement_setIdAttributeNode)` |
|      1 |  4063 | `{` |
|     11 |  4064 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     11 |  4065 | `	xmlAttrPtr pAttr = nArg > 1 ? DomAttrArg(apArg[0]) : 0;` |
|     11 |  4066 | `	if( pNd == 0 \|\| pAttr == 0 \|\| pAttr->parent != (xmlNodePtr)pNd->pNode ){` |
|      7 |  4067 | `		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);` |
|      - |  4068 | `	}` |
|      5 |  4069 | `	return DomMarkId(pAttr,nArg > 1 && ph7_value_to_bool(apArg[1]));` |
|      6 |  4070 | `}` |
|      - |  4071 | `/* DOMAttr::isId(): bool */` |
|     42 |  4072 | `DOM_METHOD(vm_builtin_DOMAttr_isId)` |
|      1 |  4073 | `{` |
|     43 |  4074 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     43 |  4075 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     21 |  4076 | `	SXUNUSED(nArg);` |
|     21 |  4077 | `	SXUNUSED(apArg);` |
|     85 |  4078 | `	ph7_result_bool(pCtx,pNode != 0 && pNode->type == XML_ATTRIBUTE_NODE` |
|     42 |  4079 | `		&& ((xmlAttrPtr)pNode)->atype == XML_ATTRIBUTE_ID);` |
|     43 |  4080 | `	return PH7_OK;` |
|      1 |  4081 | `}` |
|      - |  4082 | `/*` |
|      - |  4083 | ` * DOMDocument::getElementById(string $elementId): ?DOMElement` |
|      - |  4084 | ` *` |
|      - |  4085 | `` * The other half of the ID three: what `setIdAttribute()` is FOR. libxml keeps`` |
|      - |  4086 | `` * the table (a DTD `ATTLIST ... ID` fills it at parse time, `xmlAddID` fills it`` |
|      - |  4087 | ` * when a program marks one), and php answers the attribute's element -- but` |
|      - |  4088 | ` * only while that element is still IN the document, so an element removed from` |
|      - |  4089 | ` * the tree stops being findable even though its attribute still carries the` |
|      - |  4090 | ` * flag. A DTD-declared DEFAULT has no attribute node behind it and libxml says` |
|      - |  4091 | ` * so with its own sentinel.` |
|      - |  4092 | ` */` |
|     44 |  4093 | `DOM_METHOD(vm_builtin_DOMDocument_getElementById)` |
|      1 |  4094 | `{` |
|     45 |  4095 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     45 |  4096 | `	const char *zId = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     45 |  4097 | `	xmlAttrPtr pAttr = pNd ? xmlGetID((xmlDocPtr)pNd->pNode,(const xmlChar *)zId) : 0;` |
|     44 |  4098 | `	if( pAttr == 0 \|\| pAttr == (xmlAttrPtr)-1 \|\| pAttr->type != XML_ATTRIBUTE_NODE` |
|     26 |  4099 | `	 \|\| pAttr->parent == 0 \|\| !DomIsConnected(pAttr->parent) ){` |
|     23 |  4100 | `		ph7_result_null(pCtx);` |
|     23 |  4101 | `		return PH7_OK;` |
|      - |  4102 | `	}` |
|     23 |  4103 | `	return DomResultNodeOf(pCtx,pNd,pAttr->parent);` |
|     23 |  4104 | `}` |
|      - |  4105 | `/* DOMDocument::createAttribute(string $localName): DOMAttr -- ownerless: php` |
|      - |  4106 | ` * gives it this document but NO element until it is set on one. */` |
|     66 |  4107 | `DOM_METHOD(vm_builtin_DOMDocument_createAttribute)` |
|      2 |  4108 | `{` |
|     68 |  4109 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     68 |  4110 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|      - |  4111 | `	xmlAttrPtr pAttr;` |
|     68 |  4112 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      9 |  4113 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  4114 | `	}` |
|     60 |  4115 | `	pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,(const xmlChar *)zName,0);` |
|     60 |  4116 | `	if( pAttr == 0 ){` |
|    ! 0 |  4117 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4118 | `	}` |
|     60 |  4119 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|     60 |  4120 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     35 |  4121 | `}` |
|      - |  4122 | `/*` |
|      - |  4123 | ` * DOMDocument::createAttributeNS(?string $namespace, string $qualifiedName): DOMAttr` |
|      - |  4124 | ` *` |
|      - |  4125 | ` * The namespace is declared on the document's ROOT ELEMENT, not on the` |
|      - |  4126 | ` * attribute -- which is why a document that has no root element yet cannot` |
|      - |  4127 | ` * answer at all, and says so with php's warning and a false.` |
|      - |  4128 | ` */` |
|     42 |  4129 | `DOM_METHOD(vm_builtin_DOMDocument_createAttributeNS)` |
|      1 |  4130 | `{` |
|     43 |  4131 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     43 |  4132 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|     43 |  4133 | `	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  4134 | `	xmlNodePtr pRoot;` |
|      - |  4135 | `	xmlAttrPtr pAttr;` |
|      - |  4136 | `	dom_qname sQ;` |
|      - |  4137 | `	int rc;` |
|     43 |  4138 | `	if( pNd == 0 ){` |
|    ! 0 |  4139 | `		return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  4140 | `	}` |
|     43 |  4141 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_ATTR,&sQ);` |
|     43 |  4142 | `	if( rc ){` |
|     17 |  4143 | `		return DomThrow(pCtx,rc);` |
|      - |  4144 | `	}` |
|     27 |  4145 | `	pRoot = xmlDocGetRootElement((xmlDocPtr)pNd->pNode);` |
|     27 |  4146 | `	if( pRoot == 0 ){` |
|      3 |  4147 | `		DomQNameRelease(&sQ);` |
|      - |  4148 | ``		/* The context prints php's `DOMDocument::createAttributeNS(): ` itself. */`` |
|      3 |  4149 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Document Missing Root Element");` |
|      3 |  4150 | `		ph7_result_bool(pCtx,0);` |
|      3 |  4151 | `		return PH7_OK;` |
|      - |  4152 | `	}` |
|     25 |  4153 | `	pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,sQ.zLocal,0);` |
|     25 |  4154 | `	if( pAttr == 0 ){` |
|    ! 0 |  4155 | `		DomQNameRelease(&sQ);` |
|    ! 0 |  4156 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4157 | `	}` |
|     25 |  4158 | `	if( zUri && zUri[0] && sQ.zPrefix ){` |
|     15 |  4159 | `		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,sQ.zPrefix,1));` |
|     18 |  4160 | `	}else if( zUri && zUri[0]` |
|     11 |  4161 | `	 && xmlStrEqual(sQ.zLocal,(const xmlChar *)DOM_XMLNS_NAME) ){` |
|      - |  4162 | `		/*` |
|      - |  4163 | ``		 * The unprefixed `xmlns` -- the only name the grammar lets through with`` |
|      - |  4164 | `		 * the xmlns namespace, and the name a DEFAULT declaration is written` |
|      - |  4165 | `		 * with. It IS in that namespace and php answers so, but nothing is` |
|      - |  4166 | `		 * DECLARED for it: the binding is the attribute's alone, so it is parked` |
|      - |  4167 | `		 * on the document, which is what frees it. Without this the attribute` |
|      - |  4168 | ``		 * answered namespaceURI null, `hasAttributeNS($XMLNS, 'xmlns')` was`` |
|      - |  4169 | ``		 * false for it once written, and `getAttribute('xmlns')` -- a by-NAME`` |
|      - |  4170 | `		 * lookup, which skips a namespaced attribute -- answered its value where` |
|      - |  4171 | `		 * php answers "".` |
|      - |  4172 | `		 */` |
|      5 |  4173 | `		xmlNsPtr pNs = DomNsReuse(pRoot,zUri);` |
|      5 |  4174 | `		if( pNs == 0 ){` |
|      3 |  4175 | `			pNs = xmlNewNs(0,(const xmlChar *)zUri,0);` |
|      3 |  4176 | `			if( pNs ){` |
|      3 |  4177 | `				DomNsPark((xmlNodePtr)pAttr,pNs);` |
|      1 |  4178 | `			}` |
|      1 |  4179 | `		}` |
|      5 |  4180 | `		if( pNs ){` |
|      5 |  4181 | `			xmlSetNs((xmlNodePtr)pAttr,pNs);` |
|      3 |  4182 | `		}` |
|      9 |  4183 | `	}else if( zUri && zUri[0] ){` |
|      5 |  4184 | `		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,0,1));` |
|      2 |  4185 | `	}` |
|     25 |  4186 | `	DomQNameRelease(&sQ);` |
|     25 |  4187 | `	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);` |
|     25 |  4188 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|     22 |  4189 | `}` |
|      - |  4190 |  |
|      - |  4191 | `/* ===== getElementsByTagName (live) ===== */` |
|      - |  4192 |  |
|      - |  4193 | `/* Length-carrying: the name comes from a declared string SLOT, whose bytes are` |
|      - |  4194 | ` * NOT NUL-terminated (PH7_NativeAttrStr borrows the blob as-is). */` |
|   1314 |  4195 | `static int DomLenEq(const xmlChar *zHave,const char *zWant,int nWant)` |
|      1 |  4196 | `{` |
|   1870 |  4197 | `	return zHave != 0 && (int)SyStrlen((const char *)zHave) == nWant` |
|   1971 |  4198 | `		&& SyMemcmp((const void *)zHave,(const void *)zWant,(sxu32)nWant) == 0;` |
|      1 |  4199 | `}` |
|      - |  4200 | `/*` |
|      - |  4201 | ` * One element against a (namespace, local name) pair. nUri < 0 is the name-only` |
|      - |  4202 | `` * query, `getElementsByTagName` -- the sentinel is the LENGTH and not the`` |
|      - |  4203 | ` * pointer because an empty declared string slot reads back as a NULL one, which` |
|      - |  4204 | ` * is exactly the namespace-aware "in NO namespace" case. Under the` |
|      - |  4205 | ` * namespace-aware query php's three cases are NOT symmetric, and that asymmetry` |
|      - |  4206 | ` * is the whole content of the rule:` |
|      - |  4207 | ` *` |
|      - |  4208 | `` *   `*`          every element, in a namespace or in none`` |
|      - |  4209 | ` *   null or ""   only the elements in NO namespace (php maps its null argument` |
|      - |  4210 | ` *                and the empty string to the same question)` |
|      - |  4211 | ` *   a URI        only the elements in it` |
|      - |  4212 | ` *` |
|      - |  4213 | `` * The local name is `*` for every name, and an exact match otherwise -- against`` |
|      - |  4214 | `` * libxml's `name`, which is the LOCAL name, so a prefix never enters into it.`` |
|      - |  4215 | ` */` |
|   1874 |  4216 | `static int DomGebtnMatch(xmlNodePtr pNode,const char *zUri,int nUri,` |
|      - |  4217 | `	const char *zName,int nName)` |
|      1 |  4218 | `{` |
|   1875 |  4219 | `	if( pNode->type != XML_ELEMENT_NODE ){` |
|     47 |  4220 | `		return 0;` |
|      - |  4221 | `	}` |
|   1829 |  4222 | `	if( !(nName == 1 && zName[0] == '*') && !DomLenEq(pNode->name,zName,nName) ){` |
|    581 |  4223 | `		return 0;` |
|      - |  4224 | `	}` |
|   1249 |  4225 | `	if( nUri < 0 \|\| (nUri == 1 && zUri[0] == '*') ){` |
|    375 |  4226 | `		return 1;` |
|      - |  4227 | `	}` |
|    503 |  4228 | `	if( nUri == 0 ){` |
|    193 |  4229 | `		return pNode->ns == 0;` |
|      - |  4230 | `	}` |
|    311 |  4231 | `	return pNode->ns != 0 && DomLenEq(pNode->ns->href,zUri,nUri);` |
|    752 |  4232 | `}` |
|      - |  4233 | `/* The list is LIVE: nothing is snapshotted, both queries re-walk the subtree` |
|      - |  4234 | ` * every time DOMNodeList asks. Passing iWant < 0 counts instead of indexing. */` |
|    338 |  4235 | `static xmlNodePtr DomGebtnWalk(xmlNodePtr pRoot,const char *zUri,int nUri,` |
|      - |  4236 | `	const char *zName,int nName,int iWant,int *pnCount)` |
|      1 |  4237 | `{` |
|    339 |  4238 | `	xmlNodePtr pCur = pRoot ? pRoot->children : 0;` |
|    339 |  4239 | `	int iCount = 0;` |
|   1633 |  4240 | `	while( pCur ){` |
|   1503 |  4241 | `		if( DomGebtnMatch(pCur,zUri,nUri,zName,nName) ){` |
|    547 |  4242 | `			if( iWant >= 0 && iCount == iWant ){` |
|    209 |  4243 | `				return pCur;` |
|      - |  4244 | `			}` |
|    339 |  4245 | `			iCount++;` |
|    169 |  4246 | `		}` |
|   1295 |  4247 | `		pCur = DomWalkNext(pCur,pRoot);` |
|      1 |  4248 | `	}` |
|    131 |  4249 | `	if( pnCount ){` |
|     71 |  4250 | `		*pnCount = iCount;` |
|     35 |  4251 | `	}` |
|    131 |  4252 | `	return 0;` |
|    170 |  4253 | `}` |
|      - |  4254 |  |
|      - |  4255 | `/* ===== DOMDocument ===== */` |
|      - |  4256 |  |
|      - |  4257 | `/*` |
|      - |  4258 | ` * DOMDocument::__construct(string $version = '1.0', string $encoding = '')` |
|      - |  4259 | ` *` |
|      - |  4260 | `` * The chunk reached the document through `parent::__construct(__dom_doc_new(..))`;`` |
|      - |  4261 | ` * a native constructor writes its own two slots, and $__doc is the document` |
|      - |  4262 | ` * ITSELF (php's ownerDocument is null on a document, which __get answers).` |
|      - |  4263 | ` */` |
|   1664 |  4264 | `DOM_METHOD(vm_builtin_DOMDocument_construct)` |
|      5 |  4265 | `{` |
|   1669 |  4266 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1669 |  4267 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1669 |  4268 | `	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";` |
|   1669 |  4269 | `	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  4270 | `	xmlDocPtr pDoc;` |
|      - |  4271 | `	phl_xmldoc *pShell;` |
|      - |  4272 | `	phl_domnode *pRes;` |
|   1669 |  4273 | `	if( pThis == 0 ){` |
|    ! 0 |  4274 | `		return PH7_OK;` |
|      - |  4275 | `	}` |
|      - |  4276 | ``	/* The version goes through as WRITTEN -- `new DOMDocument('')` is a document`` |
|      - |  4277 | ``	 * whose `version` reads "" and whose declaration says `version=""`, which is`` |
|      - |  4278 | `	 * php's answer (only an omitted argument takes the "1.0" default, and that` |
|      - |  4279 | `	 * one is the signature's). */` |
|   1669 |  4280 | `	pDoc = xmlNewDoc((const xmlChar *)zVersion);` |
|   1669 |  4281 | `	if( pDoc == 0 ){` |
|    ! 0 |  4282 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4283 | `	}` |
|   1669 |  4284 | `	if( zEncoding[0] ){` |
|      5 |  4285 | `		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);` |
|      2 |  4286 | `	}` |
|   1669 |  4287 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|   1669 |  4288 | `	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|   1669 |  4289 | `	if( pRes == 0 ){` |
|    ! 0 |  4290 | `		if( pShell == 0 ){` |
|    ! 0 |  4291 | `			xmlFreeDoc(pDoc);` |
|    ! 0 |  4292 | `		}` |
|    ! 0 |  4293 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4294 | `	}` |
|   1669 |  4295 | `	DomSetRes(pVm,pThis,pRes);` |
|   1669 |  4296 | `	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);` |
|   1669 |  4297 | `	return PH7_OK;` |
|    837 |  4298 | `}` |
|      - |  4299 | `/*` |
|      - |  4300 | ` * The URI php stamps on a document parsed from MEMORY.` |
|      - |  4301 | ` *` |
|      - |  4302 | ` * A file parse takes its URI from the file; a memory parse has none, and php` |
|      - |  4303 | ` * gives it the process's CURRENT DIRECTORY with a trailing separator so that a` |
|      - |  4304 | `` * relative `xml:base` (and every `baseURI` under it) resolves against the same`` |
|      - |  4305 | ` * place a relative include would. The path is the bytes getcwd() answers, not a` |
|      - |  4306 | ` * URI: a space stays a space. Asks the VFS rather than the C library so the` |
|      - |  4307 | ` * win32 backend answers its own spelling.` |
|      - |  4308 | ` *` |
|      - |  4309 | ` * The answer travels through the context's RESULT slot, which is where the VFS` |
|      - |  4310 | ` * writes it -- every caller sets its own return value afterwards.` |
|      - |  4311 | ` */` |
|   1562 |  4312 | `static void DomCwdUri(ph7_context *pCtx,SyBlob *pOut)` |
|      5 |  4313 | `{` |
|   1567 |  4314 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|   1567 |  4315 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|   1567 |  4316 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   1562 |  4317 | `	if( pVfs && pVfs->xGetcwd && pVfs->xGetcwd(pCtx) == PH7_OK` |
|   1567 |  4318 | `	 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0 ){` |
|   1567 |  4319 | `		const char *zDir = (const char *)SyBlobData(&pCtx->pRet->sBlob);` |
|   1567 |  4320 | `		sxu32 nDir = SyBlobLength(&pCtx->pRet->sBlob);` |
|   1567 |  4321 | `		SyBlobAppend(pOut,zDir,nDir);` |
|   1567 |  4322 | `		if( nDir < 1 \|\| (zDir[nDir - 1] != '/' && zDir[nDir - 1] != '\\') ){` |
|   1567 |  4323 | `			SyBlobAppend(pOut,"/",sizeof(char));` |
|    781 |  4324 | `		}` |
|    781 |  4325 | `	}` |
|   1567 |  4326 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   1567 |  4327 | `	SyBlobNullAppend(pOut);` |
|   1567 |  4328 | `}` |
|      - |  4329 | `/* Stamp it, unless the parse already gave the document one. */` |
|   1572 |  4330 | `static void DomStampCwd(ph7_context *pCtx,xmlDocPtr pDoc)` |
|      5 |  4331 | `{` |
|      - |  4332 | `	SyBlob sDir;` |
|   1577 |  4333 | `	if( pDoc == 0 \|\| pDoc->URL ){` |
|     15 |  4334 | `		return;` |
|      - |  4335 | `	}` |
|   1563 |  4336 | `	DomCwdUri(pCtx,&sDir);` |
|   1563 |  4337 | `	if( SyBlobLength(&sDir) > 0 ){` |
|   1563 |  4338 | `		pDoc->URL = xmlStrdup((const xmlChar *)SyBlobData(&sDir));` |
|    779 |  4339 | `	}` |
|   1563 |  4340 | `	SyBlobRelease(&sDir);` |
|    791 |  4341 | `}` |
|      - |  4342 | `/*` |
|      - |  4343 | ` * The parse options one of php's load methods actually runs with: the caller's` |
|      - |  4344 | `` * `$options`, OR'd with what the document's own directives ask for.`` |
|      - |  4345 | ` *` |
|      - |  4346 | ` *   preserveWhiteSpace = false  ->  NOBLANKS   (drop ignorable whitespace)` |
|      - |  4347 | ` *   substituteEntities = true   ->  NOENT      (expand entity references)` |
|      - |  4348 | ` *   validateOnParse    = true   ->  DTDVALID   (validate against the DTD)` |
|      - |  4349 | ` *   resolveExternals   = true   ->  DTDATTR    (apply the DTD's default` |
|      - |  4350 | ` *                                               attributes -- which is also` |
|      - |  4351 | ` *                                               what makes libxml LOAD an` |
|      - |  4352 | ` *                                               external subset)` |
|      - |  4353 | ` *   recover            = true   ->  RECOVER    (keep what parsed)` |
|      - |  4354 | ` *` |
|      - |  4355 | ` * The two directions never cancel: a directive can only ADD to the argument,` |
|      - |  4356 | `` * which is why `loadXML($s, LIBXML_NOENT)` expands entities on a document whose`` |
|      - |  4357 | `` * `substituteEntities` is false.`` |
|      - |  4358 | ` */` |
|   1584 |  4359 | `static int DomParseOptions(ph7_class_instance *pThis,int iOpts)` |
|      5 |  4360 | `{` |
|   1589 |  4361 | `	if( pThis == 0 ){` |
|    ! 0 |  4362 | `		return iOpts;` |
|      - |  4363 | `	}` |
|   1589 |  4364 | `	if( !DomDocFlag(pThis,DOM_F_PRESERVE_WS) ){` |
|     19 |  4365 | `		iOpts \|= XML_PARSE_NOBLANKS;` |
|      9 |  4366 | `	}` |
|   1589 |  4367 | `	if( DomDocFlag(pThis,DOM_F_SUBST_ENT) ){` |
|     11 |  4368 | `		iOpts \|= XML_PARSE_NOENT;` |
|      5 |  4369 | `	}` |
|   1589 |  4370 | `	if( DomDocFlag(pThis,DOM_F_VALIDATE) ){` |
|     39 |  4371 | `		iOpts \|= XML_PARSE_DTDVALID;` |
|     19 |  4372 | `	}` |
|   1589 |  4373 | `	if( DomDocFlag(pThis,DOM_F_RESOLVE_EXT) ){` |
|      9 |  4374 | `		iOpts \|= XML_PARSE_DTDATTR;` |
|      4 |  4375 | `	}` |
|   1589 |  4376 | `	if( DomDocFlag(pThis,DOM_F_RECOVER) ){` |
|     13 |  4377 | `		iOpts \|= XML_PARSE_RECOVER;` |
|      6 |  4378 | `	}` |
|   1589 |  4379 | `	return iOpts;` |
|    797 |  4380 | `}` |
|      - |  4381 | `/*` |
|      - |  4382 | `` * A RECOVERING parse reports its diagnostics whatever `error_reporting()` says.`` |
|      - |  4383 | ` *` |
|      - |  4384 | ` * php forces E_WARNING back into the mask for the duration of a parse it is` |
|      - |  4385 | ` * recovering from -- the point being that a document which came back DAMAGED` |
|      - |  4386 | ` * must not do so in silence, however the script has configured reporting. It` |
|      - |  4387 | ` * forces that one level only: a libxml WARNING (an E_NOTICE) stays suppressed.` |
|      - |  4388 | ` * Answers the previous state, which the caller restores.` |
|      - |  4389 | ` */` |
|      - |  4390 | `typedef struct { sxi32 iMask; int bOn; } phl_dom_errsave;` |
|   1584 |  4391 | `static phl_dom_errsave DomForceWarnings(ph7_vm *pVm,int bRecover)` |
|      5 |  4392 | `{` |
|      - |  4393 | `	phl_dom_errsave sSave;` |
|   1589 |  4394 | `	sSave.iMask = pVm->iErrMask;` |
|   1589 |  4395 | `	sSave.bOn = pVm->bErrReport;` |
|   1589 |  4396 | `	if( bRecover ){` |
|     15 |  4397 | `		pVm->iErrMask \|= E_WARNING;` |
|     15 |  4398 | `		pVm->bErrReport = 1;` |
|      7 |  4399 | `	}` |
|   1589 |  4400 | `	return sSave;` |
|      5 |  4401 | `}` |
|   1584 |  4402 | `static void DomRestoreWarnings(ph7_vm *pVm,phl_dom_errsave sSave)` |
|      5 |  4403 | `{` |
|   1589 |  4404 | `	pVm->iErrMask = sSave.iMask;` |
|   1589 |  4405 | `	pVm->bErrReport = sSave.bOn;` |
|   1589 |  4406 | `}` |
|      - |  4407 | `/*` |
|      - |  4408 | ` * The path php names a loaded file by: the VFS's canonical absolute name, or` |
|      - |  4409 | ` * the working directory joined to it when the file does not exist and there is` |
|      - |  4410 | ` * nothing to canonicalize. Answers it NUL-terminated in *pOut.` |
|      - |  4411 | ` */` |
|     18 |  4412 | `static void DomAbsPath(ph7_context *pCtx,const char *zFile,SyBlob *pOut)` |
|      1 |  4413 | `{` |
|     19 |  4414 | `	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;` |
|     14 |  4415 | `	int bAbs = zFile[0] == '/' \|\| zFile[0] == '\\'` |
|     20 |  4416 | `		\|\| (zFile[0] && zFile[1] == ':');   /* the win32 spelling */` |
|     19 |  4417 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|     19 |  4418 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     18 |  4419 | `	if( pVfs && pVfs->xRealpath && pVfs->xRealpath(zFile,pCtx) == PH7_OK` |
|     14 |  4420 | `	 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0` |
|     11 |  4421 | `	 && SyBlobLength(&pCtx->pRet->sBlob) > 0 ){` |
|     11 |  4422 | `		SyBlobAppend(pOut,SyBlobData(&pCtx->pRet->sBlob),SyBlobLength(&pCtx->pRet->sBlob));` |
|      6 |  4423 | `	}else{` |
|      - |  4424 | `		/* Not there to canonicalize -- but php still resolves the part that IS` |
|      - |  4425 | `		 * there (expand_filepath walks each existing component through its` |
|      - |  4426 | `		 * links), so a missing file under a linked directory is named by the` |
|      - |  4427 | `		 * directory's real path; on macOS every temp path is one (/var ->` |
|      - |  4428 | `		 * /private/var). The longest existing prefix is resolved and the missing` |
|      - |  4429 | `		 * tail kept as written. A bare drive ("C:") is never a prefix: it would` |
|      - |  4430 | `		 * resolve to that drive's working directory. */` |
|      - |  4431 | `		SyBlob sFull,sPre;` |
|      - |  4432 | `		const char *zFull;` |
|      9 |  4433 | `		sxu32 nFull,i,nTail = 0;` |
|      9 |  4434 | `		SyBlobInit(&sFull,&pCtx->pVm->sAllocator);` |
|      9 |  4435 | `		SyBlobInit(&sPre,&pCtx->pVm->sAllocator);` |
|      9 |  4436 | `		if( !bAbs ){` |
|      - |  4437 | `			SyBlob sDir;` |
|      4 |  4438 | `			DomCwdUri(pCtx,&sDir);` |
|      4 |  4439 | `			SyBlobAppend(&sFull,SyBlobData(&sDir),SyBlobLength(&sDir));` |
|      4 |  4440 | `			SyBlobRelease(&sDir);` |
|      2 |  4441 | `		}` |
|      9 |  4442 | `		SyBlobAppend(&sFull,zFile,(sxu32)SyStrlen(zFile));` |
|      9 |  4443 | `		SyBlobNullAppend(&sFull);` |
|      9 |  4444 | `		zFull = (const char *)SyBlobData(&sFull);` |
|      9 |  4445 | `		nFull = (sxu32)SyStrlen(zFull);` |
|    173 |  4446 | `		for( i = nFull ; i > 1 && pVfs && pVfs->xRealpath ; --i ){` |
|    173 |  4447 | `			if( (zFull[i-1] != '/' && zFull[i-1] != '\\') \|\| zFull[i-2] == ':' ){` |
|    153 |  4448 | `				continue;` |
|      - |  4449 | `			}` |
|     21 |  4450 | `			SyBlobReset(&sPre);` |
|     21 |  4451 | `			SyBlobAppend(&sPre,zFull,i-1);` |
|     21 |  4452 | `			SyBlobNullAppend(&sPre);` |
|     21 |  4453 | `			PH7_MemObjRelease(pCtx->pRet);` |
|     20 |  4454 | `			if( pVfs->xRealpath((const char *)SyBlobData(&sPre),pCtx) == PH7_OK` |
|     14 |  4455 | `			 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0` |
|      9 |  4456 | `			 && SyBlobLength(&pCtx->pRet->sBlob) > 0 ){` |
|      9 |  4457 | `				SyBlobAppend(pOut,SyBlobData(&pCtx->pRet->sBlob),SyBlobLength(&pCtx->pRet->sBlob));` |
|      9 |  4458 | `				nTail = nFull - (i-1);` |
|      9 |  4459 | `				break;` |
|      - |  4460 | `			}` |
|      6 |  4461 | `		}` |
|      9 |  4462 | `		if( nTail > 0 ){` |
|      9 |  4463 | `			SyBlobAppend(pOut,zFull + (nFull - nTail),nTail);` |
|      5 |  4464 | `		}else{` |
|    ! 0 |  4465 | `			SyBlobAppend(pOut,zFull,nFull);` |
|      - |  4466 | `		}` |
|      9 |  4467 | `		SyBlobRelease(&sPre);` |
|      9 |  4468 | `		SyBlobRelease(&sFull);` |
|      - |  4469 | `	}` |
|     19 |  4470 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     19 |  4471 | `	SyBlobNullAppend(pOut);` |
|     19 |  4472 | `}` |
|      - |  4473 | `/*` |
|      - |  4474 | ` * Point the receiver at a freshly parsed tree, as both load methods do: the` |
|      - |  4475 | ` * document object keeps its identity and everything under the OLD tree becomes` |
|      - |  4476 | ` * stale, so the per-document wrapper cache is dropped with it. Answers 0 when` |
|      - |  4477 | `` * there is no tree to install, which is each method's `false`; the previous`` |
|      - |  4478 | ` * tree is left alone in that case, as php leaves it.` |
|      - |  4479 | ` */` |
|   1604 |  4480 | `static int DomInstallParsed(ph7_context *pCtx,ph7_class_instance *pThis,xmlDocPtr pDoc)` |
|      5 |  4481 | `{` |
|   1609 |  4482 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1609 |  4483 | `	phl_xmldoc *pShell = pDoc ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;` |
|   1609 |  4484 | `	phl_domnode *pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|      - |  4485 | `	ph7_value *pNodes;` |
|   1609 |  4486 | `	if( pRes == 0 ){` |
|     15 |  4487 | `		if( pDoc && pShell == 0 ){` |
|    ! 0 |  4488 | `			xmlFreeDoc(pDoc);` |
|    ! 0 |  4489 | `		}` |
|     15 |  4490 | `		return 0;` |
|      - |  4491 | `	}` |
|   1595 |  4492 | `	DomSetRes(pVm,pThis,pRes);` |
|   1595 |  4493 | `	pNodes = PH7_NativeAttr(pThis,DOM_NODES);` |
|   1595 |  4494 | `	if( pNodes ){` |
|   1595 |  4495 | `		PH7_MemObjRelease(pNodes);` |
|   1595 |  4496 | `		PH7_MemObjToHashmap(pNodes);` |
|    795 |  4497 | `	}` |
|   1595 |  4498 | `	return 1;` |
|    807 |  4499 | `}` |
|      - |  4500 | `/* DOMDocument::loadXML(string $source, int $options = 0): bool -- the receiver` |
|      - |  4501 | ` * is REPOINTED at a new tree, so its identity cache is dropped with it. */` |
|   1574 |  4502 | `DOM_METHOD(vm_builtin_DOMDocument_loadXML)` |
|      5 |  4503 | `{` |
|   1579 |  4504 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1579 |  4505 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1579 |  4506 | `	int nLen = 0;` |
|   1579 |  4507 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";` |
|   1579 |  4508 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|      - |  4509 | `	phl_dom_errsave sErr;` |
|      - |  4510 | `	xmlDocPtr pDoc;` |
|      - |  4511 | `	sxu32 nMark;` |
|   1579 |  4512 | `	if( pThis == 0 ){` |
|    ! 0 |  4513 | `		return PH7_OK;` |
|      - |  4514 | `	}` |
|   1579 |  4515 | `	if( nLen < 1 ){` |
|      3 |  4516 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  4517 | `			"DOMDocument::loadXML(): Argument #1 ($source) must not be empty");` |
|      - |  4518 | `	}` |
|   1577 |  4519 | `	iOpts = DomParseOptions(pThis,iOpts);` |
|   1577 |  4520 | `	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);` |
|   1577 |  4521 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   1577 |  4522 | `	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);` |
|   1577 |  4523 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,"DOMDocument::loadXML",iOpts);` |
|   1577 |  4524 | `	DomRestoreWarnings(pVm,sErr);` |
|   1577 |  4525 | `	DomStampCwd(pCtx,pDoc);` |
|   1577 |  4526 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|   1577 |  4527 | `	return PH7_OK;` |
|    792 |  4528 | `}` |
|      - |  4529 | `/*` |
|      - |  4530 | ` * DOMDocument::load(string $filename, int $options = 0): bool` |
|      - |  4531 | ` *` |
|      - |  4532 | ` * The same parse as loadXML from a FILE, and php reads that file through its` |
|      - |  4533 | ` * own stream layer (which is what makes a wrapper and a userland stream valid` |
|      - |  4534 | ` * destinations there, and what this does too) while letting libxml word the` |
|      - |  4535 | ` * failure. What only a differential decides:` |
|      - |  4536 | ` *` |
|      - |  4537 | `` *   * a file that is not THERE is libxml's own `I/O warning : failed to load`` |
|      - |  4538 | `` *     external entity "<path>"` and nothing else, while one that exists and`` |
|      - |  4539 | ` *     cannot be opened ALSO gets php's stream warning in front of it;` |
|      - |  4540 | ``  *   * the document's URI is the RESOLVED absolute path -- so `load('a/../b.xml')` `` |
|      - |  4541 | ` *     answers the canonical name -- and it is a URI, not a path: a space in it` |
|      - |  4542 | `` *     comes back as `%20`;`` |
|      - |  4543 | ` *   * a failed load leaves the receiver's previous tree exactly where it was.` |
|      - |  4544 | ` */` |
|      - |  4545 | `/*` |
|      - |  4546 | ` * Read a file for one of the two file-loading methods: the bytes into *pBody,` |
|      - |  4547 | ` * the name libxml is to know it by into *pPath. Answers 0 when the file could` |
|      - |  4548 | ` * not be read at all, with php's diagnostics already raised -- the receiver is` |
|      - |  4549 | ` * left alone then, and the method answers false.` |
|      - |  4550 | ` */` |
|      - |  4551 | `static int DomReadFileAs(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|      - |  4552 | `	SyBlob *pBody,SyBlob *pPath,int bVerbatim);` |
|     18 |  4553 | `PH7_PRIVATE int PH7_DomReadFile(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|      - |  4554 | `	SyBlob *pBody,SyBlob *pPath)` |
|      1 |  4555 | `{` |
|     19 |  4556 | `	return DomReadFileAs(pCtx,zFile,nFile,zFn,pBody,pPath,0);` |
|      1 |  4557 | `}` |
|      - |  4558 | `/*` |
|      - |  4559 | ` * bVerbatim: libxml is to know the file by the path AS WRITTEN rather than by` |
|      - |  4560 | ` * its canonical absolute name -- loadHTMLFile's rule (see its call).` |
|      - |  4561 | ` */` |
|     24 |  4562 | `static int DomReadFileAs(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,` |
|      - |  4563 | `	SyBlob *pBody,SyBlob *pPath,int bVerbatim)` |
|      1 |  4564 | `{` |
|     25 |  4565 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  4566 | `	const ph7_io_stream *pStream;` |
|      - |  4567 | `	void *pHandle;` |
|     25 |  4568 | `	if( bVerbatim ){` |
|      7 |  4569 | `		SyBlobInit(pPath,&pVm->sAllocator);` |
|      7 |  4570 | `		SyBlobAppend(pPath,zFile,(sxu32)nFile);` |
|      7 |  4571 | `		SyBlobNullAppend(pPath);` |
|      4 |  4572 | `	}else{` |
|     19 |  4573 | `		DomAbsPath(pCtx,zFile,pPath);` |
|      - |  4574 | `	}` |
|     25 |  4575 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|      - |  4576 | `	/* php hands its document loaders the context libxml_set_streams_context()` |
|      - |  4577 | ``	 * left, which is what lets a `load('http://…')` carry a script's own headers`` |
|      - |  4578 | `	 * and user agent. The slot was stored and answered and READ BY NOTHING until` |
|      - |  4579 | `	 * there was an http:// wrapper to read it; a file:// open ignores it exactly` |
|      - |  4580 | `	 * as php's does. */` |
|     25 |  4581 | `	PH7_StreamCtxArm(pVm,PH7_StreamCtxFromValue(&pVm->sXmlStreamsCtx));` |
|     25 |  4582 | `	pHandle = (pStream && pStream->xRead) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|     24 |  4583 | `		PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx)) : 0;` |
|     25 |  4584 | `	if( pHandle == 0 ){` |
|      - |  4585 | `		/* php's stream layer says nothing about a file that is simply absent --` |
|      - |  4586 | `		 * only libxml does, in its own words and with no source location. A file` |
|      - |  4587 | `		 * that IS there and would not open (a mode, a lock) gets both. */` |
|      7 |  4588 | `		const ph7_vfs *pVfs = pVm->pEngine->pVfs;` |
|      - |  4589 | `		SyBlob sMsg;` |
|      - |  4590 | `		sxu32 nMark;` |
|      7 |  4591 | `		if( pVfs && pVfs->xFileExists && pVfs->xFileExists(zFile) == PH7_OK ){` |
|    ! 0 |  4592 | `			VfsThrowOpenWarning(pCtx,zFile);` |
|    ! 0 |  4593 | `		}` |
|      7 |  4594 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      7 |  4595 | `		SyBlobFormat(&sMsg,"failed to load external entity \"%s\"\n",` |
|      6 |  4596 | `			(const char *)SyBlobData(pPath));` |
|      7 |  4597 | `		SyBlobNullAppend(&sMsg);` |
|      - |  4598 | `		/* Both of libxml's channels, as php feeds them: the structured copy is` |
|      - |  4599 | ``		 * what `libxml_get_errors()`/`libxml_get_last_error()` answer (level`` |
|      - |  4600 | `		 * WARNING, no file, no line), and the generic one is the text that gets` |
|      - |  4601 | `		 * PRINTED -- with the severity spelled into it and at E_WARNING. */` |
|      7 |  4602 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     10 |  4603 | `		PH7_LibxmlQueueError(pVm,XML_ERR_WARNING,XML_IO_LOAD_ERROR,0,0,` |
|      6 |  4604 | `			(const char *)SyBlobData(&sMsg),0);` |
|      7 |  4605 | `		if( pVm->bLibxmlInternalErr ){` |
|      3 |  4606 | `			xmlSetStructuredErrorFunc(0,0);   /* nothing to drain: it stays queued */` |
|      2 |  4607 | `		}else{` |
|      - |  4608 | `			SyBlob sGen;` |
|      5 |  4609 | `			PH7_LibxmlDropErrors(pVm,nMark);` |
|      5 |  4610 | `			SyBlobInit(&sGen,&pVm->sAllocator);` |
|      5 |  4611 | `			SyBlobFormat(&sGen,"I/O warning : %s",(const char *)SyBlobData(&sMsg));` |
|      5 |  4612 | `			SyBlobNullAppend(&sGen);` |
|      5 |  4613 | `			PH7_LibxmlRaiseGeneric(pVm,zFn,(const char *)SyBlobData(&sGen));` |
|      5 |  4614 | `			SyBlobRelease(&sGen);` |
|      - |  4615 | `		}` |
|      7 |  4616 | `		SyBlobRelease(&sMsg);` |
|      7 |  4617 | `		SyBlobRelease(pPath);` |
|      7 |  4618 | `		return 0;` |
|      - |  4619 | `	}` |
|     19 |  4620 | `	SyBlobInit(pBody,&pVm->sAllocator);` |
|     19 |  4621 | `	PH7_StreamReadWholeFile(pHandle,pStream,pBody);` |
|     19 |  4622 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     19 |  4623 | `	return 1;` |
|     13 |  4624 | `}` |
|     22 |  4625 | `DOM_METHOD(vm_builtin_DOMDocument_load)` |
|      1 |  4626 | `{` |
|     23 |  4627 | `	ph7_vm *pVm = pCtx->pVm;` |
|     23 |  4628 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  4629 | `	const char *zFile;` |
|     23 |  4630 | `	int nFile = 0;` |
|     23 |  4631 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|      - |  4632 | `	phl_dom_errsave sErr;` |
|      - |  4633 | `	SyBlob sBody,sPath;` |
|      - |  4634 | `	xmlDocPtr pDoc;` |
|      - |  4635 | `	sxu32 nMark;` |
|     23 |  4636 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|     23 |  4637 | `	if( pThis == 0 ){` |
|    ! 0 |  4638 | `		return PH7_OK;` |
|      - |  4639 | `	}` |
|     23 |  4640 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|      3 |  4641 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  4642 | `			"DOMDocument::load(): Argument #1 ($filename) must not contain any null bytes");` |
|      - |  4643 | `	}` |
|     21 |  4644 | `	if( nFile < 1 ){` |
|      3 |  4645 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  4646 | `			"DOMDocument::load(): Argument #1 ($filename) must not be empty");` |
|      - |  4647 | `	}` |
|     19 |  4648 | `	if( !PH7_DomReadFile(pCtx,zFile,nFile,"DOMDocument::load",&sBody,&sPath) ){` |
|      5 |  4649 | `		ph7_result_bool(pCtx,0);` |
|      5 |  4650 | `		return PH7_OK;` |
|      - |  4651 | `	}` |
|     15 |  4652 | `	if( SyBlobLength(&sBody) < 1 ){` |
|      - |  4653 | `		/* libxml's memory parser will not even start on nothing, so the error` |
|      - |  4654 | `		 * php's FILE parser raises there is queued by hand -- same level, same` |
|      - |  4655 | ``		 * code, same wording, so `libxml_get_errors()` reports what php's does`` |
|      - |  4656 | `		 * and the drain prints php's sentence. */` |
|      3 |  4657 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      4 |  4658 | `		PH7_LibxmlQueueError(pVm,XML_ERR_FATAL,XML_ERR_DOCUMENT_EMPTY,1,1,` |
|      2 |  4659 | `			"Document is empty\n",(const char *)SyBlobData(&sPath));` |
|      3 |  4660 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::load");` |
|      3 |  4661 | `		SyBlobRelease(&sBody);` |
|      3 |  4662 | `		SyBlobRelease(&sPath);` |
|      3 |  4663 | `		ph7_result_bool(pCtx,0);` |
|      3 |  4664 | `		return PH7_OK;` |
|      - |  4665 | `	}` |
|     13 |  4666 | `	iOpts = DomParseOptions(pThis,iOpts);` |
|     13 |  4667 | `	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);` |
|     13 |  4668 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      - |  4669 | `	/* The path is the parse's URL: libxml turns it into the document's URI and` |
|      - |  4670 | `	 * names it in every diagnostic the parse raises. */` |
|     19 |  4671 | `	pDoc = xmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),` |
|     12 |  4672 | `		(const char *)SyBlobData(&sPath),0,iOpts);` |
|     13 |  4673 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,"DOMDocument::load",iOpts);` |
|     13 |  4674 | `	DomRestoreWarnings(pVm,sErr);` |
|     13 |  4675 | `	SyBlobRelease(&sBody);` |
|     13 |  4676 | `	SyBlobRelease(&sPath);` |
|     13 |  4677 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|     13 |  4678 | `	return PH7_OK;` |
|     12 |  4679 | `}` |
|      - |  4680 | `/*` |
|      - |  4681 | ` * DOMDocument::loadHTML(string $source, int $options = 0): bool` |
|      - |  4682 | ` * DOMDocument::loadHTMLFile(string $filename, int $options = 0): bool` |
|      - |  4683 | ` *` |
|      - |  4684 | ` * The other parser: HTML is not XML and libxml has a second one for it, which` |
|      - |  4685 | `` * closes what the markup left open, supplies the `html`/`body` php's`` |
|      - |  4686 | `` * `LIBXML_HTML_NOIMPLIED` asks it not to, and stamps the DTD`` |
|      - |  4687 | `` * `LIBXML_HTML_NODEFDTD` asks it not to. What comes out is an HTML DOCUMENT --`` |
|      - |  4688 | ` * node type 13, its own serializer -- and the differences from the XML side are` |
|      - |  4689 | ` * measured ones: the document's own directives reach NOTHING here (only` |
|      - |  4690 | `` * `$options` does), a document parsed from a STRING is given no URI at all`` |
|      - |  4691 | ` * (where loadXML stamps the working directory), and there is no well-formedness` |
|      - |  4692 | ` * to fail on, so the answer is true for anything that is not empty.` |
|      - |  4693 | ` */` |
|     26 |  4694 | `static int DomLoadHtml(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile)` |
|      1 |  4695 | `{` |
|     27 |  4696 | `	ph7_vm *pVm = pCtx->pVm;` |
|     27 |  4697 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     27 |  4698 | `	const char *zFn = bFile ? "DOMDocument::loadHTMLFile" : "DOMDocument::loadHTML";` |
|     27 |  4699 | `	const char *zArg = bFile ? "filename" : "source";` |
|      - |  4700 | `	const char *zSrc;` |
|     27 |  4701 | `	int nSrc = 0;` |
|     27 |  4702 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|      - |  4703 | `	SyBlob sBody,sPath;` |
|      - |  4704 | `	xmlDocPtr pDoc;` |
|      - |  4705 | `	sxu32 nMark;` |
|     27 |  4706 | `	zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|     27 |  4707 | `	if( pThis == 0 ){` |
|    ! 0 |  4708 | `		return PH7_OK;` |
|      - |  4709 | `	}` |
|     27 |  4710 | `	if( bFile && nSrc != (int)SyStrlen(zSrc) ){` |
|    ! 0 |  4711 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    ! 0 |  4712 | `			"%s(): Argument #1 ($filename) must not contain any null bytes",zFn);` |
|      - |  4713 | `	}` |
|     27 |  4714 | `	if( nSrc < 1 ){` |
|      7 |  4715 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      2 |  4716 | `			"%s(): Argument #1 ($%s) must not be empty",zFn,zArg);` |
|      - |  4717 | `	}` |
|     23 |  4718 | `	if( bFile ){` |
|      - |  4719 | `		/* loadHTMLFile hands libxml the path AS WRITTEN -- php's` |
|      - |  4720 | `		 * htmlCreateFileParserCtxt(source): no working directory joined and no` |
|      - |  4721 | `		 * link resolved, unlike load(), which canonicalizes first. It is the name` |
|      - |  4722 | `		 * the document's URI and every diagnostic carry. */` |
|      7 |  4723 | `		if( !DomReadFileAs(pCtx,zSrc,nSrc,zFn,&sBody,&sPath,1) ){` |
|      3 |  4724 | `			ph7_result_bool(pCtx,0);` |
|      3 |  4725 | `			return PH7_OK;` |
|      - |  4726 | `		}` |
|      3 |  4727 | `	}else{` |
|      - |  4728 | `		/* A string has no URI: php leaves the document's null. */` |
|     17 |  4729 | `		SyBlobInit(&sBody,&pVm->sAllocator);` |
|     17 |  4730 | `		SyBlobAppend(&sBody,zSrc,(sxu32)nSrc);` |
|     17 |  4731 | `		SyBlobInit(&sPath,&pVm->sAllocator);` |
|     17 |  4732 | `		SyBlobNullAppend(&sPath);` |
|      - |  4733 | `	}` |
|     21 |  4734 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     21 |  4735 | `	if( SyBlobLength(&sBody) > 0 ){` |
|     28 |  4736 | `		pDoc = htmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),` |
|      9 |  4737 | `			bFile ? (const char *)SyBlobData(&sPath) : 0,0,iOpts);` |
|     10 |  4738 | `	}else{` |
|      - |  4739 | `		/* An empty FILE is still a document here, unlike on the XML side: php's` |
|      - |  4740 | ``		 * HTML parser says `Document is empty` and hands back the DTD-only`` |
|      - |  4741 | ``		 * document `htmlNewDoc` builds. libxml's memory parser will not start on`` |
|      - |  4742 | `		 * nothing, so both halves are made by hand. (An empty STRING never gets` |
|      - |  4743 | `		 * this far -- it is the ValueError above.) */` |
|      4 |  4744 | `		PH7_LibxmlQueueError(pVm,XML_ERR_ERROR,XML_ERR_DOCUMENT_EMPTY,1,1,` |
|      2 |  4745 | `			"Document is empty\n",(const char *)SyBlobData(&sPath));` |
|      3 |  4746 | `		pDoc = htmlNewDoc(0,0);` |
|      3 |  4747 | `		if( pDoc ){` |
|      3 |  4748 | `			pDoc->URL = xmlStrdup((const xmlChar *)SyBlobData(&sPath));` |
|      1 |  4749 | `		}` |
|      - |  4750 | `	}` |
|     21 |  4751 | `	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFn,iOpts);` |
|     21 |  4752 | `	SyBlobRelease(&sBody);` |
|     21 |  4753 | `	SyBlobRelease(&sPath);` |
|     21 |  4754 | `	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));` |
|     21 |  4755 | `	return PH7_OK;` |
|     14 |  4756 | `}` |
|     18 |  4757 | `DOM_METHOD(vm_builtin_DOMDocument_loadHTML)` |
|      1 |  4758 | `{` |
|     19 |  4759 | `	return DomLoadHtml(pCtx,nArg,apArg,FALSE);` |
|      1 |  4760 | `}` |
|      8 |  4761 | `DOM_METHOD(vm_builtin_DOMDocument_loadHTMLFile)` |
|      1 |  4762 | `{` |
|      9 |  4763 | `	return DomLoadHtml(pCtx,nArg,apArg,TRUE);` |
|      1 |  4764 | `}` |
|      - |  4765 | `/*` |
|      - |  4766 | ` * The two save options php reads, and what they mean to libxml.` |
|      - |  4767 | ` *` |
|      - |  4768 | `` * `LIBXML_NOEMPTYTAG` turns `<e/>` into `<e></e>` and reaches BOTH dumps -- a`` |
|      - |  4769 | `` * node's as much as a document's -- while `LIBXML_NOXMLDECL` only reaches the`` |
|      - |  4770 | ` * whole-document one (a node's output has no declaration to drop). Every other` |
|      - |  4771 | `` * bit of `$options` is ignored, unknown ones included. php spells the first one`` |
|      - |  4772 | ` * with libxml's library-wide switch; this file asks for it per dump instead,` |
|      - |  4773 | ` * which says the same thing without touching global state (and without the` |
|      - |  4774 | ` * deprecated symbol: the MSVC gate refuses it under /WX).` |
|      - |  4775 | ` *` |
|      - |  4776 | ` * Both dumps therefore run through libxml's save API. The DOCUMENT's goes out` |
|      - |  4777 | ` * in the encoding its declaration names -- which is also how a document whose` |
|      - |  4778 | ` * encoding has no converter fails, with no context to write through -- and a` |
|      - |  4779 | ` * NODE's is always UTF-8, as php's is.` |
|      - |  4780 | ` */` |
|      - |  4781 | `#define DOM_SAVE_NOXMLDECL  2` |
|      - |  4782 | `#define DOM_SAVE_NOEMPTYTAG 4` |
|   1098 |  4783 | `static int DomSaveFlags(int bFormat,int iOpts,int bDoc)` |
|      2 |  4784 | `{` |
|      - |  4785 | ``	/* AS_XML because the receiver may be an HTML document (`loadHTML` makes`` |
|      - |  4786 | `	 * one): libxml's save context would hand such a document to the HTML` |
|      - |  4787 | `	 * serializer, and php's XML savers write XML whatever the document is --` |
|      - |  4788 | ``	 * declaration, `<br/>` and all. */`` |
|   1100 |  4789 | `	int iSave = XML_SAVE_AS_XML \| (bFormat ? XML_SAVE_FORMAT : 0);` |
|   1100 |  4790 | `	if( iOpts & DOM_SAVE_NOEMPTYTAG ){` |
|     13 |  4791 | `		iSave \|= XML_SAVE_NO_EMPTY;` |
|      6 |  4792 | `	}` |
|   1100 |  4793 | `	if( bDoc && (iOpts & DOM_SAVE_NOXMLDECL) ){` |
|      5 |  4794 | `		iSave \|= XML_SAVE_NO_DECL;` |
|      2 |  4795 | `	}` |
|   1100 |  4796 | `	return iSave;` |
|      2 |  4797 | `}` |
|      - |  4798 | `/*` |
|      - |  4799 | ` * Serialize a whole document (pNode == 0) or one node the way php's savers do.` |
|      - |  4800 | ` * Answers the bytes in *pzOut (xmlFree'd by the caller) and their count, or -1.` |
|      - |  4801 | ` */` |
|   1098 |  4802 | `static int DomDumpTree(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,int iOpts,xmlChar **pzOut)` |
|      2 |  4803 | `{` |
|   1100 |  4804 | `	xmlBufferPtr pBuf = xmlBufferCreate();` |
|      - |  4805 | `	xmlSaveCtxtPtr pSave;` |
|   1100 |  4806 | `	int nOut = 0;` |
|   1100 |  4807 | `	*pzOut = 0;` |
|   1100 |  4808 | `	if( pBuf == 0 ){` |
|    ! 0 |  4809 | `		return -1;` |
|      - |  4810 | `	}` |
|      - |  4811 | `	/* A NODE's dump is UTF-8 whatever the document declares, and naming that` |
|      - |  4812 | `	 * encoding is also what keeps libxml from ESCAPING every non-ASCII character` |
|      - |  4813 | ``	 * (its no-encoding path writes `&#xE9;`, which is right for a document that`` |
|      - |  4814 | `	 * declares nothing and wrong for a node). A DOCUMENT's goes out in its own` |
|      - |  4815 | `	 * declared encoding, or in that escaping form when it declares none -- which` |
|      - |  4816 | `	 * is what php answers there. */` |
|   1100 |  4817 | `	pSave = xmlSaveToBuffer(pBuf,pNode ? "UTF-8" : (const char *)pDoc->encoding,` |
|    549 |  4818 | `		DomSaveFlags(bFormat,iOpts,pNode == 0));` |
|   1100 |  4819 | `	if( pSave == 0 ){` |
|    ! 0 |  4820 | `		xmlBufferFree(pBuf);` |
|    ! 0 |  4821 | `		return -1;` |
|      - |  4822 | `	}` |
|   1100 |  4823 | `	if( (pNode ? xmlSaveTree(pSave,pNode) : xmlSaveDoc(pSave,pDoc)) < 0 ){` |
|    ! 0 |  4824 | `		nOut = -1;` |
|    ! 0 |  4825 | `	}` |
|   1100 |  4826 | `	if( xmlSaveClose(pSave) < 0 ){` |
|    ! 0 |  4827 | `		nOut = -1;` |
|    ! 0 |  4828 | `	}` |
|   1100 |  4829 | `	if( nOut == 0 ){` |
|   1100 |  4830 | `		nOut = (int)xmlBufferLength(pBuf);` |
|   1100 |  4831 | `		*pzOut = xmlStrndup(xmlBufferContent(pBuf),nOut);` |
|   1100 |  4832 | `		if( *pzOut == 0 ){` |
|    ! 0 |  4833 | `			nOut = -1;` |
|    ! 0 |  4834 | `		}` |
|    549 |  4835 | `	}` |
|   1100 |  4836 | `	xmlBufferFree(pBuf);` |
|   1100 |  4837 | `	return nOut;` |
|    551 |  4838 | `}` |
|      - |  4839 | `/* DOMDocument::saveXML(?DOMNode $node = null, int $options = 0): string\|false */` |
|   1088 |  4840 | `DOM_METHOD(vm_builtin_DOMDocument_saveXML)` |
|      2 |  4841 | `{` |
|   1090 |  4842 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1090 |  4843 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1090 |  4844 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|   1090 |  4845 | `	phl_domnode *pTgt = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|   1090 |  4846 | `	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);` |
|   1090 |  4847 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|      - |  4848 | `	int bWhole;` |
|   1090 |  4849 | `	xmlChar *zOut = 0;` |
|      - |  4850 | `	int nOut;` |
|      - |  4851 | `	sxu32 nMark;` |
|   1090 |  4852 | `	if( pDocNd == 0 ){` |
|    ! 0 |  4853 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  4854 | `		return PH7_OK;` |
|      - |  4855 | `	}` |
|   1088 |  4856 | `	if( pTgt && pTgt->pNode` |
|    686 |  4857 | `	 && ((xmlNodePtr)pTgt->pNode)->doc != (xmlDocPtr)pDocNd->pNode ){` |
|      - |  4858 | `		/* Another document's node -- or a constructed one that belongs to none` |
|      - |  4859 | `		 * yet -- is not this document's to serialize. */` |
|      5 |  4860 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  4861 | `	}` |
|   1086 |  4862 | `	bWhole = pTgt == 0 \|\| pTgt->pNode == pDocNd->pNode;` |
|   1086 |  4863 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   1086 |  4864 | `	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,bWhole ? 0 : (xmlNodePtr)pTgt->pNode,` |
|    542 |  4865 | `		bFormat,iOpts,&zOut);` |
|   1086 |  4866 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|   1086 |  4867 | `	if( nOut < 0 ){` |
|      - |  4868 | `		/* php says so rather than answering an empty document: the encoding the` |
|      - |  4869 | `		 * declaration names has no converter and nothing was written. */` |
|    ! 0 |  4870 | `		if( bWhole ){` |
|    ! 0 |  4871 | `			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Could not save document");` |
|    ! 0 |  4872 | `		}` |
|    ! 0 |  4873 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  4874 | `		return PH7_OK;` |
|      - |  4875 | `	}` |
|   1086 |  4876 | `	ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|   1086 |  4877 | `	xmlFree(zOut);` |
|   1086 |  4878 | `	return PH7_OK;` |
|    546 |  4879 | `}` |
|      - |  4880 | `/*` |
|      - |  4881 | ` * DOMDocument::saveHTML(?DOMNode $node = null): string\|false` |
|      - |  4882 | ` * DOMDocument::saveHTMLFile(string $filename): int\|false` |
|      - |  4883 | ` *` |
|      - |  4884 | ` * The HTML serializer, which is a different one: a void element comes out` |
|      - |  4885 | `` * `<br>` rather than `<br/>`, a character with an HTML entity name comes out`` |
|      - |  4886 | ` * under that name, and the whole document carries its DOCTYPE and no XML` |
|      - |  4887 | ` * declaration. Neither method takes save OPTIONS -- php declares one parameter` |
|      - |  4888 | `` * each -- but both read `formatOutput`, a NODE's dump included.`` |
|      - |  4889 | ` *` |
|      - |  4890 | ` * A node from ANOTHER document is php's Wrong Document Error (in whichever mode` |
|      - |  4891 | ` * this document is in), which is the only refusal either one has.` |
|      - |  4892 | ` */` |
|     26 |  4893 | `static int DomDumpHtml(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,xmlChar **pzOut)` |
|      1 |  4894 | `{` |
|      - |  4895 | `	xmlBufferPtr pBuf;` |
|      - |  4896 | `	xmlOutputBufferPtr pOut;` |
|      - |  4897 | `	int nOut;` |
|     27 |  4898 | `	*pzOut = 0;` |
|     27 |  4899 | `	if( pNode == 0 ){` |
|     23 |  4900 | `		nOut = 0;` |
|     23 |  4901 | `		htmlDocDumpMemoryFormat(pDoc,pzOut,&nOut,bFormat ? 1 : 0);` |
|     23 |  4902 | `		return *pzOut ? nOut : -1;` |
|      - |  4903 | `	}` |
|      5 |  4904 | `	pBuf = xmlBufferCreate();` |
|      - |  4905 | `	/* The buffer is the write TARGET, not the output buffer's own storage:` |
|      - |  4906 | `	 * closing the latter leaves it to us to free. */` |
|      5 |  4907 | `	pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|      5 |  4908 | `	if( pOut == 0 ){` |
|    ! 0 |  4909 | `		if( pBuf ){` |
|    ! 0 |  4910 | `			xmlBufferFree(pBuf);` |
|    ! 0 |  4911 | `		}` |
|    ! 0 |  4912 | `		return -1;` |
|      - |  4913 | `	}` |
|      5 |  4914 | `	htmlNodeDumpFormatOutput(pOut,pDoc,pNode,0,bFormat ? 1 : 0);` |
|      5 |  4915 | `	xmlOutputBufferFlush(pOut);` |
|      5 |  4916 | `	nOut = (int)xmlBufferLength(pBuf);` |
|      5 |  4917 | `	*pzOut = xmlStrndup(xmlBufferContent(pBuf),nOut);` |
|      5 |  4918 | `	xmlOutputBufferClose(pOut);` |
|      5 |  4919 | `	xmlBufferFree(pBuf);` |
|      5 |  4920 | `	return *pzOut ? nOut : -1;` |
|     14 |  4921 | `}` |
|     32 |  4922 | `static int DomSaveHtml(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile)` |
|      1 |  4923 | `{` |
|     33 |  4924 | `	ph7_vm *pVm = pCtx->pVm;` |
|     33 |  4925 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     33 |  4926 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     33 |  4927 | `	phl_domnode *pTgt = (!bFile && nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|     33 |  4928 | `	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);` |
|     33 |  4929 | `	const ph7_io_stream *pStream = 0;` |
|     33 |  4930 | `	const char *zFile = "";` |
|     33 |  4931 | `	int nFile = 0,nOut;` |
|     33 |  4932 | `	xmlChar *zOut = 0;` |
|      - |  4933 | `	void *pHandle;` |
|      - |  4934 | `	sxu32 nMark;` |
|     33 |  4935 | `	if( bFile ){` |
|      7 |  4936 | `		zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|      7 |  4937 | `		if( nFile != (int)SyStrlen(zFile) ){` |
|    ! 0 |  4938 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  4939 | `				"DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not contain any null bytes");` |
|      - |  4940 | `		}` |
|      7 |  4941 | `		if( nFile < 1 ){` |
|      3 |  4942 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  4943 | `				"DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not be empty");` |
|      - |  4944 | `		}` |
|      2 |  4945 | `	}` |
|     31 |  4946 | `	if( pDocNd == 0 ){` |
|    ! 0 |  4947 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  4948 | `		return PH7_OK;` |
|      - |  4949 | `	}` |
|     31 |  4950 | `	if( pTgt && pTgt->pShell != pDocNd->pShell ){` |
|      5 |  4951 | `		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  4952 | `	}` |
|     27 |  4953 | `	if( bFile ){` |
|      - |  4954 | `		/* Writing to a FILE goes through libxml's file saver, which stamps the` |
|      - |  4955 | `` 		 * document with the encoding it is about to use: an `http-equiv` `` |
|      - |  4956 | ``		 * Content-Type meta appears in `<head>` -- in the DOCUMENT, not just in`` |
|      - |  4957 | ``		 * the output, so the next `saveHTML()` shows it too -- and it always`` |
|      - |  4958 | `		 * says UTF-8, whatever the document's own encoding is. php inherits` |
|      - |  4959 | `		 * that; the string saver READS the same meta and adds none. */` |
|      5 |  4960 | `		htmlSetMetaEncoding((xmlDocPtr)pDocNd->pNode,(const xmlChar *)"UTF-8");` |
|      2 |  4961 | `	}` |
|     27 |  4962 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     42 |  4963 | `	nOut = DomDumpHtml((xmlDocPtr)pDocNd->pNode,` |
|     15 |  4964 | `		(pTgt && pTgt->pNode != pDocNd->pNode) ? (xmlNodePtr)pTgt->pNode : 0,bFormat,&zOut);` |
|     27 |  4965 | `	PH7_LibxmlDropErrors(pVm,nMark);` |
|     27 |  4966 | `	if( nOut < 0 ){` |
|    ! 0 |  4967 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  4968 | `		return PH7_OK;` |
|      - |  4969 | `	}` |
|     27 |  4970 | `	if( !bFile ){` |
|     23 |  4971 | `		ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|     23 |  4972 | `		xmlFree(zOut);` |
|     23 |  4973 | `		return PH7_OK;` |
|      - |  4974 | `	}` |
|      5 |  4975 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|      5 |  4976 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|      - |  4977 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|      4 |  4978 | `		ph7_function_name(pCtx)) : 0;` |
|      5 |  4979 | `	if( pHandle == 0 ){` |
|      3 |  4980 | `		xmlFree(zOut);` |
|      3 |  4981 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      - |  4982 | `		/* php answers the bytes it WROTE, which is none of them -- not false. */` |
|      3 |  4983 | `		ph7_result_int(pCtx,0);` |
|      3 |  4984 | `		return PH7_OK;` |
|      - |  4985 | `	}` |
|      3 |  4986 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|    ! 0 |  4987 | `		nOut = 0;` |
|    ! 0 |  4988 | `	}` |
|      3 |  4989 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|      3 |  4990 | `	xmlFree(zOut);` |
|      3 |  4991 | `	ph7_result_int(pCtx,nOut);` |
|      3 |  4992 | `	return PH7_OK;` |
|     17 |  4993 | `}` |
|     26 |  4994 | `DOM_METHOD(vm_builtin_DOMDocument_saveHTML)` |
|      1 |  4995 | `{` |
|     27 |  4996 | `	return DomSaveHtml(pCtx,nArg,apArg,FALSE);` |
|      1 |  4997 | `}` |
|      6 |  4998 | `DOM_METHOD(vm_builtin_DOMDocument_saveHTMLFile)` |
|      1 |  4999 | `{` |
|      7 |  5000 | `	return DomSaveHtml(pCtx,nArg,apArg,TRUE);` |
|      1 |  5001 | `}` |
|      - |  5002 | `/*` |
|      - |  5003 | ` * DOMDocument::save(string $filename, int $options = 0): int\|false` |
|      - |  5004 | ` *` |
|      - |  5005 | ` * saveXML's bytes written to a file, and the COUNT of them rather than the` |
|      - |  5006 | ` * bytes -- through the stream layer, which is where php's` |
|      - |  5007 | `` * `save(<path>): Failed to open stream: <reason>` comes from. Two rules only a`` |
|      - |  5008 | `` * differential decides: `LIBXML_NOXMLDECL` does NOT reach this one (php reads`` |
|      - |  5009 | ` * it in saveXML only, so a saved document always carries its declaration),` |
|      - |  5010 | ``  * and a document whose declared encoding has no converter is a silent `false` `` |
|      - |  5011 | ` * here where saveXML says "Could not save document".` |
|      - |  5012 | ` */` |
|     18 |  5013 | `DOM_METHOD(vm_builtin_DOMDocument_save)` |
|      1 |  5014 | `{` |
|     19 |  5015 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 |  5016 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     19 |  5017 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|      - |  5018 | `	const ph7_io_stream *pStream;` |
|      - |  5019 | `	const char *zFile;` |
|     19 |  5020 | `	int nFile = 0;` |
|     19 |  5021 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     19 |  5022 | `	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);` |
|      - |  5023 | `	int nOut;` |
|     19 |  5024 | `	xmlChar *zOut = 0;` |
|      - |  5025 | `	void *pHandle;` |
|      - |  5026 | `	sxu32 nMark;` |
|     19 |  5027 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|     19 |  5028 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|      3 |  5029 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  5030 | `			"DOMDocument::save(): Argument #1 ($filename) must not contain any null bytes");` |
|      - |  5031 | `	}` |
|     17 |  5032 | `	if( nFile < 1 ){` |
|      3 |  5033 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  5034 | `			"DOMDocument::save(): Argument #1 ($filename) must not be empty");` |
|      - |  5035 | `	}` |
|     15 |  5036 | `	if( pDocNd == 0 ){` |
|    ! 0 |  5037 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5038 | `		return PH7_OK;` |
|      - |  5039 | `	}` |
|     15 |  5040 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     15 |  5041 | `	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,0,bFormat,iOpts & ~DOM_SAVE_NOXMLDECL,&zOut);` |
|      - |  5042 | `	/* php reports this failure through the return value alone. */` |
|     15 |  5043 | `	PH7_LibxmlDropErrors(pVm,nMark);` |
|     15 |  5044 | `	if( nOut < 0 ){` |
|    ! 0 |  5045 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5046 | `		return PH7_OK;` |
|      - |  5047 | `	}` |
|     15 |  5048 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|     15 |  5049 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|      - |  5050 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|     14 |  5051 | `		ph7_function_name(pCtx)) : 0;` |
|     15 |  5052 | `	if( pHandle == 0 ){` |
|      3 |  5053 | `		xmlFree(zOut);` |
|      3 |  5054 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      3 |  5055 | `		ph7_result_bool(pCtx,0);` |
|      3 |  5056 | `		return PH7_OK;` |
|      - |  5057 | `	}` |
|     13 |  5058 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|    ! 0 |  5059 | `		nOut = -1;` |
|    ! 0 |  5060 | `	}` |
|     13 |  5061 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     13 |  5062 | `	xmlFree(zOut);` |
|     13 |  5063 | `	if( nOut < 0 ){` |
|    ! 0 |  5064 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5065 | `		return PH7_OK;` |
|      - |  5066 | `	}` |
|     13 |  5067 | `	ph7_result_int(pCtx,nOut);` |
|     13 |  5068 | `	return PH7_OK;` |
|     10 |  5069 | `}` |
|      - |  5070 | `/*` |
|      - |  5071 | ` * The four DOMDocument::create* methods, which differ only in the node kind` |
|      - |  5072 | ` * they ask libxml for. Fresh nodes start as orphans, so a node that is created` |
|      - |  5073 | ` * and never appended is still freed with its document.` |
|      - |  5074 | ` */` |
|    430 |  5075 | `static int DomDocCreate(ph7_context *pCtx,int iKind,const char *zName,const char *zVal,int nVal)` |
|      3 |  5076 | `{` |
|    433 |  5077 | `	ph7_vm *pVm = pCtx->pVm;` |
|    433 |  5078 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|      - |  5079 | `	xmlDocPtr pDoc;` |
|    433 |  5080 | `	xmlNodePtr pNode = 0;` |
|      - |  5081 | `	sxu32 nMark;` |
|    433 |  5082 | `	if( pDocNd == 0 ){` |
|    ! 0 |  5083 | `		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  5084 | `	}` |
|    433 |  5085 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|    433 |  5086 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    433 |  5087 | `	switch( iKind ){` |
|     86 |  5088 | `	case XML_ELEMENT_NODE:` |
|    175 |  5089 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      7 |  5090 | `			break; /* Invalid Character Error */` |
|      - |  5091 | `		}` |
|      - |  5092 | `		/* php passes the value through xmlNewDocNode, which entity-parses` |
|      - |  5093 | `		 * it (quirk preserved: bad entities warn and drop the content). */` |
|    169 |  5094 | `		pNode = xmlNewDocNode(pDoc,0,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|    169 |  5095 | `		break;` |
|     56 |  5096 | `	case XML_TEXT_NODE:` |
|    114 |  5097 | `		pNode = xmlNewDocText(pDoc,(const xmlChar *)zVal);` |
|    114 |  5098 | `		break;` |
|      4 |  5099 | `	case XML_CDATA_SECTION_NODE:` |
|      9 |  5100 | `		pNode = xmlNewCDataBlock(pDoc,(const xmlChar *)zVal,nVal);` |
|      9 |  5101 | `		break;` |
|      5 |  5102 | `	case XML_COMMENT_NODE:` |
|     11 |  5103 | `		pNode = xmlNewDocComment(pDoc,(const xmlChar *)zVal);` |
|     11 |  5104 | `		break;` |
|     10 |  5105 | `	case XML_PI_NODE:` |
|      - |  5106 | `		/* php validates the TARGET the same way it validates an element name,` |
|      - |  5107 | ``		 * so `createProcessingInstruction('a b')` is Invalid Character Error`` |
|      - |  5108 | `		 * rather than a document that will not parse back. */` |
|     21 |  5109 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|     11 |  5110 | `			break;` |
|      - |  5111 | `		}` |
|      - |  5112 | `		/* Empty data stays a NULL content pointer, matching php's node state:` |
|      - |  5113 | ``		 * `<?bare?>` serializes with no separator space, `nodeValue` reads`` |
|      - |  5114 | ``		 * null -- and `data` reads "", because THAT getter coerces. */`` |
|     11 |  5115 | `		pNode = xmlNewDocPI(pDoc,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|     11 |  5116 | `		break;` |
|     12 |  5117 | `	case XML_ENTITY_REF_NODE:` |
|     25 |  5118 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|      9 |  5119 | `			break;` |
|      - |  5120 | `		}` |
|     17 |  5121 | `		pNode = xmlNewReference(pDoc,(const xmlChar *)zName);` |
|     17 |  5122 | `		break;` |
|     42 |  5123 | `	case XML_DOCUMENT_FRAG_NODE:` |
|     85 |  5124 | `		pNode = xmlNewDocFragment(pDoc);` |
|     84 |  5125 | `		break;` |
|      - |  5126 | `	}` |
|    648 |  5127 | `	PH7_LibxmlCaptureEnd(pVm,nMark,` |
|    215 |  5128 | `		iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");` |
|    433 |  5129 | `	if( pNode == 0 ){` |
|     25 |  5130 | `		if( iKind == XML_ELEMENT_NODE \|\| iKind == XML_PI_NODE \|\| iKind == XML_ENTITY_REF_NODE ){` |
|      - |  5131 | `			/* The three factories that take a NAME are the three that can be` |
|      - |  5132 | `			 * handed one libxml refuses. */` |
|     25 |  5133 | `			return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);` |
|      - |  5134 | `		}` |
|    ! 0 |  5135 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5136 | `		return PH7_OK;` |
|      - |  5137 | `	}` |
|    409 |  5138 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|    409 |  5139 | `	return DomResultNodeOf(pCtx,pDocNd,pNode);` |
|    218 |  5140 | `}` |
|      - |  5141 | `/* DOMDocument::createElement(string $localName, string $value = ''): DOMElement */` |
|    172 |  5142 | `DOM_METHOD(vm_builtin_DOMDocument_createElement)` |
|      3 |  5143 | `{` |
|    175 |  5144 | `	int nVal = 0;` |
|    175 |  5145 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    175 |  5146 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";` |
|    175 |  5147 | `	return DomDocCreate(pCtx,XML_ELEMENT_NODE,zName,zVal,nVal);` |
|      3 |  5148 | `}` |
|      - |  5149 | `/*` |
|      - |  5150 | ` * DOMDocument::createElementNS(?string $namespace, string $qualifiedName,` |
|      - |  5151 | ` *                              string $value = '')` |
|      - |  5152 | ` *` |
|      - |  5153 | ` * The only way to build a namespaced ELEMENT -- until this existed a program` |
|      - |  5154 | `` * could read a namespaced document and not write one, and `Call to undefined`` |
|      - |  5155 | `` * method` was the answer to the first line of every modern DOM example.`` |
|      - |  5156 | ` *` |
|      - |  5157 | ` * php's rules, measured:` |
|      - |  5158 | ` *` |
|      - |  5159 | ` *   * A NULL namespace is a plain element; an EMPTY-STRING one is not the same` |
|      - |  5160 | `` *     thing, it declares `xmlns=""` on the element and answers `''` for`` |
|      - |  5161 | ` *     namespaceURI. Either with a PREFIXED name is a Namespace Error, since a` |
|      - |  5162 | ` *     prefix names a namespace.` |
|      - |  5163 | ` *   * The declaration lands on the NEW element, always: a fresh node has no` |
|      - |  5164 | ` *     parent, so nothing the document declares elsewhere is in scope yet. What` |
|      - |  5165 | ` *     the document already makes is settled later, when the element is linked` |
|      - |  5166 | ` *     in and the redundant declaration is stripped (DomNsOnInsertEx).` |
|      - |  5167 | ` *   * The $value is not text -- php hands it to libxml, which entity-parses it,` |
|      - |  5168 | `` *     so `&amp;` becomes `&`, an undefined entity is a warning and `<` is`` |
|      - |  5169 | ` *     escaped. The same quirk createElement already carries.` |
|      - |  5170 | ` */` |
|    162 |  5171 | `DOM_METHOD(vm_builtin_DOMDocument_createElementNS)` |
|      1 |  5172 | `{` |
|    163 |  5173 | `	ph7_vm *pVm = pCtx->pVm;` |
|    163 |  5174 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|    163 |  5175 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|    163 |  5176 | `	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    163 |  5177 | `	int nVal = 0;` |
|    163 |  5178 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],&nVal) : "";` |
|      - |  5179 | `	xmlNodePtr pNode;` |
|      - |  5180 | `	dom_qname sQ;` |
|      - |  5181 | `	sxu32 nMark;` |
|      - |  5182 | `	int rc;` |
|    163 |  5183 | `	if( pDocNd == 0 ){` |
|    ! 0 |  5184 | `		return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  5185 | `	}` |
|    163 |  5186 | `	rc = DomQNameParse(zQname,zUri,DOM_QN_ELEM,&sQ);` |
|    163 |  5187 | `	if( rc ){` |
|     59 |  5188 | `		return DomThrow(pCtx,rc);` |
|      - |  5189 | `	}` |
|    105 |  5190 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    157 |  5191 | `	pNode = xmlNewDocNode((xmlDocPtr)pDocNd->pNode,0,sQ.zLocal,` |
|    104 |  5192 | `		nVal ? (const xmlChar *)zVal : 0);` |
|    105 |  5193 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::createElementNS");` |
|    105 |  5194 | `	if( pNode == 0 ){` |
|    ! 0 |  5195 | `		DomQNameRelease(&sQ);` |
|    ! 0 |  5196 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5197 | `	}` |
|    105 |  5198 | `	if( zUri != 0 ){` |
|     99 |  5199 | `		xmlNsPtr pNs = DomNsForCreate(pNode,zUri,sQ.zPrefix);` |
|     99 |  5200 | `		if( pNs == 0 ){` |
|     15 |  5201 | `			DomQNameRelease(&sQ);` |
|     15 |  5202 | `			xmlFreeNode(pNode);   /* never handed out, never an orphan */` |
|     15 |  5203 | `			return DomThrow(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  5204 | `		}` |
|     85 |  5205 | `		xmlSetNs(pNode,pNs);` |
|     42 |  5206 | `	}` |
|     91 |  5207 | `	DomQNameRelease(&sQ);` |
|     91 |  5208 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|     91 |  5209 | `	return DomResultNodeOf(pCtx,pDocNd,pNode);` |
|     82 |  5210 | `}` |
|      - |  5211 | `/*` |
|      - |  5212 | ` * DOMDocument::importNode(DOMNode $node, bool $deep = false): DOMNode\|false` |
|      - |  5213 | ` *` |
|      - |  5214 | ` * A node of ANOTHER document copied into this one, which is the only way to` |
|      - |  5215 | ` * carry a subtree across: every mutator refuses a node whose document is not` |
|      - |  5216 | ` * the parent's with php's Wrong Document Error, so without this a program that` |
|      - |  5217 | ` * read two files could not build a third out of them.` |
|      - |  5218 | ` *` |
|      - |  5219 | ` * php's rules, measured:` |
|      - |  5220 | ` *` |
|      - |  5221 | ` *   * A node ALREADY of this document is answered unchanged -- the same object,` |
|      - |  5222 | ` *     not a copy, and not detached from wherever it is.` |
|      - |  5223 | `` *   * A DOCUMENT is refused with a warning and `false`, not an exception.`` |
|      - |  5224 | ` *   * Shallow does not mean bare: an element brings its attributes and its` |
|      - |  5225 | ` *     namespace declarations, only its children stay behind. A fragment brings` |
|      - |  5226 | ` *     nothing but itself, and an attribute brings its value whatever $deep says.` |
|      - |  5227 | ` *   * The copy is an ORPHAN of this document (no parent, and freed with it), and` |
|      - |  5228 | ` *     a second import of the same node is a second copy.` |
|      - |  5229 | ` *   * A namespaced ATTRIBUTE is the one kind libxml cannot finish: its copy` |
|      - |  5230 | ` *     arrives with no namespace at all, and php re-points it at a PREFIXED` |
|      - |  5231 | ` *     binding of the same URI on the target's ROOT -- reusing one the root` |
|      - |  5232 | `` *     already has (so the prefix can change, `p:b` arriving as `z:b`) and`` |
|      - |  5233 | ` *     declaring it there otherwise.` |
|      - |  5234 | ` */` |
|     48 |  5235 | `DOM_METHOD(vm_builtin_DOMDocument_importNode)` |
|      1 |  5236 | `{` |
|     49 |  5237 | `	ph7_vm *pVm = pCtx->pVm;` |
|     49 |  5238 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     49 |  5239 | `	phl_domnode *pSrc = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     49 |  5240 | `	int bDeep = nArg > 1 && ph7_value_to_bool(apArg[1]);` |
|      - |  5241 | `	xmlDocPtr pDoc;` |
|      - |  5242 | `	xmlNodePtr pNode,pCopy;` |
|      - |  5243 | `	sxu32 nMark;` |
|     49 |  5244 | `	if( pDocNd == 0 \|\| pSrc == 0 ){` |
|    ! 0 |  5245 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5246 | `		return PH7_OK;` |
|      - |  5247 | `	}` |
|     49 |  5248 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|     49 |  5249 | `	pNode = (xmlNodePtr)pSrc->pNode;` |
|     49 |  5250 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|      - |  5251 | ``		/* The context prints php's `DOMDocument::importNode(): ` itself. */`` |
|      3 |  5252 | `		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Cannot import: Node Type Not Supported");` |
|      3 |  5253 | `		ph7_result_bool(pCtx,0);` |
|      3 |  5254 | `		return PH7_OK;` |
|      - |  5255 | `	}` |
|     47 |  5256 | `	if( pNode->doc == pDoc ){` |
|      5 |  5257 | `		ph7_result_value(pCtx,apArg[0]);` |
|      5 |  5258 | `		return PH7_OK;` |
|      - |  5259 | `	}` |
|     43 |  5260 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      - |  5261 | ``	/* 2 is libxml's `node + namespaces + attributes, no children`, which is what`` |
|      - |  5262 | `	 * makes a shallow import carry the attributes; cloneNode asks the same way. */` |
|     43 |  5263 | `	pCopy = xmlDocCopyNode(pNode,pDoc,bDeep ? 1 : 2);` |
|     43 |  5264 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::importNode");` |
|     43 |  5265 | `	if( pCopy == 0 ){` |
|    ! 0 |  5266 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5267 | `		return PH7_OK;` |
|      - |  5268 | `	}` |
|     43 |  5269 | `	if( pCopy->type == XML_ATTRIBUTE_NODE && pNode->ns != 0 && pNode->ns->href != 0 ){` |
|     13 |  5270 | `		xmlNodePtr pRoot = xmlDocGetRootElement(pDoc);` |
|     13 |  5271 | `		xmlNsPtr pNs = pRoot` |
|     12 |  5272 | `			? DomNsResolve(pRoot,(const char *)pNode->ns->href,pNode->ns->prefix,1) : 0;` |
|     13 |  5273 | `		if( pNs == 0 ){` |
|      - |  5274 | `			/* No root element to declare on. php answers an attribute that IS in` |
|      - |  5275 | `			 * the namespace anyway, through a declaration no element makes; the` |
|      - |  5276 | `			 * document owns it so that it is freed with it. */` |
|    ! 0 |  5277 | `			pNs = xmlNewNs(0,pNode->ns->href,pNode->ns->prefix);` |
|    ! 0 |  5278 | `			if( pNs ){` |
|    ! 0 |  5279 | `				DomNsPark(pCopy,pNs);` |
|    ! 0 |  5280 | `			}` |
|    ! 0 |  5281 | `		}` |
|     13 |  5282 | `		xmlSetNs(pCopy,pNs);` |
|      6 |  5283 | `	}` |
|     43 |  5284 | `	DomOrphanAdd(pDocNd->pShell,pCopy);` |
|     43 |  5285 | `	return DomResultNodeOf(pCtx,pDocNd,pCopy);` |
|     25 |  5286 | `}` |
|      - |  5287 | `/*` |
|      - |  5288 | `` * Re-home one node's WRAPPER. `adoptNode` moves the node itself between`` |
|      - |  5289 | ` * documents and answers the SAME object, which has to keep working: its $__doc` |
|      - |  5290 | `` * slot is what `ownerDocument` reads, its handle's shell is what will free the`` |
|      - |  5291 | ` * node, and its place in a document's identity cache is what makes` |
|      - |  5292 | `` * `$doc->documentElement === $doc->documentElement` true. All three move.`` |
|      - |  5293 | ` *` |
|      - |  5294 | ` * The target cache takes its reference BEFORE the source lets go, so the object` |
|      - |  5295 | ` * cannot be freed in between.` |
|      - |  5296 | ` */` |
|    174 |  5297 | `static void DomAdoptWrapper(ph7_vm *pVm,ph7_hashmap *pFrom,ph7_hashmap *pTo,` |
|      - |  5298 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode)` |
|      1 |  5299 | `{` |
|    175 |  5300 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  5301 | `	ph7_class_instance *pObj;` |
|      - |  5302 | `	ph7_value sKey,*pHit;` |
|    175 |  5303 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|    175 |  5304 | `	if( PH7_HashmapLookup(pFrom,&sKey,&pEntry) != SXRET_OK \|\| pEntry == 0 ){` |
|     79 |  5305 | `		PH7_MemObjRelease(&sKey);` |
|     79 |  5306 | `		return;   /* PHP never asked for this node: nothing to move */` |
|      - |  5307 | `	}` |
|     97 |  5308 | `	pHit = HashmapExtractNodeValue(pEntry);` |
|     97 |  5309 | `	pObj = (pHit && (pHit->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pHit->x.pOther : 0;` |
|     97 |  5310 | `	if( pObj ){` |
|     97 |  5311 | `		phl_domnode *pRes = DomResOf(pObj);` |
|      - |  5312 | `		ph7_value sVal;` |
|     97 |  5313 | `		PH7_MemObjInit(&(*pVm),&sVal);` |
|     97 |  5314 | `		sVal.x.pOther = pObj;` |
|     97 |  5315 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     97 |  5316 | `		PH7_HashmapInsert(pTo,&sKey,&sVal);` |
|     97 |  5317 | `		if( pRes ){` |
|     97 |  5318 | `			pRes->pShell = pDstShell;` |
|     48 |  5319 | `		}` |
|     97 |  5320 | `		PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDstDoc);` |
|     48 |  5321 | `	}` |
|     97 |  5322 | `	PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|     97 |  5323 | `	PH7_MemObjRelease(&sKey);` |
|     89 |  5324 | `}` |
|      - |  5325 | `/* ...for every node of the adopted subtree, attributes and their text included:` |
|      - |  5326 | `` * php's adoption reaches all of them, which `$kid->ownerDocument` shows. */`` |
|     86 |  5327 | `static void DomAdoptWrappers(ph7_vm *pVm,ph7_class_instance *pSrcDoc,` |
|      - |  5328 | `	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode)` |
|      1 |  5329 | `{` |
|     87 |  5330 | `	ph7_hashmap *pFrom = DomCache(&(*pVm),pSrcDoc);` |
|     87 |  5331 | `	ph7_hashmap *pTo = DomCache(&(*pVm),pDstDoc);` |
|     87 |  5332 | `	xmlNodePtr pCur = pNode;` |
|     87 |  5333 | `	if( pFrom == 0 \|\| pTo == 0 \|\| pFrom == pTo ){` |
|    ! 0 |  5334 | `		return;` |
|      - |  5335 | `	}` |
|    237 |  5336 | `	while( pCur ){` |
|    151 |  5337 | `		DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,pCur);` |
|    151 |  5338 | `		if( pCur->type == XML_ELEMENT_NODE ){` |
|      - |  5339 | `			xmlAttrPtr pAttr;` |
|     60 |  5340 | `			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){` |
|      - |  5341 | `				xmlNodePtr pKid;` |
|     13 |  5342 | `				DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,(xmlNodePtr)pAttr);` |
|     25 |  5343 | `				for( pKid = pAttr->children ; pKid ; pKid = pKid->next ){` |
|     13 |  5344 | `					DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,pKid);` |
|      7 |  5345 | `				}` |
|      7 |  5346 | `			}` |
|     24 |  5347 | `		}` |
|    151 |  5348 | `		pCur = DomWalkNext(pCur,pNode);` |
|      1 |  5349 | `	}` |
|     44 |  5350 | `}` |
|      - |  5351 | `/*` |
|      - |  5352 | ` * DOMDocument::adoptNode(DOMNode $node): DOMNode\|false` |
|      - |  5353 | ` *` |
|      - |  5354 | ` * The other half of importNode: the node is MOVED rather than copied, so the` |
|      - |  5355 | ` * source loses it and every wrapper PHP holds onto it keeps working and starts` |
|      - |  5356 | ` * answering this document.` |
|      - |  5357 | ` *` |
|      - |  5358 | ` * php's rules, measured:` |
|      - |  5359 | ` *` |
|      - |  5360 | ` *   * The answer is the SAME object, and it is always UNLINKED first -- even` |
|      - |  5361 | ` *     when it already belongs to this document, which is observable:` |
|      - |  5362 | `` *     `$d->adoptNode($d->documentElement)` leaves the document empty.`` |
|      - |  5363 | ` *   * A DOCUMENT is the Not Supported refusal (raised in the mode the ARGUMENT's` |
|      - |  5364 | `` *     document is in, not the receiver's); a FRAGMENT is a plain `false` with`` |
|      - |  5365 | ` *     no error at all.` |
|      - |  5366 | ` *   * An attribute is taken off its element. Every node under what moved changes` |
|      - |  5367 | ` *     document too, wrappers included.` |
|      - |  5368 | ` *   * NOTHING is re-declared: an adopted element keeps pointing at its old` |
|      - |  5369 | ` *     namespace and answers the same namespaceURI while carrying no declaration` |
|      - |  5370 | ` *     of it -- the declaration appears when it is LINKED, from the reconcile.` |
|      - |  5371 | ` */` |
|     52 |  5372 | `DOM_METHOD(vm_builtin_DOMDocument_adoptNode)` |
|      1 |  5373 | `{` |
|     53 |  5374 | `	ph7_vm *pVm = pCtx->pVm;` |
|     53 |  5375 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     53 |  5376 | `	phl_domnode *pSrc = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|     53 |  5377 | `	ph7_class_instance *pSrcDoc = nArg > 0 ? DomObjArgDoc(apArg[0]) : 0;` |
|      - |  5378 | `	xmlNodePtr pNode;` |
|     53 |  5379 | `	if( pDocNd == 0 \|\| pSrc == 0 ){` |
|    ! 0 |  5380 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5381 | `		return PH7_OK;` |
|      - |  5382 | `	}` |
|     53 |  5383 | `	pNode = (xmlNodePtr)pSrc->pNode;` |
|     53 |  5384 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|      - |  5385 | `		/* The one refusal in this file that consults the ARGUMENT's document` |
|      - |  5386 | `		 * rather than the receiver's: php reaches for the strictness of the` |
|      - |  5387 | ``		 * node it was handed, so `$strict->adoptNode($lax)` warns and`` |
|      - |  5388 | ``		 * `$lax->adoptNode($strict)` throws. */`` |
|     11 |  5389 | `		return DomThrowFor(pCtx,pSrcDoc,DOM_ERR_NOT_SUPPORTED,DOM_REFUSE_FALSE);` |
|      - |  5390 | `	}` |
|     43 |  5391 | `	if( pNode->type == XML_DOCUMENT_FRAG_NODE ){` |
|      3 |  5392 | `		ph7_result_bool(pCtx,0);` |
|      3 |  5393 | `		return PH7_OK;` |
|      - |  5394 | `	}` |
|     41 |  5395 | `	DomDetach(pSrc->pShell,pNode);` |
|     41 |  5396 | `	if( pNode->doc != (xmlDocPtr)pDocNd->pNode ){` |
|      - |  5397 | `		/*` |
|      - |  5398 | `		 * NOT xmlSetTreeDoc: a parsed document interns its node names in its` |
|      - |  5399 | `		 * own dictionary, so a node re-homed by hand keeps names owned by the` |
|      - |  5400 | `		 * document it LEFT -- and freeing the target document then frees` |
|      - |  5401 | `		 * strings the source's dictionary owns. ASan called it what it is, a` |
|      - |  5402 | `		 * bad free. xmlDOMWrapAdoptNode is libxml's own re-homing: it moves the` |
|      - |  5403 | `		 * strings, the attribute values and the ID table entries with the node.` |
|      - |  5404 | `		 * (A CONSTRUCTED node has no document and no dictionary at all, and` |
|      - |  5405 | `		 * xmlSetTreeDoc IS its whole move.)` |
|      - |  5406 | `		 */` |
|     39 |  5407 | `		if( pNode->doc == 0 ){` |
|      3 |  5408 | `			xmlSetTreeDoc(pNode,(xmlDocPtr)pDocNd->pNode);` |
|      2 |  5409 | `		}else{` |
|     37 |  5410 | `			sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     37 |  5411 | `			xmlDOMWrapAdoptNode(0,pNode->doc,pNode,(xmlDocPtr)pDocNd->pNode,0,0);` |
|     37 |  5412 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::adoptNode");` |
|      - |  5413 | `		}` |
|     39 |  5414 | `		DomAdoptWrappers(pVm,pSrcDoc,DomThisDoc(pCtx),pDocNd->pShell,pNode);` |
|     19 |  5415 | `	}` |
|     41 |  5416 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|     41 |  5417 | `	ph7_result_value(pCtx,apArg[0]);` |
|     41 |  5418 | `	return PH7_OK;` |
|     27 |  5419 | `}` |
|      - |  5420 | `/*` |
|      - |  5421 | ` * DOMElement::insertAdjacentElement(string $where, DOMElement $element): ?DOMElement` |
|      - |  5422 | ` * DOMElement::insertAdjacentText(string $where, string $data): void` |
|      - |  5423 | ` *` |
|      - |  5424 | ` * php's dom_insert_adjacent, transcribed.  The WHERE word is matched` |
|      - |  5425 | ` * case-insensitively against the four positions and anything else is the` |
|      - |  5426 | ` * Syntax refusal (code 12, new to DomErrText) -- even on a receiver no` |
|      - |  5427 | ` * position could serve; beforebegin/afterend on a parentless receiver answer` |
|      - |  5428 | ` * null BEFORE anything moves; and then the argument is ADOPTED into this` |
|      - |  5429 | ` * document -- a node of another document is MOVED here, wrappers and all,` |
|      - |  5430 | ` * where every other insertion method refuses it with Wrong Document Error.` |
|      - |  5431 | ` * Only then does the pre-insertion validity run, so a refusal (the receiver` |
|      - |  5432 | ` * inside the argument) leaves the adopted argument DETACHED --` |
|      - |  5433 | `` * `$in->insertAdjacentElement('afterbegin',$host)` costs the tree the whole`` |
|      - |  5434 | ` * host subtree, php's own answer -- and the insertion point is read AFTER the` |
|      - |  5435 | ` * adopt unlinked the argument, which is what makes inserting one's own next` |
|      - |  5436 | `` * sibling `afterend` a no-op rather than a swap.`` |
|      - |  5437 | ` *` |
|      - |  5438 | ` * One deliberate divergence: php SEGFAULTS on` |
|      - |  5439 | `` * `$a->insertAdjacentElement('beforebegin',$a)` -- its adopt unlinks the`` |
|      - |  5440 | ` * receiver and the insertion then walks a NULL parent.  PHL answers the` |
|      - |  5441 | ` * Hierarchy refusal its validity was about to reach.` |
|      - |  5442 | ` */` |
|     42 |  5443 | `static int DomInsertAdjacentOp(ph7_context *pCtx,phl_domnode *pRecv,const char *zWhere,` |
|      - |  5444 | `	phl_xmldoc *pArgShell,xmlNodePtr pOther,ph7_class_instance *pArgDoc)` |
|      1 |  5445 | `{` |
|     43 |  5446 | `	ph7_vm *pVm = pCtx->pVm;` |
|     43 |  5447 | `	xmlNodePtr pThis = (xmlNodePtr)pRecv->pNode;` |
|      - |  5448 | `	xmlNodePtr pParent,pRef;` |
|      - |  5449 | `	int iPos,iErr;` |
|     43 |  5450 | `	if( DomNameIsCi(zWhere,"beforebegin") ){` |
|     11 |  5451 | `		iPos = 0;` |
|     38 |  5452 | `	}else if( DomNameIsCi(zWhere,"afterbegin") ){` |
|     19 |  5453 | `		iPos = 1;` |
|     24 |  5454 | `	}else if( DomNameIsCi(zWhere,"beforeend") ){` |
|      5 |  5455 | `		iPos = 2;` |
|     13 |  5456 | `	}else if( DomNameIsCi(zWhere,"afterend") ){` |
|      7 |  5457 | `		iPos = 3;` |
|      4 |  5458 | `	}else{` |
|      5 |  5459 | `		DomThrowVoid(pCtx,DOM_ERR_SYNTAX);` |
|      5 |  5460 | `		return -1;` |
|      - |  5461 | `	}` |
|     39 |  5462 | `	if( (iPos == 0 \|\| iPos == 3) && pThis->parent == 0 ){` |
|      7 |  5463 | `		return 1;   /* the null answer, nothing moved */` |
|      - |  5464 | `	}` |
|      - |  5465 | `	/* The adopt: detach, re-home across documents (adoptNode's machinery),` |
|      - |  5466 | `	 * and park until linked. A document-less argument -- a constructed node --` |
|      - |  5467 | `	 * has no dict-interned strings to move, so xmlSetTreeDoc is its whole` |
|      - |  5468 | `	 * move; the wrappers cross either way. */` |
|     33 |  5469 | `	DomDetach(pArgShell,pOther);` |
|     33 |  5470 | `	if( pOther->doc != pThis->doc ){` |
|      5 |  5471 | `		if( pOther->doc == 0 ){` |
|    ! 0 |  5472 | `			xmlSetTreeDoc(pOther,pThis->doc);` |
|    ! 0 |  5473 | `		}else{` |
|      5 |  5474 | `			sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      5 |  5475 | `			xmlDOMWrapAdoptNode(0,pOther->doc,pOther,pThis->doc,0,0);` |
|      5 |  5476 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMElement::insertAdjacentElement");` |
|      - |  5477 | `		}` |
|      5 |  5478 | `		DomAdoptWrappers(pVm,pArgDoc,DomThisDoc(pCtx),pRecv->pShell,pOther);` |
|      2 |  5479 | `	}` |
|     33 |  5480 | `	DomOrphanAdd(pRecv->pShell,pOther);` |
|     33 |  5481 | `	switch( iPos ){` |
|      7 |  5482 | `	case 0:  pParent = pThis->parent;  pRef = pThis;            break;` |
|     19 |  5483 | `	case 1:  pParent = pThis;          pRef = pThis->children;  break;` |
|      5 |  5484 | `	case 2:  pParent = pThis;          pRef = 0;                break;` |
|      5 |  5485 | `	default: pParent = pThis->parent;  pRef = pThis->next;      break;` |
|      - |  5486 | `	}` |
|     33 |  5487 | `	if( pParent == 0 ){` |
|      - |  5488 | `		/* The argument WAS the receiver: adopting it took the parent away. */` |
|    ! 0 |  5489 | `		DomThrowVoid(pCtx,DOM_ERR_HIERARCHY);` |
|    ! 0 |  5490 | `		return -1;` |
|      - |  5491 | `	}` |
|     33 |  5492 | `	iErr = DomInsertValidity(pParent,pOther,0);` |
|     33 |  5493 | `	if( iErr ){` |
|      7 |  5494 | `		DomThrowVoid(pCtx,iErr);` |
|      7 |  5495 | `		return -1;` |
|      - |  5496 | `	}` |
|     27 |  5497 | `	if( pRef == pOther ){` |
|    ! 0 |  5498 | `		pRef = pOther->next;` |
|    ! 0 |  5499 | `	}` |
|     27 |  5500 | `	DomDetach(pRecv->pShell,pOther);` |
|     27 |  5501 | `	if( pRef ){` |
|     11 |  5502 | `		DomLinkBefore(pParent,pOther,pRef);` |
|      6 |  5503 | `	}else{` |
|     17 |  5504 | `		DomLinkLast(pParent,pOther);` |
|      - |  5505 | `	}` |
|     27 |  5506 | `	DomNsOnInsertEx(pOther,0);` |
|     27 |  5507 | `	return 0;` |
|     22 |  5508 | `}` |
|     30 |  5509 | `DOM_METHOD(vm_builtin_DOMElement_insertAdjacentElement)` |
|      1 |  5510 | `{` |
|     31 |  5511 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     31 |  5512 | `	phl_domnode *pOther = nArg > 1 ? DomObjArg(apArg[1]) : 0;` |
|     31 |  5513 | `	const char *zWhere = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     31 |  5514 | `	if( pNd == 0 \|\| pOther == 0 ){` |
|    ! 0 |  5515 | `		return PH7_OK;` |
|      - |  5516 | `	}` |
|     45 |  5517 | `	if( DomInsertAdjacentOp(pCtx,pNd,zWhere,pOther->pShell,(xmlNodePtr)pOther->pNode,` |
|     46 |  5518 | `		DomObjArgDoc(apArg[1])) == 0 ){` |
|      - |  5519 | `		/* The answer is the argument itself, now linked. */` |
|     19 |  5520 | `		ph7_result_value(pCtx,apArg[1]);` |
|      9 |  5521 | `	}` |
|     31 |  5522 | `	return PH7_OK;` |
|     16 |  5523 | `}` |
|     12 |  5524 | `DOM_METHOD(vm_builtin_DOMElement_insertAdjacentText)` |
|      1 |  5525 | `{` |
|     13 |  5526 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     13 |  5527 | `	const char *zWhere = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|      - |  5528 | `	const char *zData;` |
|     13 |  5529 | `	int nData = 0;` |
|      - |  5530 | `	xmlNodePtr pText;` |
|     13 |  5531 | `	if( pNd == 0 ){` |
|    ! 0 |  5532 | `		return PH7_OK;` |
|      - |  5533 | `	}` |
|     13 |  5534 | `	zData = nArg > 1 ? ph7_value_to_string(apArg[1],&nData) : "";` |
|     13 |  5535 | `	pText = xmlNewDocTextLen(((xmlNodePtr)pNd->pNode)->doc,(const xmlChar *)zData,nData);` |
|     13 |  5536 | `	if( pText == 0 ){` |
|    ! 0 |  5537 | `		return PH7_OK;` |
|      - |  5538 | `	}` |
|      - |  5539 | `	/* Park it FIRST. The op's two earliest refusals -- the Syntax word and` |
|      - |  5540 | `	 * the parentless beforebegin/afterend null -- return before its own` |
|      - |  5541 | `	 * DomOrphanAdd runs, and an unparked fresh node outlives every owner` |
|      - |  5542 | `	 * (the leak checker is what noticed). Parking is idempotent, the op's` |
|      - |  5543 | `	 * detach removes exactly one entry, and the linked node ends OFF the` |
|      - |  5544 | `	 * orphan list -- so the early paths leave it parked in the shell where` |
|      - |  5545 | `	 * teardown frees it, unobservable, which is php's answer. */` |
|     13 |  5546 | `	DomOrphanAdd(pNd->pShell,pText);` |
|     13 |  5547 | `	DomInsertAdjacentOp(pCtx,pNd,zWhere,pNd->pShell,pText,0);` |
|     13 |  5548 | `	return PH7_OK;` |
|      7 |  5549 | `}` |
|      - |  5550 | `/* DOMDocument::createTextNode / createComment / createCDATASection(string $data) */` |
|    130 |  5551 | `static int DomDocCreateData(ph7_context *pCtx,int iKind,int nArg,ph7_value **apArg)` |
|      2 |  5552 | `{` |
|    132 |  5553 | `	int nVal = 0;` |
|    132 |  5554 | `	const char *zVal = nArg > 0 ? ph7_value_to_string(apArg[0],&nVal) : "";` |
|    132 |  5555 | `	return DomDocCreate(pCtx,iKind,"",zVal,nVal);` |
|      2 |  5556 | `}` |
|    112 |  5557 | `DOM_METHOD(vm_builtin_DOMDocument_createTextNode)` |
|      2 |  5558 | `{` |
|    114 |  5559 | `	return DomDocCreateData(pCtx,XML_TEXT_NODE,nArg,apArg);` |
|      2 |  5560 | `}` |
|      - |  5561 | `/* DOMDocument::createProcessingInstruction(string $target, string $data = '')` |
|      - |  5562 | ` * / createEntityReference(string $name) / createDocumentFragment() */` |
|     20 |  5563 | `DOM_METHOD(vm_builtin_DOMDocument_createPI)` |
|      1 |  5564 | `{` |
|     21 |  5565 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     21 |  5566 | `	int nVal = 0;` |
|     21 |  5567 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";` |
|     21 |  5568 | `	return DomDocCreate(pCtx,XML_PI_NODE,zName,zVal,nVal);` |
|      1 |  5569 | `}` |
|     24 |  5570 | `DOM_METHOD(vm_builtin_DOMDocument_createEntityRef)` |
|      1 |  5571 | `{` |
|     25 |  5572 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     25 |  5573 | `	return DomDocCreate(pCtx,XML_ENTITY_REF_NODE,zName,"",0);` |
|      1 |  5574 | `}` |
|     84 |  5575 | `DOM_METHOD(vm_builtin_DOMDocument_createFragment)` |
|      1 |  5576 | `{` |
|     42 |  5577 | `	SXUNUSED(nArg);` |
|     42 |  5578 | `	SXUNUSED(apArg);` |
|     85 |  5579 | `	return DomDocCreate(pCtx,XML_DOCUMENT_FRAG_NODE,"","",0);` |
|      1 |  5580 | `}` |
|     10 |  5581 | `DOM_METHOD(vm_builtin_DOMDocument_createComment)` |
|      1 |  5582 | `{` |
|     11 |  5583 | `	return DomDocCreateData(pCtx,XML_COMMENT_NODE,nArg,apArg);` |
|      1 |  5584 | `}` |
|      8 |  5585 | `DOM_METHOD(vm_builtin_DOMDocument_createCDATASection)` |
|      1 |  5586 | `{` |
|      9 |  5587 | `	return DomDocCreateData(pCtx,XML_CDATA_SECTION_NODE,nArg,apArg);` |
|      1 |  5588 | `}` |
|      - |  5589 | `/*` |
|      - |  5590 | `` * php's normalization, which both `DOMNode::normalize()` and`` |
|      - |  5591 | `` * `DOMDocument::normalizeDocument()` are: adjacent text nodes merge into the`` |
|      - |  5592 | ` * FIRST of the run, and a text node left EMPTY is then dropped from the tree` |
|      - |  5593 | ` * entirely -- including one that was empty to begin with, which is what makes` |
|      - |  5594 | `` * `$el->normalize()` the way a program gets rid of the zero-length text nodes an`` |
|      - |  5595 | ` * edit leaves behind. Dropping them was the half missing here: a document that` |
|      - |  5596 | `` * had been normalized still serialized `<k></k>` where php writes `<k/>`, and`` |
|      - |  5597 | `` * still counted the empty node in `childNodes->length`.`` |
|      - |  5598 | ` *` |
|      - |  5599 | ` * Merged-away and dropped siblings are PARKED as orphans, never freed, so any` |
|      - |  5600 | ` * PHP wrapper to them stays valid -- php keeps exactly those alive too, through` |
|      - |  5601 | ` * its own wrapper refcount, and a variable holding one reads its old content and` |
|      - |  5602 | `` * a NULL `parentNode` in both engines.`` |
|      - |  5603 | ` *` |
|      - |  5604 | ` * The walk descends into a child ELEMENT and into that element's ATTRIBUTES` |
|      - |  5605 | ` * (an attribute's value is a child text list of its own, and a program that` |
|      - |  5606 | ` * built one in pieces has the same run of nodes to merge). What it does NOT` |
|      - |  5607 | ` * touch is the RECEIVER's own attributes -- php's switch reaches an attribute` |
|      - |  5608 | `` * only through a child element -- so `$el->normalize()` leaves `$el`'s`` |
|      - |  5609 | `` * attributes alone while `$el->parentNode->normalize()` normalizes them.`` |
|      - |  5610 | ` */` |
|     56 |  5611 | `static void DomNormalizeTree(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |  5612 | `{` |
|     57 |  5613 | `	xmlNodePtr pChild = pNode->children;` |
|    127 |  5614 | `	while( pChild ){` |
|     71 |  5615 | `		if( pChild->type == XML_TEXT_NODE ){` |
|      - |  5616 | `			xmlNodePtr pNext;` |
|     37 |  5617 | `			while( pChild->next && pChild->next->type == XML_TEXT_NODE ){` |
|      9 |  5618 | `				pNext = pChild->next;` |
|      9 |  5619 | `				if( pNext->content ){` |
|      9 |  5620 | `					xmlNodeAddContent(pChild,pNext->content);` |
|      4 |  5621 | `				}` |
|      9 |  5622 | `				xmlUnlinkNode(pNext);` |
|      9 |  5623 | `				DomOrphanAdd(pShell,pNext);` |
|      1 |  5624 | `			}` |
|     29 |  5625 | `			if( pChild->content == 0 \|\| pChild->content[0] == 0 ){` |
|      5 |  5626 | `				pNext = pChild->next;` |
|      5 |  5627 | `				xmlUnlinkNode(pChild);` |
|      5 |  5628 | `				DomOrphanAdd(pShell,pChild);` |
|      5 |  5629 | `				pChild = pNext;` |
|      5 |  5630 | `				continue;` |
|      1 |  5631 | `			}` |
|     55 |  5632 | `		}else if( pChild->type == XML_ELEMENT_NODE ){` |
|      - |  5633 | `			xmlAttrPtr pAttr;` |
|     29 |  5634 | `			DomNormalizeTree(pShell,pChild);` |
|     43 |  5635 | `			for( pAttr = pChild->properties ; pAttr ; pAttr = pAttr->next ){` |
|     15 |  5636 | `				DomNormalizeTree(pShell,(xmlNodePtr)pAttr);` |
|      8 |  5637 | `			}` |
|     29 |  5638 | `		}else if( pChild->type == XML_ATTRIBUTE_NODE ){` |
|      - |  5639 | `			/* Unreachable from a tree walk (attributes are not children), but` |
|      - |  5640 | `			 * php's switch states it and a fragment/DTD shape could reach it. */` |
|    ! 0 |  5641 | `			DomNormalizeTree(pShell,pChild);` |
|    ! 0 |  5642 | `		}` |
|     67 |  5643 | `		pChild = pChild->next;` |
|      1 |  5644 | `	}` |
|     57 |  5645 | `}` |
|      - |  5646 | `/* DOMDocument::normalizeDocument(): void and DOMNode::normalize(): void -- php` |
|      - |  5647 | ` * runs the same walk from the receiver, so the two share one body. */` |
|     14 |  5648 | `DOM_METHOD(vm_builtin_DOMDocument_normalizeDocument)` |
|      1 |  5649 | `{` |
|     15 |  5650 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  5651 | `	SXUNUSED(nArg);` |
|      7 |  5652 | `	SXUNUSED(apArg);` |
|     15 |  5653 | `	if( pNd ){` |
|     15 |  5654 | `		DomNormalizeTree(pNd->pShell,(xmlNodePtr)pNd->pNode);` |
|      7 |  5655 | `	}` |
|     15 |  5656 | `	return PH7_OK;` |
|      1 |  5657 | `}` |
|      - |  5658 | `/* DOMNode::getNodePath(): ?string -- the XPath that selects this node, or null` |
|      - |  5659 | ` * for one that is not addressable at all (anything under a fragment). php hands` |
|      - |  5660 | ` * libxml's answer straight back, positional predicate and all. */` |
|     60 |  5661 | `DOM_METHOD(vm_builtin_DOMNode_getNodePath)` |
|      1 |  5662 | `{` |
|     61 |  5663 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     61 |  5664 | `	xmlChar *zPath = pNd ? xmlGetNodePath((xmlNodePtr)pNd->pNode) : 0;` |
|     30 |  5665 | `	SXUNUSED(nArg);` |
|     30 |  5666 | `	SXUNUSED(apArg);` |
|     61 |  5667 | `	if( zPath == 0 ){` |
|     15 |  5668 | `		ph7_result_null(pCtx);` |
|     15 |  5669 | `		return PH7_OK;` |
|      - |  5670 | `	}` |
|     47 |  5671 | `	ph7_result_string(pCtx,(const char *)zPath,-1);` |
|     47 |  5672 | `	xmlFree(zPath);` |
|     47 |  5673 | `	return PH7_OK;` |
|     31 |  5674 | `}` |
|      - |  5675 |  |
|      - |  5676 | `/* ===== Character data: the in-place edit family ===== */` |
|      - |  5677 |  |
|      - |  5678 | `/*` |
|      - |  5679 | ` * Every offset and count on this surface is measured in UTF-8 CHARACTERS, not` |
|      - |  5680 | `` * bytes -- php runs `xmlUTF8Strlen` over the content and `xmlUTF8Strsub` to cut`` |
|      - |  5681 | `` * it -- so `$t->length` on "áé漢字" is 4 and `substringData(0,1)` is one`` |
|      - |  5682 | `` * character rather than one byte. PHL measured `length` with strlen(), which is`` |
|      - |  5683 | ` * a silently wrong answer for every non-ASCII document: 10 where php says 4,` |
|      - |  5684 | ` * and every offset a program then computed from it landed mid-character.` |
|      - |  5685 | ` *` |
|      - |  5686 | ` * libxml's own UTF-8 helpers are used rather than PHL's, so malformed content` |
|      - |  5687 | ` * counts and cuts identically in both engines.` |
|      - |  5688 | ` */` |
|    120 |  5689 | `static int DomCharLength(xmlNodePtr pNode)` |
|      1 |  5690 | `{` |
|    121 |  5691 | `	return (pNode && pNode->content) ? xmlUTF8Strlen(pNode->content) : 0;` |
|      1 |  5692 | `}` |
|      - |  5693 | `/*` |
|      - |  5694 | ` * php's Index Size Error: a negative bound, or an offset past the end. The` |
|      - |  5695 | ` * COUNT is clamped rather than refused once the offset is in range.` |
|      - |  5696 | ` *` |
|      - |  5697 | ` * The upper bound is compared UNSIGNED on three of the five and SIGNED on the` |
|      - |  5698 | ``  * other two, and only malformed content tells them apart: `xmlUTF8Strlen` `` |
|      - |  5699 | `` * answers -1 for content that is not valid UTF-8 (`$t->length` reports that`` |
|      - |  5700 | ` * -1), and as an UNSIGNED bound a -1 means "no limit" -- so substringData,` |
|      - |  5701 | ` * insertData and splitText all work on such a node and let libxml's own cutting` |
|      - |  5702 | ` * decide what comes back, while deleteData and replaceData refuse it outright,` |
|      - |  5703 | ` * for every offset and every count. php's own split, kept because a program` |
|      - |  5704 | ` * handed a byte string that is not UTF-8 gets a value back from three of these` |
|      - |  5705 | ` * and an exception from the other two.` |
|      - |  5706 | ` */` |
|     88 |  5707 | `static int DomCharRange(ph7_context *pCtx,xmlNodePtr pNode,ph7_int64 iOffset,` |
|      - |  5708 | `	ph7_int64 iCount,int bHasCount,int bUnsignedBound,int *pnLen,int *pRc)` |
|      1 |  5709 | `{` |
|     89 |  5710 | `	int nLen = DomCharLength(pNode);` |
|    114 |  5711 | `	int bPastEnd = bUnsignedBound ? (sxu32)iOffset > (sxu32)nLen` |
|     63 |  5712 | `	                              : iOffset > (ph7_int64)nLen;` |
|     89 |  5713 | `	*pnLen = nLen;` |
|     88 |  5714 | `	if( iOffset < 0 \|\| (bHasCount && iCount < 0)` |
|     79 |  5715 | `	 \|\| iOffset > (ph7_int64)SXI32_HIGH \|\| iCount > (ph7_int64)SXI32_HIGH` |
|     77 |  5716 | `	 \|\| bPastEnd ){` |
|     33 |  5717 | `		*pRc = DomThrow(pCtx,DOM_ERR_INDEX_SIZE);` |
|     33 |  5718 | `		return -1;` |
|      - |  5719 | `	}` |
|     57 |  5720 | `	return 0;` |
|     45 |  5721 | `}` |
|      - |  5722 | `/* DOMCharacterData::substringData(int $offset, int $count): string */` |
|     36 |  5723 | `DOM_METHOD(vm_builtin_DOMCharacterData_substringData)` |
|      1 |  5724 | `{` |
|     37 |  5725 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     37 |  5726 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     37 |  5727 | `	ph7_int64 iOffset = nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     37 |  5728 | `	ph7_int64 iCount = nArg > 1 ? ph7_value_to_int64(apArg[1]) : 0;` |
|      - |  5729 | `	xmlChar *zSub;` |
|     37 |  5730 | `	int nLen,rc = PH7_OK;` |
|     37 |  5731 | `	if( pNode == 0 ){` |
|    ! 0 |  5732 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5733 | `		return PH7_OK;` |
|      - |  5734 | `	}` |
|     37 |  5735 | `	if( DomCharRange(pCtx,pNode,iOffset,iCount,TRUE,TRUE,&nLen,&rc) != 0 ){` |
|     15 |  5736 | `		return rc;` |
|      - |  5737 | `	}` |
|     23 |  5738 | `	if( pNode->content == 0 ){` |
|      - |  5739 | `		/* php reads a NULL content pointer -- the omitted-argument` |
|      - |  5740 | `		 * constructor's node -- as "": the range still screens (so an offset` |
|      - |  5741 | `		 * past zero is Index Size), and what is left of nothing is "". */` |
|      3 |  5742 | `		ph7_result_string(pCtx,"",0);` |
|      3 |  5743 | `		return PH7_OK;` |
|      - |  5744 | `	}` |
|     21 |  5745 | `	if( (sxu32)(iOffset+iCount) > (sxu32)nLen ){` |
|      7 |  5746 | `		iCount = (ph7_int64)nLen - iOffset;` |
|      3 |  5747 | `	}` |
|     21 |  5748 | `	zSub = xmlUTF8Strsub(pNode->content,(int)iOffset,(int)iCount);` |
|     21 |  5749 | `	ph7_result_string(pCtx,zSub ? (const char *)zSub : "",-1);` |
|     21 |  5750 | `	if( zSub ){` |
|     21 |  5751 | `		xmlFree(zSub);` |
|     10 |  5752 | `	}` |
|     21 |  5753 | `	return PH7_OK;` |
|     19 |  5754 | `}` |
|      - |  5755 | `/* DOMCharacterData::appendData(string $data): true -- raw bytes, no entity` |
|      - |  5756 | `` * parsing, which is why `appendData('&amp;')` stores those five characters. */`` |
|      6 |  5757 | `DOM_METHOD(vm_builtin_DOMCharacterData_appendData)` |
|      1 |  5758 | `{` |
|      7 |  5759 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      7 |  5760 | `	int nData = 0;` |
|      7 |  5761 | `	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : "";` |
|      7 |  5762 | `	if( pNd ){` |
|      7 |  5763 | `		xmlTextConcat((xmlNodePtr)pNd->pNode,(const xmlChar *)zData,nData);` |
|      3 |  5764 | `	}` |
|      7 |  5765 | `	ph7_result_bool(pCtx,1);` |
|      7 |  5766 | `	return PH7_OK;` |
|      1 |  5767 | `}` |
|      - |  5768 | `/*` |
|      - |  5769 | ` * The three writers, which php builds the same way: the head up to $offset, the` |
|      - |  5770 | ` * replacement, then whatever the count left of the tail.` |
|      - |  5771 | ` *` |
|      - |  5772 | ` * insertData is (offset, 0, data), deleteData is (offset, count, ""), and` |
|      - |  5773 | ` * replaceData is both -- php's own three bodies say the same thing three times.` |
|      - |  5774 | ` */` |
|     52 |  5775 | `static int DomCharSplice(ph7_context *pCtx,ph7_int64 iOffset,ph7_int64 iCount,` |
|      - |  5776 | `	int bHasCount,const char *zData,int nData)` |
|      1 |  5777 | `{` |
|     53 |  5778 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     53 |  5779 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     53 |  5780 | `	xmlChar *zHead,*zTail = 0;` |
|     53 |  5781 | `	int nLen,rc = PH7_OK;` |
|     53 |  5782 | `	if( pNode == 0 ){` |
|    ! 0 |  5783 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5784 | `		return PH7_OK;` |
|      - |  5785 | `	}` |
|     53 |  5786 | `	if( pNode->content == 0 ){` |
|      - |  5787 | `		/* A writer normalizes the omitted-argument constructor's NULL content` |
|      - |  5788 | ``		 * to "" and proceeds, php's own answer: `insertData(0,'i')` on a`` |
|      - |  5789 | ``		 * `new DOMComment()` writes "i", and its nodeValue reads "" after. */`` |
|      5 |  5790 | `		xmlNodeSetContent(pNode,(const xmlChar *)"");` |
|      2 |  5791 | `	}` |
|      - |  5792 | `	/* insertData has no count and takes the unsigned bound; the two that DO` |
|      - |  5793 | `	 * take one take the signed bound. */` |
|     53 |  5794 | `	if( DomCharRange(pCtx,pNode,iOffset,iCount,bHasCount,!bHasCount,&nLen,&rc) != 0 ){` |
|     19 |  5795 | `		return rc;` |
|      - |  5796 | `	}` |
|     35 |  5797 | `	if( (sxu32)(iOffset+iCount) > (sxu32)nLen ){` |
|     11 |  5798 | `		iCount = (ph7_int64)nLen - iOffset;` |
|      5 |  5799 | `	}` |
|     27 |  5800 | `	zHead = iOffset > 0 ? xmlUTF8Strndup(pNode->content,(int)iOffset)` |
|     25 |  5801 | `	                    : xmlStrdup((const xmlChar *)"");` |
|     35 |  5802 | `	if( iOffset + iCount < (ph7_int64)nLen ){` |
|     31 |  5803 | `		zTail = xmlUTF8Strsub(pNode->content,(int)(iOffset+iCount),` |
|     20 |  5804 | `			(int)((ph7_int64)nLen - iOffset - iCount));` |
|     10 |  5805 | `	}` |
|     35 |  5806 | `	xmlNodeSetContent(pNode,zHead ? zHead : (const xmlChar *)"");` |
|     35 |  5807 | `	if( nData > 0 ){` |
|     21 |  5808 | `		xmlNodeAddContentLen(pNode,(const xmlChar *)zData,nData);` |
|     10 |  5809 | `	}` |
|     35 |  5810 | `	if( zTail ){` |
|     21 |  5811 | `		xmlNodeAddContent(pNode,zTail);` |
|     10 |  5812 | `	}` |
|     35 |  5813 | `	if( zHead ){` |
|     35 |  5814 | `		xmlFree(zHead);` |
|     17 |  5815 | `	}` |
|     35 |  5816 | `	if( zTail ){` |
|     21 |  5817 | `		xmlFree(zTail);` |
|     10 |  5818 | `	}` |
|     35 |  5819 | `	ph7_result_bool(pCtx,1);` |
|     35 |  5820 | `	return PH7_OK;` |
|     27 |  5821 | `}` |
|      - |  5822 | `/* DOMCharacterData::insertData(int $offset, string $data): true */` |
|     14 |  5823 | `DOM_METHOD(vm_builtin_DOMCharacterData_insertData)` |
|      1 |  5824 | `{` |
|     15 |  5825 | `	int nData = 0;` |
|     15 |  5826 | `	const char *zData = nArg > 1 ? ph7_value_to_string(apArg[1],&nData) : "";` |
|     15 |  5827 | `	return DomCharSplice(pCtx,nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0,0,FALSE,zData,nData);` |
|      1 |  5828 | `}` |
|      - |  5829 | `/* DOMCharacterData::deleteData(int $offset, int $count): true */` |
|     22 |  5830 | `DOM_METHOD(vm_builtin_DOMCharacterData_deleteData)` |
|      1 |  5831 | `{` |
|     45 |  5832 | `	return DomCharSplice(pCtx,nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0,` |
|     22 |  5833 | `		nArg > 1 ? ph7_value_to_int64(apArg[1]) : 0,TRUE,"",0);` |
|      1 |  5834 | `}` |
|      - |  5835 | `/* DOMCharacterData::replaceData(int $offset, int $count, string $data): true */` |
|     16 |  5836 | `DOM_METHOD(vm_builtin_DOMCharacterData_replaceData)` |
|      1 |  5837 | `{` |
|     17 |  5838 | `	int nData = 0;` |
|     17 |  5839 | `	const char *zData = nArg > 2 ? ph7_value_to_string(apArg[2],&nData) : "";` |
|     33 |  5840 | `	return DomCharSplice(pCtx,nArg > 2 ? ph7_value_to_int64(apArg[0]) : 0,` |
|     16 |  5841 | `		nArg > 2 ? ph7_value_to_int64(apArg[1]) : 0,TRUE,zData,nData);` |
|      1 |  5842 | `}` |
|      - |  5843 | `/*` |
|      - |  5844 | ` * DOMText::splitText(int $offset): DOMText\|false` |
|      - |  5845 | ` *` |
|      - |  5846 | ` * The receiver keeps the head and a SECOND node takes the tail, spliced in` |
|      - |  5847 | ` * right after it. Two details only the oracle states: an offset past the end is` |
|      - |  5848 | `` * plain `false` where a negative one is a ValueError, and splitting a CDATA`` |
|      - |  5849 | `` * section produces a TEXT node -- so `<![CDATA[abcdef]]>` split at 2 serializes`` |
|      - |  5850 | `` * as `<![CDATA[ab]]>cdef`.`` |
|      - |  5851 | ` */` |
|     22 |  5852 | `DOM_METHOD(vm_builtin_DOMText_splitText)` |
|      1 |  5853 | `{` |
|     23 |  5854 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     23 |  5855 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     23 |  5856 | `	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|      - |  5857 | `	xmlChar *zHead,*zTail;` |
|      - |  5858 | `	xmlNodePtr pNew;` |
|      - |  5859 | `	int nLen;` |
|     23 |  5860 | `	if( iOffset < 0 ){` |
|      3 |  5861 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  5862 | `			"DOMText::splitText(): Argument #1 ($offset) must be greater than or equal to 0");` |
|      - |  5863 | `	}` |
|     20 |  5864 | `	if( pNode == 0` |
|     21 |  5865 | `	 \|\| (pNode->type != XML_TEXT_NODE && pNode->type != XML_CDATA_SECTION_NODE) ){` |
|    ! 0 |  5866 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5867 | `		return PH7_OK;` |
|      - |  5868 | `	}` |
|     21 |  5869 | `	if( pNode->content == 0 ){` |
|      - |  5870 | `		/* The omitted-argument constructor's node splits as "": both halves` |
|      - |  5871 | `		 * empty, php's answer. The split WRITES, so normalizing is its own. */` |
|      3 |  5872 | `		xmlNodeSetContent(pNode,(const xmlChar *)"");` |
|      1 |  5873 | `	}` |
|     21 |  5874 | `	nLen = DomCharLength(pNode);` |
|     21 |  5875 | `	if( iOffset > (ph7_int64)nLen ){` |
|      3 |  5876 | `		ph7_result_bool(pCtx,0);` |
|      3 |  5877 | `		return PH7_OK;` |
|      - |  5878 | `	}` |
|     19 |  5879 | `	zHead = xmlUTF8Strndup(pNode->content,(int)iOffset);` |
|     19 |  5880 | `	zTail = xmlUTF8Strsub(pNode->content,(int)iOffset,(int)((ph7_int64)nLen - iOffset));` |
|     19 |  5881 | `	xmlNodeSetContent(pNode,zHead ? zHead : (const xmlChar *)"");` |
|     19 |  5882 | `	pNew = xmlNewDocText(pNode->doc,zTail ? zTail : (const xmlChar *)"");` |
|     19 |  5883 | `	if( zHead ){` |
|     19 |  5884 | `		xmlFree(zHead);` |
|      9 |  5885 | `	}` |
|     19 |  5886 | `	if( zTail ){` |
|     19 |  5887 | `		xmlFree(zTail);` |
|      9 |  5888 | `	}` |
|     19 |  5889 | `	if( pNew == 0 ){` |
|    ! 0 |  5890 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  5891 | `		return PH7_OK;` |
|      - |  5892 | `	}` |
|     19 |  5893 | `	if( pNode->parent ){` |
|      - |  5894 | `		/* Spliced by hand, as everything in this file is: xmlAddNextSibling` |
|      - |  5895 | `		 * MERGES two adjacent text nodes and frees one of them. */` |
|     13 |  5896 | `		if( pNode->next ){` |
|      9 |  5897 | `			DomLinkBefore(pNode->parent,pNew,pNode->next);` |
|      5 |  5898 | `		}else{` |
|      5 |  5899 | `			DomLinkLast(pNode->parent,pNew);` |
|      - |  5900 | `		}` |
|      7 |  5901 | `	}else{` |
|      7 |  5902 | `		DomOrphanAdd(pNd->pShell,pNew);` |
|      - |  5903 | `	}` |
|     19 |  5904 | `	return DomResultNodeOf(pCtx,pNd,pNew);` |
|     12 |  5905 | `}` |
|      - |  5906 | `/* DOMText::isWhitespaceInElementContent() and its 8.x rename` |
|      - |  5907 | ` * isElementContentWhitespace(): one body, libxml's blank-node test. */` |
|     12 |  5908 | `DOM_METHOD(vm_builtin_DOMText_isWhitespace)` |
|      1 |  5909 | `{` |
|     13 |  5910 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      6 |  5911 | `	SXUNUSED(nArg);` |
|      6 |  5912 | `	SXUNUSED(apArg);` |
|     13 |  5913 | `	ph7_result_bool(pCtx,pNd && xmlIsBlankNode((xmlNodePtr)pNd->pNode));` |
|     13 |  5914 | `	return PH7_OK;` |
|      1 |  5915 | `}` |
|      - |  5916 |  |
|      - |  5917 | `/* ===== C14N ===== */` |
|      - |  5918 |  |
|      - |  5919 | `/*` |
|      - |  5920 | ` * php canonicalizes a NODE by handing libxml the node SET an XPath produces` |
|      - |  5921 | `` * from it -- `(.//. \| .//@* \| .//namespace::*)` with the node as context -- and`` |
|      - |  5922 | ` * a DOCUMENT by handing it no set at all, which is how a document's top-level` |
|      - |  5923 | ` * comments reach the output where a node's cannot. Running a VISIBILITY` |
|      - |  5924 | ` * callback instead (the shape this file had) is close but not the same: an` |
|      - |  5925 | `` * ATTRIBUTE canonicalizes to its own ` b="2"` under php, where a "keep the`` |
|      - |  5926 | ` * target's subtree" callback answers the empty string.` |
|      - |  5927 | ` *` |
|      - |  5928 | `` * All four of php's parameters are read here. `$exclusive` picks Exclusive`` |
|      - |  5929 | `` * C14N, `$withComments` keeps comments, `$xpath` REPLACES the default node set`` |
|      - |  5930 | ``  * with the caller's query (and may register prefixes for it), and `$nsPrefixes` `` |
|      - |  5931 | ` * lists the namespace prefixes an exclusive canonicalization must declare even` |
|      - |  5932 | ` * where they are unused. Only the two bools were honoured before, so` |
|      - |  5933 | `` * `C14N(true)` -- the mode every XML-DSig signer asks for -- silently`` |
|      - |  5934 | ` * canonicalized inclusively and produced bytes that will not verify.` |
|      - |  5935 | ` */` |
|      - |  5936 |  |
|      - |  5937 | ``/* The `namespaces` sub-array of `$xpath`: prefix => URI, string pairs only. */`` |
|    108 |  5938 | `static int DomC14NRegisterNs(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 |  5939 | `{` |
|    109 |  5940 | `	xmlXPathContextPtr pXCtx = (xmlXPathContextPtr)pUserData;` |
|    109 |  5941 | `	if( pKey && pVal && ph7_value_is_string(pKey) && ph7_value_is_string(pVal) ){` |
|    163 |  5942 | `		xmlXPathRegisterNs(pXCtx,(const xmlChar *)ph7_value_to_string(pKey,0),` |
|    108 |  5943 | `			(const xmlChar *)ph7_value_to_string(pVal,0));` |
|     54 |  5944 | `	}` |
|    109 |  5945 | `	return PH7_OK;` |
|      1 |  5946 | `}` |
|      - |  5947 | `/*` |
|      - |  5948 | `` * The `$nsPrefixes` list, collected into the NULL-terminated array libxml`` |
|      - |  5949 | ` * wants. Non-string entries are skipped, exactly as php skips them.` |
|      - |  5950 | ` *` |
|      - |  5951 | ` * The bytes are COPIED. ph7_array_walk hands its callback a temporary copy of` |
|      - |  5952 | ` * each value and releases it the moment the callback returns, so keeping the` |
|      - |  5953 | ` * pointer leaves a dangling one -- which libxml then compares against real` |
|      - |  5954 | `` * prefixes and matches at random, so `C14N(true,false,null,['u','p'])` declared`` |
|      - |  5955 | ` * whichever prefix the freed memory happened to still read as.` |
|      - |  5956 | ` */` |
|      - |  5957 | `typedef struct DomC14NPrefixes DomC14NPrefixes;` |
|      - |  5958 | `struct DomC14NPrefixes {` |
|      - |  5959 | `	SyBlob sPool;    /* the prefix bytes, NUL-terminated one after another */` |
|      - |  5960 | `	SySet aOfs;      /* each prefix's offset into sPool */` |
|      - |  5961 | `	xmlChar **apPrefix;` |
|      - |  5962 | `};` |
|     18 |  5963 | `static int DomC14NCollectPrefix(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 |  5964 | `{` |
|     19 |  5965 | `	DomC14NPrefixes *pList = (DomC14NPrefixes *)pUserData;` |
|      9 |  5966 | `	SXUNUSED(pKey);` |
|     19 |  5967 | `	if( pVal && ph7_value_is_string(pVal) ){` |
|     17 |  5968 | `		sxu32 nOfs = SyBlobLength(&pList->sPool);` |
|     17 |  5969 | `		int nByte = 0;` |
|     17 |  5970 | `		const char *zVal = ph7_value_to_string(pVal,&nByte);` |
|     17 |  5971 | `		SySetPut(&pList->aOfs,(const void *)&nOfs);` |
|     17 |  5972 | `		SyBlobAppend(&pList->sPool,zVal,(sxu32)nByte);` |
|     17 |  5973 | `		SyBlobAppend(&pList->sPool,"",1);` |
|      8 |  5974 | `	}` |
|     19 |  5975 | `	return PH7_OK;` |
|      1 |  5976 | `}` |
|      - |  5977 | `/*` |
|      - |  5978 | ` * Canonicalize the receiver into *pzOut (xmlFree'd by the caller) and answer` |
|      - |  5979 | ` * its byte count, or -1 when php answers false/"" instead. *pRc carries a` |
|      - |  5980 | ` * refusal php raises before anything is written.` |
|      - |  5981 | ` *` |
|      - |  5982 | `` * iXPathPos is the 1-based position of `$xpath` in the CALLING method's`` |
|      - |  5983 | ` * parameter list: 3 on C14N, 4 on C14NFile, and php's messages print it.` |
|      - |  5984 | ` */` |
|    118 |  5985 | `static int DomC14NRun(ph7_context *pCtx,int nArg,ph7_value **apArg,int iXPathPos,` |
|      - |  5986 | `	const char *zFn,xmlChar **pzOut,int *pRc)` |
|      1 |  5987 | `{` |
|      - |  5988 | `	char zGiven[64];` |
|    119 |  5989 | `	ph7_vm *pVm = pCtx->pVm;` |
|    119 |  5990 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    119 |  5991 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    119 |  5992 | `	int iFirst = iXPathPos - 3;   /* index of $exclusive */` |
|    119 |  5993 | `	int bExclusive = nArg > iFirst && ph7_value_to_bool(apArg[iFirst]);` |
|    119 |  5994 | `	int bComments = nArg > iFirst+1 && ph7_value_to_bool(apArg[iFirst+1]);` |
|     76 |  5995 | `	ph7_value *pXPath = (nArg > iFirst+2 && ph7_value_is_array(apArg[iFirst+2]))` |
|     83 |  5996 | `		? apArg[iFirst+2] : 0;` |
|     68 |  5997 | `	ph7_value *pPrefixes = (nArg > iFirst+3 && ph7_value_is_array(apArg[iFirst+3]))` |
|     75 |  5998 | `		? apArg[iFirst+3] : 0;` |
|      - |  5999 | `	DomC14NPrefixes sPrefixes;` |
|    119 |  6000 | `	xmlXPathContextPtr pXCtx = 0;` |
|    119 |  6001 | `	xmlXPathObjectPtr pXObj = 0;` |
|    119 |  6002 | `	xmlNodeSetPtr pSet = 0;` |
|      - |  6003 | `	sxu32 nMark,n;` |
|      - |  6004 | `	int nOut;` |
|    119 |  6005 | `	*pzOut = 0;` |
|    119 |  6006 | `	sPrefixes.apPrefix = 0;` |
|    119 |  6007 | `	SyBlobInit(&sPrefixes.sPool,&pVm->sAllocator);` |
|    119 |  6008 | `	SySetInit(&sPrefixes.aOfs,&pVm->sAllocator,sizeof(sxu32));` |
|    119 |  6009 | `	if( pNode == 0 ){` |
|    ! 0 |  6010 | `		nOut = -1;` |
|    ! 0 |  6011 | `		goto done;` |
|      - |  6012 | `	}` |
|    119 |  6013 | `	if( pNode->doc == 0 ){` |
|      - |  6014 | `		/* php's plain Error, no DOM code: canonicalization asks libxml for the` |
|      - |  6015 | `		 * document's context, and a constructed node has none. */` |
|      5 |  6016 | `		*pRc = PH7_VmThrowException(pCtx,"Error","Node must be associated with a document");` |
|      5 |  6017 | `		nOut = -1;` |
|      5 |  6018 | `		goto done;` |
|      - |  6019 | `	}` |
|    115 |  6020 | `	if( pXPath ){` |
|     17 |  6021 | `		ph7_value *pQuery = ph7_array_fetch(pXPath,"query",(int)sizeof("query")-1);` |
|      - |  6022 | `		ph7_value *pNs;` |
|     17 |  6023 | `		if( pQuery == 0 ){` |
|      7 |  6024 | `			*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      2 |  6025 | `				"%s(): Argument #%d ($xpath) must have a \"query\" key",zFn,iXPathPos);` |
|      5 |  6026 | `			nOut = -1;` |
|      5 |  6027 | `			goto done;` |
|      - |  6028 | `		}` |
|     13 |  6029 | `		if( !ph7_value_is_string(pQuery) ){` |
|      4 |  6030 | `			*pRc = PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  6031 | `				"%s(): Argument #%d ($xpath) \"query\" option must be a string, %s given",` |
|      1 |  6032 | `				zFn,iXPathPos,VmValueGivenName(pQuery,zGiven,sizeof(zGiven)));` |
|      3 |  6033 | `			nOut = -1;` |
|      3 |  6034 | `			goto done;` |
|      - |  6035 | `		}` |
|     11 |  6036 | `		pXCtx = xmlXPathNewContext(pNode->doc);` |
|     11 |  6037 | `		if( pXCtx == 0 ){` |
|    ! 0 |  6038 | `			nOut = -1;` |
|    ! 0 |  6039 | `			goto done;` |
|      - |  6040 | `		}` |
|     11 |  6041 | `		pXCtx->node = pNode;` |
|     11 |  6042 | `		pNs = ph7_array_fetch(pXPath,"namespaces",(int)sizeof("namespaces")-1);` |
|     11 |  6043 | `		if( pNs && ph7_value_is_array(pNs) ){` |
|      3 |  6044 | `			ph7_array_walk(pNs,DomC14NRegisterNs,pXCtx);` |
|      1 |  6045 | `		}` |
|     11 |  6046 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     11 |  6047 | `		pXObj = xmlXPathEvalExpression((const xmlChar *)ph7_value_to_string(pQuery,0),pXCtx);` |
|      - |  6048 | `		/* php lets libxml's own complaint out first ("Invalid expression"), THEN` |
|      - |  6049 | `		 * raises its refusal, so the queue is flushed rather than dropped. */` |
|     11 |  6050 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|     11 |  6051 | `		pXCtx->node = 0;` |
|    104 |  6052 | `	}else if( pNode->type != XML_DOCUMENT_NODE ){` |
|     55 |  6053 | `		pXCtx = xmlXPathNewContext(pNode->doc);` |
|     55 |  6054 | `		if( pXCtx == 0 ){` |
|    ! 0 |  6055 | `			nOut = -1;` |
|    ! 0 |  6056 | `			goto done;` |
|      - |  6057 | `		}` |
|     55 |  6058 | `		pXCtx->node = pNode;` |
|     55 |  6059 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     55 |  6060 | `		pXObj = xmlXPathEvalExpression(` |
|     27 |  6061 | `			(const xmlChar *)"(.//. \| .//@* \| .//namespace::*)",pXCtx);` |
|     55 |  6062 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|     55 |  6063 | `		pXCtx->node = 0;` |
|     27 |  6064 | `	}` |
|    109 |  6065 | `	if( pXCtx ){` |
|     65 |  6066 | `		if( pXObj == 0 \|\| pXObj->type != XPATH_NODESET ){` |
|      3 |  6067 | `			*pRc = PH7_VmThrowException(pCtx,"Error","XPath query did not return a nodeset");` |
|      3 |  6068 | `			nOut = -1;` |
|      3 |  6069 | `			goto done;` |
|      - |  6070 | `		}` |
|     63 |  6071 | `		pSet = pXObj->nodesetval;` |
|     31 |  6072 | `	}` |
|      - |  6073 | ``	/* php reads `$nsPrefixes` only AFTER the query has been resolved, so a bad`` |
|      - |  6074 | `	 * query's refusal reaches the caller with no notice in front of it. */` |
|    107 |  6075 | `	if( pPrefixes ){` |
|     17 |  6076 | `		if( bExclusive ){` |
|     15 |  6077 | `			ph7_array_walk(pPrefixes,DomC14NCollectPrefix,&sPrefixes);` |
|     15 |  6078 | `			n = SySetUsed(&sPrefixes.aOfs);` |
|     15 |  6079 | `			if( n > 0 ){` |
|     25 |  6080 | `				sPrefixes.apPrefix = (xmlChar **)SyMemBackendAlloc(&pVm->sAllocator,` |
|     12 |  6081 | `					(sxu32)((n+1)*sizeof(xmlChar *)));` |
|     13 |  6082 | `				if( sPrefixes.apPrefix == 0 ){` |
|    ! 0 |  6083 | `					nOut = -1;` |
|    ! 0 |  6084 | `					goto done;` |
|      - |  6085 | `				}` |
|      - |  6086 | `				/* Offsets, not pointers, until the pool has stopped growing. */` |
|     29 |  6087 | `				for( n = 0 ; n < SySetUsed(&sPrefixes.aOfs) ; ++n ){` |
|     25 |  6088 | `					sPrefixes.apPrefix[n] = (xmlChar *)SyBlobData(&sPrefixes.sPool)` |
|     16 |  6089 | `						+ ((sxu32 *)SySetBasePtr(&sPrefixes.aOfs))[n];` |
|      9 |  6090 | `				}` |
|     13 |  6091 | `				sPrefixes.apPrefix[n] = 0;` |
|      6 |  6092 | `			}` |
|      8 |  6093 | `		}else{` |
|      - |  6094 | `			/* php's E_NOTICE, and the list is then ignored outright. */` |
|      3 |  6095 | `			ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,` |
|      - |  6096 | `				"Inclusive namespace prefixes only allowed in exclusive mode.");` |
|      - |  6097 | `		}` |
|      8 |  6098 | `	}` |
|      - |  6099 | `	/* Canonicalized into an output buffer of our own rather than through` |
|      - |  6100 | `	 * xmlC14NDocDumpMemory, which is php's shape and one diagnostic quieter:` |
|      - |  6101 | `	 * that wrapper adds an "Internal error : saving doc to output buffer" of` |
|      - |  6102 | `	 * its own on top of libxml's real complaint, and php -- which drives the` |
|      - |  6103 | `	 * save itself -- never prints it. */` |
|      - |  6104 | `	{` |
|    107 |  6105 | `		xmlBufferPtr pBuf = xmlBufferCreate();` |
|    107 |  6106 | `		xmlOutputBufferPtr pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|    107 |  6107 | `		if( pOut == 0 ){` |
|    ! 0 |  6108 | `			if( pBuf ){` |
|    ! 0 |  6109 | `				xmlBufferFree(pBuf);` |
|    ! 0 |  6110 | `			}` |
|    ! 0 |  6111 | `			nOut = -1;` |
|    ! 0 |  6112 | `			goto done;` |
|      - |  6113 | `		}` |
|    107 |  6114 | `		nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    160 |  6115 | `		nOut = xmlC14NDocSaveTo(pNode->doc,pSet,` |
|     53 |  6116 | `			bExclusive ? XML_C14N_EXCLUSIVE_1_0 : XML_C14N_1_0,` |
|     53 |  6117 | `			sPrefixes.apPrefix,bComments,pOut);` |
|    107 |  6118 | `		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    107 |  6119 | `		xmlOutputBufferFlush(pOut);` |
|    107 |  6120 | `		if( nOut >= 0 ){` |
|    101 |  6121 | `			const xmlChar *zBuf = xmlBufferContent(pBuf);` |
|    101 |  6122 | `			nOut = (int)xmlBufferLength(pBuf);` |
|      - |  6123 | `			/* An EMPTY canonicalization is a real answer -- a detached node` |
|      - |  6124 | `			 * is visible from nowhere in the document -- so the bytes are` |
|      - |  6125 | `			 * always allocated, even when there are none. */` |
|    101 |  6126 | `			*pzOut = xmlStrndup(zBuf ? zBuf : (const xmlChar *)"",nOut);` |
|    101 |  6127 | `			if( *pzOut == 0 ){` |
|    ! 0 |  6128 | `				nOut = -1;` |
|    ! 0 |  6129 | `			}` |
|     50 |  6130 | `		}` |
|    107 |  6131 | `		xmlOutputBufferClose(pOut);` |
|    107 |  6132 | `		xmlBufferFree(pBuf);` |
|      - |  6133 | `	}` |
|    107 |  6134 | `	if( nOut < 0 && *pzOut ){` |
|    ! 0 |  6135 | `		xmlFree(*pzOut);` |
|    ! 0 |  6136 | `		*pzOut = 0;` |
|    ! 0 |  6137 | `	}` |
|     53 |  6138 | `done:` |
|    119 |  6139 | `	if( pXObj ){` |
|     63 |  6140 | `		xmlXPathFreeObject(pXObj);` |
|     31 |  6141 | `	}` |
|    119 |  6142 | `	if( pXCtx ){` |
|     65 |  6143 | `		xmlXPathFreeContext(pXCtx);` |
|     32 |  6144 | `	}` |
|    119 |  6145 | `	if( sPrefixes.apPrefix ){` |
|     13 |  6146 | `		SyMemBackendFree(&pVm->sAllocator,(void *)sPrefixes.apPrefix);` |
|      6 |  6147 | `	}` |
|    119 |  6148 | `	SyBlobRelease(&sPrefixes.sPool);` |
|    119 |  6149 | `	SySetRelease(&sPrefixes.aOfs);` |
|    119 |  6150 | `	return nOut;` |
|      1 |  6151 | `}` |
|      - |  6152 | `/*` |
|      - |  6153 | ` * DOMNode::C14N(bool $exclusive = false, bool $withComments = false,` |
|      - |  6154 | ` *               ?array $xpath = null, ?array $nsPrefixes = null): string\|false` |
|      - |  6155 | ` *` |
|      - |  6156 | ` * FALSE when the canonicalization fails, which is the answer a signer has to` |
|      - |  6157 | ` * be able to tell from a document that canonicalizes to nothing: a detached` |
|      - |  6158 | ` * node and a fragment are both the EMPTY STRING (nothing of either is visible` |
|      - |  6159 | ` * from the document, and that is a real answer), while an entity REFERENCE` |
|      - |  6160 | ` * anywhere in the tree -- an ordinary document parsed without` |
|      - |  6161 | `` * `substituteEntities` -- is a refusal libxml states and php reports as false.`` |
|      - |  6162 | ` * Answering "" for both signed the empty string instead of failing.` |
|      - |  6163 | ` */` |
|    100 |  6164 | `DOM_METHOD(vm_builtin_DOMNode_C14N)` |
|      1 |  6165 | `{` |
|    101 |  6166 | `	xmlChar *zOut = 0;` |
|    101 |  6167 | `	int rc = PH7_OK;` |
|    101 |  6168 | `	int nOut = DomC14NRun(pCtx,nArg,apArg,3,"DOMNode::C14N",&zOut,&rc);` |
|    101 |  6169 | `	if( rc != PH7_OK ){` |
|     11 |  6170 | `		return rc;` |
|      - |  6171 | `	}` |
|     91 |  6172 | `	if( nOut < 0 ){` |
|      5 |  6173 | `		ph7_result_bool(pCtx,0);` |
|      5 |  6174 | `		return PH7_OK;` |
|      - |  6175 | `	}` |
|     87 |  6176 | `	ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|     87 |  6177 | `	xmlFree(zOut);` |
|     87 |  6178 | `	return PH7_OK;` |
|     51 |  6179 | `}` |
|      - |  6180 | `/*` |
|      - |  6181 | ` * DOMNode::C14NFile(string $uri, bool $exclusive = false,` |
|      - |  6182 | ` *                   bool $withComments = false, ?array $xpath = null,` |
|      - |  6183 | ` *                   ?array $nsPrefixes = null): int\|false` |
|      - |  6184 | ` *` |
|      - |  6185 | ` * The same canonicalization written to a destination instead of answered, and` |
|      - |  6186 | ` * the byte count rather than the bytes. The destination goes through the stream` |
|      - |  6187 | `` * layer -- php's libxml I/O is wired to php's streams, so `php://stdout` and a`` |
|      - |  6188 | ` * userland wrapper are both valid here -- which is also where php's` |
|      - |  6189 | ` * "Failed to open stream" warning comes from.` |
|      - |  6190 | ` */` |
|     22 |  6191 | `DOM_METHOD(vm_builtin_DOMNode_C14NFile)` |
|      1 |  6192 | `{` |
|     23 |  6193 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  6194 | `	const ph7_io_stream *pStream;` |
|      - |  6195 | `	void *pHandle;` |
|     23 |  6196 | `	xmlChar *zOut = 0;` |
|      - |  6197 | `	const char *zFile;` |
|     23 |  6198 | `	int nFile = 0,nOut,rc = PH7_OK;` |
|     23 |  6199 | `	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";` |
|     23 |  6200 | `	if( nFile != (int)SyStrlen(zFile) ){` |
|      3 |  6201 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  6202 | `			"DOMNode::C14NFile(): Argument #1 ($uri) must not contain any null bytes");` |
|      - |  6203 | `	}` |
|     21 |  6204 | `	if( nFile < 1 ){` |
|      3 |  6205 | `		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");` |
|      - |  6206 | `	}` |
|     19 |  6207 | `	nOut = DomC14NRun(pCtx,nArg,apArg,4,"DOMNode::C14NFile",&zOut,&rc);` |
|     19 |  6208 | `	if( rc != PH7_OK ){` |
|      3 |  6209 | `		return rc;` |
|      - |  6210 | `	}` |
|     17 |  6211 | `	if( nOut < 0 ){` |
|      3 |  6212 | `		ph7_result_bool(pCtx,0);` |
|      3 |  6213 | `		return PH7_OK;` |
|      - |  6214 | `	}` |
|     15 |  6215 | `	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);` |
|     15 |  6216 | `	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,` |
|      - |  6217 | `		PH7_IO_OPEN_WRONLY\|PH7_IO_OPEN_CREATE\|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,` |
|     14 |  6218 | `		ph7_function_name(pCtx)) : 0;` |
|     15 |  6219 | `	if( pHandle == 0 ){` |
|      5 |  6220 | `		xmlFree(zOut);` |
|      5 |  6221 | `		VfsThrowOpenWarning(pCtx,zFile);` |
|      5 |  6222 | `		ph7_result_bool(pCtx,0);` |
|      5 |  6223 | `		return PH7_OK;` |
|      - |  6224 | `	}` |
|     11 |  6225 | `	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){` |
|    ! 0 |  6226 | `		nOut = -1;` |
|    ! 0 |  6227 | `	}` |
|     11 |  6228 | `	PH7_StreamCloseHandle(pStream,pHandle);` |
|     11 |  6229 | `	xmlFree(zOut);` |
|     11 |  6230 | `	if( nOut < 0 ){` |
|    ! 0 |  6231 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  6232 | `		return PH7_OK;` |
|      - |  6233 | `	}` |
|     11 |  6234 | `	ph7_result_int(pCtx,nOut);` |
|     11 |  6235 | `	return PH7_OK;` |
|     12 |  6236 | `}` |
|      - |  6237 | `/*` |
|      - |  6238 | ` * DOMNode::__sleep(): array and DOMNode::__wakeup(): void` |
|      - |  6239 | ` *` |
|      - |  6240 | ` * php declares both on DOMNode and both do one thing: refuse. They are the` |
|      - |  6241 | ``  * MECHANISM behind the refusal, not decoration -- `serialize()` finds `__sleep` `` |
|      - |  6242 | `` * and `unserialize()` calls `__wakeup`, which is why a subclass that declares`` |
|      - |  6243 | ` * its own escapes both. Without them, PHL refused serialize() from its own deny` |
|      - |  6244 | ` * handler (same sentence) but UNSERIALIZE went through in silence and handed` |
|      - |  6245 | ` * back a DOM object with no node behind it, which then answered nothing for` |
|      - |  6246 | ` * every property a program read off it.` |
|      - |  6247 | ` */` |
|     30 |  6248 | `DOM_METHOD(vm_builtin_DOMNode_sleep)` |
|      1 |  6249 | `{` |
|     31 |  6250 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     15 |  6251 | `	SXUNUSED(nArg);` |
|     15 |  6252 | `	SXUNUSED(apArg);` |
|     31 |  6253 | `	if( pThis == 0 ){` |
|    ! 0 |  6254 | `		return PH7_OK;` |
|      - |  6255 | `	}` |
|     46 |  6256 | `	return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  6257 | `		"Serialization of '%z' is not allowed, unless serialization methods "` |
|     30 |  6258 | `		"are implemented in a subclass",&pThis->pClass->sName);` |
|     16 |  6259 | `}` |
|     16 |  6260 | `DOM_METHOD(vm_builtin_DOMNode_wakeup)` |
|      1 |  6261 | `{` |
|     17 |  6262 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      8 |  6263 | `	SXUNUSED(nArg);` |
|      8 |  6264 | `	SXUNUSED(apArg);` |
|     17 |  6265 | `	if( pThis == 0 ){` |
|    ! 0 |  6266 | `		return PH7_OK;` |
|      - |  6267 | `	}` |
|     25 |  6268 | `	return PH7_VmThrowException(pCtx,"Exception",` |
|      - |  6269 | `		"Unserialization of '%z' is not allowed, unless unserialization methods "` |
|     16 |  6270 | `		"are implemented in a subclass",&pThis->pClass->sName);` |
|      9 |  6271 | `}` |
|      - |  6272 |  |
|      - |  6273 | `/* ===== DOMNodeList and DOMNamedNodeMap ===== */` |
|      - |  6274 |  |
|      - |  6275 | `/*` |
|      - |  6276 | ` * A node list is one of three things, and which one it is decides both count()` |
|      - |  6277 | ` * and item(). Two of the three are LIVE views (they re-walk the tree on every` |
|      - |  6278 | ` * question, which is what makes getElementsByTagName track mutations); the third` |
|      - |  6279 | ` * is the document-order snapshot DOMXPath::query froze.` |
|      - |  6280 | ` */` |
|      - |  6281 | `#define DNL_CHILD 0   /* $node->childNodes */` |
|      - |  6282 | `#define DNL_GEBTN 1   /* getElementsByTagName($name) */` |
|      - |  6283 | `#define DNL_SNAP  2   /* DOMXPath::query() */` |
|      - |  6284 | `#define DNL_GEBTNNS 3 /* getElementsByTagNameNS($uri, $localName) */` |
|      - |  6285 | `/* ...and a NAMED map is one of two: an element's attribute list, or one of the` |
|      - |  6286 | ` * two DTD declaration TABLES, which are libxml hash tables rather than node` |
|      - |  6287 | ` * lists -- the reason a parameter entity, which is a child of the DTD like` |
|      - |  6288 | `` * every other declaration, is not in `entities`. */`` |
|      - |  6289 | `#define DNL_ENTS  4   /* $doctype->entities */` |
|      - |  6290 | `#define DNL_NOTS  5   /* $doctype->notations */` |
|      - |  6291 | `#define DNL_KIND  "__kind"` |
|      - |  6292 | `#define DNL_OWNER "__owner"` |
|      - |  6293 | `#define DNL_NAME  "__name"` |
|      - |  6294 | `#define DNL_URI   "__uri"` |
|      - |  6295 | `#define DNL_SNAP_SLOT "__snap"` |
|      - |  6296 |  |
|      - |  6297 | `/* The node a live list is a view OF. */` |
|   1192 |  6298 | `static phl_domnode * DomListOwner(ph7_class_instance *pList)` |
|      4 |  6299 | `{` |
|   1196 |  6300 | `	return DomResOf(PH7_NativeAttrObj(pList,DNL_OWNER));` |
|      4 |  6301 | `}` |
|    176 |  6302 | `static ph7_hashmap * DomListSnap(ph7_class_instance *pList)` |
|      1 |  6303 | `{` |
|    177 |  6304 | `	ph7_value *pVal = PH7_NativeAttr(pList,DNL_SNAP_SLOT);` |
|    177 |  6305 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  6306 | `		return 0;` |
|      - |  6307 | `	}` |
|    177 |  6308 | `	return (ph7_hashmap *)pVal->x.pOther;` |
|     89 |  6309 | `}` |
|    274 |  6310 | `static int DomListCount(ph7_class_instance *pList)` |
|      2 |  6311 | `{` |
|      - |  6312 | `	phl_domnode *pOwner;` |
|    276 |  6313 | `	const char *zName,*zUri = 0;` |
|    276 |  6314 | `	int nName,nUri = -1,iCount = 0;` |
|    276 |  6315 | `	if( pList == 0 ){` |
|    ! 0 |  6316 | `		return 0;` |
|      - |  6317 | `	}` |
|    276 |  6318 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){` |
|     69 |  6319 | `		ph7_hashmap *pMap = DomListSnap(pList);` |
|     69 |  6320 | `		return pMap ? (int)pMap->nEntry : 0;` |
|      - |  6321 | `	}` |
|    208 |  6322 | `	pOwner = DomListOwner(pList);` |
|    208 |  6323 | `	if( pOwner == 0 ){` |
|      3 |  6324 | `		return 0;` |
|      - |  6325 | `	}` |
|    206 |  6326 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){` |
|    136 |  6327 | `		return DomChildCount((xmlNodePtr)pOwner->pNode,0);` |
|      - |  6328 | `	}` |
|     71 |  6329 | `	PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);` |
|     71 |  6330 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_GEBTNNS ){` |
|     49 |  6331 | `		PH7_NativeAttrStr(pList,DNL_URI,&zUri,&nUri);` |
|     24 |  6332 | `	}` |
|     71 |  6333 | `	DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,-1,&iCount);` |
|     71 |  6334 | `	return iCount;` |
|    139 |  6335 | `}` |
|      - |  6336 | `/* The wrapper at one index, or NULL past the end. BORROWED, like every wrap. */` |
|    718 |  6337 | `static ph7_class_instance * DomListItem(ph7_vm *pVm,ph7_class_instance *pList,int iIndex)` |
|      2 |  6338 | `{` |
|      - |  6339 | `	ph7_class_instance *pDoc;` |
|      - |  6340 | `	phl_domnode *pOwner;` |
|    720 |  6341 | `	xmlNodePtr pNode = 0;` |
|    720 |  6342 | `	const char *zName,*zUri = 0;` |
|    720 |  6343 | `	int nName,nUri = -1;` |
|    720 |  6344 | `	if( pList == 0 \|\| iIndex < 0 ){` |
|    ! 0 |  6345 | `		return 0;` |
|      - |  6346 | `	}` |
|    720 |  6347 | `	pDoc = PH7_NativeAttrObj(pList,DOM_DOC);` |
|    720 |  6348 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){` |
|    109 |  6349 | `		ph7_hashmap *pMap = DomListSnap(pList);` |
|    109 |  6350 | `		ph7_hashmap_node *pEntry = 0;` |
|      - |  6351 | `		ph7_value sKey,*pHit;` |
|      - |  6352 | `		phl_domnode *pRes;` |
|    109 |  6353 | `		if( pMap == 0 ){` |
|    ! 0 |  6354 | `			return 0;` |
|      - |  6355 | `		}` |
|    109 |  6356 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)iIndex);` |
|    109 |  6357 | `		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){` |
|     21 |  6358 | `			pEntry = 0;` |
|     10 |  6359 | `		}` |
|    109 |  6360 | `		PH7_MemObjRelease(&sKey);` |
|    109 |  6361 | `		pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;` |
|    109 |  6362 | `		if( pHit && (pHit->iFlags & MEMOBJ_OBJ) ){` |
|      - |  6363 | `			/* A namespace:: axis entry holds the DOMNameSpaceNode ITSELF (a` |
|      - |  6364 | `			 * fresh object per query, one object per list -- php's answer);` |
|      - |  6365 | `			 * the snapshot owns it, so this stays a borrow like DomWrap's. */` |
|     39 |  6366 | `			return (ph7_class_instance *)pHit->x.pOther;` |
|      - |  6367 | `		}` |
|     71 |  6368 | `		pRes = (pHit && (pHit->iFlags & MEMOBJ_RES)) ? (phl_domnode *)pHit->x.pOther : 0;` |
|     71 |  6369 | `		return pRes ? DomWrap(&(*pVm),pDoc,pRes->pShell,(xmlNodePtr)pRes->pNode) : 0;` |
|      - |  6370 | `	}` |
|    612 |  6371 | `	pOwner = DomListOwner(pList);` |
|    612 |  6372 | `	if( pOwner == 0 ){` |
|     13 |  6373 | `		return 0;` |
|      - |  6374 | `	}` |
|    600 |  6375 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){` |
|    332 |  6376 | `		pNode = DomChildAt((xmlNodePtr)pOwner->pNode,iIndex);` |
|    167 |  6377 | `	}else{` |
|    269 |  6378 | `		PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);` |
|    269 |  6379 | `		if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_GEBTNNS ){` |
|    147 |  6380 | `			PH7_NativeAttrStr(pList,DNL_URI,&zUri,&nUri);` |
|     73 |  6381 | `		}` |
|    269 |  6382 | `		pNode = DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,iIndex,0);` |
|      - |  6383 | `	}` |
|    600 |  6384 | `	return DomWrap(&(*pVm),pDoc,pOwner->pShell,pNode);` |
|    361 |  6385 | `}` |
|      - |  6386 | `/*` |
|      - |  6387 | ` * Build one. pOwnerObj is the node the live view is of (NULL for a snapshot),` |
|      - |  6388 | ` * pSnap the frozen list (NULL otherwise). The caller owns the reference.` |
|      - |  6389 | ` */` |
|    626 |  6390 | `static ph7_class_instance * DomNewCollection(ph7_vm *pVm,const char *zClass,` |
|      - |  6391 | `	ph7_class_instance *pDoc,int iKind,ph7_class_instance *pOwnerObj,` |
|      - |  6392 | `	const char *zName,const char *zUri,ph7_value *pSnap)` |
|      4 |  6393 | `{` |
|    630 |  6394 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|    630 |  6395 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|    630 |  6396 | `	if( pObj == 0 ){` |
|    ! 0 |  6397 | `		return 0;` |
|      - |  6398 | `	}` |
|    630 |  6399 | `	PH7_NativeSetAttrInt(&(*pVm),pObj,DNL_KIND,iKind);` |
|    630 |  6400 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|    630 |  6401 | `	if( pOwnerObj ){` |
|    536 |  6402 | `		PH7_NativeSetAttrObj(&(*pVm),pObj,DNL_OWNER,pOwnerObj);` |
|    266 |  6403 | `	}` |
|    630 |  6404 | `	if( zName ){` |
|    147 |  6405 | `		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_NAME,zName,(int)SyStrlen(zName));` |
|     73 |  6406 | `	}` |
|    630 |  6407 | `	if( zUri ){` |
|     45 |  6408 | `		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_URI,zUri,(int)SyStrlen(zUri));` |
|     22 |  6409 | `	}` |
|    630 |  6410 | `	if( pSnap ){` |
|     95 |  6411 | `		ph7_value *pSlot = PH7_NativeAttr(pObj,DNL_SNAP_SLOT);` |
|     95 |  6412 | `		if( pSlot ){` |
|     95 |  6413 | `			PH7_MemObjStore(pSnap,pSlot);` |
|     47 |  6414 | `		}` |
|     47 |  6415 | `	}` |
|    630 |  6416 | `	return pObj;` |
|    317 |  6417 | `}` |
|      - |  6418 | `/* DOMNodeList::count(): int and ::item(int $index): ?DOMNode */` |
|     10 |  6419 | `DOM_METHOD(vm_builtin_DOMNodeList_count)` |
|      1 |  6420 | `{` |
|      5 |  6421 | `	SXUNUSED(nArg);` |
|      5 |  6422 | `	SXUNUSED(apArg);` |
|     11 |  6423 | `	ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));` |
|     11 |  6424 | `	return PH7_OK;` |
|      1 |  6425 | `}` |
|      - |  6426 | `/*` |
|      - |  6427 | ` * The index both collections take, screened before it is narrowed.` |
|      - |  6428 | ` *` |
|      - |  6429 | `` * php's is a `int` position in a list that cannot hold more than INT_MAX`` |
|      - |  6430 | ` * entries, so everything outside [0, INT_MAX] is out of range -- and the two` |
|      - |  6431 | ` * classes then disagree about what to DO with one: the list answers null and` |
|      - |  6432 | ` * the named map raises a ValueError naming the bound.  Narrowing first was a` |
|      - |  6433 | ``  * silent wrong answer either way: `item(4294967296)` and `item(PHP_INT_MIN)` `` |
|      - |  6434 | ` * truncate to 0 and answered the FIRST node of the collection.` |
|      - |  6435 | ` *` |
|      - |  6436 | ` * Answers 1 when the index is usable.` |
|      - |  6437 | ` */` |
|      - |  6438 | `#define DOM_INDEX_MAX 2147483647` |
|    396 |  6439 | `static int DomCollectionIndex(int nArg,ph7_value **apArg,int *piIndex)` |
|      2 |  6440 | `{` |
|    398 |  6441 | `	ph7_int64 iWant = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|    398 |  6442 | `	*piIndex = 0;` |
|    398 |  6443 | `	if( iWant < 0 \|\| iWant > DOM_INDEX_MAX ){` |
|     23 |  6444 | `		return 0;` |
|      - |  6445 | `	}` |
|    376 |  6446 | `	*piIndex = (int)iWant;` |
|    376 |  6447 | `	return 1;` |
|    200 |  6448 | `}` |
|    312 |  6449 | `DOM_METHOD(vm_builtin_DOMNodeList_item)` |
|      1 |  6450 | `{` |
|      - |  6451 | `	int iIndex;` |
|    313 |  6452 | `	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){` |
|     13 |  6453 | `		ph7_result_null(pCtx);` |
|     13 |  6454 | `		return PH7_OK;` |
|      - |  6455 | `	}` |
|    301 |  6456 | `	return DomResultWrap(pCtx,DomListItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));` |
|    157 |  6457 | `}` |
|      - |  6458 | ``/* php exposes `length` on both collections as a virtual property; the`` |
|      - |  6459 | ` * property HOOK below turns each recognizer into php's read and has handlers. */` |
|    268 |  6460 | `static int DomListProp(ph7_context *pCtx,const char *zName)` |
|      2 |  6461 | `{` |
|    270 |  6462 | `	if( DomNameIs(zName,"length") ){` |
|    266 |  6463 | `		ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));` |
|    266 |  6464 | `		return 1;` |
|      - |  6465 | `	}` |
|      5 |  6466 | `	return 0;` |
|    136 |  6467 | `}` |
|      - |  6468 | `/*` |
|      - |  6469 | ` * DOMNamedNodeMap: an element's attributes, keyed by name.` |
|      - |  6470 | ` *` |
|      - |  6471 | ` * It shares DOMNodeList's slots (the owner element in $__owner) but walks the` |
|      - |  6472 | ` * attribute list rather than the child list, so it gets its own two readers.` |
|      - |  6473 | ` */` |
|      - |  6474 | `/* Is this map one of the DTD DECLARATION tables rather than an element's` |
|      - |  6475 | ` * attribute list? The two are walked with entirely different machinery. */` |
|    468 |  6476 | `static int DomMapIsTable(ph7_class_instance *pMap)` |
|      3 |  6477 | `{` |
|    471 |  6478 | `	sxi64 iKind = pMap ? PH7_NativeAttrInt(pMap,DNL_KIND) : (sxi64)DNL_CHILD;` |
|    471 |  6479 | `	return iKind == DNL_ENTS \|\| iKind == DNL_NOTS;` |
|      3 |  6480 | `}` |
|      - |  6481 | `/*` |
|      - |  6482 | ` * The table itself, which is NULL for a doctype that declares nothing of that` |
|      - |  6483 | ` * kind: libxml allocates the hash only when the first declaration arrives, so` |
|      - |  6484 | ` * an absent table is an EMPTY map and not an error.` |
|      - |  6485 | ` */` |
|     94 |  6486 | `static xmlHashTablePtr DomMapHash(ph7_class_instance *pMap,phl_domnode *pOwner)` |
|      2 |  6487 | `{` |
|     96 |  6488 | `	xmlDtdPtr pDtd = pOwner ? (xmlDtdPtr)pOwner->pNode : 0;` |
|     94 |  6489 | `	if( !DomMapIsTable(pMap) \|\| pDtd == 0` |
|     96 |  6490 | `	 \|\| (pDtd->type != XML_DTD_NODE && pDtd->type != XML_DOCUMENT_TYPE_NODE) ){` |
|    ! 0 |  6491 | `		return 0;` |
|      - |  6492 | `	}` |
|     96 |  6493 | `	return (xmlHashTablePtr)(PH7_NativeAttrInt(pMap,DNL_KIND) == DNL_NOTS` |
|     47 |  6494 | `		? pDtd->notations : pDtd->entities);` |
|     49 |  6495 | `}` |
|      - |  6496 | `/*` |
|      - |  6497 | ` * A NOTATION declaration answered as a node.` |
|      - |  6498 | ` *` |
|      - |  6499 | `` * libxml's xmlNotation is `{name, PublicID, SystemID}` -- three strings and no`` |
|      - |  6500 | ` * type field -- so it cannot be handed to anything that walks a node.  php` |
|      - |  6501 | ` * builds an entity-shaped stand-in around it (the two structs share their` |
|      - |  6502 | ` * header, which is why the same reader answers both) with NO document and NO` |
|      - |  6503 | `` * parent, and that absence is php-visible: a notation's `ownerDocument` is`` |
|      - |  6504 | `` * null, its `isConnected` false and its `getRootNode()` itself.`` |
|      - |  6505 | ` *` |
|      - |  6506 | ` * php builds a FRESH one per lookup and frees it with the object; PHL builds` |
|      - |  6507 | ` * one per declaration and keeps it on the document's shell, so the wrapper` |
|      - |  6508 | `` * identity every other node has holds here too (PLAN §7.4: `$map->item(0) ===`` |
|      - |  6509 | `` * $map->item(0)` is true here and false there).`` |
|      - |  6510 | ` */` |
|     34 |  6511 | `static xmlNodePtr DomNotationNode(phl_xmldoc *pShell,xmlNotationPtr pNot)` |
|      2 |  6512 | `{` |
|      - |  6513 | `	xmlEntityPtr *apHave;` |
|      - |  6514 | `	xmlEntityPtr pNode;` |
|      - |  6515 | `	sxu32 n;` |
|     36 |  6516 | `	if( pShell == 0 \|\| pNot == 0 ){` |
|    ! 0 |  6517 | `		return 0;` |
|      - |  6518 | `	}` |
|     36 |  6519 | `	apHave = (xmlEntityPtr *)SySetBasePtr(&pShell->aNotations);` |
|     52 |  6520 | `	for( n = 0 ; n < SySetUsed(&pShell->aNotations) ; ++n ){` |
|     42 |  6521 | `		if( apHave[n]->_private == (void *)pNot ){` |
|     26 |  6522 | `			return (xmlNodePtr)apHave[n];` |
|      - |  6523 | `		}` |
|      7 |  6524 | `	}` |
|     12 |  6525 | `	pNode = (xmlEntityPtr)xmlMalloc(sizeof(xmlEntity));` |
|     12 |  6526 | `	if( pNode == 0 ){` |
|    ! 0 |  6527 | `		return 0;` |
|      - |  6528 | `	}` |
|     12 |  6529 | `	SyZero(pNode,sizeof(xmlEntity));` |
|     12 |  6530 | `	pNode->type = XML_NOTATION_NODE;` |
|     12 |  6531 | `	pNode->name = xmlStrdup(pNot->name);` |
|     12 |  6532 | `	pNode->ExternalID = xmlStrdup(pNot->PublicID);` |
|     12 |  6533 | `	pNode->SystemID = xmlStrdup(pNot->SystemID);` |
|      - |  6534 | `	/* The declaration this stands for, so a second lookup finds it again. */` |
|     12 |  6535 | `	pNode->_private = (void *)pNot;` |
|     12 |  6536 | `	if( SySetPut(&pShell->aNotations,(const void *)&pNode) != SXRET_OK ){` |
|    ! 0 |  6537 | `		if( pNode->name ){` |
|    ! 0 |  6538 | `			xmlFree((xmlChar *)pNode->name);` |
|    ! 0 |  6539 | `		}` |
|    ! 0 |  6540 | `		if( pNode->ExternalID ){` |
|    ! 0 |  6541 | `			xmlFree((xmlChar *)pNode->ExternalID);` |
|    ! 0 |  6542 | `		}` |
|    ! 0 |  6543 | `		if( pNode->SystemID ){` |
|    ! 0 |  6544 | `			xmlFree((xmlChar *)pNode->SystemID);` |
|    ! 0 |  6545 | `		}` |
|    ! 0 |  6546 | `		xmlFree(pNode);` |
|    ! 0 |  6547 | `		return 0;` |
|      - |  6548 | `	}` |
|     12 |  6549 | `	return (xmlNodePtr)pNode;` |
|     19 |  6550 | `}` |
|      - |  6551 | `/* The payload a table map hands back, as a node: an entity declaration IS one,` |
|      - |  6552 | ` * a notation declaration needs its stand-in. */` |
|     82 |  6553 | `static xmlNodePtr DomTablePayload(ph7_class_instance *pMap,phl_domnode *pOwner,void *pPayload)` |
|      2 |  6554 | `{` |
|     84 |  6555 | `	if( pPayload == 0 ){` |
|     15 |  6556 | `		return 0;` |
|      - |  6557 | `	}` |
|     70 |  6558 | `	if( PH7_NativeAttrInt(pMap,DNL_KIND) == DNL_NOTS ){` |
|     36 |  6559 | `		return DomNotationNode(pOwner->pShell,(xmlNotationPtr)pPayload);` |
|      - |  6560 | `	}` |
|     35 |  6561 | `	return (xmlNodePtr)pPayload;` |
|     43 |  6562 | `}` |
|      - |  6563 | `/*` |
|      - |  6564 | ` * php walks these tables with xmlHashScan and takes the n-th thing it is` |
|      - |  6565 | ` * handed, so the ORDER a map answers in is the hash's and not the document's.` |
|      - |  6566 | ` * The same walk, so the same order.` |
|      - |  6567 | ` */` |
|      - |  6568 | `typedef struct DomHashPick DomHashPick;` |
|      - |  6569 | `struct DomHashPick {` |
|      - |  6570 | `	int iWant;              /* index still to be stepped over */` |
|      - |  6571 | `	void *pHit;             /* the payload at index 0 of what is left */` |
|      - |  6572 | `};` |
|     90 |  6573 | `static void DomHashPickOne(void *pPayload,void *pData,const xmlChar *zName)` |
|      2 |  6574 | `{` |
|     92 |  6575 | `	DomHashPick *pPick = (DomHashPick *)pData;` |
|     45 |  6576 | `	SXUNUSED(zName);` |
|     92 |  6577 | `	if( pPick->iWant > 0 ){` |
|     19 |  6578 | `		pPick->iWant--;` |
|     83 |  6579 | `	}else if( pPick->pHit == 0 ){` |
|     34 |  6580 | `		pPick->pHit = pPayload;` |
|     16 |  6581 | `	}` |
|     92 |  6582 | `}` |
|     40 |  6583 | `static void * DomHashAt(xmlHashTablePtr pTab,int iIndex)` |
|      2 |  6584 | `{` |
|      - |  6585 | `	DomHashPick sPick;` |
|     42 |  6586 | `	if( pTab == 0 \|\| iIndex < 0 \|\| iIndex >= xmlHashSize(pTab) ){` |
|      9 |  6587 | `		return 0;` |
|      - |  6588 | `	}` |
|     34 |  6589 | `	sPick.iWant = iIndex;` |
|     34 |  6590 | `	sPick.pHit = 0;` |
|     34 |  6591 | `	xmlHashScan(pTab,DomHashPickOne,&sPick);` |
|     34 |  6592 | `	return sPick.pHit;` |
|     22 |  6593 | `}` |
|    250 |  6594 | `static ph7_class_instance * DomMapItem(ph7_vm *pVm,ph7_class_instance *pMap,int iIndex)` |
|      3 |  6595 | `{` |
|    253 |  6596 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|      - |  6597 | `	xmlNodePtr pNode;` |
|    253 |  6598 | `	if( pOwner == 0 \|\| iIndex < 0 ){` |
|      3 |  6599 | `		return 0;` |
|      - |  6600 | `	}` |
|    251 |  6601 | `	pNode = DomMapIsTable(pMap)` |
|     40 |  6602 | `		? DomTablePayload(pMap,pOwner,DomHashAt(DomMapHash(pMap,pOwner),iIndex))` |
|    228 |  6603 | `		: (xmlNodePtr)DomAttrAt((xmlNodePtr)pOwner->pNode,iIndex);` |
|    251 |  6604 | `	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,pNode);` |
|    128 |  6605 | `}` |
|     28 |  6606 | `static int DomMapCount(ph7_class_instance *pMap)` |
|      1 |  6607 | `{` |
|     29 |  6608 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|     29 |  6609 | `	if( DomMapIsTable(pMap) ){` |
|     13 |  6610 | `		xmlHashTablePtr pTab = DomMapHash(pMap,pOwner);` |
|     13 |  6611 | `		return pTab ? xmlHashSize(pTab) : 0;` |
|      - |  6612 | `	}` |
|     17 |  6613 | `	return pOwner ? DomAttrCount((xmlNodePtr)pOwner->pNode) : 0;` |
|     15 |  6614 | `}` |
|      6 |  6615 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_count)` |
|      1 |  6616 | `{` |
|      3 |  6617 | `	SXUNUSED(nArg);` |
|      3 |  6618 | `	SXUNUSED(apArg);` |
|      7 |  6619 | `	ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));` |
|      7 |  6620 | `	return PH7_OK;` |
|      1 |  6621 | `}` |
|     84 |  6622 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_item)` |
|      2 |  6623 | `{` |
|      - |  6624 | `	int iIndex;` |
|     86 |  6625 | `	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){` |
|      - |  6626 | `		/* The map REFUSES what the list answers null for, and names the bound. */` |
|     11 |  6627 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  6628 | `			"DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and %d",` |
|      - |  6629 | `			DOM_INDEX_MAX);` |
|      - |  6630 | `	}` |
|     76 |  6631 | `	return DomResultWrap(pCtx,DomMapItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));` |
|     44 |  6632 | `}` |
|      - |  6633 | `/*` |
|      - |  6634 | `` * The by-NAME lookup, which `getNamedItem()` and the `$map['href']` subscript`` |
|      - |  6635 | ` * share.` |
|      - |  6636 | ` *` |
|      - |  6637 | ` * The MAP asks libxml's name-only question, where DOMElement's own` |
|      - |  6638 | `` * getAttributeNode resolves the prefix: `getNamedItem('k')` finds the`` |
|      - |  6639 | `` * namespaced `p:k` that `getAttribute('k')` does not. A DECLARATION table is`` |
|      - |  6640 | ` * keyed by that name to begin with, so it is one lookup.` |
|      - |  6641 | ` */` |
|     82 |  6642 | `static ph7_class_instance * DomMapNamed(ph7_vm *pVm,ph7_class_instance *pMap,const char *zName)` |
|      2 |  6643 | `{` |
|     84 |  6644 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|      - |  6645 | `	xmlNodePtr pHit;` |
|     84 |  6646 | `	if( pOwner == 0 ){` |
|    ! 0 |  6647 | `		return 0;` |
|      - |  6648 | `	}` |
|     84 |  6649 | `	pHit = DomMapIsTable(pMap)` |
|     54 |  6650 | `		? DomTablePayload(pMap,pOwner,` |
|     18 |  6651 | `			xmlHashLookup(DomMapHash(pMap,pOwner),(const xmlChar *)zName))` |
|     64 |  6652 | `		: (xmlNodePtr)DomAttrByLocal((xmlNodePtr)pOwner->pNode,zName);` |
|     84 |  6653 | `	if( pHit == 0 ){` |
|     29 |  6654 | `		return 0;` |
|      - |  6655 | `	}` |
|     56 |  6656 | `	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,pHit);` |
|     43 |  6657 | `}` |
|      - |  6658 | `/* DOMNamedNodeMap::getNamedItem(string $qualifiedName): ?DOMAttr */` |
|     42 |  6659 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItem)` |
|      2 |  6660 | `{` |
|     44 |  6661 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     44 |  6662 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     44 |  6663 | `	return DomResultWrap(pCtx,DomMapNamed(pCtx->pVm,pThis,zName));` |
|      2 |  6664 | `}` |
|      - |  6665 | `/* DOMNamedNodeMap::getNamedItemNS(?string $namespace, string $localName): ?DOMNode */` |
|     16 |  6666 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItemNS)` |
|      1 |  6667 | `{` |
|     17 |  6668 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     17 |  6669 | `	phl_domnode *pOwner = pThis ? DomListOwner(pThis) : 0;` |
|     17 |  6670 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     17 |  6671 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|      - |  6672 | `	/* A NULL namespace is the map's ANY here, not the element's "in no` |
|      - |  6673 | ``	 * namespace": `$el->attributes->getNamedItemNS(null,'k')` answers a`` |
|      - |  6674 | ``	 * namespaced `p:k` where `$el->getAttributeNodeNS(null,'k')` answers null.`` |
|      - |  6675 | `	 * An EMPTY namespace is neither -- it matches a URI no document has.` |
|      - |  6676 | `	 * A DECLARATION table has no namespaces at all and php reads right past` |
|      - |  6677 | `	 * the argument there: the URI decides nothing, the name decides` |
|      - |  6678 | `	 * everything. */` |
|     25 |  6679 | `	xmlNodePtr pHit = pOwner == 0 ? 0` |
|     29 |  6680 | `		: DomMapIsTable(pThis)` |
|      9 |  6681 | `			? DomTablePayload(pThis,pOwner,` |
|      3 |  6682 | `				xmlHashLookup(DomMapHash(pThis,pOwner),(const xmlChar *)zLocal))` |
|     18 |  6683 | `		: zUri ? (xmlNodePtr)DomAttrByNs((xmlNodePtr)pOwner->pNode,zUri,zLocal)` |
|      6 |  6684 | `		       : (xmlNodePtr)DomAttrByLocal((xmlNodePtr)pOwner->pNode,zLocal);` |
|     17 |  6685 | `	if( pHit == 0 ){` |
|      7 |  6686 | `		ph7_result_null(pCtx);` |
|      7 |  6687 | `		return PH7_OK;` |
|      - |  6688 | `	}` |
|     16 |  6689 | `	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,PH7_NativeAttrObj(pThis,DOM_DOC),` |
|      5 |  6690 | `		pOwner->pShell,pHit));` |
|      9 |  6691 | `}` |
|     26 |  6692 | `static int DomMapProp(ph7_context *pCtx,const char *zName)` |
|      1 |  6693 | `{` |
|     27 |  6694 | `	if( DomNameIs(zName,"length") ){` |
|     23 |  6695 | `		ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));` |
|     23 |  6696 | `		return 1;` |
|      - |  6697 | `	}` |
|      5 |  6698 | `	return 0;` |
|     14 |  6699 | `}` |
|      - |  6700 | `/*` |
|      - |  6701 | `` * `$list[0]`, `$map['href']` and `isset($list[0])` -- ph7_class::xDim for the`` |
|      - |  6702 | ` * two collections, which are php's read_dimension / has_dimension handlers.` |
|      - |  6703 | ` *` |
|      - |  6704 | ` * php 8.3 gave both classes those handlers WITHOUT declaring ArrayAccess, so` |
|      - |  6705 | `` * `$list instanceof ArrayAccess` is FALSE there and the subscript reads anyway`` |
|      - |  6706 | ` * -- which is how every modern DOM example is written, and which no interface` |
|      - |  6707 | ` * list can state. Only the READ half exists: a store, an append and an unset` |
|      - |  6708 | `` * are all `Cannot use object of type C as array`, which is what the opcode`` |
|      - |  6709 | ` * answers for a class carrying no ArrayAccess.` |
|      - |  6710 | ` *` |
|      - |  6711 | ` * What an offset MEANS is one rule for both classes, and it is not the array` |
|      - |  6712 | `` * one. A STRING that STARTS with a number is an INDEX -- `"1x"` is 1 and`` |
|      - |  6713 | `` * `" 2 "` is 2, silently, php's is_numeric_string with errors allowed -- and`` |
|      - |  6714 | ` * one that does not is a NAME, which the list has no door for at all (so` |
|      - |  6715 | `` * `$list['x']` is null even on a document whose child element is named x).`` |
|      - |  6716 | ` * Every other offset type takes the ordinary int cast, warning exactly where` |
|      - |  6717 | `` * php's `(int)` warns (`$map[new stdClass]` is index 1 and a warning).`` |
|      - |  6718 | ` *` |
|      - |  6719 | ` * Then the two classes DISAGREE about an index outside [0, INT_MAX]: the list` |
|      - |  6720 | `` * answers null and the map raises `item()`'s own ValueError -- worded the way`` |
|      - |  6721 | ` * php words it with no function frame active to name the argument, so the` |
|      - |  6722 | `` * `DOMNamedNodeMap::item(): Argument #1 ($index) ` head is not there. isset()`` |
|      - |  6723 | ` * never refuses: php asks has_dimension, and that one answers a plain false.` |
|      - |  6724 | ` */` |
|      - |  6725 | `/*` |
|      - |  6726 | ` * Classify one offset. Answers 1 for a NAME (pScratch holds the string), 0 for` |
|      - |  6727 | ` * an INDEX in *piIndex. Works on a COPY: the conversion is destructive, the` |
|      - |  6728 | ` * caller's offset must survive it (empty() asks the same offset twice), and` |
|      - |  6729 | `` * php's own `$map[$k]` leaves $k alone. The caller releases pScratch.`` |
|      - |  6730 | ` */` |
|    308 |  6731 | `static int DomDimClassify(ph7_vm *pVm,ph7_value *pOffset,ph7_value *pScratch,sxi64 *piIndex)` |
|      2 |  6732 | `{` |
|    310 |  6733 | `	PH7_MemObjInit(&(*pVm),pScratch);` |
|    310 |  6734 | `	PH7_MemObjStore(pOffset,pScratch);` |
|    310 |  6735 | `	*piIndex = 0;` |
|    310 |  6736 | `	if( (pScratch->iFlags & MEMOBJ_STRING) && !PH7_MemObjStringNumericPrefix(pScratch,0) ){` |
|     69 |  6737 | `		return 1; /* a NAME: the caller reads the bytes out of the copy */` |
|      - |  6738 | `	}` |
|      - |  6739 | `	/* php reads an int out of every other offset type -- null is 0, a bool its` |
|      - |  6740 | `	 * value, a float truncated, an array 0 or 1 by emptiness, an object 1 behind` |
|      - |  6741 | ``	 * `could not be converted to int`. The cast is what hands back the reference`` |
|      - |  6742 | `	 * an object / array copy took (MemObjIntValue unrefs before it overwrites the` |
|      - |  6743 | `	 * type in place), so the release below has only a string blob left to free. */` |
|    242 |  6744 | `	PH7_MemObjWarnIntCast(pScratch);` |
|    242 |  6745 | `	*piIndex = ph7_value_to_int64(pScratch);` |
|    242 |  6746 | `	return 0;` |
|    156 |  6747 | `}` |
|      - |  6748 | `/* Hand the hook's answer back: the node, or nothing at all for a miss (which` |
|      - |  6749 | ` * the caller initialized NULL, and which isset() reads as false). */` |
|    218 |  6750 | `static void DomDimAnswer(ph7_vm *pVm,PH7_NativeDimCtx *pCtx,ph7_class_instance *pHit)` |
|      2 |  6751 | `{` |
|      - |  6752 | `	ph7_value sVal;` |
|    220 |  6753 | `	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){` |
|     27 |  6754 | `		pCtx->pResult->x.iVal = pHit ? 1 : 0;` |
|     27 |  6755 | `		MemObjSetType(pCtx->pResult,MEMOBJ_BOOL);` |
|     50 |  6756 | `		return;` |
|      - |  6757 | `	}` |
|    194 |  6758 | `	if( pHit == 0 ){` |
|     47 |  6759 | `		return;` |
|      - |  6760 | `	}` |
|    148 |  6761 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|    148 |  6762 | `	sVal.x.pOther = pHit;      /* borrowed, like every DomWrap answer */` |
|    148 |  6763 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    148 |  6764 | `	PH7_MemObjStore(&sVal,pCtx->pResult);   /* takes its own reference */` |
|    111 |  6765 | `}` |
|      - |  6766 | ``/* php's refusal for the keyless `$list[]` spelling, which reaches the handler`` |
|      - |  6767 | ` * with no offset at all. */` |
|      4 |  6768 | `static void DomDimNoOffset(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|      1 |  6769 | `{` |
|      5 |  6770 | `	SyString *pName = &pThis->pClass->sName;` |
|      5 |  6771 | `	pCtx->zThrowClass = "Error";` |
|      7 |  6772 | `	SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      4 |  6773 | `		"Cannot access %.*s without offset",(int)pName->nByte,pName->zString);` |
|      5 |  6774 | `}` |
|    192 |  6775 | `static void DomListDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|      2 |  6776 | `{` |
|      - |  6777 | `	ph7_value sKey;` |
|      - |  6778 | `	sxi64 iIndex;` |
|      - |  6779 | `	int bNamed;` |
|    194 |  6780 | `	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){` |
|      - |  6781 | `		/* The WRITE modes only ask for a refusal WORDING, and these two classes` |
|      - |  6782 | ``		 * have none of their own: php answers `Cannot use object of type C as`` |
|      - |  6783 | ``		 * array` for a store, an append and an unset, which is what the caller`` |
|      - |  6784 | `		 * formats when the hook declines. */` |
|     57 |  6785 | `		return;` |
|      - |  6786 | `	}` |
|    168 |  6787 | `	if( pCtx->pOffset == 0 ){` |
|      5 |  6788 | `		DomDimNoOffset(pThis,pCtx);` |
|      5 |  6789 | `		return;` |
|      - |  6790 | `	}` |
|    164 |  6791 | `	bNamed = DomDimClassify(&(*pVm),pCtx->pOffset,&sKey,&iIndex);` |
|    164 |  6792 | `	PH7_MemObjRelease(&sKey);` |
|    164 |  6793 | `	if( bNamed \|\| iIndex < 0 \|\| iIndex > DOM_INDEX_MAX ){` |
|     58 |  6794 | `		return; /* a name the list cannot answer, or an index it answers null to */` |
|      - |  6795 | `	}` |
|    108 |  6796 | `	DomDimAnswer(&(*pVm),pCtx,DomListItem(&(*pVm),pThis,(int)iIndex));` |
|     98 |  6797 | `}` |
|    158 |  6798 | `static void DomMapDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)` |
|      2 |  6799 | `{` |
|      - |  6800 | `	ph7_value sKey;` |
|      - |  6801 | `	sxi64 iIndex;` |
|      - |  6802 | `	int bNamed;` |
|    160 |  6803 | `	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){` |
|     50 |  6804 | `		return;   /* see DomListDim: the write side is php's generic sentence */` |
|      - |  6805 | `	}` |
|    148 |  6806 | `	if( pCtx->pOffset == 0 ){` |
|    ! 0 |  6807 | `		DomDimNoOffset(pThis,pCtx);` |
|    ! 0 |  6808 | `		return;` |
|      - |  6809 | `	}` |
|    148 |  6810 | `	bNamed = DomDimClassify(&(*pVm),pCtx->pOffset,&sKey,&iIndex);` |
|    148 |  6811 | `	if( bNamed ){` |
|     41 |  6812 | `		DomDimAnswer(&(*pVm),pCtx,DomMapNamed(&(*pVm),pThis,ph7_value_to_string(&sKey,0)));` |
|     41 |  6813 | `		PH7_MemObjRelease(&sKey);` |
|     41 |  6814 | `		return;` |
|      - |  6815 | `	}` |
|    108 |  6816 | `	PH7_MemObjRelease(&sKey);` |
|    108 |  6817 | `	if( iIndex < 0 \|\| iIndex > DOM_INDEX_MAX ){` |
|      - |  6818 | `		/* The range is screened BEFORE the map is looked at, so a map with no` |
|      - |  6819 | ``		 * owner at all -- `(new DOMNamedNodeMap())[-1]` -- refuses too. */`` |
|     36 |  6820 | `		if( pCtx->iMode == PH7_NATIVE_DIM_READ ){` |
|     28 |  6821 | `			pCtx->zThrowClass = "ValueError";` |
|     28 |  6822 | `			SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),` |
|      - |  6823 | `				"must be between 0 and %d",DOM_INDEX_MAX);` |
|     13 |  6824 | `		}` |
|     36 |  6825 | `		return;` |
|      - |  6826 | `	}` |
|     74 |  6827 | `	DomDimAnswer(&(*pVm),pCtx,DomMapItem(&(*pVm),pThis,(int)iIndex));` |
|     81 |  6828 | `}` |
|      - |  6829 | `/*` |
|      - |  6830 | ` * Both collections are IteratorAggregates, as php's are -- the chunk made` |
|      - |  6831 | ``  * DOMNodeList an `Iterator` with its own cursor (so `$list instanceof Iterator` `` |
|      - |  6832 | ` * was true where php says false) and gave DOMNamedNodeMap no iteration at all,` |
|      - |  6833 | `` * which meant `foreach ($el->attributes as $a)` walked the map's own private`` |
|      - |  6834 | ` * slots instead of the attributes.` |
|      - |  6835 | ` *` |
|      - |  6836 | ` * The cursor lives in the shared InternalIterator (oo_native.c); a vtable states` |
|      - |  6837 | ` * only how to REACH a position. DOMNodeList keys by index, DOMNamedNodeMap by` |
|      - |  6838 | ` * attribute name, which is what php answers for each.` |
|      - |  6839 | ` */` |
|    416 |  6840 | `static void DomIterSettle(ph7_vm *pVm,ph7_class_instance *pIt,int bNamed)` |
|      1 |  6841 | `{` |
|    417 |  6842 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|    417 |  6843 | `	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);` |
|      - |  6844 | `	ph7_class_instance *pCur;` |
|    417 |  6845 | `	pCur = bNamed ? DomMapItem(&(*pVm),pSrc,(int)iPos) : DomListItem(&(*pVm),pSrc,(int)iPos);` |
|    417 |  6846 | `	if( pCur == 0 ){` |
|    111 |  6847 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|    111 |  6848 | `		return;` |
|      - |  6849 | `	}` |
|    307 |  6850 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pCur);  /* borrowed: no unref */` |
|    307 |  6851 | `	if( bNamed ){` |
|     77 |  6852 | `		phl_domnode *pNd = DomResOf(pCur);` |
|     77 |  6853 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     77 |  6854 | `		const char *zKey = (pNode && pNode->name) ? (const char *)pNode->name : "";` |
|     77 |  6855 | `		PH7_NativeSetAttrStr(&(*pVm),pIt,PH7_NATIVE_IT_KEY,zKey,(int)SyStrlen(zKey));` |
|     39 |  6856 | `	}else{` |
|    231 |  6857 | `		PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);` |
|      - |  6858 | `	}` |
|    307 |  6859 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|    209 |  6860 | `}` |
|    146 |  6861 | `static void DomListRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 |  6862 | `{` |
|    147 |  6863 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    147 |  6864 | `	DomIterSettle(&(*pVm),pIt,0);` |
|    147 |  6865 | `}` |
|    166 |  6866 | `static void DomListNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 |  6867 | `{` |
|    250 |  6868 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    166 |  6869 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    167 |  6870 | `	DomIterSettle(&(*pVm),pIt,0);` |
|    167 |  6871 | `}` |
|     58 |  6872 | `static void DomMapRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 |  6873 | `{` |
|     59 |  6874 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|     59 |  6875 | `	DomIterSettle(&(*pVm),pIt,1);` |
|     59 |  6876 | `}` |
|     46 |  6877 | `static void DomMapNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|      1 |  6878 | `{` |
|     70 |  6879 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|     46 |  6880 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|     47 |  6881 | `	DomIterSettle(&(*pVm),pIt,1);` |
|     47 |  6882 | `}` |
|      - |  6883 | `static const PH7_NativeIterVtab sDomListIterVtab = { DomListRewind, DomListNext, 0, 0 };` |
|      - |  6884 | `static const PH7_NativeIterVtab sDomMapIterVtab  = { DomMapRewind,  DomMapNext, 0, 0 };` |
|      - |  6885 | `/* Both getIterator()s: a fresh InternalIterator per call, as php's are. */` |
|    106 |  6886 | `DOM_METHOD(vm_builtin_Dom_getIterator)` |
|      1 |  6887 | `{` |
|    107 |  6888 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6889 | `	ph7_class_instance *pIt;` |
|     53 |  6890 | `	SXUNUSED(nArg);` |
|     53 |  6891 | `	SXUNUSED(apArg);` |
|    107 |  6892 | `	if( pThis == 0 ){` |
|    ! 0 |  6893 | `		return PH7_OK;` |
|      - |  6894 | `	}` |
|    107 |  6895 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|    107 |  6896 | `	if( pIt == 0 ){` |
|    ! 0 |  6897 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6898 | `	}` |
|    107 |  6899 | `	PH7_NativeResultObject(pCtx,pIt);` |
|    107 |  6900 | `	return PH7_OK;` |
|     54 |  6901 | `}` |
|      - |  6902 |  |
|      - |  6903 | `/* ===== DOMXPath ===== */` |
|      - |  6904 |  |
|      - |  6905 | `/* The prefix => URI table registerNamespace() feeds, replayed onto the fresh` |
|      - |  6906 | ` * evaluation context each query. Hidden slot, materialized by DomXPathSlotMap` |
|      - |  6907 | ` * below with the same three moves the document's identity cache needs. */` |
|      - |  6908 | `#define XP_NSREG "__nsreg"` |
|      - |  6909 | ``/* php declares `document` and `registerNodeNamespaces` VIRTUAL -- its object holds`` |
|      - |  6910 | ` * neither, and both are read_property handlers -- so the two values live in hidden` |
|      - |  6911 | ` * slots and the class declares the php-visible names with no storage at all. */` |
|      - |  6912 | `#define XP_DOC   "__xdoc"` |
|      - |  6913 | `#define XP_NSDEF "__xnsdef"` |
|      - |  6914 | `/*` |
|      - |  6915 | ` * DOMXPath::quote(string $str): string  (static, php 8.4)` |
|      - |  6916 | ` *` |
|      - |  6917 | ` * XPath 1.0 has no escape inside a string literal, so a value is quotable` |
|      - |  6918 | `` * only with the quote character it does not contain: no `'` and it goes in`` |
|      - |  6919 | `` * single quotes, no `"` in double ones, and a value carrying BOTH becomes a`` |
|      - |  6920 | `` * `concat()` of runs, split by the rule stated at the loop below. php's`` |
|      - |  6921 | ` * algorithm exactly, and its output byte for byte.` |
|      - |  6922 | ` */` |
|     38 |  6923 | `DOM_METHOD(vm_builtin_DOMXPath_quote)` |
|      1 |  6924 | `{` |
|     39 |  6925 | `	int nStr = 0;` |
|     39 |  6926 | `	const char *zStr = nArg > 0 ? ph7_value_to_string(apArg[0],&nStr) : "";` |
|     39 |  6927 | `	int bSq = 0,bDq = 0,i;` |
|      - |  6928 | `	SyBlob sOut;` |
|    225 |  6929 | `	for( i = 0 ; i < nStr ; ++i ){` |
|    187 |  6930 | `		if( zStr[i] == '\'' ){` |
|     29 |  6931 | `			bSq = 1;` |
|    173 |  6932 | `		}else if( zStr[i] == '"' ){` |
|     27 |  6933 | `			bDq = 1;` |
|     13 |  6934 | `		}` |
|     94 |  6935 | `	}` |
|     39 |  6936 | `	if( !bSq ){` |
|     17 |  6937 | `		ph7_result_string_format(pCtx,"'%.*s'",nStr,zStr);` |
|     17 |  6938 | `		return PH7_OK;` |
|      - |  6939 | `	}` |
|     23 |  6940 | `	if( !bDq ){` |
|      9 |  6941 | `		ph7_result_string_format(pCtx,"\"%.*s\"",nStr,zStr);` |
|      9 |  6942 | `		return PH7_OK;` |
|      - |  6943 | `	}` |
|     15 |  6944 | `	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     15 |  6945 | `	SyBlobAppend(&sOut,"concat(",sizeof("concat(")-1);` |
|     15 |  6946 | `	i = 0;` |
|     49 |  6947 | `	while( i < nStr ){` |
|      - |  6948 | `		/* Whichever quote kind appears FIRST in what is left decides the run:` |
|      - |  6949 | `		 * the run is wrapped in the OTHER kind and reaches to that first` |
|      - |  6950 | `		 * occurrence, so it swallows every quote of the kind it is not wrapped` |
|      - |  6951 | ``		 * in. `a'b"c'd"e` is four runs that way, and `0"&'<` is two -- the`` |
|      - |  6952 | `		 * first single-quoted, because its first quote character is the double` |
|      - |  6953 | `		 * one. */` |
|     35 |  6954 | `		int iStart = i,j;` |
|     35 |  6955 | `		char cQuote = '"';` |
|     57 |  6956 | `		for( j = i ; j < nStr ; ++j ){` |
|     57 |  6957 | `			if( zStr[j] == '\'' \|\| zStr[j] == '"' ){` |
|     35 |  6958 | `				cQuote = zStr[j] == '"' ? '\'' : '"';` |
|     35 |  6959 | `				break;` |
|      - |  6960 | `			}` |
|     12 |  6961 | `		}` |
|    123 |  6962 | `		while( i < nStr && zStr[i] != cQuote ){` |
|     89 |  6963 | `			i++;` |
|      1 |  6964 | `		}` |
|     35 |  6965 | `		if( iStart > 0 ){` |
|     21 |  6966 | `			SyBlobAppend(&sOut,",",1);` |
|     10 |  6967 | `		}` |
|     35 |  6968 | `		SyBlobAppend(&sOut,&cQuote,1);` |
|     35 |  6969 | `		SyBlobAppend(&sOut,zStr + iStart,(sxu32)(i - iStart));` |
|     35 |  6970 | `		SyBlobAppend(&sOut,&cQuote,1);` |
|      1 |  6971 | `	}` |
|     15 |  6972 | `	SyBlobAppend(&sOut,")",1);` |
|     15 |  6973 | `	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     15 |  6974 | `	SyBlobRelease(&sOut);` |
|     15 |  6975 | `	return PH7_OK;` |
|     20 |  6976 | `}` |
|      - |  6977 |  |
|      - |  6978 | `/* ===== The PHP-function bridge (php:function / php:functionString / own-URI) ===== */` |
|      - |  6979 |  |
|      - |  6980 | ``/* php's reserved URI: `php:function()` is reached through whatever PREFIX the`` |
|      - |  6981 | ` * caller bound to it, so every lookup here is by URI. */` |
|      - |  6982 | `#define XP_PHPNS "http://php.net/xpath"` |
|      - |  6983 | `/* Which callables an evaluation may reach: php's register_phpfunctions. */` |
|      - |  6984 | `#define XP_MODE_NONE   0   /* registerPhpFunctions() never called -- nothing runs */` |
|      - |  6985 | `#define XP_MODE_ALL    1   /* called bare -- any callable name runs */` |
|      - |  6986 | `#define XP_MODE_LIST   2   /* called with a restriction -- the table below decides */` |
|      - |  6987 | `#define XP_FNMODE "__fnmode"` |
|      - |  6988 | `#define XP_FNREG  "__fnreg"    /* restricted: xpath name => the callable to run */` |
|      - |  6989 | `#define XP_NSFN   "__nsfn"     /* own-URI: "<uri>\x01<name>" => callable */` |
|    108 |  6990 | `static ph7_hashmap * DomXPathSlotMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)` |
|      1 |  6991 | `{` |
|    109 |  6992 | `	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;` |
|    109 |  6993 | `	if( pSlot == 0 ){` |
|    ! 0 |  6994 | `		return 0;` |
|      - |  6995 | `	}` |
|    109 |  6996 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     43 |  6997 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|    ! 0 |  6998 | `			return 0;` |
|      - |  6999 | `		}` |
|     21 |  7000 | `	}` |
|    109 |  7001 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|     55 |  7002 | `}` |
|      - |  7003 | `/* Insert (or replace) one string-keyed entry. */` |
|     26 |  7004 | `static void DomXPathMapPut(ph7_vm *pVm,ph7_hashmap *pMap,const char *zKey,int nKey,ph7_value *pVal)` |
|      1 |  7005 | `{` |
|      - |  7006 | `	ph7_value sKey;` |
|     27 |  7007 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     27 |  7008 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|     27 |  7009 | `	PH7_HashmapInsert(pMap,&sKey,pVal);` |
|     27 |  7010 | `	PH7_MemObjRelease(&sKey);` |
|     27 |  7011 | `}` |
|      - |  7012 | `/* ...and the matching read, or NULL. The value BELONGS to the map. */` |
|     42 |  7013 | `static ph7_value * DomXPathMapGet(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,` |
|      - |  7014 | `	const char *zKey,int nKey)` |
|      1 |  7015 | `{` |
|     43 |  7016 | `	ph7_hashmap *pMap = DomXPathSlotMap(&(*pVm),pThis,zSlot);` |
|     43 |  7017 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  7018 | `	ph7_value sKey,*pHit;` |
|     43 |  7019 | `	if( pMap == 0 ){` |
|    ! 0 |  7020 | `		return 0;` |
|      - |  7021 | `	}` |
|     43 |  7022 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     43 |  7023 | `	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);` |
|     43 |  7024 | `	if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){` |
|      9 |  7025 | `		pEntry = 0;` |
|      4 |  7026 | `	}` |
|     43 |  7027 | `	PH7_MemObjRelease(&sKey);` |
|     43 |  7028 | `	pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;` |
|     43 |  7029 | `	return pHit;` |
|     22 |  7030 | `}` |
|      - |  7031 | `/*` |
|      - |  7032 | ` * What one evaluation needs to reach PHP from inside libxml, and what it` |
|      - |  7033 | ` * brings BACK.` |
|      - |  7034 | ` *` |
|      - |  7035 | ` * The bringing back is the whole design problem: a refusal raised from the` |
|      - |  7036 | ` * callback would run the enclosing catch RIGHT THERE, in the middle of` |
|      - |  7037 | ` * libxml's own recursion (the builtin-throw rail), so nothing is raised here.` |
|      - |  7038 | ` * The reason is PARKED -- a code and the name it quotes -- the evaluation is` |
|      - |  7039 | ` * stopped by setting the parser's error field (not xmlXPathErr, which would` |
|      - |  7040 | ` * queue a libxml diagnostic php does not print), and DomXPathEvalRun raises` |
|      - |  7041 | ` * once libxml has unwound. A throw from the CALLBACK ITSELF is the same` |
|      - |  7042 | ` * story one level up: its dispatch status is parked in rcUnwound and returned` |
|      - |  7043 | `` * from the method verbatim, which is what makes `php:function("boom") or`` |
|      - |  7044 | `` * php:function("after")` run neither the `or` arm nor anything past it --`` |
|      - |  7045 | ` * php's answer.` |
|      - |  7046 | ` */` |
|      - |  7047 | `#define XP_FN_OK        0` |
|      - |  7048 | `#define XP_FN_NOREG     1   /* registerPhpFunctions() was never called */` |
|      - |  7049 | `#define XP_FN_NOHANDLER 2   /* restricted, and this name is not in the table */` |
|      - |  7050 | `#define XP_FN_NOTSTR    3   /* the handler name argument is not a string */` |
|      - |  7051 | `#define XP_FN_NONAME    4   /* php:function() with no arguments at all */` |
|      - |  7052 | `#define XP_FN_BADCB     5   /* the name is not callable */` |
|      - |  7053 | `#define XP_FN_NOTNODE   6   /* the callback answered an object that is not a node */` |
|      - |  7054 | `typedef struct DomXPathFnCtx DomXPathFnCtx;` |
|      - |  7055 | `struct DomXPathFnCtx {` |
|      - |  7056 | `	ph7_context *pCtx;            /* the method's own call context */` |
|      - |  7057 | `	ph7_class_instance *pThis;    /* the DOMXPath */` |
|      - |  7058 | `	ph7_class_instance *pDoc;     /* its document object (where wrappers cache) */` |
|      - |  7059 | `	phl_domnode *pDocNd;` |
|      - |  7060 | `	int iErr;                     /* XP_FN_* -- raised after libxml unwinds */` |
|      - |  7061 | `	SyBlob sErrName;              /* the name that refusal quotes */` |
|      - |  7062 | `	sxi32 rcUnwound;              /* a callback that did not return */` |
|      - |  7063 | `};` |
|      - |  7064 | `/* Stop the evaluation without emitting a libxml diagnostic. */` |
|     22 |  7065 | `static void DomXPathFnStop(xmlXPathParserContextPtr pPCtx,DomXPathFnCtx *pFn,int iErr,` |
|      - |  7066 | `	const char *zName,int nName)` |
|      1 |  7067 | `{` |
|     23 |  7068 | `	if( pFn->iErr == XP_FN_OK ){` |
|     23 |  7069 | `		pFn->iErr = iErr;` |
|     23 |  7070 | `		SyBlobReset(&pFn->sErrName);` |
|     23 |  7071 | `		if( zName && nName > 0 ){` |
|     11 |  7072 | `			SyBlobAppend(&pFn->sErrName,zName,(sxu32)nName);` |
|      5 |  7073 | `		}` |
|     11 |  7074 | `	}` |
|     23 |  7075 | `	pPCtx->error = XPATH_EXPR_ERROR;` |
|     23 |  7076 | `}` |
|      - |  7077 | `/*` |
|      - |  7078 | ` * What a callback that did not RETURN leaves behind, which php's two` |
|      - |  7079 | ` * dispatchers do differently and both visibly.` |
|      - |  7080 | ` *` |
|      - |  7081 | ` * The one that looks a callable up in a REGISTERED table -- restricted` |
|      - |  7082 | ` * php:function, and every own-URI name -- returns without pushing, and libxml,` |
|      - |  7083 | ` * finding its value stack one short, queues its own "Stack usage error" before` |
|      - |  7084 | ` * unwinding; that entry is then on the list libxml_get_errors() answers. The` |
|      - |  7085 | ` * UNRESTRICTED php:function path pushes a value first, so its queue stays` |
|      - |  7086 | ` * clean. Either way the exception is the answer, and nothing further of the` |
|      - |  7087 | ` * expression runs.` |
|      - |  7088 | ` */` |
|      2 |  7089 | `static void DomXPathFnUnwound(xmlXPathParserContextPtr pPCtx,int bRegistered)` |
|      1 |  7090 | `{` |
|      3 |  7091 | `	if( !bRegistered ){` |
|      3 |  7092 | `		valuePush(pPCtx,xmlXPathNewCString(""));` |
|      3 |  7093 | `		pPCtx->error = XPATH_EXPR_ERROR;` |
|      1 |  7094 | `	}` |
|      3 |  7095 | `}` |
|      - |  7096 | `/* One XPath argument as php sees it: a nodeset becomes an ARRAY of wrappers` |
|      - |  7097 | ` * (php's own conversion), the three scalars their php types. bAsString is` |
|      - |  7098 | ` * php:functionString's flag, under which a nodeset arrives as its string` |
|      - |  7099 | ` * value instead. */` |
|     58 |  7100 | `static void DomXPathArgToValue(DomXPathFnCtx *pFn,xmlXPathObjectPtr pArg,int bAsString,` |
|      - |  7101 | `	ph7_value *pOut)` |
|      1 |  7102 | `{` |
|     59 |  7103 | `	ph7_vm *pVm = pFn->pCtx->pVm;` |
|     59 |  7104 | `	if( pArg == 0 ){` |
|    ! 0 |  7105 | `		PH7_MemObjInit(pVm,pOut);` |
|    ! 0 |  7106 | `		return;` |
|      - |  7107 | `	}` |
|     59 |  7108 | `	if( pArg->type == XPATH_NODESET && !bAsString ){` |
|      - |  7109 | `		/* The array is built on its OWN reference rather than the method's call` |
|      - |  7110 | `		 * context: a predicate calls this once per node, and a context-owned` |
|      - |  7111 | `		 * one would live until the whole evaluation ended. */` |
|      9 |  7112 | `		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);` |
|      - |  7113 | `		int i;` |
|      9 |  7114 | `		PH7_MemObjInit(pVm,pOut);` |
|      9 |  7115 | `		if( pMap == 0 ){` |
|    ! 0 |  7116 | `			return;` |
|      - |  7117 | `		}` |
|      - |  7118 | `		/* pOut CARRIES the map's only reference, and the caller's release of` |
|      - |  7119 | `		 * it after the call is what frees it. */` |
|      9 |  7120 | `		pOut->x.pOther = pMap;` |
|      9 |  7121 | `		pOut->iFlags = MEMOBJ_HASHMAP;` |
|     21 |  7122 | `		for( i = 0 ; pArg->nodesetval && i < pArg->nodesetval->nodeNr ; ++i ){` |
|     13 |  7123 | `			xmlNodePtr pNode = pArg->nodesetval->nodeTab[i];` |
|      - |  7124 | `			ph7_value sElem;` |
|      - |  7125 | `			ph7_class_instance *pObj;` |
|     13 |  7126 | `			if( pNode == 0 ){` |
|    ! 0 |  7127 | `				continue;` |
|      - |  7128 | `			}` |
|     13 |  7129 | `			if( pNode->type == XML_NAMESPACE_DECL ){` |
|    ! 0 |  7130 | `				xmlNsPtr pNs = (xmlNsPtr)pNode;` |
|    ! 0 |  7131 | `				xmlNodePtr pElem = (xmlNodePtr)pNs->next;` |
|    ! 0 |  7132 | `				xmlNsPtr pOrig = (pElem && pElem->type == XML_ELEMENT_NODE)` |
|    ! 0 |  7133 | `					? xmlSearchNs((xmlDocPtr)pFn->pDocNd->pNode,pElem,pNs->prefix) : 0;` |
|    ! 0 |  7134 | `				if( pOrig == 0 ){` |
|    ! 0 |  7135 | `					continue;` |
|      - |  7136 | `				}` |
|    ! 0 |  7137 | `				pObj = DomNewNsNode(pVm,pFn->pDoc,pFn->pDocNd->pShell,pOrig,pElem);` |
|    ! 0 |  7138 | `				if( pObj == 0 ){` |
|    ! 0 |  7139 | `					continue;` |
|      - |  7140 | `				}` |
|    ! 0 |  7141 | `				PH7_MemObjInit(pVm,&sElem);` |
|    ! 0 |  7142 | `				sElem.x.pOther = pObj;` |
|    ! 0 |  7143 | `				sElem.iFlags = MEMOBJ_OBJ;` |
|    ! 0 |  7144 | `				ph7_array_add_elem(pOut,0,&sElem);   /* takes its own reference */` |
|    ! 0 |  7145 | `				PH7_ClassInstanceUnref(pObj);        /* ...and ours goes back */` |
|    ! 0 |  7146 | `				continue;` |
|      - |  7147 | `			}` |
|     13 |  7148 | `			pObj = DomWrap(pVm,pFn->pDoc,pFn->pDocNd->pShell,pNode);` |
|     13 |  7149 | `			if( pObj == 0 ){` |
|    ! 0 |  7150 | `				continue;` |
|      - |  7151 | `			}` |
|     13 |  7152 | `			PH7_MemObjInit(pVm,&sElem);` |
|     13 |  7153 | `			sElem.x.pOther = pObj;   /* BORROWED from the cache; the insert refs it */` |
|     13 |  7154 | `			sElem.iFlags = MEMOBJ_OBJ;` |
|     13 |  7155 | `			ph7_array_add_elem(pOut,0,&sElem);` |
|      7 |  7156 | `		}` |
|      9 |  7157 | `		return;` |
|      - |  7158 | `	}` |
|     51 |  7159 | `	switch( pArg->type ){` |
|      3 |  7160 | `	case XPATH_BOOLEAN:` |
|      7 |  7161 | `		PH7_MemObjInitFromBool(pVm,pOut,pArg->boolval);` |
|      7 |  7162 | `		break;` |
|      2 |  7163 | `	case XPATH_NUMBER:` |
|      5 |  7164 | `		PH7_MemObjInitFromReal(pVm,pOut,pArg->floatval);` |
|      5 |  7165 | `		break;` |
|     20 |  7166 | `	default: {` |
|     41 |  7167 | `		xmlChar *zStr = xmlXPathCastToString(pArg);` |
|     41 |  7168 | `		PH7_MemObjInitFromString(pVm,pOut,0);` |
|     41 |  7169 | `		if( zStr ){` |
|     41 |  7170 | `			PH7_MemObjStringAppend(pOut,(const char *)zStr,(sxu32)SyStrlen((const char *)zStr));` |
|     41 |  7171 | `			xmlFree(zStr);` |
|     20 |  7172 | `		}` |
|     40 |  7173 | `		break;` |
|      - |  7174 | `	}` |
|      - |  7175 | `	}` |
|     30 |  7176 | `}` |
|      - |  7177 | `/* The callback's ANSWER, pushed back on the XPath stack: a bool stays a` |
|      - |  7178 | ` * boolean, a DOM node becomes a one-node set, and everything else is php's` |
|      - |  7179 | ` * string conversion -- the SAME three for both spellings, since` |
|      - |  7180 | ` * functionString's flag is about the ARGUMENTS. The conversion is the` |
|      - |  7181 | ` * user-visible one (an array draws php's "Array to string conversion" notice` |
|      - |  7182 | ` * and reads "Array"); a non-node object is the TypeError parked above. */` |
|     58 |  7183 | `static void DomXPathPushResult(xmlXPathParserContextPtr pPCtx,DomXPathFnCtx *pFn,` |
|      - |  7184 | `	ph7_value *pRes)` |
|      1 |  7185 | `{` |
|     59 |  7186 | `	if( (pRes->iFlags & MEMOBJ_BOOL) && (pRes->iFlags & MEMOBJ_STRING) == 0 ){` |
|      3 |  7187 | `		valuePush(pPCtx,xmlXPathNewBoolean(pRes->x.iVal != 0));` |
|      3 |  7188 | `		return;` |
|      - |  7189 | `	}` |
|     57 |  7190 | `	if( pRes->iFlags & MEMOBJ_OBJ ){` |
|      7 |  7191 | `		ph7_class_instance *pObj = (ph7_class_instance *)pRes->x.pOther;` |
|      7 |  7192 | `		phl_domnode *pNd = DomResOf(pObj);` |
|      7 |  7193 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      7 |  7194 | `		if( pNode == 0 \|\| pNode->type == XML_NAMESPACE_DECL ){` |
|      5 |  7195 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOTNODE,0,0);` |
|      5 |  7196 | `			return;` |
|      - |  7197 | `		}` |
|      3 |  7198 | `		valuePush(pPCtx,xmlXPathNewNodeSet(pNode));` |
|      3 |  7199 | `		return;` |
|      - |  7200 | `	}` |
|      - |  7201 | `	{` |
|     51 |  7202 | `		int nStr = 0;` |
|      - |  7203 | `		const char *zStr;` |
|      - |  7204 | `		xmlChar *zDup;` |
|     51 |  7205 | `		if( (pRes->iFlags & MEMOBJ_STRING) == 0 ){` |
|     17 |  7206 | `			sxi32 rcStr = PH7_MemObjToStringUV(pRes);` |
|     17 |  7207 | `			if( PH7_CALLBACK_UNWOUND(rcStr) ){` |
|      - |  7208 | `				/* A __toString() that threw: the same rail as the callback's` |
|      - |  7209 | `				 * own throw, one conversion later. */` |
|    ! 0 |  7210 | `				pFn->rcUnwound = rcStr;` |
|    ! 0 |  7211 | `				return;` |
|      - |  7212 | `			}` |
|      8 |  7213 | `		}` |
|     51 |  7214 | `		zStr = ph7_value_to_string(pRes,&nStr);` |
|     51 |  7215 | `		zDup = xmlStrndup((const xmlChar *)zStr,nStr);` |
|     51 |  7216 | `		valuePush(pPCtx,xmlXPathWrapString(zDup));` |
|      - |  7217 | `	}` |
|     30 |  7218 | `}` |
|      - |  7219 | `/*` |
|      - |  7220 | ` * The one C function behind every PHP-backed XPath name. libxml reaches it` |
|      - |  7221 | ` * through the lookup below, with the called name and URI on the context.` |
|      - |  7222 | ` */` |
|     78 |  7223 | `static void DomXPathPhpFn(xmlXPathParserContextPtr pPCtx,int nArgs)` |
|      1 |  7224 | `{` |
|     79 |  7225 | `	xmlXPathContextPtr pXCtx = pPCtx ? pPCtx->context : 0;` |
|     79 |  7226 | `	DomXPathFnCtx *pFn = pXCtx ? (DomXPathFnCtx *)pXCtx->funcLookupData : 0;` |
|     79 |  7227 | `	const xmlChar *zFn = pXCtx ? pXCtx->function : 0;` |
|     79 |  7228 | `	const xmlChar *zUri = pXCtx ? pXCtx->functionURI : 0;` |
|     79 |  7229 | `	int bPhpNs = zUri && xmlStrEqual(zUri,(const xmlChar *)XP_PHPNS);` |
|     79 |  7230 | `	int bAsString = bPhpNs && zFn && xmlStrEqual(zFn,(const xmlChar *)"functionString");` |
|     79 |  7231 | `	int bRegistered = !bPhpNs;   /* a table lookup rather than the name itself */` |
|      - |  7232 | `	xmlXPathObjectPtr *apArg;` |
|     79 |  7233 | `	ph7_value *apVal = 0,sResult,sName;` |
|     79 |  7234 | `	ph7_value *pCallable = 0;` |
|     79 |  7235 | `	int bNameOwned = 0;` |
|      - |  7236 | `	ph7_vm *pVm;` |
|     79 |  7237 | `	int nSkip = bPhpNs ? 1 : 0;   /* php:function's first argument NAMES the callback */` |
|      - |  7238 | `	int i,nCall;` |
|      - |  7239 | `	sxi32 rc;` |
|     79 |  7240 | `	if( pFn == 0 ){` |
|    ! 0 |  7241 | `		return;` |
|      - |  7242 | `	}` |
|     79 |  7243 | `	pVm = pFn->pCtx->pVm;` |
|      - |  7244 | `	/* Take the arguments off the stack FIRST (valuePop answers them last-first),` |
|      - |  7245 | `	 * so every exit below leaves libxml's stack where it found it. */` |
|     79 |  7246 | `	apArg = nArgs > 0` |
|    114 |  7247 | `		? (xmlXPathObjectPtr *)SyMemBackendAlloc(&pVm->sAllocator,` |
|     76 |  7248 | `			sizeof(xmlXPathObjectPtr) * (sxu32)nArgs)` |
|     39 |  7249 | `		: 0;` |
|     79 |  7250 | `	if( nArgs > 0 && apArg == 0 ){` |
|    ! 0 |  7251 | `		pPCtx->error = XPATH_MEMORY_ERROR;` |
|    ! 0 |  7252 | `		return;` |
|      - |  7253 | `	}` |
|    215 |  7254 | `	for( i = nArgs - 1 ; i >= 0 ; --i ){` |
|    137 |  7255 | `		apArg[i] = valuePop(pPCtx);` |
|     69 |  7256 | `	}` |
|     79 |  7257 | `	if( pFn->iErr != XP_FN_OK \|\| pFn->rcUnwound != 0 ){` |
|    ! 0 |  7258 | `		goto done;   /* a previous call already stopped this evaluation */` |
|      - |  7259 | `	}` |
|     79 |  7260 | `	if( bPhpNs ){` |
|     69 |  7261 | `		int nName = 0;` |
|      - |  7262 | `		const char *zName;` |
|     69 |  7263 | `		sxi64 iMode = PH7_NativeAttrInt(pFn->pThis,XP_FNMODE);` |
|     69 |  7264 | `		if( nArgs < 1 ){` |
|      3 |  7265 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NONAME,0,0);` |
|      3 |  7266 | `			goto done;` |
|      - |  7267 | `		}` |
|     67 |  7268 | `		if( apArg[0] == 0 \|\| apArg[0]->type != XPATH_STRING ){` |
|      3 |  7269 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOTSTR,0,0);` |
|      3 |  7270 | `			goto done;` |
|      - |  7271 | `		}` |
|     65 |  7272 | `		zName = apArg[0]->stringval ? (const char *)apArg[0]->stringval : "";` |
|     65 |  7273 | `		nName = (int)SyStrlen(zName);` |
|     65 |  7274 | `		if( iMode == XP_MODE_NONE ){` |
|      5 |  7275 | `			DomXPathFnStop(pPCtx,pFn,XP_FN_NOREG,0,0);` |
|      5 |  7276 | `			goto done;` |
|      - |  7277 | `		}` |
|     61 |  7278 | `		if( iMode == XP_MODE_LIST ){` |
|     23 |  7279 | `			bRegistered = 1;` |
|     23 |  7280 | `			pCallable = DomXPathMapGet(pVm,pFn->pThis,XP_FNREG,zName,nName);` |
|     23 |  7281 | `			if( pCallable == 0 ){` |
|      9 |  7282 | `				DomXPathFnStop(pPCtx,pFn,XP_FN_NOHANDLER,zName,nName);` |
|      9 |  7283 | `				goto done;` |
|      - |  7284 | `			}` |
|      8 |  7285 | `		}else{` |
|      - |  7286 | `			/* Unrestricted: the NAME ITSELF is the callable, screened here` |
|      - |  7287 | `			 * because no registration screened it. */` |
|     39 |  7288 | `			PH7_MemObjInitFromString(pVm,&sName,0);` |
|     39 |  7289 | `			PH7_MemObjStringAppend(&sName,zName,(sxu32)nName);` |
|     39 |  7290 | `			bNameOwned = 1;` |
|     39 |  7291 | `			if( !PH7_VmIsCallable(pVm,&sName,TRUE) ){` |
|      3 |  7292 | `				DomXPathFnStop(pPCtx,pFn,XP_FN_BADCB,zName,nName);` |
|      3 |  7293 | `				goto done;` |
|      - |  7294 | `			}` |
|     37 |  7295 | `			pCallable = &sName;` |
|      - |  7296 | `		}` |
|     26 |  7297 | `	}else{` |
|      - |  7298 | `		SyBlob sKey;` |
|     11 |  7299 | `		SyBlobInit(&sKey,&pVm->sAllocator);` |
|     11 |  7300 | `		SyBlobAppend(&sKey,(const char *)zUri,zUri ? (sxu32)SyStrlen((const char *)zUri) : 0);` |
|     11 |  7301 | `		SyBlobAppend(&sKey,"\1",1);` |
|     11 |  7302 | `		SyBlobAppend(&sKey,(const char *)zFn,zFn ? (sxu32)SyStrlen((const char *)zFn) : 0);` |
|     16 |  7303 | `		pCallable = DomXPathMapGet(pVm,pFn->pThis,XP_NSFN,` |
|     10 |  7304 | `			(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey));` |
|     11 |  7305 | `		SyBlobRelease(&sKey);` |
|     11 |  7306 | `		if( pCallable == 0 ){` |
|    ! 0 |  7307 | `			goto done;   /* not ours after all: libxml reports the unknown function */` |
|      - |  7308 | `		}` |
|      - |  7309 | `	}` |
|     61 |  7310 | `	nCall = nArgs - nSkip;` |
|     61 |  7311 | `	if( nCall > 0 ){` |
|     67 |  7312 | `		apVal = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|     44 |  7313 | `			sizeof(ph7_value) * (sxu32)nCall);` |
|     45 |  7314 | `		if( apVal == 0 ){` |
|    ! 0 |  7315 | `			pPCtx->error = XPATH_MEMORY_ERROR;` |
|    ! 0 |  7316 | `			goto done;` |
|      - |  7317 | `		}` |
|    103 |  7318 | `		for( i = 0 ; i < nCall ; ++i ){` |
|     59 |  7319 | `			DomXPathArgToValue(pFn,apArg[i + nSkip],bAsString,&apVal[i]);` |
|     30 |  7320 | `		}` |
|     22 |  7321 | `	}` |
|     61 |  7322 | `	PH7_MemObjInit(pVm,&sResult);` |
|      - |  7323 | `	{` |
|     61 |  7324 | `		ph7_value **apPtr = nCall > 0` |
|     66 |  7325 | `			? (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|     44 |  7326 | `				sizeof(ph7_value *) * (sxu32)nCall)` |
|     30 |  7327 | `			: 0;` |
|     61 |  7328 | `		if( nCall > 0 && apPtr == 0 ){` |
|    ! 0 |  7329 | `			pPCtx->error = XPATH_MEMORY_ERROR;` |
|    ! 0 |  7330 | `			PH7_MemObjRelease(&sResult);` |
|    ! 0 |  7331 | `			goto done;` |
|      - |  7332 | `		}` |
|      - |  7333 | `		/* Dispatch off a COPY: pCallable points into a registration map this` |
|      - |  7334 | `		 * very callback can rewrite (a callback calling registerPhpFunctions` |
|      - |  7335 | `		 * on its own DOMXPath), and the map's value would go out from under` |
|      - |  7336 | `		 * the dispatch. */` |
|      - |  7337 | `		ph7_value sCall;` |
|     61 |  7338 | `		PH7_MemObjInit(pVm,&sCall);` |
|     61 |  7339 | `		PH7_MemObjStore(pCallable,&sCall);` |
|    119 |  7340 | `		for( i = 0 ; i < nCall ; ++i ){` |
|     59 |  7341 | `			apPtr[i] = &apVal[i];` |
|     30 |  7342 | `		}` |
|     61 |  7343 | `		rc = PH7_VmCallCallbackByValue(pVm,&sCall,nCall,apPtr,&sResult,0);` |
|     61 |  7344 | `		PH7_MemObjRelease(&sCall);` |
|     61 |  7345 | `		if( apPtr ){` |
|     45 |  7346 | `			SyMemBackendFree(&pVm->sAllocator,apPtr);` |
|     22 |  7347 | `		}` |
|      - |  7348 | `	}` |
|     61 |  7349 | `	if( PH7_CALLBACK_UNWOUND(rc) ){` |
|      3 |  7350 | `		pFn->rcUnwound = rc;` |
|      3 |  7351 | `		DomXPathFnUnwound(pPCtx,bRegistered);` |
|      2 |  7352 | `	}else{` |
|     59 |  7353 | `		DomXPathPushResult(pPCtx,pFn,&sResult);` |
|      - |  7354 | `	}` |
|     61 |  7355 | `	PH7_MemObjRelease(&sResult);` |
|     39 |  7356 | `done:` |
|     79 |  7357 | `	if( bNameOwned ){` |
|     39 |  7358 | `		PH7_MemObjRelease(&sName);` |
|     19 |  7359 | `	}` |
|     79 |  7360 | `	if( apVal ){` |
|    103 |  7361 | `		for( i = 0 ; i < nArgs - nSkip ; ++i ){` |
|     59 |  7362 | `			PH7_MemObjRelease(&apVal[i]);` |
|     30 |  7363 | `		}` |
|     45 |  7364 | `		SyMemBackendFree(&pVm->sAllocator,apVal);` |
|     22 |  7365 | `	}` |
|    215 |  7366 | `	for( i = 0 ; i < nArgs ; ++i ){` |
|    137 |  7367 | `		if( apArg[i] ){` |
|    137 |  7368 | `			xmlXPathFreeObject(apArg[i]);` |
|     68 |  7369 | `		}` |
|     69 |  7370 | `	}` |
|     79 |  7371 | `	if( apArg ){` |
|     77 |  7372 | `		SyMemBackendFree(&pVm->sAllocator,apArg);` |
|     38 |  7373 | `	}` |
|     40 |  7374 | `}` |
|      - |  7375 | `/*` |
|      - |  7376 | ` * libxml's function-resolution hook: answer the bridge for php's two reserved` |
|      - |  7377 | ` * names and for any (URI, name) this object registered, and NULL for` |
|      - |  7378 | ` * everything else -- which is what makes libxml fall through to its own table` |
|      - |  7379 | `` * (so `count()` and friends still resolve).`` |
|      - |  7380 | ` */` |
|    143 |  7381 | `static xmlXPathFunction DomXPathFnLookup(void *pUserData,const xmlChar *zName,const xmlChar *zUri)` |
|      1 |  7382 | `{` |
|    144 |  7383 | `	DomXPathFnCtx *pFn = (DomXPathFnCtx *)pUserData;` |
|      - |  7384 | `	SyBlob sKey;` |
|      - |  7385 | `	ph7_value *pHit;` |
|    144 |  7386 | `	if( pFn == 0 \|\| zUri == 0 \|\| zName == 0 ){` |
|     70 |  7387 | `		return 0;` |
|      - |  7388 | `	}` |
|     75 |  7389 | `	if( xmlStrEqual(zUri,(const xmlChar *)XP_PHPNS) ){` |
|     64 |  7390 | `		if( xmlStrEqual(zName,(const xmlChar *)"function")` |
|     38 |  7391 | `		 \|\| xmlStrEqual(zName,(const xmlChar *)"functionString") ){` |
|     65 |  7392 | `			return DomXPathPhpFn;` |
|      - |  7393 | `		}` |
|    ! 0 |  7394 | `		return 0;` |
|      - |  7395 | `	}` |
|     11 |  7396 | `	SyBlobInit(&sKey,&pFn->pCtx->pVm->sAllocator);` |
|     11 |  7397 | `	SyBlobAppend(&sKey,(const char *)zUri,(sxu32)SyStrlen((const char *)zUri));` |
|     11 |  7398 | `	SyBlobAppend(&sKey,"\1",1);` |
|     11 |  7399 | `	SyBlobAppend(&sKey,(const char *)zName,(sxu32)SyStrlen((const char *)zName));` |
|     16 |  7400 | `	pHit = DomXPathMapGet(pFn->pCtx->pVm,pFn->pThis,XP_NSFN,` |
|     10 |  7401 | `		(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey));` |
|     11 |  7402 | `	SyBlobRelease(&sKey);` |
|     11 |  7403 | `	return pHit ? DomXPathPhpFn : 0;` |
|     39 |  7404 | `}` |
|      - |  7405 | `/* The parked refusal, raised once libxml has unwound. */` |
|     22 |  7406 | `static int DomXPathFnRaise(ph7_context *pCtx,DomXPathFnCtx *pFn)` |
|      1 |  7407 | `{` |
|     23 |  7408 | `	const char *zName = (const char *)SyBlobData(&pFn->sErrName);` |
|     23 |  7409 | `	int nName = (int)SyBlobLength(&pFn->sErrName);` |
|     23 |  7410 | `	switch( pFn->iErr ){` |
|      2 |  7411 | `	case XP_FN_NOREG:` |
|      5 |  7412 | `		return PH7_VmThrowException(pCtx,"Error","No callbacks were registered");` |
|      4 |  7413 | `	case XP_FN_NOHANDLER:` |
|     13 |  7414 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      4 |  7415 | `			"No callback handler \"%.*s\" registered",nName,zName);` |
|      1 |  7416 | `	case XP_FN_NOTSTR:` |
|      3 |  7417 | `		return PH7_VmThrowException(pCtx,"TypeError","Handler name must be a string");` |
|      1 |  7418 | `	case XP_FN_NONAME:` |
|      3 |  7419 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  7420 | `			"Function name must be passed as the first argument");` |
|      1 |  7421 | `	case XP_FN_BADCB:` |
|      4 |  7422 | `		return PH7_VmThrowException(pCtx,"Error",` |
|      - |  7423 | `			"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",` |
|      1 |  7424 | `			nName,zName,nName,zName);` |
|      2 |  7425 | `	case XP_FN_NOTNODE:` |
|      5 |  7426 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  7427 | `			"Only objects that are instances of DOM nodes can be converted to an XPath expression");` |
|    ! 0 |  7428 | `	default:` |
|    ! 0 |  7429 | `		break;` |
|      - |  7430 | `	}` |
|    ! 0 |  7431 | `	return PH7_OK;` |
|     12 |  7432 | `}` |
|      - |  7433 | `/*` |
|      - |  7434 | ` * Build the evaluation context for one query()/evaluate() call: a FRESH` |
|      - |  7435 | ` * xmlXPathContext (php keeps a persistent one; replaying the registration` |
|      - |  7436 | ` * table onto a fresh one answers the same), anchored at the explicit context` |
|      - |  7437 | ` * node -- or, with none, at the document ELEMENT, php's own substitution (so` |
|      - |  7438 | ` * query('file') matches a child of the root; an explicitly PASSED document` |
|      - |  7439 | ` * node is NOT substituted and carries no namespaces).` |
|      - |  7440 | ` *` |
|      - |  7441 | ` * bRegNodeNs is php's $registerNodeNS: the context NODE's in-scope` |
|      - |  7442 | ` * declarations go into pXCtx->namespaces, the array xmlXPathNsLookup consults` |
|      - |  7443 | ` * BEFORE the registered table -- which is why a document prefix beats a` |
|      - |  7444 | ` * registerNamespace() one only for that call. The caller frees the returned` |
|      - |  7445 | ` * list with xmlFree AFTER evaluating (the xmlNs entries belong to the tree;` |
|      - |  7446 | ` * only the array is owned).` |
|      - |  7447 | ` */` |
|    228 |  7448 | `static xmlNsPtr * DomXPathCtxOpen(ph7_context *pCtx,phl_domnode *pDocNd,` |
|      - |  7449 | `	phl_domnode *pCtxNd,int bRegNodeNs,xmlXPathContextPtr *ppXCtx)` |
|      1 |  7450 | `{` |
|    229 |  7451 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7452 | `	xmlXPathContextPtr pXCtx;` |
|      - |  7453 | `	ph7_value *pNsReg;` |
|    229 |  7454 | `	xmlNsPtr *aNs = 0;` |
|    229 |  7455 | `	*ppXCtx = 0;` |
|    229 |  7456 | `	pXCtx = xmlXPathNewContext((xmlDocPtr)pDocNd->pNode);` |
|    229 |  7457 | `	if( pXCtx == 0 ){` |
|    ! 0 |  7458 | `		return 0;` |
|      - |  7459 | `	}` |
|    229 |  7460 | `	if( pCtxNd ){` |
|     25 |  7461 | `		pXCtx->node = (xmlNodePtr)pCtxNd->pNode;` |
|     13 |  7462 | `	}else{` |
|    205 |  7463 | `		pXCtx->node = xmlDocGetRootElement((xmlDocPtr)pDocNd->pNode);` |
|      - |  7464 | `	}` |
|    229 |  7465 | `	pNsReg = pThis ? PH7_NativeAttr(pThis,XP_NSREG) : 0;` |
|    229 |  7466 | `	if( pNsReg && (pNsReg->iFlags & MEMOBJ_HASHMAP) ){` |
|     95 |  7467 | `		ph7_array_walk(pNsReg,DomC14NRegisterNs,pXCtx);` |
|     47 |  7468 | `	}` |
|    229 |  7469 | `	if( bRegNodeNs && pXCtx->node ){` |
|    223 |  7470 | `		aNs = xmlGetNsList((xmlDocPtr)pDocNd->pNode,pXCtx->node);` |
|    223 |  7471 | `		if( aNs ){` |
|     45 |  7472 | `			int nNs = 0;` |
|    105 |  7473 | `			while( aNs[nNs] ){` |
|     61 |  7474 | `				nNs++;` |
|      1 |  7475 | `			}` |
|     45 |  7476 | `			pXCtx->namespaces = aNs;` |
|     45 |  7477 | `			pXCtx->nsNr = nNs;` |
|     22 |  7478 | `		}` |
|    111 |  7479 | `	}` |
|    229 |  7480 | `	*ppXCtx = pXCtx;` |
|    229 |  7481 | `	return aNs;` |
|    115 |  7482 | `}` |
|      - |  7483 | `/*` |
|      - |  7484 | ` * Freeze a nodeset result into the document-order snapshot a DNL_SNAP` |
|      - |  7485 | ` * DOMNodeList serves (php's query() is not live), and answer the list. A` |
|      - |  7486 | ` * non-nodeset pObj answers the EMPTY list: php's query() gives that for a` |
|      - |  7487 | `` * scalar-typed expression (`count(//x)`), not false.`` |
|      - |  7488 | ` */` |
|     94 |  7489 | `static int DomXPathResultList(ph7_context *pCtx,ph7_class_instance *pDoc,` |
|      - |  7490 | `	phl_domnode *pDocNd,xmlXPathObjectPtr pObj)` |
|      1 |  7491 | `{` |
|     95 |  7492 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  7493 | `	ph7_class_instance *pList;` |
|     95 |  7494 | `	ph7_value *pSnap = ph7_context_new_array(pCtx);` |
|     95 |  7495 | `	if( pSnap == 0 ){` |
|    ! 0 |  7496 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  7497 | `	}` |
|     95 |  7498 | `	if( pObj && pObj->type == XPATH_NODESET && pObj->nodesetval ){` |
|      - |  7499 | `		int i;` |
|    229 |  7500 | `		for( i = 0 ; i < pObj->nodesetval->nodeNr ; i++ ){` |
|    139 |  7501 | `			xmlNodePtr pNode = pObj->nodesetval->nodeTab[i];` |
|      - |  7502 | `			phl_domnode *pWrap;` |
|      - |  7503 | `			ph7_value *pRes;` |
|    139 |  7504 | `			if( pNode == 0 ){` |
|    ! 0 |  7505 | `				continue;` |
|      - |  7506 | `			}` |
|    139 |  7507 | `			if( pNode->type == XML_NAMESPACE_DECL ){` |
|      - |  7508 | `				/*` |
|      - |  7509 | `				 * A namespace:: axis result. libxml hands the set a COPY that` |
|      - |  7510 | `` 				 * dies with the XPath object (xmlXPathNodeSetDupNs, its `next` `` |
|      - |  7511 | `				 * pointing at the element the axis ran ON), so the snapshot` |
|      - |  7512 | `				 * wraps the ORIGINAL in-scope declaration found back through` |
|      - |  7513 | `				 * that element -- as php answers it: a DOMNameSpaceNode whose` |
|      - |  7514 | `				 * parentNode is the axis element even for a declaration an` |
|      - |  7515 | `				 * ANCESTOR made, fresh per query, stored as the OBJECT itself` |
|      - |  7516 | `				 * (item() twice on one list is one object, php's answer too).` |
|      - |  7517 | `				 */` |
|     43 |  7518 | `				xmlNsPtr pNs = (xmlNsPtr)pNode;` |
|     43 |  7519 | `				xmlNodePtr pElem = (xmlNodePtr)pNs->next;` |
|      - |  7520 | `				xmlNsPtr pOrig;` |
|      - |  7521 | `				ph7_class_instance *pNsObj;` |
|     43 |  7522 | `				if( pElem == 0 \|\| pElem->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  7523 | `					continue; /* not derivable: no element behind the copy */` |
|      - |  7524 | `				}` |
|     43 |  7525 | `				pOrig = xmlSearchNs((xmlDocPtr)pDocNd->pNode,pElem,pNs->prefix);` |
|     43 |  7526 | `				if( pOrig == 0 ){` |
|    ! 0 |  7527 | `					continue;` |
|      - |  7528 | `				}` |
|     43 |  7529 | `				pNsObj = DomNewNsNode(pVm,pDoc,pDocNd->pShell,pOrig,pElem);` |
|     43 |  7530 | `				pRes = ph7_context_new_scalar(pCtx);` |
|     43 |  7531 | `				if( pNsObj == 0 \|\| pRes == 0 ){` |
|    ! 0 |  7532 | `					if( pNsObj ){` |
|    ! 0 |  7533 | `						PH7_ClassInstanceUnref(pNsObj);` |
|    ! 0 |  7534 | `					}` |
|    ! 0 |  7535 | `					break;` |
|      - |  7536 | `				}` |
|      - |  7537 | `				/* pRes CARRIES the constructor's reference (no bump here): the` |
|      - |  7538 | `				 * array's insert takes its own, and the call context's release` |
|      - |  7539 | `				 * of pRes at method end consumes ours -- ending at exactly the` |
|      - |  7540 | `				 * array's one. */` |
|     43 |  7541 | `				pRes->x.pOther = pNsObj;` |
|     43 |  7542 | `				pRes->iFlags = MEMOBJ_OBJ;` |
|     43 |  7543 | `				ph7_array_add_elem(pSnap,0,pRes);` |
|     43 |  7544 | `				continue;` |
|      - |  7545 | `			}` |
|     97 |  7546 | `			pWrap = DomNewRes(pVm,pDocNd->pShell,pNode);` |
|     97 |  7547 | `			pRes = ph7_context_new_scalar(pCtx);` |
|     97 |  7548 | `			if( pWrap == 0 \|\| pRes == 0 ){` |
|    ! 0 |  7549 | `				break;` |
|      - |  7550 | `			}` |
|     97 |  7551 | `			ph7_value_resource(pRes,pWrap);` |
|     97 |  7552 | `			ph7_array_add_elem(pSnap,0,pRes);` |
|     49 |  7553 | `		}` |
|     45 |  7554 | `	}` |
|     95 |  7555 | `	pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_SNAP,0,0,0,pSnap);` |
|     95 |  7556 | `	if( pList == 0 ){` |
|    ! 0 |  7557 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  7558 | `	}` |
|     95 |  7559 | `	PH7_NativeResultObject(pCtx,pList);` |
|     95 |  7560 | `	return PH7_OK;` |
|     48 |  7561 | `}` |
|      - |  7562 | `/*` |
|      - |  7563 | ` * The one evaluation body under DOMXPath::query and DOMXPath::evaluate. The` |
|      - |  7564 | ` * two differ only in what they make of the RESULT: query wants a node list` |
|      - |  7565 | ` * (a scalar gets the empty one), evaluate answers the XPath TYPE as php's` |
|      - |  7566 | ` * value -- boolean as bool, number as float, string as string, nodeset as` |
|      - |  7567 | ` * the same snapshot list. An expression that does not evaluate (bad grammar,` |
|      - |  7568 | ` * unknown function, unresolved prefix) answers false from both, with the` |
|      - |  7569 | ` * libxml diagnostics on the shared queue.` |
|      - |  7570 | ` */` |
|    232 |  7571 | `static int DomXPathEvalRun(ph7_context *pCtx,int nArg,ph7_value **apArg,` |
|      - |  7572 | `	const char *zMethod,int bTyped)` |
|      1 |  7573 | `{` |
|    233 |  7574 | `	ph7_vm *pVm = pCtx->pVm;` |
|    233 |  7575 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    233 |  7576 | `	ph7_class_instance *pDoc = pThis ? PH7_NativeAttrObj(pThis,XP_DOC) : 0;` |
|    233 |  7577 | `	phl_domnode *pDocNd = DomResOf(pDoc);` |
|    233 |  7578 | `	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    233 |  7579 | `	phl_domnode *pCtxNd = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;` |
|      - |  7580 | ``	/* php's stub says `= true`, but the live default of the third argument is`` |
|      - |  7581 | `	 * the registerNodeNamespaces PROPERTY (the constructor's second argument` |
|      - |  7582 | `	 * lands there, and a later property write moves the default with it). */` |
|    233 |  7583 | `	int bRegNodeNs = nArg > 2 ? ph7_value_to_bool(apArg[2])` |
|    227 |  7584 | `		: (pThis ? PH7_NativeAttrTruthy(pThis,XP_NSDEF) : 1);` |
|      - |  7585 | `	xmlXPathContextPtr pXCtx;` |
|      - |  7586 | `	xmlXPathObjectPtr pObj;` |
|      - |  7587 | `	xmlNsPtr *aNodeNs;` |
|      - |  7588 | `	DomXPathFnCtx sFn;` |
|      - |  7589 | `	sxu32 nMark;` |
|      - |  7590 | `	sxi32 rc;` |
|    233 |  7591 | `	if( pDocNd == 0 ){` |
|    ! 0 |  7592 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  7593 | `		return PH7_OK;` |
|      - |  7594 | `	}` |
|    232 |  7595 | `	if( pCtxNd && pCtxNd->pNode` |
|     29 |  7596 | `	 && ((xmlNodePtr)pCtxNd->pNode)->doc != (xmlDocPtr)pDocNd->pNode ){` |
|      - |  7597 | `		/* php's plain Error, no DOM code -- a context node of another document` |
|      - |  7598 | `		 * (or of none, a constructed node) cannot anchor this evaluation. */` |
|      5 |  7599 | `		return PH7_VmThrowException(pCtx,"Error","Node from wrong document");` |
|      - |  7600 | `	}` |
|    229 |  7601 | `	aNodeNs = DomXPathCtxOpen(pCtx,pDocNd,pCtxNd,bRegNodeNs,&pXCtx);` |
|    229 |  7602 | `	if( pXCtx == 0 ){` |
|    ! 0 |  7603 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  7604 | `		return PH7_OK;` |
|      - |  7605 | `	}` |
|      - |  7606 | `	/* The PHP-function bridge rides this one evaluation: the record lives on` |
|      - |  7607 | `	 * THIS stack frame, and libxml carries a pointer to it as its lookup data. */` |
|    229 |  7608 | `	sFn.pCtx = pCtx;` |
|    229 |  7609 | `	sFn.pThis = pThis;` |
|    229 |  7610 | `	sFn.pDoc = pDoc;` |
|    229 |  7611 | `	sFn.pDocNd = pDocNd;` |
|    229 |  7612 | `	sFn.iErr = XP_FN_OK;` |
|    229 |  7613 | `	sFn.rcUnwound = 0;` |
|    229 |  7614 | `	SyBlobInit(&sFn.sErrName,&pVm->sAllocator);` |
|    229 |  7615 | `	xmlXPathRegisterFuncLookup(pXCtx,DomXPathFnLookup,&sFn);` |
|    229 |  7616 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    229 |  7617 | `	pObj = xmlXPathEvalExpression((const xmlChar *)zExpr,pXCtx);` |
|    229 |  7618 | `	PH7_LibxmlCaptureEnd(pVm,nMark,zMethod);` |
|    229 |  7619 | `	if( aNodeNs ){` |
|     45 |  7620 | `		pXCtx->namespaces = 0;` |
|     45 |  7621 | `		pXCtx->nsNr = 0;` |
|     45 |  7622 | `		xmlFree(aNodeNs);` |
|     22 |  7623 | `	}` |
|    229 |  7624 | `	if( sFn.rcUnwound != 0 \|\| sFn.iErr != XP_FN_OK ){` |
|      - |  7625 | `		/* A callback did not return, or the bridge parked a refusal it could` |
|      - |  7626 | `		 * not raise from inside libxml's recursion. Either way the evaluation` |
|      - |  7627 | `		 * is over and this is its answer -- raised HERE, where the enclosing` |
|      - |  7628 | `		 * catch runs with libxml already unwound. */` |
|     25 |  7629 | `		sxi32 rcFn = sFn.rcUnwound;` |
|     25 |  7630 | `		if( pObj ){` |
|    ! 0 |  7631 | `			xmlXPathFreeObject(pObj);` |
|    ! 0 |  7632 | `		}` |
|     25 |  7633 | `		xmlXPathFreeContext(pXCtx);` |
|     25 |  7634 | `		if( rcFn == 0 ){` |
|     23 |  7635 | `			rcFn = DomXPathFnRaise(pCtx,&sFn);` |
|     12 |  7636 | `		}else{` |
|      3 |  7637 | `			pCtx->nThrowRc = rcFn;` |
|      - |  7638 | `		}` |
|     25 |  7639 | `		SyBlobRelease(&sFn.sErrName);` |
|     25 |  7640 | `		return rcFn;` |
|      - |  7641 | `	}` |
|    205 |  7642 | `	SyBlobRelease(&sFn.sErrName);` |
|    205 |  7643 | `	if( pObj == 0 ){` |
|     27 |  7644 | `		xmlXPathFreeContext(pXCtx);` |
|     27 |  7645 | `		ph7_result_bool(pCtx,0);` |
|     27 |  7646 | `		return PH7_OK;` |
|      - |  7647 | `	}` |
|    179 |  7648 | `	if( !bTyped ){` |
|     85 |  7649 | `		rc = DomXPathResultList(pCtx,pDoc,pDocNd,pObj);` |
|     43 |  7650 | `	}else{` |
|     95 |  7651 | `		switch( pObj->type ){` |
|      2 |  7652 | `		case XPATH_BOOLEAN:` |
|      5 |  7653 | `			ph7_result_bool(pCtx,pObj->boolval);` |
|      5 |  7654 | `			rc = PH7_OK;` |
|      5 |  7655 | `			break;` |
|     15 |  7656 | `		case XPATH_NUMBER:` |
|     31 |  7657 | `			ph7_result_double(pCtx,pObj->floatval);` |
|     31 |  7658 | `			rc = PH7_OK;` |
|     31 |  7659 | `			break;` |
|     25 |  7660 | `		case XPATH_STRING:` |
|     51 |  7661 | `			ph7_result_string(pCtx,pObj->stringval ? (const char *)pObj->stringval : "",-1);` |
|     51 |  7662 | `			rc = PH7_OK;` |
|     51 |  7663 | `			break;` |
|      5 |  7664 | `		case XPATH_NODESET:` |
|     11 |  7665 | `			rc = DomXPathResultList(pCtx,pDoc,pDocNd,pObj);` |
|     11 |  7666 | `			break;` |
|    ! 0 |  7667 | `		default:` |
|    ! 0 |  7668 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  7669 | `			rc = PH7_OK;` |
|    ! 0 |  7670 | `			break;` |
|      - |  7671 | `		}` |
|      - |  7672 | `	}` |
|    179 |  7673 | `	xmlXPathFreeObject(pObj);` |
|    179 |  7674 | `	xmlXPathFreeContext(pXCtx);` |
|    179 |  7675 | `	return rc;` |
|    117 |  7676 | `}` |
|      - |  7677 | `/*` |
|      - |  7678 | ` * DOMXPath::query(string $expression, ?DOMNode $contextNode = null,` |
|      - |  7679 | ` *                 bool $registerNodeNS = true): DOMNodeList\|false` |
|      - |  7680 | ` */` |
|    100 |  7681 | `DOM_METHOD(vm_builtin_DOMXPath_query)` |
|      1 |  7682 | `{` |
|    101 |  7683 | `	return DomXPathEvalRun(pCtx,nArg,apArg,"DOMXPath::query",0);` |
|      1 |  7684 | `}` |
|      - |  7685 | `/*` |
|      - |  7686 | ` * DOMXPath::evaluate(string $expression, ?DOMNode $contextNode = null,` |
|      - |  7687 | ` *                    bool $registerNodeNS = true): mixed` |
|      - |  7688 | ` */` |
|    132 |  7689 | `DOM_METHOD(vm_builtin_DOMXPath_evaluate)` |
|      1 |  7690 | `{` |
|    133 |  7691 | `	return DomXPathEvalRun(pCtx,nArg,apArg,"DOMXPath::evaluate",1);` |
|      1 |  7692 | `}` |
|      - |  7693 | `/* DOMXPath::__construct(DOMDocument $document, bool $registerNodeNS = true) */` |
|     74 |  7694 | `DOM_METHOD(vm_builtin_DOMXPath_construct)` |
|      1 |  7695 | `{` |
|     75 |  7696 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     75 |  7697 | `	if( pThis && nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|    112 |  7698 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,XP_DOC,` |
|     74 |  7699 | `			(ph7_class_instance *)apArg[0]->x.pOther);` |
|     37 |  7700 | `	}` |
|     75 |  7701 | `	if( pThis && nArg > 1 ){` |
|      7 |  7702 | `		PH7_NativeSetAttrBool(pCtx->pVm,pThis,XP_NSDEF,` |
|      4 |  7703 | `			ph7_value_to_bool(apArg[1]));` |
|      2 |  7704 | `	}` |
|     75 |  7705 | `	return PH7_OK;` |
|      1 |  7706 | `}` |
|      - |  7707 | `/*` |
|      - |  7708 | ` * DOMXPath::registerNamespace(string $prefix, string $namespace): bool` |
|      - |  7709 | ` *` |
|      - |  7710 | ` * php hands the pair to xmlXPathRegisterNs on its persistent context and` |
|      - |  7711 | ` * answers its status: only the EMPTY prefix refuses (an invalid NCName one is` |
|      - |  7712 | ` * taken, and an empty URI is a registration too -- the prefix then resolves,` |
|      - |  7713 | ` * to a namespace nothing is in). Here the pair goes into the per-object table` |
|      - |  7714 | ` * the next evaluation replays.` |
|      - |  7715 | ` */` |
|     40 |  7716 | `DOM_METHOD(vm_builtin_DOMXPath_registerNamespace)` |
|      1 |  7717 | `{` |
|     41 |  7718 | `	ph7_vm *pVm = pCtx->pVm;` |
|     41 |  7719 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     41 |  7720 | `	int nPfx = 0;` |
|     41 |  7721 | `	const char *zPfx = nArg > 0 ? ph7_value_to_string(apArg[0],&nPfx) : "";` |
|      - |  7722 | `	ph7_hashmap *pMap;` |
|      - |  7723 | `	ph7_value sKey,sVal;` |
|     41 |  7724 | `	if( nPfx < 1 \|\| nArg < 2 ){` |
|      3 |  7725 | `		ph7_result_bool(pCtx,0);` |
|      3 |  7726 | `		return PH7_OK;` |
|      - |  7727 | `	}` |
|     39 |  7728 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_NSREG);` |
|     39 |  7729 | `	if( pMap == 0 ){` |
|    ! 0 |  7730 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  7731 | `		return PH7_OK;` |
|      - |  7732 | `	}` |
|     39 |  7733 | `	PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     39 |  7734 | `	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);` |
|     39 |  7735 | `	PH7_MemObjInitFromString(pVm,&sVal,0);` |
|      - |  7736 | `	{` |
|     39 |  7737 | `		int nUri = 0;` |
|     39 |  7738 | `		const char *zUri = ph7_value_to_string(apArg[1],&nUri);` |
|     39 |  7739 | `		PH7_MemObjStringAppend(&sVal,zUri,(sxu32)nUri);` |
|      - |  7740 | `	}` |
|     39 |  7741 | `	PH7_HashmapInsert(pMap,&sKey,&sVal);` |
|     39 |  7742 | `	PH7_MemObjRelease(&sKey);` |
|     39 |  7743 | `	PH7_MemObjRelease(&sVal);` |
|     39 |  7744 | `	ph7_result_bool(pCtx,1);` |
|     39 |  7745 | `	return PH7_OK;` |
|     21 |  7746 | `}` |
|      - |  7747 |  |
|      - |  7748 | `/* One row of the $restrict ARRAY: the value must be callable, and the NAME an` |
|      - |  7749 | ` * expression calls it by is the string key when there is one -- php's alias --` |
|      - |  7750 | ` * and otherwise the value coerced to a string (an array callable therefore` |
|      - |  7751 | ` * registers under "Array", with php's own conversion notice). */` |
|      - |  7752 | `struct DomXPathRestrict {` |
|      - |  7753 | `	ph7_context *pCtx;` |
|      - |  7754 | `	ph7_hashmap *pMap;` |
|      - |  7755 | `	sxi32 rc;` |
|      - |  7756 | `};` |
|     16 |  7757 | `static int DomXPathRestrictRow(ph7_value *pKey,ph7_value *pVal,void *pUserData)` |
|      1 |  7758 | `{` |
|     17 |  7759 | `	struct DomXPathRestrict *pWalk = (struct DomXPathRestrict *)pUserData;` |
|     17 |  7760 | `	ph7_vm *pVm = pWalk->pCtx->pVm;` |
|      - |  7761 | `	char zBuf[128];` |
|      - |  7762 | `	const char *zWhy;` |
|     17 |  7763 | `	if( pWalk->rc != PH7_OK ){` |
|    ! 0 |  7764 | `		return PH7_OK;` |
|      - |  7765 | `	}` |
|     17 |  7766 | `	zWhy = PH7_VmCallableReason(pVm,pVal,zBuf,(int)sizeof(zBuf));` |
|     17 |  7767 | `	if( zWhy ){` |
|      7 |  7768 | `		pWalk->rc = PH7_VmThrowException(pWalk->pCtx,"TypeError",` |
|      - |  7769 | `			"DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be an array "` |
|      2 |  7770 | `			"with valid callbacks as values, %s",zWhy);` |
|      5 |  7771 | `		return PH7_ABORT;` |
|      - |  7772 | `	}` |
|     15 |  7773 | `	if( pKey && ph7_value_is_string(pKey) ){` |
|      5 |  7774 | `		int nKey = 0;` |
|      5 |  7775 | `		const char *zKey = ph7_value_to_string(pKey,&nKey);` |
|      5 |  7776 | `		DomXPathMapPut(pVm,pWalk->pMap,zKey,nKey,pVal);` |
|      3 |  7777 | `	}else{` |
|      - |  7778 | `		/* ph7_value_to_string COERCES in place, which would rewrite the map's` |
|      - |  7779 | `		 * own value; name off a copy. */` |
|      - |  7780 | `		ph7_value sName;` |
|      9 |  7781 | `		int nName = 0;` |
|      - |  7782 | `		const char *zName;` |
|      9 |  7783 | `		PH7_MemObjInit(pVm,&sName);` |
|      9 |  7784 | `		PH7_MemObjStore(pVal,&sName);` |
|      9 |  7785 | `		zName = ph7_value_to_string(&sName,&nName);` |
|      9 |  7786 | `		DomXPathMapPut(pVm,pWalk->pMap,zName,nName,pVal);` |
|      9 |  7787 | `		PH7_MemObjRelease(&sName);` |
|      - |  7788 | `	}` |
|     13 |  7789 | `	return PH7_OK;` |
|      9 |  7790 | `}` |
|      - |  7791 | `/*` |
|      - |  7792 | ` * DOMXPath::registerPhpFunctions(array\|string\|null $restrict = null): void` |
|      - |  7793 | ` *` |
|      - |  7794 | ` * Bare (or null) opens the door to ANY callable name; a string or an array` |
|      - |  7795 | ` * restricts it to the named ones, accumulating across calls -- a later bare` |
|      - |  7796 | ` * call re-opens without forgetting the table, and a later restriction closes` |
|      - |  7797 | ` * it again with everything registered so far still reachable. Each name is` |
|      - |  7798 | ` * screened for callability HERE, so an evaluation never has to.` |
|      - |  7799 | ` */` |
|     32 |  7800 | `DOM_METHOD(vm_builtin_DOMXPath_registerPhpFunctions)` |
|      1 |  7801 | `{` |
|     33 |  7802 | `	ph7_vm *pVm = pCtx->pVm;` |
|     33 |  7803 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7804 | `	ph7_hashmap *pMap;` |
|     33 |  7805 | `	if( pThis == 0 ){` |
|    ! 0 |  7806 | `		return PH7_OK;` |
|      - |  7807 | `	}` |
|     33 |  7808 | `	if( nArg < 1 \|\| ph7_value_is_null(apArg[0]) ){` |
|     15 |  7809 | `		PH7_NativeSetAttrInt(pVm,pThis,XP_FNMODE,XP_MODE_ALL);` |
|     15 |  7810 | `		return PH7_OK;` |
|      - |  7811 | `	}` |
|     19 |  7812 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_FNREG);` |
|     19 |  7813 | `	if( pMap == 0 ){` |
|    ! 0 |  7814 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  7815 | `	}` |
|      - |  7816 | `	/* The mode moves FIRST, and each row is taken as it is screened: php's` |
|      - |  7817 | `	 * refusal leaves the object restricted with everything registered up to` |
|      - |  7818 | `` 	 * the bad row -- `registerPhpFunctions(['strrev','nope','strtolower'])` `` |
|      - |  7819 | `	 * throws, and afterwards strrev runs while strtolower does not. */` |
|     19 |  7820 | `	PH7_NativeSetAttrInt(pVm,pThis,XP_FNMODE,XP_MODE_LIST);` |
|     19 |  7821 | `	if( ph7_value_is_array(apArg[0]) ){` |
|      - |  7822 | `		struct DomXPathRestrict sWalk;` |
|     13 |  7823 | `		sWalk.pCtx = pCtx;` |
|     13 |  7824 | `		sWalk.pMap = pMap;` |
|     13 |  7825 | `		sWalk.rc = PH7_OK;` |
|     13 |  7826 | `		ph7_array_walk(apArg[0],DomXPathRestrictRow,&sWalk);` |
|     13 |  7827 | `		if( sWalk.rc != PH7_OK ){` |
|      5 |  7828 | `			return sWalk.rc;` |
|      - |  7829 | `		}` |
|      5 |  7830 | `	}else{` |
|      - |  7831 | `		char zBuf[128];` |
|      7 |  7832 | `		const char *zWhy = PH7_VmCallableReason(pVm,apArg[0],zBuf,(int)sizeof(zBuf));` |
|      7 |  7833 | `		int nName = 0;` |
|      - |  7834 | `		const char *zName;` |
|      7 |  7835 | `		if( zWhy ){` |
|      4 |  7836 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  7837 | `				"DOMXPath::registerPhpFunctions(): Argument #1 ($restrict) must be a callable, %s",` |
|      1 |  7838 | `				zWhy);` |
|      - |  7839 | `		}` |
|      5 |  7840 | `		zName = ph7_value_to_string(apArg[0],&nName);` |
|      5 |  7841 | `		DomXPathMapPut(pVm,pMap,zName,nName,apArg[0]);` |
|      - |  7842 | `	}` |
|     13 |  7843 | `	return PH7_OK;` |
|     17 |  7844 | `}` |
|      - |  7845 | `/* php's callback NAME grammar for registerPhpFunctionNS: an XML NCName, which` |
|      - |  7846 | ` * is what an expression can spell as a function name. */` |
|     18 |  7847 | `static int DomXPathIsCallbackName(const char *zName,int nName)` |
|      1 |  7848 | `{` |
|      - |  7849 | `	int i;` |
|     19 |  7850 | `	if( nName < 1 ){` |
|      3 |  7851 | `		return 0;` |
|      - |  7852 | `	}` |
|     17 |  7853 | `	if( xmlValidateNCName((const xmlChar *)zName,0) != 0 ){` |
|      5 |  7854 | `		return 0;` |
|      - |  7855 | `	}` |
|      - |  7856 | `	/* xmlValidateNCName reads to the NUL, and a name may not carry one. */` |
|     67 |  7857 | `	for( i = 0 ; i < nName ; ++i ){` |
|     55 |  7858 | `		if( zName[i] == 0 ){` |
|    ! 0 |  7859 | `			return 0;` |
|      - |  7860 | `		}` |
|     28 |  7861 | `	}` |
|     13 |  7862 | `	return (int)SyStrlen(zName) == nName;` |
|     10 |  7863 | `}` |
|      - |  7864 | `/*` |
|      - |  7865 | ` * DOMXPath::registerPhpFunctionNS(string $namespaceURI, string $name,` |
|      - |  7866 | ` *                                 callable $callable): void` |
|      - |  7867 | ` *` |
|      - |  7868 | ` * php 8.4's narrow door: one callable under one name in the caller's OWN` |
|      - |  7869 | `` * namespace -- no `php:function("name")` indirection, and independent of`` |
|      - |  7870 | ` * registerPhpFunctions' mode (it neither needs it nor opens it). php's own` |
|      - |  7871 | ` * URI is refused, the name must be an NCName, and the callable is screened` |
|      - |  7872 | ` * here.` |
|      - |  7873 | ` */` |
|     20 |  7874 | `DOM_METHOD(vm_builtin_DOMXPath_registerPhpFunctionNS)` |
|      1 |  7875 | `{` |
|     21 |  7876 | `	ph7_vm *pVm = pCtx->pVm;` |
|     21 |  7877 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     21 |  7878 | `	int nUri = 0,nName = 0;` |
|     21 |  7879 | `	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],&nUri) : "";` |
|     21 |  7880 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],&nName) : "";` |
|      - |  7881 | `	char zBuf[128];` |
|      - |  7882 | `	const char *zWhy;` |
|      - |  7883 | `	ph7_hashmap *pMap;` |
|      - |  7884 | `	SyBlob sKey;` |
|     21 |  7885 | `	if( pThis == 0 \|\| nArg < 3 ){` |
|    ! 0 |  7886 | `		return PH7_OK;` |
|      - |  7887 | `	}` |
|     21 |  7888 | `	if( nUri == (int)sizeof(XP_PHPNS)-1 && SyMemcmp(zUri,XP_PHPNS,(sxu32)nUri) == 0 ){` |
|      3 |  7889 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  7890 | `			"DOMXPath::registerPhpFunctionNS(): Argument #1 ($namespaceURI) must not be "` |
|      - |  7891 | `			"\"%s\" because it is reserved by PHP",XP_PHPNS);` |
|      - |  7892 | `	}` |
|     19 |  7893 | `	if( !DomXPathIsCallbackName(zName,nName) ){` |
|      7 |  7894 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  7895 | `			"DOMXPath::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name");` |
|      - |  7896 | `	}` |
|     13 |  7897 | `	zWhy = PH7_VmCallableReason(pVm,apArg[2],zBuf,(int)sizeof(zBuf));` |
|     13 |  7898 | `	if( zWhy ){` |
|      4 |  7899 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  7900 | `			"DOMXPath::registerPhpFunctionNS(): Argument #3 ($callable) must be a valid callback, %s",` |
|      1 |  7901 | `			zWhy);` |
|      - |  7902 | `	}` |
|     11 |  7903 | `	pMap = DomXPathSlotMap(pVm,pThis,XP_NSFN);` |
|     11 |  7904 | `	if( pMap == 0 ){` |
|    ! 0 |  7905 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  7906 | `	}` |
|      - |  7907 | `	/* One key from the pair: a URI cannot carry \x01, so the join is` |
|      - |  7908 | `	 * unambiguous without escaping. */` |
|     11 |  7909 | `	SyBlobInit(&sKey,&pVm->sAllocator);` |
|     11 |  7910 | `	SyBlobAppend(&sKey,zUri,(sxu32)nUri);` |
|     11 |  7911 | `	SyBlobAppend(&sKey,"\1",1);` |
|     11 |  7912 | `	SyBlobAppend(&sKey,zName,(sxu32)nName);` |
|     11 |  7913 | `	DomXPathMapPut(pVm,pMap,(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey),apArg[2]);` |
|     11 |  7914 | `	SyBlobRelease(&sKey);` |
|     11 |  7915 | `	return PH7_OK;` |
|     11 |  7916 | `}` |
|      - |  7917 |  |
|      - |  7918 | `/* ===== Schema validation ===== */` |
|      - |  7919 |  |
|      - |  7920 | `/*` |
|      - |  7921 | ` * The four schema doors -- {XML Schema, RelaxNG} x {a FILE, a STRING} -- and` |
|      - |  7922 | `` * php's `validate()` beside them, all one shape:`` |
|      - |  7923 | ` *` |
|      - |  7924 | ` *   parse the schema (loudly: every libxml complaint reaches the caller's` |
|      - |  7925 | ` *   error handler), and if that fails say "Invalid Schema" / "Invalid RelaxNG"` |
|      - |  7926 | ` *   and answer false; otherwise validate the document and answer whether it` |
|      - |  7927 | ` *   came back clean.` |
|      - |  7928 | ` *` |
|      - |  7929 | ` * Only the pair of libxml families differs, so the switch is four calls wide` |
|      - |  7930 | ` * and the plumbing -- the argument screens, the diagnostic capture, the` |
|      - |  7931 | ` * refusals -- is written once.  The names a caller sees are php's: a filename` |
|      - |  7932 | ` * that is empty or carries a NUL is a ValueError naming the argument, raised` |
|      - |  7933 | ` * before anything is opened.` |
|      - |  7934 | ` */` |
|      - |  7935 | `#define DOM_VAL_SCHEMA 0` |
|      - |  7936 | `#define DOM_VAL_RELAX  1` |
|      - |  7937 |  |
|      - |  7938 | `/*` |
|      - |  7939 | ` * Schema, RelaxNG and DTD-validity diagnostics: onto the shared per-VM queue` |
|      - |  7940 | ` * through PH7_LibxmlQueueError, exactly like the global structured handler.` |
|      - |  7941 | ` *` |
|      - |  7942 | ` * php installs libxml's printf-style pair here instead, which is why its` |
|      - |  7943 | ` * validation diagnostics read as libxml writes them -- "I/O warning : failed` |
|      - |  7944 | ` * to load external entity ...", a parse error over three lines with the` |
|      - |  7945 | ` * offending source and a caret under it -- while every message this engine` |
|      - |  7946 | ` * drains is one structured record with its location appended.  The structured` |
|      - |  7947 | ` * handler is the one this file must keep: it is also what feeds` |
|      - |  7948 | `` * `libxml_get_errors()`, and php's own switches to exactly this shape once`` |
|      - |  7949 | `` * `libxml_use_internal_errors(true)` is on.  The ANSWERS agree; the wording of`` |
|      - |  7950 | ` * a failure does not (the error-format class).` |
|      - |  7951 | ` */` |
|      - |  7952 | `#if LIBXML_VERSION >= 21200` |
|     12 |  7953 | `static void DomSchemaErr(void *pUserData,const xmlError *pErr)` |
|      - |  7954 | `#else` |
|      8 |  7955 | `static void DomSchemaErr(void *pUserData,xmlErrorPtr pErr)` |
|      - |  7956 | `#endif` |
|      1 |  7957 | `{` |
|     21 |  7958 | `	if( pErr == 0 ){` |
|    ! 0 |  7959 | `		return;` |
|      - |  7960 | `	}` |
|     33 |  7961 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|     20 |  7962 | `		pErr->int2,pErr->message,pErr->file);` |
|     13 |  7963 | `}` |
|      - |  7964 | `/* php's own last word when a schema will not parse, under the method's name. */` |
|      8 |  7965 | `static void DomValidateSaySo(ph7_vm *pVm,const char *zFn,const char *zWhat)` |
|      1 |  7966 | `{` |
|      - |  7967 | `	SyBlob sMsg;` |
|      9 |  7968 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      9 |  7969 | `	SyBlobFormat(&sMsg,"%s(): %s",zFn,zWhat);` |
|      9 |  7970 | `	SyBlobNullAppend(&sMsg);` |
|      9 |  7971 | `	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));` |
|      9 |  7972 | `	SyBlobRelease(&sMsg);` |
|      9 |  7973 | `}` |
|      - |  7974 | `/*` |
|      - |  7975 | ` * The argument every schema door takes: a filename or the schema itself. The` |
|      - |  7976 | ` * two refusals are php's own and answer before any parse.` |
|      - |  7977 | ` */` |
|     36 |  7978 | `static int DomValidateArg(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,` |
|      - |  7979 | `	const char *zFn,const char **pzSrc,int *pnSrc,int *pRc)` |
|      1 |  7980 | `{` |
|     37 |  7981 | `	int nSrc = 0;` |
|     37 |  7982 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";` |
|     37 |  7983 | `	const char *zParam = bFile ? "filename" : "source";` |
|     37 |  7984 | `	if( bFile && nSrc != (int)SyStrlen(zSrc) ){` |
|      4 |  7985 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      1 |  7986 | `			"%s(): Argument #1 ($%s) must not contain any null bytes",zFn,zParam);` |
|      3 |  7987 | `		return 0;` |
|      - |  7988 | `	}` |
|     35 |  7989 | `	if( nSrc < 1 ){` |
|     13 |  7990 | `		*pRc = PH7_VmThrowException(pCtx,"ValueError",` |
|      4 |  7991 | `			"%s(): Argument #1 ($%s) must not be empty",zFn,zParam);` |
|      9 |  7992 | `		return 0;` |
|      - |  7993 | `	}` |
|     27 |  7994 | `	*pzSrc = zSrc;` |
|     27 |  7995 | `	*pnSrc = nSrc;` |
|     27 |  7996 | `	return 1;` |
|     19 |  7997 | `}` |
|     36 |  7998 | `static int DomValidateRun(ph7_context *pCtx,int nArg,ph7_value **apArg,int iKind,` |
|      - |  7999 | `	int bFile,const char *zFn)` |
|      1 |  8000 | `{` |
|     37 |  8001 | `	ph7_vm *pVm = pCtx->pVm;` |
|     37 |  8002 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|     37 |  8003 | `	int nSrc = 0,rc = PH7_OK,iRc;` |
|     37 |  8004 | `	const char *zSrc = "";` |
|      - |  8005 | `	sxu32 nMark;` |
|      - |  8006 | `	/* php reads the option word from the SCHEMA pair only; RelaxNG's two` |
|      - |  8007 | `	 * declare no second parameter at all. LIBXML_SCHEMA_CREATE is the one bit` |
|      - |  8008 | `	 * it acts on -- "write the schema's default values into the document". */` |
|     31 |  8009 | `	int bCreate = iKind == DOM_VAL_SCHEMA && nArg > 1` |
|     47 |  8010 | `		&& (ph7_value_to_int(apArg[1]) & XML_SCHEMA_VAL_VC_I_CREATE) != 0;` |
|     37 |  8011 | `	if( !DomValidateArg(pCtx,nArg,apArg,bFile,zFn,&zSrc,&nSrc,&rc) ){` |
|     11 |  8012 | `		return rc;` |
|      - |  8013 | `	}` |
|     27 |  8014 | `	if( pDocNd == 0 ){` |
|    ! 0 |  8015 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  8016 | `		return PH7_OK;` |
|      - |  8017 | `	}` |
|     27 |  8018 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     27 |  8019 | `	if( iKind == DOM_VAL_SCHEMA ){` |
|     14 |  8020 | `		xmlSchemaParserCtxtPtr pParser = bFile ? xmlSchemaNewParserCtxt(zSrc)` |
|     11 |  8021 | `		                                       : xmlSchemaNewMemParserCtxt(zSrc,nSrc);` |
|      - |  8022 | `		xmlSchemaPtr pSchema;` |
|      - |  8023 | `		xmlSchemaValidCtxtPtr pValid;` |
|     17 |  8024 | `		if( pParser == 0 ){` |
|    ! 0 |  8025 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    ! 0 |  8026 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  8027 | `			return PH7_OK;` |
|      - |  8028 | `		}` |
|     17 |  8029 | `		xmlSchemaSetParserStructuredErrors(pParser,DomSchemaErr,pVm);` |
|     17 |  8030 | `		pSchema = xmlSchemaParse(pParser);` |
|     17 |  8031 | `		xmlSchemaFreeParserCtxt(pParser);` |
|     17 |  8032 | `		if( pSchema == 0 ){` |
|      5 |  8033 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|      5 |  8034 | `			DomValidateSaySo(pVm,zFn,"Invalid Schema");` |
|      5 |  8035 | `			ph7_result_bool(pCtx,0);` |
|      5 |  8036 | `			return PH7_OK;` |
|      - |  8037 | `		}` |
|     13 |  8038 | `		pValid = xmlSchemaNewValidCtxt(pSchema);` |
|     13 |  8039 | `		if( pValid == 0 ){` |
|    ! 0 |  8040 | `			xmlSchemaFree(pSchema);` |
|    ! 0 |  8041 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    ! 0 |  8042 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  8043 | `			return PH7_OK;` |
|      - |  8044 | `		}` |
|     13 |  8045 | `		if( bCreate ){` |
|      3 |  8046 | `			xmlSchemaSetValidOptions(pValid,XML_SCHEMA_VAL_VC_I_CREATE);` |
|      1 |  8047 | `		}` |
|     13 |  8048 | `		xmlSchemaSetValidStructuredErrors(pValid,DomSchemaErr,pVm);` |
|     13 |  8049 | `		iRc = xmlSchemaValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);` |
|     13 |  8050 | `		xmlSchemaFreeValidCtxt(pValid);` |
|     13 |  8051 | `		xmlSchemaFree(pSchema);` |
|      7 |  8052 | `	}else{` |
|      9 |  8053 | `		xmlRelaxNGParserCtxtPtr pParser = bFile ? xmlRelaxNGNewParserCtxt(zSrc)` |
|      7 |  8054 | `		                                        : xmlRelaxNGNewMemParserCtxt(zSrc,nSrc);` |
|      - |  8055 | `		xmlRelaxNGPtr pSchema;` |
|      - |  8056 | `		xmlRelaxNGValidCtxtPtr pValid;` |
|     11 |  8057 | `		if( pParser == 0 ){` |
|    ! 0 |  8058 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    ! 0 |  8059 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  8060 | `			return PH7_OK;` |
|      - |  8061 | `		}` |
|     11 |  8062 | `		xmlRelaxNGSetParserStructuredErrors(pParser,DomSchemaErr,pVm);` |
|     11 |  8063 | `		pSchema = xmlRelaxNGParse(pParser);` |
|     11 |  8064 | `		xmlRelaxNGFreeParserCtxt(pParser);` |
|     11 |  8065 | `		if( pSchema == 0 ){` |
|      5 |  8066 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|      5 |  8067 | `			DomValidateSaySo(pVm,zFn,"Invalid RelaxNG");` |
|      5 |  8068 | `			ph7_result_bool(pCtx,0);` |
|      5 |  8069 | `			return PH7_OK;` |
|      - |  8070 | `		}` |
|      7 |  8071 | `		pValid = xmlRelaxNGNewValidCtxt(pSchema);` |
|      7 |  8072 | `		if( pValid == 0 ){` |
|    ! 0 |  8073 | `			xmlRelaxNGFree(pSchema);` |
|    ! 0 |  8074 | `			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|    ! 0 |  8075 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  8076 | `			return PH7_OK;` |
|      - |  8077 | `		}` |
|      7 |  8078 | `		xmlRelaxNGSetValidStructuredErrors(pValid,DomSchemaErr,pVm);` |
|      7 |  8079 | `		iRc = xmlRelaxNGValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);` |
|      7 |  8080 | `		xmlRelaxNGFreeValidCtxt(pValid);` |
|      7 |  8081 | `		xmlRelaxNGFree(pSchema);` |
|      - |  8082 | `	}` |
|     19 |  8083 | `	PH7_LibxmlCaptureEnd(pVm,nMark,zFn);` |
|     19 |  8084 | `	ph7_result_bool(pCtx,iRc == 0);` |
|     19 |  8085 | `	return PH7_OK;` |
|     19 |  8086 | `}` |
|      - |  8087 | `/* DOMDocument::schemaValidate(string $filename, int $flags = 0): bool */` |
|     14 |  8088 | `DOM_METHOD(vm_builtin_DOMDocument_schemaValidate)` |
|      1 |  8089 | `{` |
|     15 |  8090 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_SCHEMA,TRUE,"DOMDocument::schemaValidate");` |
|      1 |  8091 | `}` |
|      - |  8092 | `/* DOMDocument::schemaValidateSource(string $source, int $flags = 0): bool */` |
|      8 |  8093 | `DOM_METHOD(vm_builtin_DOMDocument_schemaValidateSource)` |
|      1 |  8094 | `{` |
|      9 |  8095 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_SCHEMA,FALSE,"DOMDocument::schemaValidateSource");` |
|      1 |  8096 | `}` |
|      - |  8097 | `/* DOMDocument::relaxNGValidate(string $filename): bool */` |
|      8 |  8098 | `DOM_METHOD(vm_builtin_DOMDocument_relaxNGValidate)` |
|      1 |  8099 | `{` |
|      9 |  8100 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_RELAX,TRUE,"DOMDocument::relaxNGValidate");` |
|      1 |  8101 | `}` |
|      - |  8102 | `/* DOMDocument::relaxNGValidateSource(string $source): bool */` |
|      6 |  8103 | `DOM_METHOD(vm_builtin_DOMDocument_relaxNGValidateSource)` |
|      1 |  8104 | `{` |
|      7 |  8105 | `	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_RELAX,FALSE,"DOMDocument::relaxNGValidateSource");` |
|      1 |  8106 | `}` |
|      - |  8107 | `/*` |
|      - |  8108 | ` * DOMDocument::validate(): bool -- against the document's OWN DTD, which is` |
|      - |  8109 | ` * the one question of the five that takes no argument. libxml's validity` |
|      - |  8110 | ` * complaints ("no DTD found!", "root and DTD name do not match") reach the` |
|      - |  8111 | ` * caller through the same per-VM queue every other diagnostic here does.` |
|      - |  8112 | ` */` |
|      8 |  8113 | `DOM_METHOD(vm_builtin_DOMDocument_validate)` |
|      1 |  8114 | `{` |
|      9 |  8115 | `	ph7_vm *pVm = pCtx->pVm;` |
|      9 |  8116 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|      - |  8117 | `	xmlValidCtxtPtr pValid;` |
|      - |  8118 | `	sxu32 nMark;` |
|      - |  8119 | `	int iRc;` |
|      4 |  8120 | `	SXUNUSED(nArg);` |
|      4 |  8121 | `	SXUNUSED(apArg);` |
|      9 |  8122 | `	if( pDocNd == 0 ){` |
|    ! 0 |  8123 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  8124 | `		return PH7_OK;` |
|      - |  8125 | `	}` |
|      9 |  8126 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      9 |  8127 | `	pValid = xmlNewValidCtxt();` |
|      9 |  8128 | `	if( pValid == 0 ){` |
|    ! 0 |  8129 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::validate");` |
|    ! 0 |  8130 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  8131 | `		return PH7_OK;` |
|      - |  8132 | `	}` |
|      9 |  8133 | `	iRc = xmlValidateDocument(pValid,(xmlDocPtr)pDocNd->pNode);` |
|      9 |  8134 | `	xmlFreeValidCtxt(pValid);` |
|      9 |  8135 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::validate");` |
|      9 |  8136 | `	ph7_result_bool(pCtx,iRc != 0);` |
|      9 |  8137 | `	return PH7_OK;` |
|      5 |  8138 | `}` |
|      - |  8139 | `/*` |
|      - |  8140 | ` * The MARKERS libxml leaves around everything it substituted.` |
|      - |  8141 | ` *` |
|      - |  8142 | ` * An XInclude pass wraps each replacement in an XML_XINCLUDE_START /` |
|      - |  8143 | ` * XML_XINCLUDE_END pair, which are nodes in the tree like any other: they` |
|      - |  8144 | `` * answer from `childNodes`, they shift every index after them, and the first`` |
|      - |  8145 | `` * child of an element whose only content was an `<xi:include>` is one of them`` |
|      - |  8146 | ` * rather than what was included.  php takes them out before answering, so the` |
|      - |  8147 | ` * document a caller gets back is the substituted one and nothing else.  They` |
|      - |  8148 | ` * are parked on the orphan set rather than freed, like every other node this` |
|      - |  8149 | ` * file unlinks.` |
|      - |  8150 | ` */` |
|     14 |  8151 | `static void DomDropXIncludeMarks(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |  8152 | `{` |
|      - |  8153 | `	xmlNodePtr pNext;` |
|     29 |  8154 | `	while( pNode ){` |
|     15 |  8155 | `		pNext = pNode->next;` |
|     15 |  8156 | `		if( pNode->type == XML_XINCLUDE_START \|\| pNode->type == XML_XINCLUDE_END ){` |
|      5 |  8157 | `			xmlUnlinkNode(pNode);` |
|      5 |  8158 | `			DomOrphanAdd(pShell,pNode);` |
|      3 |  8159 | `		}else{` |
|     11 |  8160 | `			DomDropXIncludeMarks(pShell,pNode->children);` |
|      - |  8161 | `		}` |
|     15 |  8162 | `		pNode = pNext;` |
|      1 |  8163 | `	}` |
|     15 |  8164 | `}` |
|      - |  8165 | `/*` |
|      - |  8166 | ` * DOMDocument::xinclude(int $options = 0): int\|false` |
|      - |  8167 | ` *` |
|      - |  8168 | ` * php answers the COUNT of substitutions libxml made, -1 when one of them` |
|      - |  8169 | ` * failed -- and FALSE when there were none at all, which is not an error and` |
|      - |  8170 | ` * is the one answer a caller has to screen for separately.` |
|      - |  8171 | ` */` |
|      6 |  8172 | `DOM_METHOD(vm_builtin_DOMDocument_xinclude)` |
|      1 |  8173 | `{` |
|      7 |  8174 | `	ph7_vm *pVm = pCtx->pVm;` |
|      7 |  8175 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|      7 |  8176 | `	int iOpts = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|      - |  8177 | `	sxu32 nMark;` |
|      - |  8178 | `	int nDone;` |
|      7 |  8179 | `	if( pDocNd == 0 ){` |
|    ! 0 |  8180 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  8181 | `		return PH7_OK;` |
|      - |  8182 | `	}` |
|      7 |  8183 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      7 |  8184 | `	nDone = xmlXIncludeProcessFlags((xmlDocPtr)pDocNd->pNode,iOpts);` |
|      7 |  8185 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::xinclude");` |
|      7 |  8186 | `	if( nDone >= 0 ){` |
|      7 |  8187 | `		DomDropXIncludeMarks(pDocNd->pShell,` |
|      4 |  8188 | `			((xmlDocPtr)pDocNd->pNode)->children);` |
|      2 |  8189 | `	}` |
|      7 |  8190 | `	if( nDone == 0 ){` |
|      3 |  8191 | `		ph7_result_bool(pCtx,0);` |
|      2 |  8192 | `	}else{` |
|      5 |  8193 | `		ph7_result_int(pCtx,nDone);` |
|      - |  8194 | `	}` |
|      7 |  8195 | `	return PH7_OK;` |
|      4 |  8196 | `}` |
|      - |  8197 |  |
|      - |  8198 | `/*` |
|      - |  8199 | ` * DOMDocument::registerNodeClass(string $baseClass, ?string $extendedClass): true` |
|      - |  8200 | ` *` |
|      - |  8201 | ` * php lets a program say which class a node should be WRAPPED in, per` |
|      - |  8202 | `` * document: register `MyElement` against `DOMElement` and every element of`` |
|      - |  8203 | ` * that document -- read from the tree or made by a factory -- comes back a` |
|      - |  8204 | ` * MyElement, so a walk can call the program's own methods on what it finds` |
|      - |  8205 | ` * instead of carrying a parallel table of its own.` |
|      - |  8206 | ` *` |
|      - |  8207 | ` * The lookup is by the class the extension would have used and by nothing` |
|      - |  8208 | `` * else: registering against `DOMNode` or `DOMCharacterData` changes NO`` |
|      - |  8209 | ` * wrapping, because an element is wrapped as a DOMElement and a text node as a` |
|      - |  8210 | ` * DOMText, and neither name is the one registered.` |
|      - |  8211 | ` *` |
|      - |  8212 | ` * The map is the document's, stored in a hidden slot beside its identity cache` |
|      - |  8213 | ` * and carried by a document CLONE the way the parser directives are.` |
|      - |  8214 | ` */` |
|      - |  8215 | `/* The map, materialized on the document the way its identity cache is. */` |
|   2748 |  8216 | `static ph7_hashmap * DomNodeClassMap(ph7_vm *pVm,ph7_class_instance *pDoc,int bMake)` |
|      5 |  8217 | `{` |
|   2753 |  8218 | `	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NCLS) : 0;` |
|   2753 |  8219 | `	if( pSlot == 0 \|\| (!bMake && (pSlot->iFlags & MEMOBJ_HASHMAP) == 0) ){` |
|   2711 |  8220 | `		return 0;` |
|      - |  8221 | `	}` |
|     44 |  8222 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     10 |  8223 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|    ! 0 |  8224 | `			return 0;` |
|      - |  8225 | `		}` |
|      4 |  8226 | `	}` |
|     44 |  8227 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|   1379 |  8228 | `}` |
|      - |  8229 | `/* The class a node of pDoc's tree is wrapped in: the registered one when the` |
|      - |  8230 | ` * document names it, php's own otherwise. */` |
|   2732 |  8231 | `static const char * DomWrapClassName(ph7_vm *pVm,ph7_class_instance *pDoc,int iKind,` |
|      - |  8232 | `	SyBlob *pOut)` |
|      5 |  8233 | `{` |
|   2737 |  8234 | `	const char *zBase = DomClassOfKind(iKind);` |
|   2737 |  8235 | `	ph7_hashmap *pMap = DomNodeClassMap(&(*pVm),pDoc,FALSE);` |
|   2737 |  8236 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  8237 | `	ph7_value sKey,*pHit;` |
|   2737 |  8238 | `	if( pMap == 0 ){` |
|   2711 |  8239 | `		return zBase;` |
|      - |  8240 | `	}` |
|     28 |  8241 | `	PH7_MemObjInitFromString(&(*pVm),&sKey,0);` |
|     28 |  8242 | `	PH7_MemObjStringAppend(&sKey,zBase,(sxu32)SyStrlen(zBase));` |
|     28 |  8243 | `	if( PH7_HashmapLookup(pMap,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|     18 |  8244 | `		pHit = HashmapExtractNodeValue(pEntry);` |
|     18 |  8245 | `		if( pHit && (pHit->iFlags & MEMOBJ_STRING) ){` |
|     18 |  8246 | `			SyBlobAppend(pOut,SyBlobData(&pHit->sBlob),SyBlobLength(&pHit->sBlob));` |
|     18 |  8247 | `			SyBlobNullAppend(pOut);` |
|     18 |  8248 | `			zBase = (const char *)SyBlobData(pOut);` |
|      8 |  8249 | `		}` |
|      8 |  8250 | `	}` |
|     28 |  8251 | `	PH7_MemObjRelease(&sKey);` |
|     28 |  8252 | `	return zBase;` |
|   1371 |  8253 | `}` |
|     26 |  8254 | `DOM_METHOD(vm_builtin_DOMDocument_registerNodeClass)` |
|      2 |  8255 | `{` |
|     28 |  8256 | `	ph7_vm *pVm = pCtx->pVm;` |
|     28 |  8257 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     28 |  8258 | `	int nBase = 0,nExt = 0;` |
|     28 |  8259 | `	const char *zBase = nArg > 0 ? ph7_value_to_string(apArg[0],&nBase) : "";` |
|     28 |  8260 | `	const char *zExt = (nArg > 1 && !ph7_value_is_null(apArg[1]))` |
|     38 |  8261 | `		? ph7_value_to_string(apArg[1],&nExt) : 0;` |
|     28 |  8262 | `	ph7_class *pBase,*pExt = 0,*pNode;` |
|      - |  8263 | `	ph7_hashmap *pMap;` |
|      - |  8264 | `	ph7_value sKey,sVal;` |
|     28 |  8265 | `	pBase = PH7_VmExtractClass(pVm,zBase,(sxu32)nBase,FALSE,0);` |
|     28 |  8266 | `	pNode = PH7_VmExtractClass(pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);` |
|     28 |  8267 | `	if( pBase == 0 \|\| pNode == 0 \|\| !PH7_VmInstanceOf(pBase,pNode) ){` |
|      7 |  8268 | `		return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  8269 | `			"DOMDocument::registerNodeClass(): Argument #1 ($baseClass) must be a "` |
|      2 |  8270 | `			"class name derived from DOMNode, %.*s given",nBase,zBase);` |
|      - |  8271 | `	}` |
|     24 |  8272 | `	if( zExt ){` |
|     22 |  8273 | `		pExt = PH7_VmExtractClass(pVm,zExt,(sxu32)nExt,FALSE,0);` |
|     22 |  8274 | `		if( pExt == 0 ){` |
|      4 |  8275 | `			return PH7_VmThrowException(pCtx,"TypeError",` |
|      - |  8276 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be "` |
|      1 |  8277 | `				"a valid class name or null, %.*s given",nExt,zExt);` |
|      - |  8278 | `		}` |
|     20 |  8279 | `		if( !PH7_VmInstanceOf(pExt,pBase) ){` |
|      - |  8280 | `			/* php's plain Error here, not a TypeError: the name IS a class, it` |
|      - |  8281 | `			 * is simply the wrong one. */` |
|      4 |  8282 | `			return PH7_VmThrowException(pCtx,"Error",` |
|      - |  8283 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be "` |
|      - |  8284 | `				"a class name derived from %z or null, %.*s given",` |
|      1 |  8285 | `				&pBase->sName,nExt,zExt);` |
|      - |  8286 | `		}` |
|     18 |  8287 | `		if( pExt->iFlags & PH7_CLASS_ABSTRACT ){` |
|      3 |  8288 | `			return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  8289 | `				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must "` |
|      - |  8290 | `				"not be an abstract class");` |
|      - |  8291 | `		}` |
|      7 |  8292 | `	}` |
|     18 |  8293 | `	pMap = DomNodeClassMap(pVm,pThis,TRUE);` |
|     18 |  8294 | `	if( pMap == 0 ){` |
|    ! 0 |  8295 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8296 | `	}` |
|      - |  8297 | `	/* Keyed by the base's OWN spelling, which is the one the wrap looks up. */` |
|     18 |  8298 | `	PH7_MemObjInitFromString(pVm,&sKey,&pBase->sName);` |
|     18 |  8299 | `	if( pExt ){` |
|     16 |  8300 | `		PH7_MemObjInitFromString(pVm,&sVal,&pExt->sName);` |
|     16 |  8301 | `		PH7_HashmapInsert(pMap,&sKey,&sVal);` |
|     16 |  8302 | `		PH7_MemObjRelease(&sVal);` |
|      9 |  8303 | `	}else{` |
|      3 |  8304 | `		ph7_hashmap_node *pEntry = 0;` |
|      3 |  8305 | `		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|      3 |  8306 | `			PH7_HashmapUnlinkNode(pEntry,TRUE);` |
|      1 |  8307 | `		}` |
|      - |  8308 | `	}` |
|     18 |  8309 | `	PH7_MemObjRelease(&sKey);` |
|     18 |  8310 | `	ph7_result_bool(pCtx,1);` |
|     18 |  8311 | `	return PH7_OK;` |
|     15 |  8312 | `}` |
|      - |  8313 |  |
|      - |  8314 | `/* ===== DOMImplementation ===== */` |
|      - |  8315 |  |
|      - |  8316 | `/*` |
|      - |  8317 | ` * php's factory for the two things that cannot be made from a document that` |
|      - |  8318 | ` * does not exist yet: a DOCTYPE, and a document with a namespaced root.` |
|      - |  8319 | ` *` |
|      - |  8320 | `` * It carries no state at all -- `new DOMImplementation` is enough, its three`` |
|      - |  8321 | `` * methods are ordinary instance methods, and `$doc->implementation` answers a`` |
|      - |  8322 | ` * FRESH one on every read.` |
|      - |  8323 | ` */` |
|      - |  8324 |  |
|      - |  8325 | `/* A node that belongs to NO document, wrapped and owned the way a constructed` |
|      - |  8326 | ` * one is: parked on the per-VM limbo shell, its own identity-cache holder. The` |
|      - |  8327 | ` * caller owns the reference. */` |
|     20 |  8328 | `static ph7_class_instance * DomLimboWrap(ph7_vm *pVm,xmlNodePtr pNode)` |
|      1 |  8329 | `{` |
|     21 |  8330 | `	const char *zClass = DomClassOfKind((int)pNode->type);` |
|     21 |  8331 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|     21 |  8332 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|     21 |  8333 | `	phl_xmldoc *pShell = pObj ? DomLimboShell(&(*pVm)) : 0;` |
|     21 |  8334 | `	phl_domnode *pRes = pShell ? DomNewRes(&(*pVm),pShell,pNode) : 0;` |
|     21 |  8335 | `	if( pRes == 0 ){` |
|    ! 0 |  8336 | `		if( pObj ){` |
|    ! 0 |  8337 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  8338 | `		}` |
|    ! 0 |  8339 | `		return 0;` |
|      - |  8340 | `	}` |
|     21 |  8341 | `	DomOrphanAdd(pShell,pNode);` |
|     21 |  8342 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|     21 |  8343 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pObj);` |
|     21 |  8344 | `	DomCacheStore(&(*pVm),pObj,pNode,pObj);` |
|     21 |  8345 | `	return pObj;` |
|     11 |  8346 | `}` |
|      - |  8347 | `/*` |
|      - |  8348 | ` * DOMImplementation::hasFeature(string $feature, string $version): bool` |
|      - |  8349 | ` *` |
|      - |  8350 | ` * php's table is two rows wide and the version is compared as a STRING: only` |
|      - |  8351 | ` * "1.0", "2.0" and "" are versions at all, and of those "Core" answers for` |
|      - |  8352 | ``  * "1.0" alone where "XML" answers for every one. So `hasFeature('Core','2.0')` `` |
|      - |  8353 | ``  * is false while `hasFeature('XML','2.0')` is true, and `hasFeature('Core','1')` `` |
|      - |  8354 | ` * -- a version that is not spelled the way the table spells it -- is false.` |
|      - |  8355 | ` */` |
|     22 |  8356 | `DOM_METHOD(vm_builtin_DOMImplementation_hasFeature)` |
|      1 |  8357 | `{` |
|     23 |  8358 | `	const char *zFeature = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     23 |  8359 | `	const char *zVersion = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     31 |  8360 | `	int bKnown = DomNameIs(zVersion,"1.0") \|\| DomNameIs(zVersion,"2.0")` |
|     27 |  8361 | `		\|\| zVersion[0] == 0;` |
|     49 |  8362 | `	ph7_result_bool(pCtx,bKnown` |
|     31 |  8363 | `		&& (DomNameIsCi(zFeature,"XML")` |
|     16 |  8364 | `			\|\| (DomNameIsCi(zFeature,"Core") && DomNameIs(zVersion,"1.0"))));` |
|     23 |  8365 | `	return PH7_OK;` |
|      1 |  8366 | `}` |
|      - |  8367 | `/*` |
|      - |  8368 | ` * DOMImplementation::createDocumentType(string $qualifiedName,` |
|      - |  8369 | ` *     string $publicId = '', string $systemId = ''): DOMDocumentType` |
|      - |  8370 | ` *` |
|      - |  8371 | `` * The name is not checked at ALL beyond being non-empty -- `1bad`, `a b` and`` |
|      - |  8372 | `` * `p:q:r` are each a doctype php builds without a word -- because nothing has`` |
|      - |  8373 | `` * parsed it: the name is the bytes the `<!DOCTYPE ...>` line will carry.`` |
|      - |  8374 | ` */` |
|     22 |  8375 | `DOM_METHOD(vm_builtin_DOMImplementation_createDocumentType)` |
|      1 |  8376 | `{` |
|     23 |  8377 | `	int nName = 0;` |
|     23 |  8378 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";` |
|     23 |  8379 | `	const char *zPub = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     23 |  8380 | `	const char *zSys = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|      - |  8381 | `	ph7_class_instance *pObj;` |
|      - |  8382 | `	xmlDtdPtr pDtd;` |
|     23 |  8383 | `	if( nName < 1 ){` |
|      3 |  8384 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|      - |  8385 | `			"DOMImplementation::createDocumentType(): Argument #1 ($qualifiedName) "` |
|      - |  8386 | `			"must not be empty");` |
|      - |  8387 | `	}` |
|     41 |  8388 | `	pDtd = xmlNewDtd(0,(const xmlChar *)zName,` |
|     20 |  8389 | `		zPub[0] ? (const xmlChar *)zPub : 0,` |
|     20 |  8390 | `		zSys[0] ? (const xmlChar *)zSys : 0);` |
|     21 |  8391 | `	if( pDtd == 0 ){` |
|    ! 0 |  8392 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8393 | `	}` |
|     21 |  8394 | `	pObj = DomLimboWrap(pCtx->pVm,(xmlNodePtr)pDtd);` |
|     21 |  8395 | `	if( pObj == 0 ){` |
|    ! 0 |  8396 | `		xmlFreeDtd(pDtd);` |
|    ! 0 |  8397 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8398 | `	}` |
|     21 |  8399 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     21 |  8400 | `	return PH7_OK;` |
|     12 |  8401 | `}` |
|      - |  8402 | `/*` |
|      - |  8403 | ` * DOMImplementation::createDocument(?string $namespace = null,` |
|      - |  8404 | ` *     string $qualifiedName = '', ?DOMDocumentType $doctype = null): DOMDocument` |
|      - |  8405 | ` *` |
|      - |  8406 | ` * An empty qualified name is a document with no root at all, which is what` |
|      - |  8407 | ` * makes the three-argument call with only a doctype meaningful.  The name is a` |
|      - |  8408 | `` * QName or nothing (`1bad` and `a:b:c` are the Namespace Error), and the`` |
|      - |  8409 | ` * namespace decides what becomes of its PREFIX: with a URI the element is` |
|      - |  8410 | ` * declared under it, and WITHOUT one the prefix is simply dropped -- php` |
|      - |  8411 | ` * builds the element from the local name and hangs the declaration on it` |
|      - |  8412 | `` * afterwards, so `createDocument('', 'p:root')` is `<root/>`.`` |
|      - |  8413 | ` *` |
|      - |  8414 | ` * A doctype that already belongs to a document is the Wrong Document Error,` |
|      - |  8415 | ` * so the same DOMDocumentType cannot seed two documents.` |
|      - |  8416 | ` */` |
|     26 |  8417 | `DOM_METHOD(vm_builtin_DOMImplementation_createDocument)` |
|      1 |  8418 | `{` |
|     27 |  8419 | `	ph7_vm *pVm = pCtx->pVm;` |
|     27 |  8420 | `	const xmlChar *zUri = DomArgUri(nArg,apArg,0);` |
|     27 |  8421 | `	int nName = 0;` |
|     27 |  8422 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],&nName) : "";` |
|     27 |  8423 | `	phl_domnode *pDtdNd = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? DomObjArg(apArg[2]) : 0;` |
|     27 |  8424 | `	xmlDtdPtr pDtd = pDtdNd ? (xmlDtdPtr)pDtdNd->pNode : 0;` |
|      - |  8425 | `	ph7_class *pClass;` |
|      - |  8426 | `	ph7_class_instance *pObj;` |
|      - |  8427 | `	phl_xmldoc *pShell;` |
|      - |  8428 | `	phl_domnode *pRes;` |
|      - |  8429 | `	xmlDocPtr pDoc;` |
|     27 |  8430 | `	xmlNodePtr pRoot = 0;` |
|     27 |  8431 | `	xmlNsPtr pNs = 0;` |
|     27 |  8432 | `	xmlChar *zPrefix = 0,*zLocal = 0;` |
|     27 |  8433 | `	if( pDtd && pDtd->doc ){` |
|      - |  8434 | `		/* php's own screen, and the reason a doctype seeds ONE document. */` |
|      3 |  8435 | `		return DomThrowAlways(pCtx,DOM_ERR_WRONG_DOC);` |
|      - |  8436 | `	}` |
|     25 |  8437 | `	if( nName > 0 ){` |
|     19 |  8438 | `		if( xmlValidateQName((const xmlChar *)zName,0) != 0 ){` |
|      9 |  8439 | `			return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  8440 | `		}` |
|     11 |  8441 | `		zLocal = xmlSplitQName2((const xmlChar *)zName,&zPrefix);` |
|     11 |  8442 | `		if( zLocal == 0 ){` |
|      7 |  8443 | `			zLocal = xmlStrdup((const xmlChar *)zName);` |
|      3 |  8444 | `		}` |
|     11 |  8445 | `		if( zUri && zUri[0] ){` |
|      - |  8446 | `			/* php asks libxml for the declaration BEFORE it has a node to hang` |
|      - |  8447 | ``			 * it on, and takes a refusal (the `xml` prefix over its own URI is`` |
|      - |  8448 | `			 * one) as the Namespace Error. */` |
|      5 |  8449 | `			pNs = xmlNewNs(0,zUri,zPrefix);` |
|      5 |  8450 | `			if( pNs == 0 ){` |
|    ! 0 |  8451 | `				if( zLocal ){` |
|    ! 0 |  8452 | `					xmlFree(zLocal);` |
|    ! 0 |  8453 | `				}` |
|    ! 0 |  8454 | `				if( zPrefix ){` |
|    ! 0 |  8455 | `					xmlFree(zPrefix);` |
|    ! 0 |  8456 | `				}` |
|    ! 0 |  8457 | `				return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);` |
|      - |  8458 | `			}` |
|      2 |  8459 | `		}` |
|      5 |  8460 | `	}` |
|     17 |  8461 | `	pDoc = xmlNewDoc((const xmlChar *)"1.0");` |
|     17 |  8462 | `	pClass = pDoc ? PH7_VmExtractClass(pVm,"DOMDocument",sizeof("DOMDocument")-1,FALSE,0) : 0;` |
|     17 |  8463 | `	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;` |
|     17 |  8464 | `	pShell = pObj ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;` |
|     17 |  8465 | `	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|     17 |  8466 | `	if( pRes == 0 ){` |
|    ! 0 |  8467 | `		if( pDoc && pShell == 0 ){` |
|    ! 0 |  8468 | `			xmlFreeDoc(pDoc);` |
|    ! 0 |  8469 | `		}` |
|    ! 0 |  8470 | `		if( pObj ){` |
|    ! 0 |  8471 | `			PH7_ClassInstanceUnref(pObj);` |
|    ! 0 |  8472 | `		}` |
|    ! 0 |  8473 | `		if( pNs ){` |
|    ! 0 |  8474 | `			xmlFreeNs(pNs);` |
|    ! 0 |  8475 | `		}` |
|    ! 0 |  8476 | `		if( zLocal ){` |
|    ! 0 |  8477 | `			xmlFree(zLocal);` |
|    ! 0 |  8478 | `		}` |
|    ! 0 |  8479 | `		if( zPrefix ){` |
|    ! 0 |  8480 | `			xmlFree(zPrefix);` |
|    ! 0 |  8481 | `		}` |
|    ! 0 |  8482 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  8483 | `	}` |
|     17 |  8484 | `	DomSetRes(pVm,pObj,pRes);` |
|     17 |  8485 | `	PH7_NativeSetAttrObj(pVm,pObj,DOM_DOC,pObj);` |
|     17 |  8486 | `	if( pDtd ){` |
|      - |  8487 | `		/* The doctype MOVES: it leaves the limbo shell for this document's` |
|      - |  8488 | `		 * tree, and every wrapper of it -- its own and its declarations' --` |
|      - |  8489 | `		 * re-homes on adoptNode's machinery, which is what makes the doctype` |
|      - |  8490 | ``		 * answer this document as its `ownerDocument` afterwards rather than`` |
|      - |  8491 | `		 * the holder it was its own. */` |
|      5 |  8492 | `		ph7_class_instance *pDtdObj = (ph7_class_instance *)apArg[2]->x.pOther;` |
|      5 |  8493 | `		DomOrphanRemove(pDtdNd->pShell,(xmlNodePtr)pDtd);` |
|      5 |  8494 | `		pDtd->doc = pDoc;` |
|      5 |  8495 | `		pDoc->intSubset = pDtd;` |
|      5 |  8496 | `		DomLinkLast((xmlNodePtr)pDoc,(xmlNodePtr)pDtd);` |
|      5 |  8497 | `		DomAdoptWrappers(pVm,pDtdObj,pObj,pShell,(xmlNodePtr)pDtd);` |
|      2 |  8498 | `	}` |
|     17 |  8499 | `	if( nName > 0 ){` |
|     11 |  8500 | `		pRoot = xmlNewDocNode(pDoc,0,zLocal,0);` |
|     11 |  8501 | `		if( pRoot ){` |
|     11 |  8502 | `			xmlDocSetRootElement(pDoc,pRoot);` |
|     11 |  8503 | `			if( pNs ){` |
|      5 |  8504 | `				pNs->next = pRoot->nsDef;` |
|      5 |  8505 | `				pRoot->nsDef = pNs;` |
|      5 |  8506 | `				xmlSetNs(pRoot,pNs);` |
|      5 |  8507 | `				pNs = 0;` |
|      2 |  8508 | `			}` |
|      5 |  8509 | `		}` |
|      5 |  8510 | `	}` |
|     17 |  8511 | `	if( pNs ){` |
|    ! 0 |  8512 | `		xmlFreeNs(pNs);` |
|    ! 0 |  8513 | `	}` |
|     17 |  8514 | `	if( zLocal ){` |
|     11 |  8515 | `		xmlFree(zLocal);` |
|      5 |  8516 | `	}` |
|     17 |  8517 | `	if( zPrefix ){` |
|      5 |  8518 | `		xmlFree(zPrefix);` |
|      2 |  8519 | `	}` |
|     17 |  8520 | `	PH7_NativeResultObject(pCtx,pObj);` |
|     17 |  8521 | `	return PH7_OK;` |
|     14 |  8522 | `}` |
|      - |  8523 |  |
|      - |  8524 | `/* ===== The shared __get dispatch ===== */` |
|      - |  8525 |  |
|      - |  8526 | `/*` |
|      - |  8527 | ` * DOMNode's virtual properties.` |
|      - |  8528 | ` *` |
|      - |  8529 | ` * php exposes these through property handlers on the class; PHL answers them` |
|      - |  8530 | ` * from __get, as the chunk did. Returns 1 when it recognised the name, so a` |
|      - |  8531 | ` * subclass's own __get can state its extras and then defer here -- which is` |
|      - |  8532 | `` * what `parent::__get($name)` did.`` |
|      - |  8533 | ` */` |
|   3066 |  8534 | `static int DomNodeProp(ph7_context *pCtx,const char *zName)` |
|      5 |  8535 | `{` |
|   3071 |  8536 | `	ph7_vm *pVm = pCtx->pVm;` |
|   3071 |  8537 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   3071 |  8538 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|   3071 |  8539 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   3071 |  8540 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   4582 |  8541 | `	int bIsDoc = pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE);` |
|   3071 |  8542 | `	if( DomNameIs(zName,"nodeName") ){` |
|    607 |  8543 | `		DomNodeName(pCtx,pNode);` |
|   2769 |  8544 | `	}else if( DomNameIs(zName,"nodeValue") ){` |
|     97 |  8545 | `		DomNodeValue(pCtx,pNode);` |
|   2419 |  8546 | `	}else if( DomNameIs(zName,"nodeType") ){` |
|    107 |  8547 | `		ph7_result_int(pCtx,DomNodeTypeOf(pNode));` |
|   2318 |  8548 | `	}else if( DomNameIs(zName,"textContent") ){` |
|     57 |  8549 | `		DomTextContent(pCtx,pNode);` |
|   2237 |  8550 | `	}else if( DomNameIs(zName,"parentNode") ){` |
|    125 |  8551 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);` |
|   2148 |  8552 | `	}else if( DomNameIs(zName,"firstChild") ){` |
|      - |  8553 | `		/* Through the entity-reference resolver: an ADOPTED constructed` |
|      - |  8554 | `		 * reference's raw children are cleared, and php's reader answers the` |
|      - |  8555 | `		 * document's declaration anyway.` |
|      - |  8556 | `		 *` |
|      - |  8557 | `		 * Both ends are gated on php's dom_node_children_valid, which the` |
|      - |  8558 | `		 * DOCTYPE is not: libxml links a DTD's declarations as its children` |
|      - |  8559 | ``		 * and php's `firstChild`/`lastChild`/`hasChildNodes()` answer null,`` |
|      - |  8560 | ``		 * null and false there all the same -- while `childNodes` (which does`` |
|      - |  8561 | `		 * NOT consult it) lists them. */` |
|    591 |  8562 | `		DomResultNodeOf(pCtx,pNd,DomNodeChildFirst(pNode));` |
|   1792 |  8563 | `	}else if( DomNameIs(zName,"lastChild") ){` |
|     87 |  8564 | `		DomResultNodeOf(pCtx,pNd,DomNodeChildLast(pNode));` |
|   1455 |  8565 | `	}else if( DomNameIs(zName,"nextSibling") ){` |
|     69 |  8566 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->next : 0);` |
|   1378 |  8567 | `	}else if( DomNameIs(zName,"previousSibling") ){` |
|     15 |  8568 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->prev : 0);` |
|   1337 |  8569 | `	}else if( DomNameIs(zName,"ownerDocument") ){` |
|      - |  8570 | `		/* A document has no owner document, which is also why DomWrap answers` |
|      - |  8571 | `		 * the document itself rather than a second wrapper for it. The NODE's` |
|      - |  8572 | `		 * document is the source of truth, not the $__doc slot: a constructed` |
|      - |  8573 | `		 * ownerless node's slot points at its own holder, and php answers` |
|      - |  8574 | `		 * null there until an insertion adopts it. */` |
|    135 |  8575 | `		DomResultWrap(pCtx,(bIsDoc \|\| pNode == 0 \|\| pNode->doc == 0) ? 0 : pDoc);` |
|   1263 |  8576 | `	}else if( DomNameIs(zName,"parentElement") ){` |
|      - |  8577 | ``		/* php's `?DOMElement`: the parent when it IS an element, so a root`` |
|      - |  8578 | `		 * element (whose parent is the document) answers null. An ATTRIBUTE` |
|      - |  8579 | `		 * answers its element -- libxml parents an attribute, and php reports` |
|      - |  8580 | ``		 * that parent from both this property and `parentNode`. */`` |
|     49 |  8581 | `		xmlNodePtr pPar = pNode ? pNode->parent : 0;` |
|     49 |  8582 | `		DomResultNodeOf(pCtx,pNd,(pPar && pPar->type == XML_ELEMENT_NODE) ? pPar : 0);` |
|   1173 |  8583 | `	}else if( DomNameIs(zName,"namespaceURI") ){` |
|    329 |  8584 | `		DomNamespaceUri(pCtx,pNode);` |
|    985 |  8585 | `	}else if( DomNameIs(zName,"prefix") ){` |
|    127 |  8586 | `		DomPrefix(pCtx,pNode);` |
|    758 |  8587 | `	}else if( DomNameIs(zName,"localName") ){` |
|    125 |  8588 | `		DomLocalName(pCtx,pNode);` |
|    633 |  8589 | `	}else if( DomNameIs(zName,"isConnected") ){` |
|     77 |  8590 | `		ph7_result_bool(pCtx,DomIsConnected(pNode));` |
|    533 |  8591 | `	}else if( DomNameIs(zName,"baseURI") ){` |
|      - |  8592 | ``		/* php's is libxml's own xmlNodeGetBase(): the nearest `xml:base` on the`` |
|      - |  8593 | `		 * way up, resolved against the DOCUMENT's URI, and that URI itself when` |
|      - |  8594 | `		 * no ancestor declares one. So it answers null exactly when the document` |
|      - |  8595 | ``		 * was never given a URI -- a `new DOMDocument()` that was not loaded --`` |
|      - |  8596 | `		 * and a node created and never appended still answers its document's. */` |
|     39 |  8597 | `		xmlChar *zBase = pNode ? xmlNodeGetBase(pNode->doc,pNode) : 0;` |
|     39 |  8598 | `		if( zBase ){` |
|     23 |  8599 | `			ph7_result_string(pCtx,(const char *)zBase,-1);` |
|     23 |  8600 | `			xmlFree(zBase);` |
|     12 |  8601 | `		}else{` |
|     17 |  8602 | `			ph7_result_null(pCtx);` |
|      1 |  8603 | `		}` |
|    476 |  8604 | `	}else if( DomNameIs(zName,"childNodes") ){` |
|    245 |  8605 | `		ph7_class_instance *pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_CHILD,pThis,0,0,0);` |
|    245 |  8606 | `		if( pList == 0 ){` |
|    ! 0 |  8607 | `			return -1;` |
|      - |  8608 | `		}` |
|    245 |  8609 | `		PH7_NativeResultObject(pCtx,pList);` |
|    335 |  8610 | `	}else if( DomNameIs(zName,"attributes") ){` |
|      - |  8611 | `		/* php: NULL for anything that is not an element. */` |
|    130 |  8612 | `		if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|     11 |  8613 | `			ph7_result_null(pCtx);` |
|      6 |  8614 | `		}else{` |
|    120 |  8615 | `			ph7_class_instance *pMap = DomNewCollection(pVm,"DOMNamedNodeMap",pDoc,DNL_CHILD,pThis,0,0,0);` |
|    120 |  8616 | `			if( pMap == 0 ){` |
|    ! 0 |  8617 | `				return -1;` |
|      - |  8618 | `			}` |
|    120 |  8619 | `			PH7_NativeResultObject(pCtx,pMap);` |
|      - |  8620 | `		}` |
|     66 |  8621 | `	}else{` |
|     85 |  8622 | `		return 0;` |
|      - |  8623 | `	}` |
|   2987 |  8624 | `	return 1;` |
|   1538 |  8625 | `}` |
|      - |  8626 | `/*` |
|      - |  8627 | ` * Is this receiver a node class with NO node behind it?` |
|      - |  8628 | ` *` |
|      - |  8629 | `` * `new DOMNode()`, `new DOMCharacterData()`, `new DOMEntity()`, `new DOMNotation()`,`` |
|      - |  8630 | `` * `new DOMDocumentType()` and `new DOMNameSpaceNode()` all construct in php and none`` |
|      - |  8631 | ` * of them builds a libxml node -- so every property handler on such an object fetches` |
|      - |  8632 | `` * a null pointer and answers php's `DOMException: Invalid State Error`, on a read and`` |
|      - |  8633 | `` * on an `isset()` alike. A name the class does NOT declare never reaches a handler at`` |
|      - |  8634 | ` * all and keeps the ordinary undefined-property warning, which is why the test is` |
|      - |  8635 | ` * against the DECLARATION rather than against the reader.` |
|      - |  8636 | ` */` |
|   6626 |  8637 | `static int DomNodeLess(ph7_context *pCtx,const char *zName)` |
|      5 |  8638 | `{` |
|   6631 |  8639 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8640 | `	phl_domnode *pNd;` |
|   6631 |  8641 | `	if( pThis == 0 \|\| PH7_NativeAttr(pThis,DOM_RES) == 0 ){` |
|    314 |  8642 | `		return 0;   /* not one of the classes that carries a node at all */` |
|      - |  8643 | `	}` |
|   6319 |  8644 | `	pNd = DomResOf(pThis);` |
|   6319 |  8645 | `	if( pNd && pNd->pNode ){` |
|   6227 |  8646 | `		return 0;` |
|      - |  8647 | `	}` |
|     93 |  8648 | `	return PH7_ClassExtractAttribute(pThis->pClass,zName,(sxu32)SyStrlen(zName)) != 0;` |
|   3319 |  8649 | `}` |
|      - |  8650 | `/*` |
|      - |  8651 | ` * The write half. A per-class WRITER answers one of these; the name it does` |
|      - |  8652 | ` * not write is looked up in the class's READER by the property HOOK, which` |
|      - |  8653 | ` * decides between php's two refusals -- a property the table carries is` |
|      - |  8654 | ` * read-only, one it does not is nothing this class answers and goes back on` |
|      - |  8655 | ` * the ordinary path (where PHL's §10 policy meets a dynamic property).` |
|      - |  8656 | ` */` |
|      - |  8657 | `#define DOM_SET_UNKNOWN  0   /* not a property of this class */` |
|      - |  8658 | `#define DOM_SET_DONE     1   /* written, or a refusal already raised into *pRc */` |
|      - |  8659 | `/*` |
|      - |  8660 | `` * php's `Cannot modify readonly property C::$p`, worded under the INSTANCE's`` |
|      - |  8661 | ` * class so a userland subclass of DOMElement is reported under its own name.` |
|      - |  8662 | ` *` |
|      - |  8663 | ` * Three writers refuse a name of their own HERE rather than by declining it: the` |
|      - |  8664 | ` * two DTD halves whose read-only properties are DEPRECATED (php's readonly Error` |
|      - |  8665 | ` * carries no deprecation notice, and the reader would have raised one) and the` |
|      - |  8666 | ` * document's read-only four for the same reason.` |
|      - |  8667 | ` */` |
|     98 |  8668 | `static sxi32 DomRefuseWrite(ph7_context *pCtx,const char *zName)` |
|      1 |  8669 | `{` |
|     99 |  8670 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     99 |  8671 | `	if( pThis == 0 ){` |
|    ! 0 |  8672 | `		return PH7_OK;` |
|      - |  8673 | `	}` |
|    148 |  8674 | `	return DomPropThrow(pCtx,"Error",0,"Cannot modify readonly property %z::$%s",` |
|     98 |  8675 | `		&pThis->pClass->sName,zName);` |
|     50 |  8676 | `}` |
|      - |  8677 | `/* A libxml string slot answered as php answers it: the bytes, or null when the` |
|      - |  8678 | `` * document never carried one (`encoding` on a declaration-less document). */`` |
|    209 |  8679 | `static void DomResultXmlStr(ph7_context *pCtx,const xmlChar *zVal)` |
|      1 |  8680 | `{` |
|    210 |  8681 | `	if( zVal ){` |
|    132 |  8682 | `		ph7_result_string(pCtx,(const char *)zVal,-1);` |
|     67 |  8683 | `	}else{` |
|     79 |  8684 | `		ph7_result_null(pCtx);` |
|      - |  8685 | `	}` |
|    210 |  8686 | `}` |
|      - |  8687 | `/*` |
|      - |  8688 | ` * The DOCUMENT's own state block.` |
|      - |  8689 | ` *` |
|      - |  8690 | ` * Nine of php's twenty-two DOMDocument properties are the XML DECLARATION and` |
|      - |  8691 | ` * the document's URI, read straight off libxml's xmlDoc -- and php spells most` |
|      - |  8692 | ` * of them twice, once under the DOM level-3 name and once under the level-1 one` |
|      - |  8693 | `` * it kept for compatibility (`version`/`xmlVersion`, `encoding`/`xmlEncoding`,`` |
|      - |  8694 | `` * `standalone`/`xmlStandalone`).  The pairs are not synonyms in every`` |
|      - |  8695 | ``  * direction: `xmlEncoding` and `actualEncoding` READ the same slot `encoding` `` |
|      - |  8696 | `` * writes and are themselves read-only, which is what makes `$d->xmlEncoding =`` |
|      - |  8697 | `` * 'UTF-8'` php's readonly Error and `$d->encoding = 'UTF-8'` the write that`` |
|      - |  8698 | `` * changes the bytes `saveXML()` emits.`` |
|      - |  8699 | ` *` |
|      - |  8700 | `` * `actualEncoding` and `config` carry php 8.4's #[\Deprecated]: the notice`` |
|      - |  8701 | `` * fires on a READ and on an `isset()` alike (both go through php's property`` |
|      - |  8702 | ` * handler), which is why it is raised HERE rather than in __get -- and NOT on a` |
|      - |  8703 | ` * write, where the readonly refusal comes first and is raised by the writer` |
|      - |  8704 | ` * below without consulting this reader.` |
|      - |  8705 | ` */` |
|    585 |  8706 | `static int DomDocStateProp(ph7_context *pCtx,const char *zName,xmlDocPtr pDoc,int bDepr)` |
|      1 |  8707 | `{` |
|    586 |  8708 | `	int bDeprAe = DomNameIs(zName,"actualEncoding");` |
|    586 |  8709 | `	if( bDeprAe \|\| DomNameIs(zName,"config") ){` |
|      - |  8710 | `		/* ...and NOT on the get_debug_info walk either: php marks the DECLARATION` |
|      - |  8711 | `		 * deprecated and its debug handler reads the C function behind it, so` |
|      - |  8712 | `		 * print_r()/var_dump() of a document raise nothing (bDepr is 0 there). */` |
|     43 |  8713 | `		if( bDepr ){` |
|     46 |  8714 | `			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|     15 |  8715 | `				bDeprAe ? "Property DOMDocument::$actualEncoding is deprecated"` |
|      - |  8716 | `				        : "Property DOMDocument::$config is deprecated");` |
|     15 |  8717 | `		}` |
|      - |  8718 | ``		/* `config` is php's DOM level-3 configuration slot and has never been`` |
|      - |  8719 | `		 * filled in there: the handler answers null and nothing else. */` |
|     43 |  8720 | `		if( bDeprAe ){` |
|     21 |  8721 | `			DomResultXmlStr(pCtx,pDoc ? pDoc->encoding : 0);` |
|     11 |  8722 | `		}else{` |
|     23 |  8723 | `			ph7_result_null(pCtx);` |
|      - |  8724 | `		}` |
|     43 |  8725 | `		return 1;` |
|      - |  8726 | `	}` |
|    544 |  8727 | `	if( DomNameIs(zName,"encoding") \|\| DomNameIs(zName,"xmlEncoding") ){` |
|     60 |  8728 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->encoding : 0);` |
|     60 |  8729 | `		return 1;` |
|      - |  8730 | `	}` |
|    485 |  8731 | `	if( DomNameIs(zName,"version") \|\| DomNameIs(zName,"xmlVersion") ){` |
|     73 |  8732 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->version : 0);` |
|     73 |  8733 | `		return 1;` |
|      - |  8734 | `	}` |
|    413 |  8735 | `	if( DomNameIs(zName,"documentURI") ){` |
|     33 |  8736 | `		DomResultXmlStr(pCtx,pDoc ? pDoc->URL : 0);` |
|     33 |  8737 | `		return 1;` |
|      - |  8738 | `	}` |
|    381 |  8739 | `	if( DomNameIs(zName,"standalone") \|\| DomNameIs(zName,"xmlStandalone") ){` |
|      - |  8740 | `		/* libxml records four states in one int -- no declaration (-1), a` |
|      - |  8741 | ``		 * declaration without the attribute (-2), `no` (0) and `yes` (1) -- and`` |
|      - |  8742 | `		 * php's bool is true for the last one only. */` |
|     71 |  8743 | `		ph7_result_bool(pCtx,pDoc != 0 && pDoc->standalone == 1);` |
|     71 |  8744 | `		return 1;` |
|      - |  8745 | `	}` |
|    311 |  8746 | `	return 0;` |
|    294 |  8747 | `}` |
|      - |  8748 | `/*` |
|      - |  8749 | ` * The three DOMParentNode properties.  php declares them on the three` |
|      - |  8750 | ` * implementers ONLY -- DOMDocument, DOMElement and DOMDocumentFragment -- so` |
|      - |  8751 | `` * `$text->childElementCount` is the Undefined property warning there, which`` |
|      - |  8752 | ` * is why childElementCount cannot live in DomNodeProp (it did, and every node` |
|      - |  8753 | ` * kind answered 0 in silence where php warns and answers null).` |
|      - |  8754 | ` */` |
|   2216 |  8755 | `static int DomParentNodeProp(ph7_context *pCtx,const char *zName)` |
|      5 |  8756 | `{` |
|   2221 |  8757 | `	int bLast = DomNameIs(zName,"lastElementChild");` |
|   2221 |  8758 | `	if( bLast \|\| DomNameIs(zName,"firstElementChild") ){` |
|     37 |  8759 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|     37 |  8760 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     37 |  8761 | `		xmlNodePtr pChild = pNode ? (bLast ? pNode->last : pNode->children) : 0;` |
|     51 |  8762 | `		while( pChild && pChild->type != XML_ELEMENT_NODE ){` |
|     15 |  8763 | `			pChild = bLast ? pChild->prev : pChild->next;` |
|      1 |  8764 | `		}` |
|     37 |  8765 | `		DomResultNodeOf(pCtx,pNd,pChild);` |
|     37 |  8766 | `		return 1;` |
|      - |  8767 | `	}` |
|   2185 |  8768 | `	if( DomNameIs(zName,"childElementCount") ){` |
|     25 |  8769 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|     25 |  8770 | `		ph7_result_int(pCtx,DomChildCount(pNd ? (xmlNodePtr)pNd->pNode : 0,1));` |
|     25 |  8771 | `		return 1;` |
|      - |  8772 | `	}` |
|   2161 |  8773 | `	return 0;` |
|   1113 |  8774 | `}` |
|      - |  8775 | `/*` |
|      - |  8776 | ` * The two DOMChildNode-side properties.  php declares them on DOMElement and` |
|      - |  8777 | ` * DOMCharacterData only -- an attribute, a PI or the document warns Undefined` |
|      - |  8778 | ` * property -- and they skip every node kind that is not an element.` |
|      - |  8779 | ` */` |
|   2174 |  8780 | `static int DomChildNodeProp(ph7_context *pCtx,const char *zName)` |
|      5 |  8781 | `{` |
|   2179 |  8782 | `	int bNext = DomNameIs(zName,"nextElementSibling");` |
|   2179 |  8783 | `	if( bNext \|\| DomNameIs(zName,"previousElementSibling") ){` |
|     27 |  8784 | `		phl_domnode *pNd = DomThisNode(pCtx);` |
|     27 |  8785 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     27 |  8786 | `		xmlNodePtr pSib = pNode ? (bNext ? pNode->next : pNode->prev) : 0;` |
|     51 |  8787 | `		while( pSib && pSib->type != XML_ELEMENT_NODE ){` |
|     25 |  8788 | `			pSib = bNext ? pSib->next : pSib->prev;` |
|      1 |  8789 | `		}` |
|     27 |  8790 | `		DomResultNodeOf(pCtx,pNd,pSib);` |
|     27 |  8791 | `		return 1;` |
|      - |  8792 | `	}` |
|   2153 |  8793 | `	return 0;` |
|   1092 |  8794 | `}` |
|      - |  8795 | `/* DOMDocument adds documentElement and the state block above. */` |
|   2393 |  8796 | `static int DomDocPropEx(ph7_context *pCtx,const char *zName,int bDepr)` |
|      4 |  8797 | `{` |
|   2397 |  8798 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   2397 |  8799 | `	if( DomNameIs(zName,"documentElement") ){` |
|   1750 |  8800 | `		DomResultNodeOf(pCtx,pNd,pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0);` |
|   1750 |  8801 | `		return 1;` |
|      - |  8802 | `	}` |
|    649 |  8803 | `	if( DomNameIs(zName,"implementation") ){` |
|      - |  8804 | `		/* A FRESH object on every read, which is php's: the class has no state` |
|      - |  8805 | `		 * and nothing ties one to a document. */` |
|     17 |  8806 | `		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,"DOMImplementation",` |
|      - |  8807 | `			sizeof("DOMImplementation")-1,FALSE,0);` |
|     17 |  8808 | `		ph7_class_instance *pImpl = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;` |
|     17 |  8809 | `		if( pImpl ){` |
|     17 |  8810 | `			PH7_NativeResultObject(pCtx,pImpl);` |
|      9 |  8811 | `		}else{` |
|    ! 0 |  8812 | `			ph7_result_null(pCtx);` |
|      - |  8813 | `		}` |
|     17 |  8814 | `		return 1;` |
|      - |  8815 | `	}` |
|    633 |  8816 | `	if( DomNameIs(zName,"doctype") ){` |
|      - |  8817 | `		/* The INTERNAL subset alone, which is php's: a DTD pulled in from the` |
|      - |  8818 | `` 		 * SYSTEM identifier lands in `extSubset` and is not what `doctype` `` |
|      - |  8819 | `		 * answers. Null for a document that declares none. */` |
|     94 |  8820 | `		DomResultNodeOf(pCtx,pNd,` |
|     46 |  8821 | `			pNd ? (xmlNodePtr)xmlGetIntSubset((xmlDocPtr)pNd->pNode) : 0);` |
|     48 |  8822 | `		return 1;` |
|      - |  8823 | `	}` |
|    586 |  8824 | `	if( DomDocStateProp(pCtx,zName,pNd ? (xmlDocPtr)pNd->pNode : 0,bDepr) ){` |
|    276 |  8825 | `		return 1;` |
|      - |  8826 | `	}` |
|      - |  8827 | `	{` |
|      - |  8828 | `		/* The seven directives, out of the one hidden word that holds them. */` |
|      - |  8829 | `		sxu32 i;` |
|   2003 |  8830 | `		for( i = 0 ; i < SX_ARRAYSIZE(aDomDocFlag) ; ++i ){` |
|   1809 |  8831 | `			if( DomNameIs(zName,aDomDocFlag[i].zName) ){` |
|    175 |  8832 | `				ph7_result_bool(pCtx,` |
|    116 |  8833 | `					DomDocFlag(PH7_ContextThis(pCtx),aDomDocFlag[i].iBit));` |
|    117 |  8834 | `				return 1;` |
|      - |  8835 | `			}` |
|    847 |  8836 | `		}` |
|      - |  8837 | `	}` |
|    195 |  8838 | `	if( DomParentNodeProp(pCtx,zName) ){` |
|     29 |  8839 | `		return 1;` |
|      - |  8840 | `	}` |
|    167 |  8841 | `	return DomNodeProp(pCtx,zName);` |
|   1201 |  8842 | `}` |
|   2153 |  8843 | `static int DomDocProp(ph7_context *pCtx,const char *zName)` |
|      4 |  8844 | `{` |
|   2157 |  8845 | `	return DomDocPropEx(pCtx,zName,1);` |
|      4 |  8846 | `}` |
|      - |  8847 | `/* The same reader with php's two #[\Deprecated] notices held back: the` |
|      - |  8848 | ` * get_debug_info walk below reads the handler, not the declaration. */` |
|    240 |  8849 | `static int DomDocPropQuiet(ph7_context *pCtx,const char *zName)` |
|      1 |  8850 | `{` |
|    241 |  8851 | `	return DomDocPropEx(pCtx,zName,0);` |
|      1 |  8852 | `}` |
|      - |  8853 | `/*` |
|      - |  8854 | `` * DOMDocumentType: what the `<!DOCTYPE ...>` line SAYS.`` |
|      - |  8855 | ` *` |
|      - |  8856 | `` * The DTD node has been reachable all along (`$doc->firstChild` on any document`` |
|      - |  8857 | ` * carrying a doctype), so what was missing was not the node but every question` |
|      - |  8858 | ``  * about it: the class, so `instanceof DOMDocumentType` and `get_class()` `` |
|      - |  8859 | ` * answer, and the four identifiers a program reads off one.` |
|      - |  8860 | ` *` |
|      - |  8861 | `` * php's `publicId`/`systemId` here are plain strings that answer "" when the`` |
|      - |  8862 | `` * declaration carries none -- unlike DOMEntity's, which are `?string` and null`` |
|      - |  8863 | ` * for the same absence -- so the two classes cannot share a reader.` |
|      - |  8864 | ` */` |
|    178 |  8865 | `static xmlDtdPtr DomThisDtd(ph7_context *pCtx)` |
|      2 |  8866 | `{` |
|    180 |  8867 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    180 |  8868 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    178 |  8869 | `	if( pNode == 0` |
|    177 |  8870 | `	 \|\| (pNode->type != XML_DTD_NODE && pNode->type != XML_DOCUMENT_TYPE_NODE) ){` |
|      7 |  8871 | `		return 0;` |
|      - |  8872 | `	}` |
|    174 |  8873 | `	return (xmlDtdPtr)pNode;` |
|     91 |  8874 | `}` |
|      - |  8875 | `/*` |
|      - |  8876 | `` * `internalSubset`: the bytes BETWEEN the brackets, rebuilt by dumping each`` |
|      - |  8877 | ` * declaration the subset holds.  php reads them off the DOCUMENT's internal` |
|      - |  8878 | ` * subset rather than the receiver's own children -- so a doctype cloned out of` |
|      - |  8879 | ` * a document that has none answers null -- and answers null, not "", when` |
|      - |  8880 | ` * there is no subset to dump at all.` |
|      - |  8881 | ` */` |
|     18 |  8882 | `static void DomInternalSubset(ph7_context *pCtx,xmlDtdPtr pDtd)` |
|      1 |  8883 | `{` |
|     19 |  8884 | `	xmlDtdPtr pSub = (pDtd && pDtd->doc) ? xmlGetIntSubset(pDtd->doc) : 0;` |
|     19 |  8885 | `	xmlNodePtr pChild = pSub ? pSub->children : 0;` |
|      - |  8886 | `	xmlBufferPtr pBuf;` |
|      - |  8887 | `	xmlOutputBufferPtr pOut;` |
|     19 |  8888 | `	if( pChild == 0 ){` |
|     11 |  8889 | `		ph7_result_null(pCtx);` |
|     11 |  8890 | `		return;` |
|      - |  8891 | `	}` |
|      9 |  8892 | `	pBuf = xmlBufferCreate();` |
|      9 |  8893 | `	pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;` |
|      9 |  8894 | `	if( pOut == 0 ){` |
|    ! 0 |  8895 | `		if( pBuf ){` |
|    ! 0 |  8896 | `			xmlBufferFree(pBuf);` |
|    ! 0 |  8897 | `		}` |
|    ! 0 |  8898 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8899 | `		return;` |
|      - |  8900 | `	}` |
|     17 |  8901 | `	for( ; pChild ; pChild = pChild->next ){` |
|      9 |  8902 | `		xmlNodeDumpOutput(pOut,pSub->doc,pChild,0,0,0);` |
|      5 |  8903 | `	}` |
|      9 |  8904 | `	xmlOutputBufferFlush(pOut);` |
|      9 |  8905 | `	ph7_result_string(pCtx,(const char *)xmlBufferContent(pBuf),(int)xmlBufferLength(pBuf));` |
|      9 |  8906 | `	xmlOutputBufferClose(pOut);` |
|      9 |  8907 | `	xmlBufferFree(pBuf);` |
|     10 |  8908 | `}` |
|    178 |  8909 | `static int DomDocTypeProp(ph7_context *pCtx,const char *zName)` |
|      2 |  8910 | `{` |
|    180 |  8911 | `	xmlDtdPtr pDtd = DomThisDtd(pCtx);` |
|    180 |  8912 | `	if( DomNameIs(zName,"name") ){` |
|      - |  8913 | `		/* The name the DOCTYPE declares, which is also its nodeName. */` |
|     33 |  8914 | `		ph7_result_string(pCtx,(pDtd && pDtd->name) ? (const char *)pDtd->name : "",-1);` |
|     33 |  8915 | `		return 1;` |
|      - |  8916 | `	}` |
|    148 |  8917 | `	if( DomNameIs(zName,"publicId") ){` |
|     15 |  8918 | `		ph7_result_string(pCtx,(pDtd && pDtd->ExternalID) ? (const char *)pDtd->ExternalID : "",-1);` |
|     15 |  8919 | `		return 1;` |
|      - |  8920 | `	}` |
|    134 |  8921 | `	if( DomNameIs(zName,"systemId") ){` |
|     15 |  8922 | `		ph7_result_string(pCtx,(pDtd && pDtd->SystemID) ? (const char *)pDtd->SystemID : "",-1);` |
|     15 |  8923 | `		return 1;` |
|      - |  8924 | `	}` |
|    120 |  8925 | `	if( DomNameIs(zName,"internalSubset") ){` |
|     19 |  8926 | `		DomInternalSubset(pCtx,pDtd);` |
|     19 |  8927 | `		return 1;` |
|      - |  8928 | `	}` |
|    102 |  8929 | `	if( DomNameIs(zName,"entities") \|\| DomNameIs(zName,"notations") ){` |
|     54 |  8930 | `		ph7_class_instance *pMap = DomNewCollection(pCtx->pVm,"DOMNamedNodeMap",` |
|     26 |  8931 | `			DomThisDoc(pCtx),DomNameIs(zName,"notations") ? DNL_NOTS : DNL_ENTS,` |
|     13 |  8932 | `			PH7_ContextThis(pCtx),0,0,0);` |
|     28 |  8933 | `		if( pMap ){` |
|     28 |  8934 | `			PH7_NativeResultObject(pCtx,pMap);` |
|     15 |  8935 | `		}else{` |
|    ! 0 |  8936 | `			ph7_result_null(pCtx);` |
|      - |  8937 | `		}` |
|     28 |  8938 | `		return 1;` |
|      - |  8939 | `	}` |
|     75 |  8940 | `	return DomNodeProp(pCtx,zName);` |
|     91 |  8941 | `}` |
|      - |  8942 | `/*` |
|      - |  8943 | `` * DOMEntity: an `<!ENTITY ...>` declaration of the internal subset.`` |
|      - |  8944 | ` *` |
|      - |  8945 | ` * Its three identifiers are the DOM's "for an UNPARSED entity" rule, which php` |
|      - |  8946 | `` * follows to the letter: `publicId`, `systemId` and `notationName` answer null`` |
|      - |  8947 | `` * for every entity that is not `NDATA`-declared, so the external-but-parsed`` |
|      - |  8948 | `` * `<!ENTITY e SYSTEM "e.xml">` reads null from all three while`` |
|      - |  8949 | `` * `<!ENTITY g SYSTEM "g.gif" NDATA gif>` reads its own two and the notation's`` |
|      - |  8950 | `` * name.  (`baseURI`, which DOMNode answers, is where the resolved system`` |
|      - |  8951 | ` * identifier does show for both.)` |
|      - |  8952 | ` *` |
|      - |  8953 | ` * The other three are php 8.4's deprecated block, and like DOMDocument's the` |
|      - |  8954 | `` * notice fires on a READ and on an `isset()` alike -- both go through php's`` |
|      - |  8955 | ` * property handler -- and not on a write, where the readonly refusal comes` |
|      - |  8956 | ` * first and never consults this reader.` |
|      - |  8957 | ` */` |
|     92 |  8958 | `static xmlEntityPtr DomThisEntity(ph7_context *pCtx)` |
|      1 |  8959 | `{` |
|     93 |  8960 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     93 |  8961 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  8962 | `	/* Only a real entity DECLARATION is xmlEntity-shaped. An ELEMENT` |
|      - |  8963 | `	 * declaration wears this class in php and is an xmlElement underneath,` |
|      - |  8964 | `	 * whose fields past the node header are another struct's. */` |
|     93 |  8965 | `	if( pNode == 0 \|\| pNode->type != XML_ENTITY_DECL ){` |
|      7 |  8966 | `		return 0;` |
|      - |  8967 | `	}` |
|     87 |  8968 | `	return (xmlEntityPtr)pNode;` |
|     47 |  8969 | `}` |
|     92 |  8970 | `static int DomEntityPropEx(ph7_context *pCtx,const char *zName,int bDepr)` |
|      1 |  8971 | `{` |
|     93 |  8972 | `	xmlEntityPtr pEnt = DomThisEntity(pCtx);` |
|     93 |  8973 | `	int bUnparsed = pEnt && pEnt->etype == XML_EXTERNAL_GENERAL_UNPARSED_ENTITY;` |
|     93 |  8974 | `	if( DomNameIs(zName,"publicId") ){` |
|      9 |  8975 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->ExternalID : 0);` |
|      9 |  8976 | `		return 1;` |
|      - |  8977 | `	}` |
|     85 |  8978 | `	if( DomNameIs(zName,"systemId") ){` |
|      9 |  8979 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->SystemID : 0);` |
|      9 |  8980 | `		return 1;` |
|      - |  8981 | `	}` |
|     77 |  8982 | `	if( DomNameIs(zName,"notationName") ){` |
|      - |  8983 | ``		/* libxml keeps an unparsed entity's notation name in `content`. */`` |
|     11 |  8984 | `		DomResultXmlStr(pCtx,bUnparsed ? pEnt->content : 0);` |
|     11 |  8985 | `		return 1;` |
|      - |  8986 | `	}` |
|     67 |  8987 | `	if( DomNameIs(zName,"actualEncoding") ){` |
|      3 |  8988 | `		if( bDepr ){` |
|      3 |  8989 | `			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|      - |  8990 | `				"Property DOMEntity::$actualEncoding is deprecated");` |
|      1 |  8991 | `		}` |
|      3 |  8992 | `		ph7_result_null(pCtx);` |
|      3 |  8993 | `		return 1;` |
|      - |  8994 | `	}` |
|     65 |  8995 | `	if( DomNameIs(zName,"encoding") ){` |
|      5 |  8996 | `		if( bDepr ){` |
|      5 |  8997 | `			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|      - |  8998 | `				"Property DOMEntity::$encoding is deprecated");` |
|      2 |  8999 | `		}` |
|      5 |  9000 | `		ph7_result_null(pCtx);` |
|      5 |  9001 | `		return 1;` |
|      - |  9002 | `	}` |
|     61 |  9003 | `	if( DomNameIs(zName,"version") ){` |
|      - |  9004 | `		/* php has never filled any of the three in: the handler answers NULL` |
|      - |  9005 | `		 * and does nothing else. */` |
|      3 |  9006 | `		if( bDepr ){` |
|      3 |  9007 | `			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,` |
|      - |  9008 | `				"Property DOMEntity::$version is deprecated");` |
|      1 |  9009 | `		}` |
|      3 |  9010 | `		ph7_result_null(pCtx);` |
|      3 |  9011 | `		return 1;` |
|      - |  9012 | `	}` |
|     59 |  9013 | `	return DomNodeProp(pCtx,zName);` |
|     47 |  9014 | `}` |
|     92 |  9015 | `static int DomEntityProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9016 | `{` |
|     93 |  9017 | `	return DomEntityPropEx(pCtx,zName,1);` |
|      1 |  9018 | `}` |
|      - |  9019 | `/* ...and the same reader without php's three #[\Deprecated] notices, for the` |
|      - |  9020 | ` * get_debug_info walk (which reads the handler, not the declaration). */` |
|    ! 0 |  9021 | `static int DomEntityPropQuiet(ph7_context *pCtx,const char *zName)` |
|    ! 0 |  9022 | `{` |
|    ! 0 |  9023 | `	return DomEntityPropEx(pCtx,zName,0);` |
|    ! 0 |  9024 | `}` |
|      - |  9025 | `/*` |
|      - |  9026 | `` * php declares `schemaTypeInfo` on both DOMElement and DOMAttr and has never`` |
|      - |  9027 | ` * filled it in: ext/dom answers NULL from a handler that does nothing else.` |
|      - |  9028 | ` * It is a declared property all the same, so a read is NOT the undefined-property` |
|      - |  9029 | ` * warning -- which is the whole difference this row buys.` |
|      - |  9030 | ` */` |
|      2 |  9031 | `static int DomSchemaTypeInfo(ph7_context *pCtx)` |
|      1 |  9032 | `{` |
|      3 |  9033 | `	ph7_result_null(pCtx);` |
|      3 |  9034 | `	return 1;` |
|      1 |  9035 | `}` |
|      - |  9036 | `/* DOMElement adds tagName, and the two attribute-backed names php exposes as` |
|      - |  9037 | `` * properties: `className` IS the class attribute and `id` IS the id one, both`` |
|      - |  9038 | ` * answering "" when the attribute is absent. */` |
|   2002 |  9039 | `static int DomElemProp(ph7_context *pCtx,const char *zName)` |
|      5 |  9040 | `{` |
|      - |  9041 | `	phl_domnode *pNd;` |
|   2007 |  9042 | `	int bClass = DomNameIs(zName,"className");` |
|   2007 |  9043 | `	if( DomNameIs(zName,"tagName") ){` |
|     58 |  9044 | `		pNd = DomThisNode(pCtx);` |
|     58 |  9045 | `		DomNodeName(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|     58 |  9046 | `		return 1;` |
|      - |  9047 | `	}` |
|   1951 |  9048 | `	if( bClass \|\| DomNameIs(zName,"id") ){` |
|      - |  9049 | `		xmlChar *zVal;` |
|      9 |  9050 | `		pNd = DomThisNode(pCtx);` |
|      9 |  9051 | `		zVal = pNd ? xmlGetNoNsProp((xmlNodePtr)pNd->pNode,` |
|      8 |  9052 | `			(const xmlChar *)(bClass ? "class" : "id")) : 0;` |
|      9 |  9053 | `		ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|      9 |  9054 | `		if( zVal ){` |
|      5 |  9055 | `			xmlFree(zVal);` |
|      2 |  9056 | `		}` |
|      9 |  9057 | `		return 1;` |
|      - |  9058 | `	}` |
|   1943 |  9059 | `	if( DomNameIs(zName,"schemaTypeInfo") ){` |
|    ! 0 |  9060 | `		return DomSchemaTypeInfo(pCtx);` |
|      - |  9061 | `	}` |
|   1943 |  9062 | `	if( DomParentNodeProp(pCtx,zName) \|\| DomChildNodeProp(pCtx,zName) ){` |
|     43 |  9063 | `		return 1;` |
|      - |  9064 | `	}` |
|   1901 |  9065 | `	return DomNodeProp(pCtx,zName);` |
|   1006 |  9066 | `}` |
|      - |  9067 | `/* DOMDocumentFragment: the three DOMParentNode properties over DOMNode's. */` |
|     84 |  9068 | `static int DomFragProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9069 | `{` |
|     85 |  9070 | `	if( DomParentNodeProp(pCtx,zName) ){` |
|     11 |  9071 | `		return 1;` |
|      - |  9072 | `	}` |
|     75 |  9073 | `	return DomNodeProp(pCtx,zName);` |
|     43 |  9074 | `}` |
|      - |  9075 | `/* DOMAttr adds name/value/ownerElement/specified/schemaTypeInfo. */` |
|    486 |  9076 | `static int DomAttrProp(ph7_context *pCtx,const char *zName)` |
|      3 |  9077 | `{` |
|    489 |  9078 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    489 |  9079 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    489 |  9080 | `	if( DomNameIs(zName,"name") ){` |
|      9 |  9081 | `		DomNodeName(pCtx,pNode);` |
|      9 |  9082 | `		return 1;` |
|      - |  9083 | `	}` |
|    481 |  9084 | `	if( DomNameIs(zName,"value") ){` |
|     54 |  9085 | `		DomNodeValue(pCtx,pNode);` |
|     54 |  9086 | `		return 1;` |
|      - |  9087 | `	}` |
|    429 |  9088 | `	if( DomNameIs(zName,"ownerElement") ){` |
|     52 |  9089 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);` |
|     52 |  9090 | `		return 1;` |
|      - |  9091 | `	}` |
|      - |  9092 | ``	/* php's `specified` is a DOM level-1 remnant: ext/dom answers TRUE for every`` |
|      - |  9093 | `	 * attribute a program can reach, including one it just created. */` |
|    378 |  9094 | `	if( DomNameIs(zName,"specified") ){` |
|      3 |  9095 | `		ph7_result_bool(pCtx,1);` |
|      3 |  9096 | `		return 1;` |
|      - |  9097 | `	}` |
|    376 |  9098 | `	if( DomNameIs(zName,"schemaTypeInfo") ){` |
|      3 |  9099 | `		return DomSchemaTypeInfo(pCtx);` |
|      - |  9100 | `	}` |
|    374 |  9101 | `	return DomNodeProp(pCtx,zName);` |
|    246 |  9102 | `}` |
|      - |  9103 | `/* DOMCharacterData adds data/length; DOMText adds wholeText on top of those. */` |
|    384 |  9104 | `static int DomCharDataProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9105 | `{` |
|    386 |  9106 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    386 |  9107 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    386 |  9108 | `	if( DomNameIs(zName,"data") ){` |
|    116 |  9109 | `		DomDataValue(pCtx,pNode);` |
|    116 |  9110 | `		return 1;` |
|      - |  9111 | `	}` |
|    272 |  9112 | `	if( DomNameIs(zName,"length") ){` |
|      - |  9113 | `		/* php counts UTF-8 CHARACTERS here (xmlUTF8Strlen over the node's own` |
|      - |  9114 | `		 * content), which is the same unit every offset on this class uses. */` |
|     13 |  9115 | `		ph7_result_int(pCtx,DomCharLength(pNode));` |
|     13 |  9116 | `		return 1;` |
|      - |  9117 | `	}` |
|    260 |  9118 | `	return 0;` |
|    194 |  9119 | `}` |
|    384 |  9120 | `static int DomCharProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9121 | `{` |
|    386 |  9122 | `	if( DomCharDataProp(pCtx,zName) \|\| DomChildNodeProp(pCtx,zName) ){` |
|    134 |  9123 | `		return 1;` |
|      - |  9124 | `	}` |
|    254 |  9125 | `	return DomNodeProp(pCtx,zName);` |
|    194 |  9126 | `}` |
|      - |  9127 | `/*` |
|      - |  9128 | ` * DOMText::wholeText is the whole RUN, not the node: php walks back to the` |
|      - |  9129 | ` * first adjacent text-or-CDATA sibling and forward to the last, concatenating` |
|      - |  9130 | ` * all of them, which is what makes it the answer to "what does this element` |
|      - |  9131 | ` * actually say" after an edit has left the text in pieces. Answering the node's` |
|      - |  9132 | ` * own data (what PHL did) is the same string only when the run is one node` |
|      - |  9133 | ` * long, and silently short otherwise.` |
|      - |  9134 | ` */` |
|     54 |  9135 | `static int DomIsTextRun(xmlNodePtr pNode)` |
|      1 |  9136 | `{` |
|     48 |  9137 | `	return pNode != 0` |
|     74 |  9138 | `		&& (pNode->type == XML_TEXT_NODE \|\| pNode->type == XML_CDATA_SECTION_NODE);` |
|      1 |  9139 | `}` |
|    318 |  9140 | `static int DomTextProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9141 | `{` |
|      - |  9142 | `	phl_domnode *pNd;` |
|      - |  9143 | `	xmlNodePtr pNode,pCur;` |
|      - |  9144 | `	SyBlob sOut;` |
|    320 |  9145 | `	if( DomNameIs(zName,"wholeText") ){` |
|     13 |  9146 | `		pNd = DomThisNode(pCtx);` |
|     13 |  9147 | `		pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     13 |  9148 | `		if( !DomIsTextRun(pNode) ){` |
|    ! 0 |  9149 | `			DomNodeValue(pCtx,pNode);` |
|    ! 0 |  9150 | `			return 1;` |
|      - |  9151 | `		}` |
|     15 |  9152 | `		while( DomIsTextRun(pNode->prev) ){` |
|      3 |  9153 | `			pNode = pNode->prev;` |
|      1 |  9154 | `		}` |
|     13 |  9155 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     29 |  9156 | `		for( pCur = pNode ; DomIsTextRun(pCur) ; pCur = pCur->next ){` |
|     17 |  9157 | `			xmlChar *zPart = xmlNodeGetContent(pCur);` |
|     17 |  9158 | `			if( zPart ){` |
|     17 |  9159 | `				SyBlobAppend(&sOut,(const void *)zPart,(sxu32)SyStrlen((const char *)zPart));` |
|     17 |  9160 | `				xmlFree(zPart);` |
|      8 |  9161 | `			}` |
|      9 |  9162 | `		}` |
|     13 |  9163 | `		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     13 |  9164 | `		SyBlobRelease(&sOut);` |
|     13 |  9165 | `		return 1;` |
|      - |  9166 | `	}` |
|    308 |  9167 | `	return DomCharProp(pCtx,zName);` |
|    161 |  9168 | `}` |
|      - |  9169 | `/*` |
|      - |  9170 | ` * php's typed-property store for the DOM's own string-shaped properties: a` |
|      - |  9171 | ` * scalar coerces, null is accepted only where the declared type is nullable` |
|      - |  9172 | ` * (and means the empty string), and an array or an object is a TypeError` |
|      - |  9173 | ` * naming the class that DECLARES the property rather than the one the write` |
|      - |  9174 | ` * went through. The value is coerced through a COPY -- ph7_value_to_string()` |
|      - |  9175 | ` * converts the object it is handed, and that object is the caller's own` |
|      - |  9176 | `` * `$v` in `$node->nodeValue = $v`.`` |
|      - |  9177 | ` */` |
|    236 |  9178 | `static int DomWriteText(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|      - |  9179 | `	const char *zType,ph7_value *pVal,SyBlob *pOut,int *pRc)` |
|      2 |  9180 | `{` |
|    238 |  9181 | `	int bNullable = zType[0] == '?';` |
|      - |  9182 | `	ph7_value sTmp;` |
|    236 |  9183 | `	if( pVal == 0` |
|    236 |  9184 | `	 \|\| (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0` |
|    224 |  9185 | `	 \|\| ((pVal->iFlags & MEMOBJ_NULL) != 0 && !bNullable) ){` |
|      - |  9186 | `		char zBuf[128];` |
|     35 |  9187 | `		const char *zGiven = "null";` |
|     35 |  9188 | `		if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|     11 |  9189 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|     11 |  9190 | `			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sName);` |
|     11 |  9191 | `			zGiven = zBuf;` |
|     30 |  9192 | `		}else if( pVal ){` |
|     25 |  9193 | `			zGiven = ph7_type_name(pVal);` |
|     12 |  9194 | `		}` |
|     52 |  9195 | `		*pRc = DomPropThrow(pCtx,"TypeError",0,` |
|     17 |  9196 | `			"Cannot assign %s to property %s::$%s of type %s",zGiven,zOwner,zProp,zType);` |
|     35 |  9197 | `		return 0;` |
|      - |  9198 | `	}` |
|    204 |  9199 | `	SyBlobInit(pOut,&pCtx->pVm->sAllocator);` |
|    204 |  9200 | `	if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|    192 |  9201 | `		PH7_MemObjInit(pCtx->pVm,&sTmp);` |
|    192 |  9202 | `		PH7_MemObjLoad(pVal,&sTmp);` |
|    192 |  9203 | `		PH7_MemObjToString(&sTmp);` |
|    192 |  9204 | `		SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));` |
|    192 |  9205 | `		PH7_MemObjRelease(&sTmp);` |
|     95 |  9206 | `	}` |
|    204 |  9207 | `	SyBlobNullAppend(pOut);` |
|    204 |  9208 | `	return 1;` |
|    120 |  9209 | `}` |
|      - |  9210 | `/*` |
|      - |  9211 | `` * The same screen for a `bool` property. php's weak mode takes an int, a float`` |
|      - |  9212 | `` * or a string and answers its truthiness (`"0"` and `""` are false), and refuses`` |
|      - |  9213 | `` * null, an array and an object -- the one difference from the `?string` block`` |
|      - |  9214 | `` * above being that a bool property is NOT nullable, so `= null` is the TypeError`` |
|      - |  9215 | ` * rather than the empty write. Answers 1 when the caller may go on and use` |
|      - |  9216 | ` * ph7_value_to_bool(), 0 when the refusal has been raised into *pRc.` |
|      - |  9217 | ` */` |
|    376 |  9218 | `static int DomWriteBool(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|      - |  9219 | `	ph7_value *pVal,int *pRc)` |
|      1 |  9220 | `{` |
|    377 |  9221 | `	if( pVal == 0 \|\| (pVal->iFlags & (MEMOBJ_HASHMAP\|MEMOBJ_OBJ\|MEMOBJ_NULL)) != 0 ){` |
|      - |  9222 | `		char zBuf[128];` |
|     15 |  9223 | `		const char *zGiven = "null";` |
|     15 |  9224 | `		if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  9225 | `			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|    ! 0 |  9226 | `			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sName);` |
|    ! 0 |  9227 | `			zGiven = zBuf;` |
|     15 |  9228 | `		}else if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 ){` |
|      7 |  9229 | `			zGiven = ph7_type_name(pVal);` |
|      3 |  9230 | `		}` |
|     22 |  9231 | `		*pRc = DomPropThrow(pCtx,"TypeError",0,` |
|      7 |  9232 | `			"Cannot assign %s to property %s::$%s of type bool",zGiven,zOwner,zProp);` |
|     15 |  9233 | `		return 0;` |
|      - |  9234 | `	}` |
|    363 |  9235 | `	return 1;` |
|    189 |  9236 | `}` |
|      - |  9237 | `/* Has this node ever been handed to PHP? The identity cache is the record. */` |
|    158 |  9238 | `static int DomIsWrapped(ph7_vm *pVm,ph7_class_instance *pDoc,xmlNodePtr pNode)` |
|      1 |  9239 | `{` |
|    159 |  9240 | `	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);` |
|    159 |  9241 | `	ph7_hashmap_node *pEntry = 0;` |
|      - |  9242 | `	ph7_value sKey;` |
|      - |  9243 | `	int bHit;` |
|    159 |  9244 | `	if( pCache == 0 ){` |
|    ! 0 |  9245 | `		return 0;` |
|      - |  9246 | `	}` |
|    159 |  9247 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|    159 |  9248 | `	bHit = PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry != 0;` |
|    159 |  9249 | `	PH7_MemObjRelease(&sKey);` |
|    159 |  9250 | `	return bHit;` |
|     80 |  9251 | `}` |
|      - |  9252 | `/*` |
|      - |  9253 | ` * php FREES the subtree a content write replaces -- except the nodes a PHP` |
|      - |  9254 | ` * variable still holds a wrapper for, which it unlinks and keeps as roots of` |
|      - |  9255 | ` * their own detached fragments. That is observable: after` |
|      - |  9256 | `` * `$el->textContent = 'flat'`, a variable holding a grandchild still reads its`` |
|      - |  9257 | `` * text and answers NULL for `parentNode`. Parking the subtree whole (which is`` |
|      - |  9258 | ` * what this engine must do -- a wrapper's handle is a raw pointer, so nothing` |
|      - |  9259 | ` * here is ever freed before the document is) left every such parent attached,` |
|      - |  9260 | ` * so the same variable answered its old parent's name. Detach exactly the nodes` |
|      - |  9261 | ` * php would have kept: the OUTERMOST wrapped ones, php's own rule, since it` |
|      - |  9262 | ` * stops recursing at a node it is keeping.` |
|      - |  9263 | ` */` |
|    146 |  9264 | `static void DomPartWrapped(ph7_vm *pVm,ph7_class_instance *pDoc,phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |  9265 | `{` |
|    147 |  9266 | `	xmlNodePtr pChild = pNode ? pNode->children : 0;` |
|    149 |  9267 | `	while( pChild ){` |
|      3 |  9268 | `		xmlNodePtr pNext = pChild->next;` |
|      3 |  9269 | `		if( DomIsWrapped(&(*pVm),pDoc,pChild) ){` |
|    ! 0 |  9270 | `			xmlUnlinkNode(pChild);` |
|    ! 0 |  9271 | `			DomOrphanAdd(pShell,pChild);` |
|    ! 0 |  9272 | `		}else{` |
|      3 |  9273 | `			DomPartWrapped(&(*pVm),pDoc,pShell,pChild);` |
|      - |  9274 | `		}` |
|      3 |  9275 | `		pChild = pNext;` |
|      1 |  9276 | `	}` |
|    147 |  9277 | `}` |
|      - |  9278 | `/*` |
|      - |  9279 | ` * Everything a content write has to do before libxml sees it: the node's` |
|      - |  9280 | ` * children go to the document's ORPHAN set rather than being freed, because a` |
|      - |  9281 | ` * PHP variable may still hold a wrapper for one of them and the wrapper's` |
|      - |  9282 | ` * handle is a raw pointer. (php keeps such a node alive through its own` |
|      - |  9283 | ` * wrapper refcount; this engine parks it, exactly as removeChild does.)` |
|      - |  9284 | ` */` |
|    110 |  9285 | `static void DomDropChildren(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      2 |  9286 | `{` |
|    112 |  9287 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|    112 |  9288 | `	xmlNodePtr pChild = pNode ? pNode->children : 0;` |
|    268 |  9289 | `	while( pChild ){` |
|    157 |  9290 | `		xmlNodePtr pNext = pChild->next;` |
|    157 |  9291 | `		if( !DomIsWrapped(pCtx->pVm,pDoc,pChild) ){` |
|    145 |  9292 | `			DomPartWrapped(pCtx->pVm,pDoc,pShell,pChild);` |
|     72 |  9293 | `		}` |
|    157 |  9294 | `		xmlUnlinkNode(pChild);` |
|    157 |  9295 | `		DomOrphanAdd(pShell,pChild);` |
|    157 |  9296 | `		pChild = pNext;` |
|      1 |  9297 | `	}` |
|    112 |  9298 | `}` |
|      - |  9299 | `/*` |
|      - |  9300 | ` * php's two content writes, which are NOT the same write.` |
|      - |  9301 | ` *` |
|      - |  9302 | `` * `nodeValue` is libxml's xmlNodeSetContent, and on an element or an attribute`` |
|      - |  9303 | `` * that PARSES entity references: `$el->nodeValue = 'a&b'` is libxml's`` |
|      - |  9304 | ` * "unterminated entity reference" and leaves the node EMPTY, while` |
|      - |  9305 | `` * `'a&amp;b'` stores the one character. `textContent` sets one raw text child`` |
|      - |  9306 | ` * instead, so the same two strings store what they say. Every other node kind` |
|      - |  9307 | ` * takes its content literally either way.` |
|      - |  9308 | ` */` |
|    100 |  9309 | `static void DomSetContent(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode,` |
|      - |  9310 | `	const char *zText,int bParseEntities)` |
|      2 |  9311 | `{` |
|    102 |  9312 | `	int bTree = pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE;` |
|      - |  9313 | `	xmlNodePtr pText;` |
|    102 |  9314 | `	DomDropChildren(pCtx,pShell,pNode);` |
|    102 |  9315 | `	if( !bTree \|\| (bParseEntities && zText[0]) ){` |
|      - |  9316 | `		/* The parsing write is the one that can FAIL -- an unterminated entity` |
|      - |  9317 | `		 * reference leaves the node empty and libxml says so. Route that through` |
|      - |  9318 | `		 * the per-VM queue like every other libxml diagnostic here, or it prints` |
|      - |  9319 | ``		 * itself on stderr past error_reporting(), past `@`, and past`` |
|      - |  9320 | `		 * libxml_get_errors(). */` |
|      - |  9321 | `		SyBlob sFn;` |
|     76 |  9322 | `		sxu32 nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|     76 |  9323 | `		xmlNodeSetContent(pNode,(const xmlChar *)zText);` |
|      - |  9324 | `		/* php attributes the warning to the CALLER's scope -- a property write` |
|      - |  9325 | `		 * is not a call, so there is no accessor name to print. */` |
|     76 |  9326 | `		SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|     76 |  9327 | `		PH7_VmActiveFuncName(pCtx->pVm,&sFn);` |
|     76 |  9328 | `		PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,(const char *)SyBlobData(&sFn));` |
|     76 |  9329 | `		SyBlobRelease(&sFn);` |
|     76 |  9330 | `		return;` |
|      - |  9331 | `	}` |
|      - |  9332 | `	/* One raw text child -- and php leaves one even for the EMPTY string, which` |
|      - |  9333 | ``	 * is why `$el->nodeValue = ''` serializes as <r></r> rather than <r/>.`` |
|      - |  9334 | `	 * (The entity-parsing write is the exception: a string libxml refuses, like` |
|      - |  9335 | ``	 * `'a&b'`, leaves the element with no children at all.) */`` |
|     27 |  9336 | `	pText = xmlNewDocText(pNode->doc,(const xmlChar *)zText);` |
|     27 |  9337 | `	if( pText ){` |
|     27 |  9338 | `		DomLinkLast(pNode,pText);` |
|     13 |  9339 | `	}` |
|     52 |  9340 | `}` |
|      - |  9341 | `/*` |
|      - |  9342 | ` * The same refusal, raised from a property WRITE.` |
|      - |  9343 | ` *` |
|      - |  9344 | ` * A write is not a call, so php has no accessor name to print in front of the` |
|      - |  9345 | `` * warning and attributes it to the CALLER's scope instead (`f(): Namespace`` |
|      - |  9346 | `` * Error`, `Unknown: ...` at file scope) -- the shape DomSetContent already uses`` |
|      - |  9347 | ` * for the libxml diagnostic a content write can produce. Nothing is answered` |
|      - |  9348 | ` * either way: a property write has no return value.` |
|      - |  9349 | ` */` |
|     10 |  9350 | `static int DomThrowWrite(ph7_context *pCtx,int iCode)` |
|      1 |  9351 | `{` |
|     11 |  9352 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|      - |  9353 | `	SyBlob sFn;` |
|      - |  9354 | `	SyString sName;` |
|      - |  9355 | `	int rc;` |
|     11 |  9356 | `	if( pDoc == 0 \|\| DomDocFlag(pDoc,DOM_F_STRICT_ERR) ){` |
|      7 |  9357 | `		return DomThrowAlways(pCtx,iCode);` |
|      - |  9358 | `	}` |
|      5 |  9359 | `	SyBlobInit(&sFn,&pCtx->pVm->sAllocator);` |
|      5 |  9360 | `	PH7_VmActiveFuncName(pCtx->pVm,&sFn);` |
|      5 |  9361 | `	SyStringInitFromBuf(&sName,SyBlobData(&sFn),SyBlobLength(&sFn));` |
|      5 |  9362 | `	rc = PH7_VmThrowError(pCtx->pVm,&sName,PH7_CTX_WARNING,DomErrText(iCode));` |
|      5 |  9363 | `	SyBlobRelease(&sFn);` |
|      5 |  9364 | `	return rc;` |
|      6 |  9365 | `}` |
|      - |  9366 | `/*` |
|      - |  9367 | ` * The node kinds a value write REACHES.` |
|      - |  9368 | ` *` |
|      - |  9369 | ` * php's two writers do not accept the same list, and everything off it is a` |
|      - |  9370 | ` * silent NO-OP -- the write is accepted and the node is left exactly as it was.` |
|      - |  9371 | ` * PHL had one exclusion, the document, and wrote to everything else, which cost` |
|      - |  9372 | ` * three answers and one crash:` |
|      - |  9373 | ` *` |
|      - |  9374 | ` *   - an ENTITY REFERENCE's children are the entity DECLARATION's, shared by` |
|      - |  9375 | ` *     every reference to it and owned by the DTD; dropping them freed nodes the` |
|      - |  9376 | `` *     document frees again at teardown -- `$ref->nodeValue = 'x'` aborted the`` |
|      - |  9377 | ` *     process on a double free;` |
|      - |  9378 | ` *   - a DOCTYPE's children are the declarations of the internal SUBSET, so` |
|      - |  9379 | ``  *     `$doc->doctype->nodeValue = 'x'` silently emptied `<!DOCTYPE r [ ... ]>` `` |
|      - |  9380 | ` *     of every entity, element and attribute declaration in it;` |
|      - |  9381 | `` *   - a FRAGMENT is emptied by `textContent` and left alone by `nodeValue`,`` |
|      - |  9382 | ` *     which is the one kind where the two writers really do disagree.` |
|      - |  9383 | ` */` |
|     78 |  9384 | `static int DomValueWritable(xmlNodePtr pNode,int bValue)` |
|      1 |  9385 | `{` |
|     79 |  9386 | `	if( pNode == 0 ){` |
|      3 |  9387 | `		return 0;` |
|      - |  9388 | `	}` |
|     77 |  9389 | `	switch( pNode->type ){` |
|     30 |  9390 | `	case XML_ELEMENT_NODE:` |
|      - |  9391 | `	case XML_ATTRIBUTE_NODE:` |
|      - |  9392 | `	case XML_TEXT_NODE:` |
|      - |  9393 | `	case XML_COMMENT_NODE:` |
|      - |  9394 | `	case XML_CDATA_SECTION_NODE:` |
|      - |  9395 | `	case XML_PI_NODE:` |
|     61 |  9396 | `		return 1;` |
|      2 |  9397 | `	case XML_DOCUMENT_FRAG_NODE:` |
|      5 |  9398 | `		return !bValue;` |
|      6 |  9399 | `	default:` |
|     13 |  9400 | `		return 0;` |
|      - |  9401 | `	}` |
|     40 |  9402 | `}` |
|      - |  9403 | `/* DOMNode's three writable properties. */` |
|    182 |  9404 | `static int DomSetNodeProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9405 | `{` |
|    183 |  9406 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    183 |  9407 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    183 |  9408 | `	int bValue = DomNameIs(zName,"nodeValue");` |
|      - |  9409 | `	SyBlob sVal;` |
|    183 |  9410 | `	if( bValue \|\| DomNameIs(zName,"textContent") ){` |
|      - |  9411 | `		/* php's one REFUSAL among the no-ops, and it answers before the type` |
|      - |  9412 | ``		 * check does: `$ref->textContent = []` is the readonly Error where`` |
|      - |  9413 | ``		 * `$ref->nodeValue = []` is the TypeError. Left unwritten here, the`` |
|      - |  9414 | `		 * accessor macro falls through to it. */` |
|     93 |  9415 | `		if( !bValue && pNode && pNode->type == XML_ENTITY_REF_NODE ){` |
|      5 |  9416 | `			return DOM_SET_UNKNOWN;` |
|      - |  9417 | `		}` |
|    132 |  9418 | `		if( DomWriteText(pCtx,"DOMNode",bValue ? "nodeValue" : "textContent",` |
|     89 |  9419 | `			bValue ? "?string" : "string",pVal,&sVal,pRc) == 0 ){` |
|     11 |  9420 | `			return DOM_SET_DONE;` |
|      - |  9421 | `		}` |
|     79 |  9422 | `		if( DomValueWritable(pNode,bValue) ){` |
|     63 |  9423 | `			DomSetContent(pCtx,pNd->pShell,pNode,(const char *)SyBlobData(&sVal),bValue);` |
|     31 |  9424 | `		}` |
|     79 |  9425 | `		SyBlobRelease(&sVal);` |
|     79 |  9426 | `		return DOM_SET_DONE;` |
|      - |  9427 | `	}` |
|     91 |  9428 | `	if( DomNameIs(zName,"prefix") ){` |
|      - |  9429 | `		xmlNsPtr pNs;` |
|      - |  9430 | `		xmlNodePtr pDecl;` |
|     29 |  9431 | `		if( DomWriteText(pCtx,"DOMNode","prefix","string",pVal,&sVal,pRc) == 0 ){` |
|      5 |  9432 | `			return DOM_SET_DONE;` |
|      - |  9433 | `		}` |
|      - |  9434 | `		/* Only a node that HAS a namespace can be re-prefixed; php ignores the` |
|      - |  9435 | `		 * write for anything else, including an element in no namespace. */` |
|     25 |  9436 | `		if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->href ){` |
|     21 |  9437 | `			const char *zPrefix = (const char *)SyBlobData(&sVal);` |
|      - |  9438 | `			/* An attribute's declaration goes on its ELEMENT. */` |
|     21 |  9439 | `			pDecl = pNode->type == XML_ATTRIBUTE_NODE ? pNode->parent : pNode;` |
|     20 |  9440 | `			if( DomNameIs(zPrefix,"xml")` |
|     15 |  9441 | `			 && !DomNameIs((const char *)pNode->ns->href,` |
|      - |  9442 | `				"http://www.w3.org/XML/1998/namespace") ){` |
|      - |  9443 | ``				/* php's reserved-prefix refusal: `xml` may only name ITS namespace. */`` |
|      9 |  9444 | `				SyBlobRelease(&sVal);` |
|      9 |  9445 | `				*pRc = DomThrowWrite(pCtx,DOM_ERR_NAMESPACE);` |
|      9 |  9446 | `				return DOM_SET_DONE;` |
|      - |  9447 | `			}` |
|      - |  9448 | `			/* php looks only at the declarations THIS node carries -- an` |
|      - |  9449 | `			 * ancestor's is not reused, which is why re-prefixing a child grows` |
|      - |  9450 | ``			 * a second `xmlns:q` beside the one its parent already has. */`` |
|     29 |  9451 | `			for( pNs = pDecl ? pDecl->nsDef : 0 ; pNs ; pNs = pNs->next ){` |
|     19 |  9452 | `				const char *zHave = pNs->prefix ? (const char *)pNs->prefix : "";` |
|     18 |  9453 | `				if( DomNameIs(zHave,zPrefix) && pNs->href` |
|      5 |  9454 | `				 && xmlStrEqual(pNs->href,pNode->ns->href) ){` |
|      3 |  9455 | `					break;` |
|      - |  9456 | `				}` |
|      9 |  9457 | `			}` |
|     13 |  9458 | `			if( pNs == 0 ){` |
|      - |  9459 | `				/* None binds this prefix to the node's own URI: php declares one,` |
|      - |  9460 | ``				 * which is how `$el->prefix = ''` grows an `xmlns="..."` on the`` |
|      - |  9461 | `				 * element itself -- and how a prefix already bound HERE to another` |
|      - |  9462 | `				 * URI becomes libxml's refusal and php's Namespace Error. */` |
|     16 |  9463 | `				pNs = pDecl ? xmlNewNs(pDecl,pNode->ns->href,` |
|     10 |  9464 | `					zPrefix[0] ? (const xmlChar *)zPrefix : 0) : 0;` |
|      5 |  9465 | `			}` |
|     13 |  9466 | `			if( pNs == 0 ){` |
|      3 |  9467 | `				SyBlobRelease(&sVal);` |
|      3 |  9468 | `				*pRc = DomThrowWrite(pCtx,DOM_ERR_NAMESPACE);` |
|      3 |  9469 | `				return DOM_SET_DONE;` |
|      - |  9470 | `			}` |
|     11 |  9471 | `			xmlSetNs(pNode,pNs);` |
|      5 |  9472 | `		}` |
|     15 |  9473 | `		SyBlobRelease(&sVal);` |
|     15 |  9474 | `		return DOM_SET_DONE;` |
|      - |  9475 | `	}` |
|     63 |  9476 | `	return DOM_SET_UNKNOWN;` |
|     92 |  9477 | `}` |
|      - |  9478 | `/* DOMElement adds className and id, both of them ATTRIBUTES under the name. */` |
|    108 |  9479 | `static int DomSetElemProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9480 | `{` |
|    109 |  9481 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    109 |  9482 | `	int bClass = DomNameIs(zName,"className");` |
|      - |  9483 | `	SyBlob sVal;` |
|    109 |  9484 | `	if( bClass \|\| DomNameIs(zName,"id") ){` |
|      9 |  9485 | `		if( DomWriteText(pCtx,"DOMElement",bClass ? "className" : "id","string",` |
|      7 |  9486 | `			pVal,&sVal,pRc) == 0 ){` |
|      3 |  9487 | `			return DOM_SET_DONE;` |
|      - |  9488 | `		}` |
|      5 |  9489 | `		if( pNd ){` |
|      7 |  9490 | `			xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)(bClass ? "class" : "id"),` |
|      4 |  9491 | `				(const xmlChar *)SyBlobData(&sVal));` |
|      2 |  9492 | `		}` |
|      5 |  9493 | `		SyBlobRelease(&sVal);` |
|      5 |  9494 | `		return DOM_SET_DONE;` |
|      - |  9495 | `	}` |
|    103 |  9496 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|     55 |  9497 | `}` |
|      - |  9498 | `/* DOMAttr::value and DOMCharacterData::data are the node's own content. The` |
|      - |  9499 | `` * attribute's parses entity references, as its `nodeValue` does -- only`` |
|      - |  9500 | `` * `textContent` takes an attribute's bytes literally; character data has no`` |
|      - |  9501 | ` * parsing write at all, whichever name it is written under. */` |
|     80 |  9502 | `static int DomSetContentProp(ph7_context *pCtx,const char *zOwner,const char *zProp,` |
|      - |  9503 | `	const char *zName,ph7_value *pVal,int bParseEntities,int *pRc)` |
|      2 |  9504 | `{` |
|     82 |  9505 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|      - |  9506 | `	SyBlob sVal;` |
|     82 |  9507 | `	if( !DomNameIs(zName,zProp) ){` |
|     39 |  9508 | `		return DOM_SET_UNKNOWN;` |
|      - |  9509 | `	}` |
|     44 |  9510 | `	if( DomWriteText(pCtx,zOwner,zProp,"string",pVal,&sVal,pRc) == 0 ){` |
|      5 |  9511 | `		return DOM_SET_DONE;` |
|      - |  9512 | `	}` |
|     40 |  9513 | `	if( pNd ){` |
|     59 |  9514 | `		DomSetContent(pCtx,pNd->pShell,(xmlNodePtr)pNd->pNode,` |
|     38 |  9515 | `			(const char *)SyBlobData(&sVal),bParseEntities);` |
|     19 |  9516 | `	}` |
|     40 |  9517 | `	SyBlobRelease(&sVal);` |
|     40 |  9518 | `	return DOM_SET_DONE;` |
|     42 |  9519 | `}` |
|     40 |  9520 | `static int DomSetAttrProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      2 |  9521 | `{` |
|     42 |  9522 | `	int rc = DomSetContentProp(pCtx,"DOMAttr","value",zName,pVal,TRUE,pRc);` |
|     42 |  9523 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      2 |  9524 | `}` |
|     30 |  9525 | `static int DomSetCharProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9526 | `{` |
|     31 |  9527 | `	int rc = DomSetContentProp(pCtx,"DOMCharacterData","data",zName,pVal,FALSE,pRc);` |
|     31 |  9528 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      1 |  9529 | `}` |
|     10 |  9530 | `static int DomSetPiProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9531 | `{` |
|     11 |  9532 | `	int rc = DomSetContentProp(pCtx,"DOMProcessingInstruction","data",zName,pVal,FALSE,pRc);` |
|     11 |  9533 | `	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      1 |  9534 | `}` |
|      - |  9535 | `/*` |
|      - |  9536 | ` * The DOCUMENT's writable state: the three declaration slots php lets a program` |
|      - |  9537 | `` * change, plus `documentURI`.`` |
|      - |  9538 | ` *` |
|      - |  9539 | `` * php declares them `?string`/`bool`, so a null goes through the string three as`` |
|      - |  9540 | `` * the EMPTY string (`$d->version = null` writes `<?xml version=""?>`) and is a`` |
|      - |  9541 | ` * TypeError on the bool pair; an array or an object is a TypeError on all of` |
|      - |  9542 | ` * them.  The read-only four are refused HERE rather than through the reader,` |
|      - |  9543 | ` * because two of them are deprecated and php's readonly Error comes without the` |
|      - |  9544 | ` * deprecation notice a read would have raised.` |
|      - |  9545 | ` */` |
|    456 |  9546 | `static int DomSetDocProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9547 | `{` |
|    457 |  9548 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    457 |  9549 | `	xmlDocPtr pDoc = pNd ? (xmlDocPtr)pNd->pNode : 0;` |
|    457 |  9550 | `	int bVersion = DomNameIs(zName,"version") \|\| DomNameIs(zName,"xmlVersion");` |
|    457 |  9551 | `	int bUri = DomNameIs(zName,"documentURI");` |
|      - |  9552 | `	SyBlob sVal;` |
|    456 |  9553 | `	if( DomNameIs(zName,"actualEncoding") \|\| DomNameIs(zName,"config")` |
|    454 |  9554 | `	 \|\| DomNameIs(zName,"xmlEncoding") ){` |
|      7 |  9555 | `		*pRc = DomRefuseWrite(pCtx,zName);` |
|      7 |  9556 | `		return DOM_SET_DONE;` |
|      - |  9557 | `	}` |
|    451 |  9558 | `	if( bVersion \|\| bUri \|\| DomNameIs(zName,"encoding") ){` |
|      - |  9559 | `		const char *zNew;` |
|     73 |  9560 | `		if( DomWriteText(pCtx,"DOMDocument",zName,"?string",pVal,&sVal,pRc) == 0 ){` |
|     15 |  9561 | `			return DOM_SET_DONE;` |
|      - |  9562 | `		}` |
|     59 |  9563 | `		zNew = (const char *)SyBlobData(&sVal);` |
|     59 |  9564 | `		if( pDoc == 0 ){` |
|    ! 0 |  9565 | `			SyBlobRelease(&sVal);` |
|    ! 0 |  9566 | `			return DOM_SET_DONE;` |
|      - |  9567 | `		}` |
|     59 |  9568 | `		if( bVersion ){` |
|     27 |  9569 | `			if( pDoc->version ){` |
|     27 |  9570 | `				xmlFree((xmlChar *)pDoc->version);` |
|     13 |  9571 | `			}` |
|     27 |  9572 | `			pDoc->version = xmlStrdup((const xmlChar *)zNew);` |
|     46 |  9573 | `		}else if( bUri ){` |
|     15 |  9574 | `			if( pDoc->URL ){` |
|     15 |  9575 | `				xmlFree((xmlChar *)pDoc->URL);` |
|      7 |  9576 | `			}` |
|     15 |  9577 | `			pDoc->URL = xmlStrdup((const xmlChar *)zNew);` |
|      8 |  9578 | `		}else{` |
|      - |  9579 | `			/* php asks libxml for a converter and refuses the name outright when` |
|      - |  9580 | ``			 * there is none -- so `$d->encoding = 'x'` (and the empty string a`` |
|      - |  9581 | `			 * null coerces to) is a ValueError BEFORE anything is written,` |
|      - |  9582 | `			 * rather than a document that cannot be serialized later. */` |
|      - |  9583 | `			/* A null is refused before libxml is asked anything, as php does: the` |
|      - |  9584 | `			 * empty string it coerces to is a name a current libxml (2.15) answers` |
|      - |  9585 | `			 * WITH a converter, so asking would let the null through. */` |
|     19 |  9586 | `			xmlCharEncodingHandlerPtr pEnc = ph7_value_is_null(pVal) ? 0` |
|     17 |  9587 | `				: xmlFindCharEncodingHandler(zNew);` |
|     19 |  9588 | `			if( pEnc == 0 ){` |
|      8 |  9589 | `				SyBlobRelease(&sVal);` |
|      8 |  9590 | `				*pRc = DomPropThrow(pCtx,"ValueError",0,"Invalid document encoding");` |
|      8 |  9591 | `				return DOM_SET_DONE;` |
|      - |  9592 | `			}` |
|     12 |  9593 | `			xmlCharEncCloseFunc(pEnc);` |
|     12 |  9594 | `			if( pDoc->encoding ){` |
|    ! 0 |  9595 | `				xmlFree((xmlChar *)pDoc->encoding);` |
|    ! 0 |  9596 | `			}` |
|     12 |  9597 | `			pDoc->encoding = xmlStrdup((const xmlChar *)zNew);` |
|      - |  9598 | `		}` |
|     52 |  9599 | `		SyBlobRelease(&sVal);` |
|     52 |  9600 | `		return DOM_SET_DONE;` |
|      - |  9601 | `	}` |
|      - |  9602 | `	{` |
|      - |  9603 | ``		/* The seven directives. php declares each `bool`, so the screen is the`` |
|      - |  9604 | `		 * typed-property one a real slot would have applied, and the value lands` |
|      - |  9605 | `		 * in the hidden word rather than in a property of its own. */` |
|      - |  9606 | `		sxu32 i;` |
|   2015 |  9607 | `		for( i = 0 ; i < SX_ARRAYSIZE(aDomDocFlag) ; ++i ){` |
|   1971 |  9608 | `			if( DomNameIs(zName,aDomDocFlag[i].zName) ){` |
|    335 |  9609 | `				ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  9610 | `				sxi64 iWord;` |
|    335 |  9611 | `				if( DomWriteBool(pCtx,"DOMDocument",zName,pVal,pRc) == 0 ){` |
|      5 |  9612 | `					return DOM_SET_DONE;` |
|      - |  9613 | `				}` |
|    331 |  9614 | `				iWord = pThis ? PH7_NativeAttrInt(pThis,DOM_DFLAGS) : 0;` |
|    331 |  9615 | `				if( ph7_value_to_bool(pVal) ){` |
|    149 |  9616 | `					iWord \|= aDomDocFlag[i].iBit;` |
|     75 |  9617 | `				}else{` |
|    183 |  9618 | `					iWord &= ~(sxi64)aDomDocFlag[i].iBit;` |
|      - |  9619 | `				}` |
|    331 |  9620 | `				if( pThis ){` |
|    331 |  9621 | `					PH7_NativeSetAttrInt(pCtx->pVm,pThis,DOM_DFLAGS,iWord);` |
|    165 |  9622 | `				}` |
|    331 |  9623 | `				return DOM_SET_DONE;` |
|      - |  9624 | `			}` |
|    819 |  9625 | `		}` |
|      - |  9626 | `	}` |
|     45 |  9627 | `	if( DomNameIs(zName,"standalone") \|\| DomNameIs(zName,"xmlStandalone") ){` |
|     35 |  9628 | `		if( DomWriteBool(pCtx,"DOMDocument",zName,pVal,pRc) == 0 ){` |
|      9 |  9629 | `			return DOM_SET_DONE;` |
|      - |  9630 | `		}` |
|     27 |  9631 | `		if( pDoc ){` |
|      - |  9632 | `			/* Either way it becomes a DECLARED answer: writing false is` |
|      - |  9633 | ``			 * `standalone="no"` in the output, not the absent attribute. */`` |
|     27 |  9634 | `			pDoc->standalone = ph7_value_to_bool(pVal) ? 1 : 0;` |
|     13 |  9635 | `		}` |
|     27 |  9636 | `		return DOM_SET_DONE;` |
|      - |  9637 | `	}` |
|     11 |  9638 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|    229 |  9639 | `}` |
|      - |  9640 | ``/* The two collections have nothing writable of their own; `length` is read-only. */`` |
|      6 |  9641 | `static int DomSetNothing(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9642 | `{` |
|      3 |  9643 | `	SXUNUSED(pCtx); SXUNUSED(zName); SXUNUSED(pVal); SXUNUSED(pRc);` |
|      7 |  9644 | `	return DOM_SET_UNKNOWN;` |
|      1 |  9645 | `}` |
|      - |  9646 | `/* DOMProcessingInstruction adds target (its name) and data (its content) --` |
|      - |  9647 | ` * php declares it under DOMNode, not DOMCharacterData, so the character-data` |
|      - |  9648 | ` * methods are deliberately absent from it. */` |
|     80 |  9649 | `static int DomPiProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9650 | `{` |
|     81 |  9651 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     81 |  9652 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     81 |  9653 | `	if( DomNameIs(zName,"target") ){` |
|     15 |  9654 | `		ph7_result_string(pCtx,(pNode && pNode->name) ? (const char *)pNode->name : "",-1);` |
|     15 |  9655 | `		return 1;` |
|      - |  9656 | `	}` |
|     67 |  9657 | `	if( DomNameIs(zName,"data") ){` |
|     13 |  9658 | `		DomDataValue(pCtx,pNode);` |
|     13 |  9659 | `		return 1;` |
|      - |  9660 | `	}` |
|     55 |  9661 | `	return DomNodeProp(pCtx,zName);` |
|     41 |  9662 | `}` |
|      - |  9663 | `/*` |
|      - |  9664 | ` * DOMNameSpaceNode's ten properties. It is not a DOMNode -- php gives it its` |
|      - |  9665 | ` * own class with no parent -- so it shares none of the readers above: what it` |
|      - |  9666 | ` * carries is the DECLARATION (an xmlNs) and the element that makes it.` |
|      - |  9667 | ` */` |
|    194 |  9668 | `static int DomNsNodeProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9669 | `{` |
|    195 |  9670 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    195 |  9671 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    195 |  9672 | `	xmlNsPtr pNs = pNd ? (xmlNsPtr)pNd->pNode : 0;` |
|    195 |  9673 | `	ph7_class_instance *pOwner = pThis ? PH7_NativeAttrObj(pThis,DOM_NS_OWNER) : 0;` |
|    195 |  9674 | `	phl_domnode *pOwnerNd = DomResOf(pOwner);` |
|    195 |  9675 | `	const char *zPrefix = (pNs && pNs->prefix) ? (const char *)pNs->prefix : 0;` |
|    195 |  9676 | `	if( pNs == 0 ){` |
|     13 |  9677 | `		return 0;` |
|      - |  9678 | `	}` |
|    183 |  9679 | `	if( DomNameIs(zName,"nodeName") ){` |
|     31 |  9680 | `		if( zPrefix ){` |
|     27 |  9681 | `			ph7_result_string_format(pCtx,"%s:%s",DOM_XMLNS_NAME,zPrefix);` |
|     14 |  9682 | `		}else{` |
|      5 |  9683 | `			ph7_result_string(pCtx,DOM_XMLNS_NAME,-1);` |
|      - |  9684 | `		}` |
|     31 |  9685 | `		return 1;` |
|      - |  9686 | `	}` |
|    153 |  9687 | `	if( DomNameIs(zName,"nodeValue") ){` |
|      - |  9688 | `		/* php builds its wrapper as a fake node whose text CHILD carries the` |
|      - |  9689 | `		 * URI, and an EMPTY href writes no child at all -- so the xmlns=""` |
|      - |  9690 | `		 * UNDECLARATION answers null here while namespaceURI below answers` |
|      - |  9691 | `		 * the empty string off the href itself. Both doors (the attribute` |
|      - |  9692 | `		 * lookups and the namespace:: axis) share this recognizer. */` |
|     35 |  9693 | `		if( pNs->href && pNs->href[0] ){` |
|     33 |  9694 | `			ph7_result_string(pCtx,(const char *)pNs->href,-1);` |
|     17 |  9695 | `		}else{` |
|      3 |  9696 | `			ph7_result_null(pCtx);` |
|      - |  9697 | `		}` |
|     35 |  9698 | `		return 1;` |
|      - |  9699 | `	}` |
|    119 |  9700 | `	if( DomNameIs(zName,"namespaceURI") ){` |
|     21 |  9701 | `		ph7_result_string(pCtx,pNs->href ? (const char *)pNs->href : "",-1);` |
|     21 |  9702 | `		return 1;` |
|      - |  9703 | `	}` |
|     99 |  9704 | `	if( DomNameIs(zName,"nodeType") ){` |
|     17 |  9705 | `		ph7_result_int(pCtx,XML_NAMESPACE_DECL);` |
|     17 |  9706 | `		return 1;` |
|      - |  9707 | `	}` |
|      - |  9708 | ``	/* php answers the EMPTY prefix for the default declaration, and `xmlns` as`` |
|      - |  9709 | `	 * its local name -- the two halves of the name it is spelled with. */` |
|     83 |  9710 | `	if( DomNameIs(zName,"prefix") ){` |
|     21 |  9711 | `		ph7_result_string(pCtx,zPrefix ? zPrefix : "",-1);` |
|     21 |  9712 | `		return 1;` |
|      - |  9713 | `	}` |
|     63 |  9714 | `	if( DomNameIs(zName,"localName") ){` |
|     19 |  9715 | `		ph7_result_string(pCtx,zPrefix ? zPrefix : DOM_XMLNS_NAME,-1);` |
|     19 |  9716 | `		return 1;` |
|      - |  9717 | `	}` |
|     45 |  9718 | `	if( DomNameIs(zName,"isConnected") ){` |
|      5 |  9719 | `		ph7_result_bool(pCtx,pOwnerNd != 0` |
|      2 |  9720 | `			&& DomIsConnected((xmlNodePtr)pOwnerNd->pNode));` |
|      3 |  9721 | `		return 1;` |
|      - |  9722 | `	}` |
|     43 |  9723 | `	if( DomNameIs(zName,"ownerDocument") ){` |
|     11 |  9724 | `		DomResultWrap(pCtx,DomThisDoc(pCtx));` |
|     11 |  9725 | `		return 1;` |
|      - |  9726 | `	}` |
|     33 |  9727 | `	if( DomNameIs(zName,"parentNode") \|\| DomNameIs(zName,"parentElement") ){` |
|     29 |  9728 | `		DomResultWrap(pCtx,pOwner);` |
|     29 |  9729 | `		return 1;` |
|      - |  9730 | `	}` |
|      5 |  9731 | `	return 0;` |
|     98 |  9732 | `}` |
|      - |  9733 | `/*` |
|      - |  9734 | ` * Every property DOMEntity adds is read-only, and the refusal is raised HERE` |
|      - |  9735 | ` * rather than by falling through to the reader: the reader is where the three` |
|      - |  9736 | ` * deprecated names raise their notice, and php's write never reaches it -- the` |
|      - |  9737 | ` * readonly Error comes first and says nothing about deprecation.` |
|      - |  9738 | ` */` |
|     12 |  9739 | `static int DomSetEntityProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9740 | `{` |
|     12 |  9741 | `	if( DomNameIs(zName,"publicId") \|\| DomNameIs(zName,"systemId")` |
|      9 |  9742 | `	 \|\| DomNameIs(zName,"notationName") \|\| DomNameIs(zName,"actualEncoding")` |
|      6 |  9743 | `	 \|\| DomNameIs(zName,"encoding") \|\| DomNameIs(zName,"version") ){` |
|     13 |  9744 | `		*pRc = DomRefuseWrite(pCtx,zName);` |
|     13 |  9745 | `		return DOM_SET_DONE;` |
|      - |  9746 | `	}` |
|    ! 0 |  9747 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      7 |  9748 | `}` |
|      - |  9749 | `/*` |
|      - |  9750 | `` * DOMNotation: the two identifiers a `<!NOTATION ...>` declares.`` |
|      - |  9751 | ` *` |
|      - |  9752 | ` * Both are plain strings that answer "" for the half that is absent -- a` |
|      - |  9753 | `` * SYSTEM-only notation reads "" from `publicId` -- where DOMEntity's same-named`` |
|      - |  9754 | `` * pair are `?string`. The node under them is the stand-in DomNotationNode`` |
|      - |  9755 | ` * built, which shares the entity's layout, so both identifiers are read from` |
|      - |  9756 | ` * the same two fields.` |
|      - |  9757 | ` */` |
|     80 |  9758 | `static int DomNotationProp(ph7_context *pCtx,const char *zName)` |
|      2 |  9759 | `{` |
|     82 |  9760 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     82 |  9761 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     79 |  9762 | `	xmlEntityPtr pNot = (pNode && pNode->type == XML_NOTATION_NODE)` |
|    114 |  9763 | `		? (xmlEntityPtr)pNode : 0;` |
|     82 |  9764 | `	if( DomNameIs(zName,"publicId") ){` |
|      9 |  9765 | `		ph7_result_string(pCtx,(pNot && pNot->ExternalID) ? (const char *)pNot->ExternalID : "",-1);` |
|      9 |  9766 | `		return 1;` |
|      - |  9767 | `	}` |
|     74 |  9768 | `	if( DomNameIs(zName,"systemId") ){` |
|     10 |  9769 | `		ph7_result_string(pCtx,(pNot && pNot->SystemID) ? (const char *)pNot->SystemID : "",-1);` |
|     10 |  9770 | `		return 1;` |
|      - |  9771 | `	}` |
|     65 |  9772 | `	return DomNodeProp(pCtx,zName);` |
|     42 |  9773 | `}` |
|      4 |  9774 | `static int DomSetNotationProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9775 | `{` |
|      5 |  9776 | `	if( DomNameIs(zName,"publicId") \|\| DomNameIs(zName,"systemId") ){` |
|      5 |  9777 | `		*pRc = DomRefuseWrite(pCtx,zName);` |
|      5 |  9778 | `		return DOM_SET_DONE;` |
|      - |  9779 | `	}` |
|    ! 0 |  9780 | `	return DomSetNodeProp(pCtx,zName,pVal,pRc);` |
|      3 |  9781 | `}` |
|      - |  9782 | `/*` |
|      - |  9783 | ` * DOMXPath's two, out of the hidden slots that hold them: php declares both` |
|      - |  9784 | `` * VIRTUAL, so `document` is read-only because its handler has no writer (and not`` |
|      - |  9785 | `` * because the slot is `readonly`, which is why php's isReadOnly() is false there)`` |
|      - |  9786 | `` * and the write refusal is the reader's, exactly as it is for a node's `nodeName`.`` |
|      - |  9787 | ` */` |
|     24 |  9788 | `static int DomXPathProp(ph7_context *pCtx,const char *zName)` |
|      1 |  9789 | `{` |
|     25 |  9790 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 |  9791 | `	if( DomNameIs(zName,"document") ){` |
|     11 |  9792 | `		DomResultWrap(pCtx,pThis ? PH7_NativeAttrObj(pThis,XP_DOC) : 0);` |
|     11 |  9793 | `		return 1;` |
|      - |  9794 | `	}` |
|     15 |  9795 | `	if( DomNameIs(zName,"registerNodeNamespaces") ){` |
|     13 |  9796 | `		ph7_result_bool(pCtx,pThis ? PH7_NativeAttrTruthy(pThis,XP_NSDEF) : 1);` |
|     13 |  9797 | `		return 1;` |
|      - |  9798 | `	}` |
|      3 |  9799 | `	return 0;` |
|     13 |  9800 | `}` |
|     12 |  9801 | `static int DomSetXPathProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)` |
|      1 |  9802 | `{` |
|     13 |  9803 | `	if( DomNameIs(zName,"registerNodeNamespaces") ){` |
|      9 |  9804 | `		ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      9 |  9805 | `		if( DomWriteBool(pCtx,"DOMXPath",zName,pVal,pRc) == 0 ){` |
|      3 |  9806 | `			return DOM_SET_DONE;` |
|      - |  9807 | `		}` |
|      7 |  9808 | `		if( pThis ){` |
|      7 |  9809 | `			PH7_NativeSetAttrBool(pCtx->pVm,pThis,XP_NSDEF,ph7_value_to_bool(pVal));` |
|      3 |  9810 | `		}` |
|      7 |  9811 | `		return DOM_SET_DONE;` |
|      - |  9812 | `	}` |
|      - |  9813 | ``	/* `document` falls through to the shared refusal, which reads the class's own`` |
|      - |  9814 | ``	 * recognizer and words php's `Cannot modify readonly property`. */`` |
|      5 |  9815 | `	return DOM_SET_UNKNOWN;` |
|      7 |  9816 | `}` |
|      - |  9817 | `/*` |
|      - |  9818 | ` * DOMDocumentFragment::appendXML(string $data): bool` |
|      - |  9819 | ` *` |
|      - |  9820 | ` * php parses the chunk as a well-balanced FRAGMENT (no single root required,` |
|      - |  9821 | ` * bare text allowed) and appends what it produced; anything libxml refuses is` |
|      - |  9822 | `` * `false` with nothing appended.`` |
|      - |  9823 | ` */` |
|     44 |  9824 | `DOM_METHOD(vm_builtin_DOMDocumentFragment_appendXML)` |
|      1 |  9825 | `{` |
|     45 |  9826 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|     45 |  9827 | `	const char *zXml = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|     45 |  9828 | `	xmlNodePtr pFrag,pList = 0;` |
|      - |  9829 | `	sxu32 nMark;` |
|      - |  9830 | `	int rc;` |
|     45 |  9831 | `	if( pNd == 0 ){` |
|    ! 0 |  9832 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  9833 | `		return PH7_OK;` |
|      - |  9834 | `	}` |
|     45 |  9835 | `	pFrag = (xmlNodePtr)pNd->pNode;` |
|     45 |  9836 | `	if( DomNodeReadOnly(pFrag) ){` |
|      - |  9837 | ``		/* A CONSTRUCTED fragment -- `new DOMDocumentFragment()` -- refuses`` |
|      - |  9838 | `		 * this door the way every child-list door refuses an ownerless` |
|      - |  9839 | `		 * receiver, where its append() takes the same chunk's nodes. */` |
|      3 |  9840 | `		return DomThrow(pCtx,DOM_ERR_NO_MOD);` |
|      - |  9841 | `	}` |
|     43 |  9842 | `	nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);` |
|     43 |  9843 | `	rc = xmlParseBalancedChunkMemory(pFrag->doc,0,0,0,(const xmlChar *)zXml,&pList);` |
|     43 |  9844 | `	PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,"DOMDocumentFragment::appendXML");` |
|     43 |  9845 | `	if( rc != 0 ){` |
|      3 |  9846 | `		if( pList ){` |
|    ! 0 |  9847 | `			xmlFreeNodeList(pList);` |
|    ! 0 |  9848 | `		}` |
|      3 |  9849 | `		ph7_result_bool(pCtx,0);` |
|      3 |  9850 | `		return PH7_OK;` |
|      - |  9851 | `	}` |
|    101 |  9852 | `	while( pList ){` |
|     61 |  9853 | `		xmlNodePtr pNext = pList->next;` |
|     61 |  9854 | `		pList->next = pList->prev = 0;` |
|     61 |  9855 | `		DomLinkLast(pFrag,pList);` |
|     61 |  9856 | `		pList = pNext;` |
|      1 |  9857 | `	}` |
|     41 |  9858 | `	ph7_result_bool(pCtx,1);` |
|     41 |  9859 | `	return PH7_OK;` |
|     23 |  9860 | `}` |
|      - |  9861 | `/* DOMDocument::getElementsByTagName / DOMElement::getElementsByTagName --` |
|      - |  9862 | ` * php declares it on those two, not on DOMNode, so both specs name it. */` |
|    102 |  9863 | `DOM_METHOD(vm_builtin_Dom_getElementsByTagName)` |
|      1 |  9864 | `{` |
|    103 |  9865 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    103 |  9866 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|      - |  9867 | `	ph7_class_instance *pList;` |
|    103 |  9868 | `	if( pThis == 0 ){` |
|    ! 0 |  9869 | `		return PH7_OK;` |
|      - |  9870 | `	}` |
|    103 |  9871 | `	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTN,pThis,zName,0,0);` |
|    103 |  9872 | `	if( pList == 0 ){` |
|    ! 0 |  9873 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  9874 | `	}` |
|    103 |  9875 | `	PH7_NativeResultObject(pCtx,pList);` |
|    103 |  9876 | `	return PH7_OK;` |
|     52 |  9877 | `}` |
|      - |  9878 | `/*` |
|      - |  9879 | ` * DOMDocument::getElementsByTagNameNS / DOMElement::getElementsByTagNameNS` |
|      - |  9880 | ` * (?string $namespace, string $localName): DOMNodeList` |
|      - |  9881 | ` *` |
|      - |  9882 | ` * The namespace-aware half of the only two lookups the DOM has, and the one` |
|      - |  9883 | ` * every namespaced format is read with -- an XSLT stylesheet, a SOAP envelope, a` |
|      - |  9884 | ` * sitemap. Undefined here, so the URI could be answered for a node already found` |
|      - |  9885 | ` * and never searched FOR.` |
|      - |  9886 | ` *` |
|      - |  9887 | ` * The list is live and the receiver is never in it, exactly as the name-only` |
|      - |  9888 | ` * one; DomGebtnMatch carries php's asymmetric wildcard rules.` |
|      - |  9889 | ` */` |
|     44 |  9890 | `DOM_METHOD(vm_builtin_Dom_getElementsByTagNameNS)` |
|      1 |  9891 | `{` |
|     45 |  9892 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     45 |  9893 | `	const char *zUri = DomArgStrOrNull(nArg,apArg,0);` |
|     45 |  9894 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  9895 | `	ph7_class_instance *pList;` |
|     45 |  9896 | `	if( pThis == 0 ){` |
|    ! 0 |  9897 | `		return PH7_OK;` |
|      - |  9898 | `	}` |
|     67 |  9899 | `	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTNNS,pThis,` |
|     22 |  9900 | `		zName,zUri ? zUri : "",0);` |
|     45 |  9901 | `	if( pList == 0 ){` |
|    ! 0 |  9902 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  9903 | `	}` |
|     45 |  9904 | `	PH7_NativeResultObject(pCtx,pList);` |
|     45 |  9905 | `	return PH7_OK;` |
|     23 |  9906 | `}` |
|      - |  9907 |  |
|      - |  9908 | `/*` |
|      - |  9909 | ` * php's get_debug_info for the DOM (ph7_class::xPresent), and the ONLY table any` |
|      - |  9910 | ` * presentation surface of a node class shows.` |
|      - |  9911 | ` *` |
|      - |  9912 | ` * Every property php declares on these classes is VIRTUAL -- a read_property` |
|      - |  9913 | ` * handler over libxml's own state -- so php's get_properties answers a node's REAL` |
|      - |  9914 | `` * table and nothing else: `(array)`, `get_object_vars()`, `foreach`,`` |
|      - |  9915 | ``  * `json_encode()`, `var_export()`, `serialize()` and `get_mangled_object_vars()` `` |
|      - |  9916 | ` * see an EMPTY document, and only a SUBCLASS's own properties ever appear there.` |
|      - |  9917 | ` * That half needs no hook at all (the slot walk already answers it), which is why` |
|      - |  9918 | ` * the non-debug call declines and lets the engine fall back to it.` |
|      - |  9919 | ` *` |
|      - |  9920 | ` * The DEBUG half is php's dom_get_debug_info_helper: the object's real properties` |
|      - |  9921 | ` * first (a subclass's own, under php's mangled keys), then the class's` |
|      - |  9922 | ` * property-handler table walked own-entries-first with the parent chain behind it.` |
|      - |  9923 | ` * Two rules come out of that helper and are reproduced here. An OBJECT value is` |
|      - |  9924 | `` * never recursed into -- php substitutes the literal `(object value omitted)`, so`` |
|      - |  9925 | `` * `print_r($doc)` does not print the whole tree through `documentElement`. And a`` |
|      - |  9926 | ` * handler that FAILS contributes no row at all, which in ext/dom is exactly one` |
|      - |  9927 | `` * case: `ownerDocument` is missing from a node libxml never gave a document (one`` |
|      - |  9928 | `` * built with `new`, and the stand-in a NOTATION is read through).`` |
|      - |  9929 | ` *` |
|      - |  9930 | ` * The values are read through the class's own recognizer rather than through` |
|      - |  9931 | ` * __get: php's walk is the C handler, so it runs no userland code and raises none` |
|      - |  9932 | `` * of the deprecations a php-level read of `actualEncoding`/`config` would.`` |
|      - |  9933 | ` */` |
|      - |  9934 | `#define DOM_NODE_DEBUG \` |
|      - |  9935 | `	"nodeName", "nodeValue", "nodeType", "parentNode", "parentElement", "childNodes", \` |
|      - |  9936 | `	"firstChild", "lastChild", "previousSibling", "nextSibling", "attributes", \` |
|      - |  9937 | `	"isConnected", "ownerDocument", "namespaceURI", "prefix", "localName", \` |
|      - |  9938 | `	"baseURI", "textContent"` |
|      - |  9939 | `#define DOM_CHARDATA_DEBUG \` |
|      - |  9940 | `	"data", "length", "previousElementSibling", "nextElementSibling"` |
|      - |  9941 | `static const char * const azDomNodeDebug[] = { DOM_NODE_DEBUG };` |
|      - |  9942 | `static const char * const azDomDocDebug[] = {` |
|      - |  9943 | `	"doctype", "implementation", "documentElement", "actualEncoding", "encoding",` |
|      - |  9944 | `	"xmlEncoding", "standalone", "xmlStandalone", "version", "xmlVersion",` |
|      - |  9945 | `	"strictErrorChecking", "documentURI", "config", "formatOutput", "validateOnParse",` |
|      - |  9946 | `	"resolveExternals", "preserveWhiteSpace", "recover", "substituteEntities",` |
|      - |  9947 | `	"firstElementChild", "lastElementChild", "childElementCount", DOM_NODE_DEBUG` |
|      - |  9948 | `};` |
|      - |  9949 | `static const char * const azDomElemDebug[] = {` |
|      - |  9950 | `	"tagName", "className", "id", "schemaTypeInfo", "firstElementChild",` |
|      - |  9951 | `	"lastElementChild", "childElementCount", "previousElementSibling",` |
|      - |  9952 | `	"nextElementSibling", DOM_NODE_DEBUG` |
|      - |  9953 | `};` |
|      - |  9954 | `static const char * const azDomAttrDebug[] = {` |
|      - |  9955 | `	"name", "specified", "value", "ownerElement", "schemaTypeInfo", DOM_NODE_DEBUG` |
|      - |  9956 | `};` |
|      - |  9957 | `static const char * const azDomCharDebug[] = { DOM_CHARDATA_DEBUG, DOM_NODE_DEBUG };` |
|      - |  9958 | `static const char * const azDomTextDebug[] = {` |
|      - |  9959 | `	"wholeText", DOM_CHARDATA_DEBUG, DOM_NODE_DEBUG` |
|      - |  9960 | `};` |
|      - |  9961 | `static const char * const azDomPiDebug[] = { "target", "data", DOM_NODE_DEBUG };` |
|      - |  9962 | `static const char * const azDomFragDebug[] = {` |
|      - |  9963 | `	"firstElementChild", "lastElementChild", "childElementCount", DOM_NODE_DEBUG` |
|      - |  9964 | `};` |
|      - |  9965 | `static const char * const azDomDocTypeDebug[] = {` |
|      - |  9966 | `	"name", "entities", "notations", "publicId", "systemId", "internalSubset",` |
|      - |  9967 | `	DOM_NODE_DEBUG` |
|      - |  9968 | `};` |
|      - |  9969 | `static const char * const azDomEntityDebug[] = {` |
|      - |  9970 | `	"publicId", "systemId", "notationName", "actualEncoding", "encoding", "version",` |
|      - |  9971 | `	DOM_NODE_DEBUG` |
|      - |  9972 | `};` |
|      - |  9973 | `static const char * const azDomNotationDebug[] = { "publicId", "systemId", DOM_NODE_DEBUG };` |
|      - |  9974 | `static const char * const azDomListDebug[] = { "length" };` |
|      - |  9975 | `static const char * const azDomNsNodeDebug[] = {` |
|      - |  9976 | `	"nodeName", "nodeValue", "nodeType", "prefix", "localName", "namespaceURI",` |
|      - |  9977 | `	"isConnected", "ownerDocument", "parentNode", "parentElement"` |
|      - |  9978 | `};` |
|      - |  9979 | `static const char * const azDomXPathDebug[] = { "document", "registerNodeNamespaces" };` |
|      - |  9980 | `/*` |
|      - |  9981 | ` * The DECLARATIONS behind those names. php declares every one of them on the class` |
|      - |  9982 | `` * -- Reflection lists them, `property_exists()` answers true, `isVirtual()` is true`` |
|      - |  9983 | `` * and `hasDefaultValue()` false -- and keeps no slot for any: PH7_MOD_VIRTUAL is`` |
|      - |  9984 | ` * that pair of facts. Each row states php's own declared type, and the rows are in` |
|      - |  9985 | ` * php's own declaration order, which is the order Reflection reports and (own class` |
|      - |  9986 | ` * first) the order the debug table above walks.` |
|      - |  9987 | ` */` |
|      - |  9988 | `#define DOM_VPROP(NAME,TYPE) \` |
|      - |  9989 | `	{ NAME, PH7_MOD_PUBLIC\|PH7_MOD_VIRTUAL, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, TYPE }` |
|      - |  9990 | `#define DOM_NODE_VPROPS \` |
|      - |  9991 | `	DOM_VPROP("nodeName","string"), \` |
|      - |  9992 | `	DOM_VPROP("nodeValue","?string"), \` |
|      - |  9993 | `	DOM_VPROP("nodeType","int"), \` |
|      - |  9994 | `	DOM_VPROP("parentNode","?DOMNode"), \` |
|      - |  9995 | `	DOM_VPROP("parentElement","?DOMElement"), \` |
|      - |  9996 | `	DOM_VPROP("childNodes","DOMNodeList"), \` |
|      - |  9997 | `	DOM_VPROP("firstChild","?DOMNode"), \` |
|      - |  9998 | `	DOM_VPROP("lastChild","?DOMNode"), \` |
|      - |  9999 | `	DOM_VPROP("previousSibling","?DOMNode"), \` |
|      - | 10000 | `	DOM_VPROP("nextSibling","?DOMNode"), \` |
|      - | 10001 | `	DOM_VPROP("attributes","?DOMNamedNodeMap"), \` |
|      - | 10002 | `	DOM_VPROP("isConnected","bool"), \` |
|      - | 10003 | `	DOM_VPROP("ownerDocument","?DOMDocument"), \` |
|      - | 10004 | `	DOM_VPROP("namespaceURI","?string"), \` |
|      - | 10005 | `	DOM_VPROP("prefix","string"), \` |
|      - | 10006 | `	DOM_VPROP("localName","?string"), \` |
|      - | 10007 | `	DOM_VPROP("baseURI","?string"), \` |
|      - | 10008 | `	DOM_VPROP("textContent","string")` |
|      - | 10009 | `/* php's DOMParentNode trio, declared on each of the three classes that carry it. */` |
|      - | 10010 | `#define DOM_PARENT_VPROPS \` |
|      - | 10011 | `	DOM_VPROP("firstElementChild","?DOMElement"), \` |
|      - | 10012 | `	DOM_VPROP("lastElementChild","?DOMElement"), \` |
|      - | 10013 | `	DOM_VPROP("childElementCount","int")` |
|      - | 10014 | `/* ...and its DOMChildNode pair. */` |
|      - | 10015 | `#define DOM_CHILD_VPROPS \` |
|      - | 10016 | `	DOM_VPROP("previousElementSibling","?DOMElement"), \` |
|      - | 10017 | `	DOM_VPROP("nextElementSibling","?DOMElement")` |
|      - | 10018 | `/*` |
|      - | 10019 | ` * One row per class that HAS a property-handler table -- php's own` |
|      - | 10020 | `` * `dom_xxx_prop_handlers`, which is what its read_property / has_property /`` |
|      - | 10021 | ` * write_property consult before anything else about the object.` |
|      - | 10022 | ` *` |
|      - | 10023 | ` * xRead is the READ handler and xWrite the write one; xDebug is xRead with php's` |
|      - | 10024 | ` * two #[\Deprecated] notices held back, because get_debug_info reads the C` |
|      - | 10025 | ` * function behind the declaration and not the declaration. bNode says the` |
|      - | 10026 | `` * receiver's $__res is an xmlNode, which is what the `ownerDocument` rule may be`` |
|      - | 10027 | ` * asked about -- a namespace declaration carries an xmlNs instead and has no such` |
|      - | 10028 | ` * field to read.` |
|      - | 10029 | ` */` |
|      - | 10030 | `typedef struct DomPropSpec DomPropSpec;` |
|      - | 10031 | `struct DomPropSpec {` |
|      - | 10032 | `	const char *zClass;` |
|      - | 10033 | `	int (*xRead)(ph7_context *,const char *);` |
|      - | 10034 | `	int (*xWrite)(ph7_context *,const char *,ph7_value *,int *);` |
|      - | 10035 | `	int (*xDebug)(ph7_context *,const char *);` |
|      - | 10036 | `	const char * const *azName;` |
|      - | 10037 | `	sxu32 nName;` |
|      - | 10038 | `	int bNode;` |
|      - | 10039 | `};` |
|      - | 10040 | `static const DomPropSpec aDomProp[] = {` |
|      - | 10041 | `	{ "DOMDocument", DomDocProp, DomSetDocProp, DomDocPropQuiet,` |
|      - | 10042 | `	  azDomDocDebug, SX_ARRAYSIZE(azDomDocDebug), 1 },` |
|      - | 10043 | `	{ "DOMElement", DomElemProp, DomSetElemProp, DomElemProp,` |
|      - | 10044 | `	  azDomElemDebug, SX_ARRAYSIZE(azDomElemDebug), 1 },` |
|      - | 10045 | `	{ "DOMAttr", DomAttrProp, DomSetAttrProp, DomAttrProp,` |
|      - | 10046 | `	  azDomAttrDebug, SX_ARRAYSIZE(azDomAttrDebug), 1 },` |
|      - | 10047 | `	{ "DOMText", DomTextProp, DomSetCharProp, DomTextProp,` |
|      - | 10048 | `	  azDomTextDebug, SX_ARRAYSIZE(azDomTextDebug), 1 },` |
|      - | 10049 | `	{ "DOMCharacterData", DomCharProp, DomSetCharProp, DomCharProp,` |
|      - | 10050 | `	  azDomCharDebug, SX_ARRAYSIZE(azDomCharDebug), 1 },` |
|      - | 10051 | `	{ "DOMProcessingInstruction", DomPiProp, DomSetPiProp, DomPiProp,` |
|      - | 10052 | `	  azDomPiDebug, SX_ARRAYSIZE(azDomPiDebug), 1 },` |
|      - | 10053 | `	/* The fragment writes what DOMNode writes; only its READ set is wider. */` |
|      - | 10054 | `	{ "DOMDocumentFragment", DomFragProp, DomSetNodeProp, DomFragProp,` |
|      - | 10055 | `	  azDomFragDebug, SX_ARRAYSIZE(azDomFragDebug), 1 },` |
|      - | 10056 | `	/* The DTD half's OWN properties are all read-only, so its writer states none` |
|      - | 10057 | `	 * of them and each lands on the hook's readonly Error. DOMNode's three still` |
|      - | 10058 | ``	 * write here -- `nodeValue` and `textContent` are accepted and ignored on a`` |
|      - | 10059 | `	 * doctype, which is not the same answer as refusing them. */` |
|      - | 10060 | `	{ "DOMDocumentType", DomDocTypeProp, DomSetNodeProp, DomDocTypeProp,` |
|      - | 10061 | `	  azDomDocTypeDebug, SX_ARRAYSIZE(azDomDocTypeDebug), 1 },` |
|      - | 10062 | `	{ "DOMEntity", DomEntityProp, DomSetEntityProp, DomEntityPropQuiet,` |
|      - | 10063 | `	  azDomEntityDebug, SX_ARRAYSIZE(azDomEntityDebug), 1 },` |
|      - | 10064 | `	{ "DOMNotation", DomNotationProp, DomSetNotationProp, DomNotationProp,` |
|      - | 10065 | `	  azDomNotationDebug, SX_ARRAYSIZE(azDomNotationDebug), 1 },` |
|      - | 10066 | `	{ "DOMNodeList", DomListProp, DomSetNothing, DomListProp,` |
|      - | 10067 | `	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },` |
|      - | 10068 | `	{ "DOMNamedNodeMap", DomMapProp, DomSetNothing, DomMapProp,` |
|      - | 10069 | `	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },` |
|      - | 10070 | `	{ "DOMNameSpaceNode", DomNsNodeProp, DomSetNothing, DomNsNodeProp,` |
|      - | 10071 | `	  azDomNsNodeDebug, SX_ARRAYSIZE(azDomNsNodeDebug), 0 },` |
|      - | 10072 | `	{ "DOMXPath", DomXPathProp, DomSetXPathProp, DomXPathProp,` |
|      - | 10073 | `	  azDomXPathDebug, SX_ARRAYSIZE(azDomXPathDebug), 0 },` |
|      - | 10074 | `	/* DOMComment, DOMCdataSection and DOMEntityReference name no row of their own:` |
|      - | 10075 | `	 * they declare no property php's table does not already carry, so the base-chain` |
|      - | 10076 | `	 * walk below reaches their parent's -- which is php's answer for them too. */` |
|      - | 10077 | `	{ "DOMNode", DomNodeProp, DomSetNodeProp, DomNodeProp,` |
|      - | 10078 | `	  azDomNodeDebug, SX_ARRAYSIZE(azDomNodeDebug), 1 }` |
|      - | 10079 | `};` |
|      - | 10080 | `/* The nearest ancestor with a handler table -- php's own lookup, which is why a` |
|      - | 10081 | ` * userland subclass of DOMElement shows DOMElement's twenty-seven. */` |
|   8941 | 10082 | `static const DomPropSpec * DomPropSpecOf(ph7_class *pClass)` |
|      5 | 10083 | `{` |
|   9244 | 10084 | `	for( ; pClass ; pClass = pClass->pBase ){` |
|   9244 | 10085 | `		sxu32 nName = SyStringLength(&pClass->sName);` |
|      - | 10086 | `		sxu32 i;` |
|  32030 | 10087 | `		for( i = 0 ; i < SX_ARRAYSIZE(aDomProp) ; ++i ){` |
|  31727 | 10088 | `			if( SyStrlen(aDomProp[i].zClass) == nName` |
|  21222 | 10089 | `			 && SyStrncmp(SyStringData(&pClass->sName),aDomProp[i].zClass,nName) == 0 ){` |
|   8946 | 10090 | `				return &aDomProp[i];` |
|      - | 10091 | `			}` |
|  11398 | 10092 | `		}` |
|    150 | 10093 | `	}` |
|    ! 0 | 10094 | `	return 0;` |
|   4476 | 10095 | `}` |
|      - | 10096 | `/*` |
|      - | 10097 | ` * php's read_property / has_property / write_property for every DOM class, as` |
|      - | 10098 | ` * ph7_class::xProp.` |
|      - | 10099 | ` *` |
|      - | 10100 | `` * ext/dom has no `__get`/`__set`/`__isset` anywhere: each class carries a table`` |
|      - | 10101 | ` * of property handlers and php's object handlers consult it FIRST, so a name the` |
|      - | 10102 | ` * table holds is answered by the handler and only a name it does NOT hold falls` |
|      - | 10103 | ` * through to the standard path -- which is where a subclass's own magic accessor` |
|      - | 10104 | ` * finally gets a say. Routing the surface through the magic trio had the order` |
|      - | 10105 | `` * exactly backwards: a subclass that wrote `__get` without delegating replaced`` |
|      - | 10106 | ` * the whole DOM surface for its instances, and every DOM class carried three` |
|      - | 10107 | ` * methods php does not.` |
|      - | 10108 | ` *` |
|      - | 10109 | ` * The recognizers below ARE the table: one per class, answering 1 for a name it` |
|      - | 10110 | ` * knows. A name none of them knows leaves bAnswered at 0, and the member opcode` |
|      - | 10111 | ` * takes the ordinary path from there -- php's own fall-through, undefined-property` |
|      - | 10112 | ` * warning and all.` |
|      - | 10113 | ` *` |
|      - | 10114 | ` * UNSET is not answered here. php's unset_property for a virtual property has` |
|      - | 10115 | `` * nothing to remove and refuses with `Cannot unset C::$p`, which the opcode`` |
|      - | 10116 | ` * already words off the declaration itself; declining leaves that answer, and` |
|      - | 10117 | ` * leaves a name the class does NOT declare on the __unset path php sends it to.` |
|      - | 10118 | ` *` |
|      - | 10119 | ` * Neither is WRITE, the member opcode's question -- it is asked before the value` |
|      - | 10120 | ` * exists, and a handler that really STORES needs it. The opcode recognizes the` |
|      - | 10121 | ` * name as this table's (PH7_ClassNativePropOwns) and routes the write to STORE` |
|      - | 10122 | ` * below, at the point php's write_property gets its zval.` |
|      - | 10123 | ` */` |
|   8933 | 10124 | `static void DomPropHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)` |
|      5 | 10125 | `{` |
|   8938 | 10126 | `	const DomPropSpec *pSpec = pThis ? DomPropSpecOf(pThis->pClass) : 0;` |
|   8938 | 10127 | `	sxu32 nName = SyStringLength(pCtx->pName);` |
|      - | 10128 | `	ph7_context sCtx;` |
|      - | 10129 | `	ph7_value sVal;` |
|      - | 10130 | `	char zName[128];` |
|      - | 10131 | `	int bKnown;` |
|   8933 | 10132 | `	if( pSpec == 0` |
|   8938 | 10133 | `	 \|\| pCtx->iMode == PH7_NATIVE_PROP_UNSET \|\| pCtx->iMode == PH7_NATIVE_PROP_WRITE ){` |
|   1807 | 10134 | `		return;` |
|      - | 10135 | `	}` |
|   8222 | 10136 | `	if( nName >= sizeof(zName) ){` |
|      - | 10137 | `		/* Longer than any name ext/dom declares: the ordinary path owns it. */` |
|    ! 0 | 10138 | `		return;` |
|      - | 10139 | `	}` |
|   8222 | 10140 | `	SyMemcpy(SyStringData(pCtx->pName),zName,nName);` |
|   8222 | 10141 | `	zName[nName] = 0;` |
|   8222 | 10142 | `	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){` |
|      - | 10143 | `		/* php's table membership, which for ext/dom is the DECLARATION: every one` |
|      - | 10144 | `		 * of its ninety names is declared virtual on the class, and nothing else` |
|      - | 10145 | `		 * is the handler's. Answered without running a recognizer -- a write shape` |
|      - | 10146 | `		 * asks this before the value exists, and the deprecated names would raise` |
|      - | 10147 | `		 * their notice on a read the program has not made. */` |
|   1434 | 10148 | `		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,zName,nName);` |
|   1434 | 10149 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|   1426 | 10150 | `			pCtx->bAnswered = 1;` |
|    712 | 10151 | `		}` |
|   1434 | 10152 | `		return;` |
|      - | 10153 | `	}` |
|      - | 10154 | `	/* The recognizers answer by WRITING a result, so the scratch context carries` |
|      - | 10155 | `	 * one slot of its own -- and the refusal channel, which is what stops a` |
|      - | 10156 | `	 * readonly Error or a DOMException from running the enclosing catch in the` |
|      - | 10157 | `	 * middle of this access. */` |
|   6790 | 10158 | `	PH7_MemObjInit(pVm,&sVal);` |
|   6790 | 10159 | `	VmInitCallContext(&sCtx,pVm,0,&sVal,0);` |
|   6790 | 10160 | `	sCtx.pThis = pThis;` |
|   6790 | 10161 | `	sCtx.pCalledClass = pThis->pClass;` |
|   6790 | 10162 | `	sCtx.pPropCtx = pCtx;` |
|   6790 | 10163 | `	if( pCtx->iMode == PH7_NATIVE_PROP_STORE ){` |
|      - | 10164 | `		/* php's write_property, with the value. A name the WRITER does not state` |
|      - | 10165 | `		 * is read-only when the class declares it at all -- and the refusal is` |
|      - | 10166 | `		 * php's own, raised where the store would have landed -- and nothing this` |
|      - | 10167 | `		 * class knows otherwise, which puts the write back on the ordinary path. */` |
|    712 | 10168 | `		int rcW = PH7_OK;` |
|    712 | 10169 | `		if( pSpec->xWrite(&sCtx,zName,pCtx->pResult,&rcW) == DOM_SET_DONE ){` |
|    636 | 10170 | `			pCtx->bAnswered = 1;` |
|    636 | 10171 | `			if( pCtx->zThrowClass == 0 && DomNodeLess(&sCtx,zName) ){` |
|      - | 10172 | `				/* A writable name on an object with no node behind it: php's` |
|      - | 10173 | `				 * handler reaches its DOM_GET_OBJ and answers Invalid State --` |
|      - | 10174 | `				 * AFTER the declared type has had its say, which is why a` |
|      - | 10175 | `				 * TypeError already recorded stands. */` |
|      3 | 10176 | `				DomPropRefuse(&sCtx,"DOMException",DOM_ERR_INVALID_STATE,` |
|      1 | 10177 | `					DomErrText(DOM_ERR_INVALID_STATE));` |
|      3 | 10178 | `			}` |
|    394 | 10179 | `		}else if( pSpec->xRead(&sCtx,zName) ){` |
|     77 | 10180 | `			pCtx->bAnswered = 1;` |
|     77 | 10181 | `			DomRefuseWrite(&sCtx,zName);` |
|     38 | 10182 | `		}` |
|    712 | 10183 | `		VmReleaseCallContext(&sCtx);` |
|    712 | 10184 | `		PH7_MemObjRelease(&sVal);` |
|    712 | 10185 | `		return;` |
|      - | 10186 | `	}` |
|   6080 | 10187 | `	if( DomNodeLess(&sCtx,zName) ){` |
|      - | 10188 | `		/* A name the class DECLARES, on an object libxml gave no node -- every` |
|      - | 10189 | `		 * handler would fetch a null pointer, and php answers Invalid State on a` |
|      - | 10190 | `		 * read and on an isset() alike. */` |
|     37 | 10191 | `		DomPropRefuse(&sCtx,"DOMException",DOM_ERR_INVALID_STATE,` |
|     18 | 10192 | `			DomErrText(DOM_ERR_INVALID_STATE));` |
|     37 | 10193 | `		VmReleaseCallContext(&sCtx);` |
|     37 | 10194 | `		PH7_MemObjRelease(&sVal);` |
|     37 | 10195 | `		return;` |
|      - | 10196 | `	}` |
|   6044 | 10197 | `	bKnown = pSpec->xRead(&sCtx,zName) != 0;` |
|   6044 | 10198 | `	if( bKnown && pCtx->zThrowClass == 0 ){` |
|   5934 | 10199 | `		pCtx->bAnswered = 1;` |
|   5934 | 10200 | `		if( pCtx->iMode == PH7_NATIVE_PROP_READ ){` |
|   5854 | 10201 | `			PH7_MemObjStore(&sVal,pCtx->pResult);` |
|   2930 | 10202 | `		}else{` |
|      - | 10203 | `			/* php's has_property fetches the value and judges it: by NULL-ness for` |
|      - | 10204 | ``			 * isset() (`isset($n->nextSibling)` is false on a last child while`` |
|      - | 10205 | ``			 * `isset($n->nodeName)` is true) and by TRUTH for the two check_empty`` |
|      - | 10206 | `			 * questions -- ext/dom's handler makes no distinction between them. */` |
|    121 | 10207 | `			int bSet = pCtx->iMode == PH7_NATIVE_PROP_ISSET` |
|     76 | 10208 | `				? (sVal.iFlags & MEMOBJ_NULL) == 0` |
|     42 | 10209 | `				: ph7_value_to_bool(&sVal);` |
|     81 | 10210 | `			PH7_MemObjRelease(pCtx->pResult);` |
|     81 | 10211 | `			ph7_value_bool(pCtx->pResult,bSet);` |
|      - | 10212 | `		}` |
|   2965 | 10213 | `	}` |
|   6044 | 10214 | `	VmReleaseCallContext(&sCtx);` |
|   6044 | 10215 | `	PH7_MemObjRelease(&sVal);` |
|   4472 | 10216 | `}` |
|      - | 10217 | ``/* php's `dom_node_owner_document_read` answers FAILURE -- and the debug walk then`` |
|      - | 10218 | `` * writes no row -- for a node libxml gave no document: one built with `new`, and`` |
|      - | 10219 | ` * the entity-shaped stand-in a NOTATION declaration is read through. A DOCUMENT` |
|      - | 10220 | ` * itself answers null and keeps its row. */` |
|    244 | 10221 | `static int DomDebugSkip(ph7_class_instance *pThis,const char *zName,int bNode)` |
|      1 | 10222 | `{` |
|      - | 10223 | `	phl_domnode *pNd;` |
|      - | 10224 | `	xmlNodePtr pNode;` |
|    245 | 10225 | `	if( !bNode \|\| !DomNameIs(zName,"ownerDocument") ){` |
|    239 | 10226 | `		return 0;` |
|      - | 10227 | `	}` |
|      7 | 10228 | `	pNd = DomResOf(pThis);` |
|      7 | 10229 | `	pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      6 | 10230 | `	if( pNode == 0` |
|      7 | 10231 | `	 \|\| pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|      7 | 10232 | `		return 0;` |
|      - | 10233 | `	}` |
|    ! 0 | 10234 | `	return pNode->doc == 0;` |
|    123 | 10235 | `}` |
|     24 | 10236 | `static sxi32 DomPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)` |
|      1 | 10237 | `{` |
|      - | 10238 | `	const DomPropSpec *pSpec;` |
|      - | 10239 | `	ph7_context sCtx;` |
|      - | 10240 | `	ph7_value sVal,sKey;` |
|      - | 10241 | `	sxu32 i;` |
|     25 | 10242 | `	if( !bDebug ){` |
|      - | 10243 | `		/* php's get_properties for a DOM object is the object's own table -- which` |
|      - | 10244 | `		 * for anything but a subclass is empty -- so the engine's slot walk IS the` |
|      - | 10245 | `		 * answer and this hook has nothing to add. */` |
|     17 | 10246 | `		return SXERR_NOTFOUND;` |
|      - | 10247 | `	}` |
|      9 | 10248 | `	pSpec = pThis ? DomPropSpecOf(pThis->pClass) : 0;` |
|      9 | 10249 | `	if( pSpec == 0 \|\| (pOut->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 10250 | `		return SXERR_NOTFOUND;` |
|      - | 10251 | `	}` |
|      9 | 10252 | `	if( pSpec->bNode && PH7_NativeAttr(pThis,DOM_RES) != 0 ){` |
|      7 | 10253 | `		phl_domnode *pNd = DomResOf(pThis);` |
|      7 | 10254 | `		if( pNd == 0 \|\| pNd->pNode == 0 ){` |
|      - | 10255 | ``			/* An object with no node behind it (one built with `new`): every handler`` |
|      - | 10256 | `			 * would refuse, so the walk contributes nothing and the object shows its` |
|      - | 10257 | `			 * real table alone. php's dump THROWS out of the debug handler here` |
|      - | 10258 | `			 * instead, after printing the header -- see ECOSYSTEM.md F42. */` |
|    ! 0 | 10259 | `			return SXERR_NOTFOUND;` |
|      - | 10260 | `		}` |
|      3 | 10261 | `	}` |
|      - | 10262 | `	/* A subclass's own properties come FIRST and under php's mangled keys, which` |
|      - | 10263 | ``	 * is what prints `[p:MyDoc:private]` beside the fabricated rows. */`` |
|      9 | 10264 | `	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pOut->x.pOther);` |
|      9 | 10265 | `	PH7_MemObjInit(pVm,&sVal);` |
|      - | 10266 | `	/* One scratch call context for the whole walk: the recognizers answer by` |
|      - | 10267 | `	 * WRITING a result, and rule 54 says a second call on the same context would` |
|      - | 10268 | `	 * append to the first answer -- so the slot is blanked between rows. */` |
|      9 | 10269 | `	VmInitCallContext(&sCtx,pVm,0,&sVal,0);` |
|      9 | 10270 | `	sCtx.pThis = pThis;` |
|      9 | 10271 | `	sCtx.pCalledClass = pThis->pClass;` |
|    253 | 10272 | `	for( i = 0 ; i < pSpec->nName ; ++i ){` |
|    245 | 10273 | `		const char *zName = pSpec->azName[i];` |
|    245 | 10274 | `		if( DomDebugSkip(pThis,zName,pSpec->bNode) ){` |
|    ! 0 | 10275 | `			continue;` |
|      - | 10276 | `		}` |
|    245 | 10277 | `		PH7_MemObjRelease(&sVal);` |
|    245 | 10278 | `		PH7_MemObjInit(pVm,&sVal);` |
|    245 | 10279 | `		if( pSpec->xDebug(&sCtx,zName) == 0 ){` |
|    ! 0 | 10280 | `			continue;` |
|      - | 10281 | `		}` |
|    245 | 10282 | `		if( sVal.iFlags & MEMOBJ_OBJ ){` |
|      - | 10283 | `			/* php prints the literal rather than the object: a document would` |
|      - | 10284 | ``			 * otherwise dump its whole tree through `documentElement`. */`` |
|      - | 10285 | `			ph7_value sOmit;` |
|     15 | 10286 | `			PH7_MemObjInitFromString(pVm,&sOmit,0);` |
|     15 | 10287 | `			PH7_MemObjStringAppend(&sOmit,"(object value omitted)",` |
|      - | 10288 | `				sizeof("(object value omitted)")-1);` |
|     15 | 10289 | `			PH7_MemObjInitFromString(pVm,&sKey,0);` |
|     15 | 10290 | `			PH7_MemObjStringAppend(&sKey,zName,(sxu32)SyStrlen(zName));` |
|     15 | 10291 | `			ph7_array_add_elem(pOut,&sKey,&sOmit);` |
|     15 | 10292 | `			PH7_MemObjRelease(&sKey);` |
|     15 | 10293 | `			PH7_MemObjRelease(&sOmit);` |
|     15 | 10294 | `			continue;` |
|      - | 10295 | `		}` |
|    231 | 10296 | `		PH7_MemObjInitFromString(pVm,&sKey,0);` |
|    231 | 10297 | `		PH7_MemObjStringAppend(&sKey,zName,(sxu32)SyStrlen(zName));` |
|    231 | 10298 | `		ph7_array_add_elem(pOut,&sKey,&sVal);` |
|    231 | 10299 | `		PH7_MemObjRelease(&sKey);` |
|    116 | 10300 | `	}` |
|      9 | 10301 | `	VmReleaseCallContext(&sCtx);` |
|      9 | 10302 | `	PH7_MemObjRelease(&sVal);` |
|      9 | 10303 | `	return SXRET_OK;` |
|     13 | 10304 | `}` |
|      - | 10305 | `/*` |
|      - | 10306 | ` * Install the DOM library: every class declared from C, no embedded chunk and` |
|      - | 10307 | ` * no globally visible thunk left.  Called from PH7_VmInit inside the` |
|      - | 10308 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml (the capture plumbing must` |
|      - | 10309 | ` * exist) and after the Reflection install (DOMException needs Exception).` |
|      - | 10310 | ` */` |
|   6721 | 10311 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)` |
|      5 | 10312 | `{` |
|      - | 10313 | `	/* php's eighteen DOMNode properties -- all VIRTUAL there, so the object holds` |
|      - | 10314 | `	 * none of them and every one is answered by DomNodeProp -- followed by the two` |
|      - | 10315 | `	 * slots every wrapper really carries and the identity cache, which are PHL's` |
|      - | 10316 | `	 * own storage and hidden from every surface. */` |
|      - | 10317 | `	static const PH7_NativePropDef aNodeProp[] = {` |
|      - | 10318 | `		DOM_NODE_VPROPS,` |
|      - | 10319 | `		{ DOM_RES, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10320 | `		{ DOM_DOC, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10321 | `		/* The identity cache. Only a DOCUMENT'S is a document's; a CONSTRUCTED` |
|      - | 10322 | `		 * ownerless node is its own holder (its $__doc points at itself) and` |
|      - | 10323 | `		 * caches its tree's wrappers HERE until an insertion adopts them into` |
|      - | 10324 | `		 * a real document's cache. Empty and unread on every owned node. */` |
|      - | 10325 | `		{ DOM_NODES, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10326 | `	};` |
|      - | 10327 | ``	/* php's own signatures. Declaring `DOMNode $node` is what makes`` |
|      - | 10328 | ``	 * `$n->appendChild(1)` the TypeError php raises instead of a warning from`` |
|      - | 10329 | `	 * reading ->__res off an int. */` |
|      - | 10330 | `	static const PH7_NativeMethodDef aNodeMethod[] = {` |
|      - | 10331 | `		{ "appendChild",    PH7_MOD_PUBLIC, "DOMNode $node", "", vm_builtin_DOMNode_appendChild },` |
|      - | 10332 | `		{ "insertBefore",   PH7_MOD_PUBLIC, "DOMNode $node, ?DOMNode $child = null", "",` |
|      - | 10333 | `		  vm_builtin_DOMNode_insertBefore },` |
|      - | 10334 | `		{ "removeChild",    PH7_MOD_PUBLIC, "DOMNode $child", "", vm_builtin_DOMNode_removeChild },` |
|      - | 10335 | `		{ "replaceChild",   PH7_MOD_PUBLIC, "DOMNode $node, DOMNode $child", "",` |
|      - | 10336 | `		  vm_builtin_DOMNode_replaceChild },` |
|      - | 10337 | `		{ "hasChildNodes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasChildNodes },` |
|      - | 10338 | `		{ "hasAttributes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasAttributes },` |
|      - | 10339 | `		{ "isSameNode",     PH7_MOD_PUBLIC, "DOMNode $otherNode", "@bool", vm_builtin_DOMNode_isSameNode },` |
|      - | 10340 | `		/* php declares no return type at all on this one, not even a tentative` |
|      - | 10341 | `		 * one, so the row states none either. */` |
|      - | 10342 | `		{ "cloneNode",      PH7_MOD_PUBLIC, "bool $deep = false", "", vm_builtin_DOMNode_cloneNode },` |
|      - | 10343 | `		/* php runs the same walk normalizeDocument() does, from the receiver. */` |
|      - | 10344 | `		{ "normalize",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },` |
|      - | 10345 | `		{ "getNodePath",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_DOMNode_getNodePath },` |
|      - | 10346 | `		{ "isEqualNode",    PH7_MOD_PUBLIC, "?DOMNode $otherNode", "bool",` |
|      - | 10347 | `		  vm_builtin_DOMNode_isEqualNode },` |
|      - | 10348 | `		{ "isSupported",    PH7_MOD_PUBLIC, "string $feature, string $version", "@bool",` |
|      - | 10349 | `		  vm_builtin_DOMNode_isSupported },` |
|      - | 10350 | `		/* php's declared type names DOMNameSpaceNode, a class PHL does not have;` |
|      - | 10351 | `		 * the row states it anyway so Reflection reports php's, and nothing can` |
|      - | 10352 | `		 * be handed one. (php's own zpp rejects a NON-object here with a` |
|      - | 10353 | `		 * "?object" message instead -- PLAN §7.4, the error-format class.) */` |
|      - | 10354 | `		{ "contains",       PH7_MOD_PUBLIC, "DOMNode\|DOMNameSpaceNode\|null $other", "bool",` |
|      - | 10355 | `		  vm_builtin_DOMNode_contains },` |
|      - | 10356 | `		{ "getRootNode",    PH7_MOD_PUBLIC, "?array $options = null", "DOMNode",` |
|      - | 10357 | `		  vm_builtin_DOMNode_getRootNode },` |
|      - | 10358 | `		{ "compareDocumentPosition", PH7_MOD_PUBLIC, "DOMNode $other", "int",` |
|      - | 10359 | `		  vm_builtin_DOMNode_compareDocumentPosition },` |
|      - | 10360 | `		{ "getLineNo",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNode_getLineNo },` |
|      - | 10361 | `		{ "C14N",           PH7_MOD_PUBLIC,` |
|      - | 10362 | `		  "bool $exclusive = false, bool $withComments = false, ?array $xpath = null, "` |
|      - | 10363 | `		  "?array $nsPrefixes = null", "@string\|false", vm_builtin_DOMNode_C14N },` |
|      - | 10364 | `		{ "C14NFile",       PH7_MOD_PUBLIC,` |
|      - | 10365 | `		  "string $uri, bool $exclusive = false, bool $withComments = false, "` |
|      - | 10366 | `		  "?array $xpath = null, ?array $nsPrefixes = null", "@int\|false",` |
|      - | 10367 | `		  vm_builtin_DOMNode_C14NFile },` |
|      - | 10368 | `		/* The refusal MACHINERY, not decoration: serialize() finds __sleep and` |
|      - | 10369 | `		 * unserialize() calls __wakeup, so a subclass declaring either escapes. */` |
|      - | 10370 | `		{ "__sleep",        PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },` |
|      - | 10371 | `		{ "__wakeup",       PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },` |
|      - | 10372 | `		{ "lookupNamespaceURI", PH7_MOD_PUBLIC, "?string $prefix", "@?string",` |
|      - | 10373 | `		  vm_builtin_DOMNode_lookupNamespaceURI },` |
|      - | 10374 | `		{ "lookupPrefix",   PH7_MOD_PUBLIC, "string $namespace", "@?string",` |
|      - | 10375 | `		  vm_builtin_DOMNode_lookupPrefix },` |
|      - | 10376 | `		{ "isDefaultNamespace", PH7_MOD_PUBLIC, "string $namespace", "@bool",` |
|      - | 10377 | `		  vm_builtin_DOMNode_isDefaultNamespace },` |
|      - | 10378 | `	};` |
|      - | 10379 | `	/* php declares the six on DOMNode; every node class inherits them. */` |
|      - | 10380 | `	static const PH7_NativeConstDef aNodeConst[] = {` |
|      - | 10381 | `		{ "DOCUMENT_POSITION_DISCONNECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10382 | `		  DOM_POS_DISCONNECTED, 0, 0.0 },` |
|      - | 10383 | `		{ "DOCUMENT_POSITION_PRECEDING", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10384 | `		  DOM_POS_PRECEDING, 0, 0.0 },` |
|      - | 10385 | `		{ "DOCUMENT_POSITION_FOLLOWING", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10386 | `		  DOM_POS_FOLLOWING, 0, 0.0 },` |
|      - | 10387 | `		{ "DOCUMENT_POSITION_CONTAINS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10388 | `		  DOM_POS_CONTAINS, 0, 0.0 },` |
|      - | 10389 | `		{ "DOCUMENT_POSITION_CONTAINED_BY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10390 | `		  DOM_POS_CONTAINED_BY, 0, 0.0 },` |
|      - | 10391 | `		{ "DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,` |
|      - | 10392 | `		  DOM_POS_IMPL_SPEC, 0, 0.0 },` |
|      - | 10393 | `	};` |
|      - | 10394 | `	/* php's twenty-two, in its own declaration order. Nine are the XML declaration` |
|      - | 10395 | `	 * and the document's URI read straight off libxml, three are the DOMParentNode` |
|      - | 10396 | `	 * trio, and seven are the directives DOM_DFLAGS holds -- none of them a slot,` |
|      - | 10397 | ``	 * which is what makes php's `(array)$doc` an EMPTY array. */`` |
|      - | 10398 | `	static const PH7_NativePropDef aDocProp[] = {` |
|      - | 10399 | `		DOM_VPROP("doctype","?DOMDocumentType"),` |
|      - | 10400 | `		DOM_VPROP("implementation","DOMImplementation"),` |
|      - | 10401 | `		DOM_VPROP("documentElement","?DOMElement"),` |
|      - | 10402 | `		DOM_VPROP("actualEncoding","?string"),` |
|      - | 10403 | `		DOM_VPROP("encoding","?string"),` |
|      - | 10404 | `		DOM_VPROP("xmlEncoding","?string"),` |
|      - | 10405 | `		DOM_VPROP("standalone","bool"),` |
|      - | 10406 | `		DOM_VPROP("xmlStandalone","bool"),` |
|      - | 10407 | `		DOM_VPROP("version","?string"),` |
|      - | 10408 | `		DOM_VPROP("xmlVersion","?string"),` |
|      - | 10409 | `		DOM_VPROP("strictErrorChecking","bool"),` |
|      - | 10410 | `		DOM_VPROP("documentURI","?string"),` |
|      - | 10411 | `		DOM_VPROP("config","mixed"),` |
|      - | 10412 | `		DOM_VPROP("formatOutput","bool"),` |
|      - | 10413 | `		DOM_VPROP("validateOnParse","bool"),` |
|      - | 10414 | `		DOM_VPROP("resolveExternals","bool"),` |
|      - | 10415 | `		DOM_VPROP("preserveWhiteSpace","bool"),` |
|      - | 10416 | `		DOM_VPROP("recover","bool"),` |
|      - | 10417 | `		DOM_VPROP("substituteEntities","bool"),` |
|      - | 10418 | `		DOM_PARENT_VPROPS,` |
|      - | 10419 | `		/* The seven directives, as the one word that holds them. */` |
|      - | 10420 | `		{ DOM_DFLAGS,           PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN,` |
|      - | 10421 | `		  { 0, 0, PH7_NATIVE_VAL_INT, DOM_F_DEFAULT, 0, 0.0 }, 0 },` |
|      - | 10422 | `		/* The identity cache DomWrap keys by node pointer. */` |
|      - | 10423 | `		{ DOM_NODES,            PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10424 | `		/* ...and the base-class => user-class table registerNodeClass writes. */` |
|      - | 10425 | `		{ DOM_NCLS,             PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10426 | `	};` |
|      - | 10427 | `	static const PH7_NativeMethodDef aDocMethod[] = {` |
|      - | 10428 | `		{ "__construct",          PH7_MOD_PUBLIC, "string $version = '1.0', string $encoding = ''", "",` |
|      - | 10429 | `		  vm_builtin_DOMDocument_construct },` |
|      - | 10430 | `		{ "loadXML",              PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",` |
|      - | 10431 | `		  vm_builtin_DOMDocument_loadXML },` |
|      - | 10432 | `		{ "load",                 PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@bool",` |
|      - | 10433 | `		  vm_builtin_DOMDocument_load },` |
|      - | 10434 | `		{ "save",                 PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@int\|false",` |
|      - | 10435 | `		  vm_builtin_DOMDocument_save },` |
|      - | 10436 | `		{ "loadHTML",             PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",` |
|      - | 10437 | `		  vm_builtin_DOMDocument_loadHTML },` |
|      - | 10438 | `		{ "loadHTMLFile",         PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@bool",` |
|      - | 10439 | `		  vm_builtin_DOMDocument_loadHTMLFile },` |
|      - | 10440 | `		{ "saveHTML",             PH7_MOD_PUBLIC, "?DOMNode $node = null", "@string\|false",` |
|      - | 10441 | `		  vm_builtin_DOMDocument_saveHTML },` |
|      - | 10442 | `		{ "saveHTMLFile",         PH7_MOD_PUBLIC, "string $filename", "@int\|false",` |
|      - | 10443 | `		  vm_builtin_DOMDocument_saveHTMLFile },` |
|      - | 10444 | `		{ "saveXML",              PH7_MOD_PUBLIC, "?DOMNode $node = null, int $options = 0", "@string\|false",` |
|      - | 10445 | `		  vm_builtin_DOMDocument_saveXML },` |
|      - | 10446 | `		{ "createElement",        PH7_MOD_PUBLIC, "string $localName, string $value = ''", "",` |
|      - | 10447 | `		  vm_builtin_DOMDocument_createElement },` |
|      - | 10448 | `		/* php declares no return type at all on this one either -- its answer is` |
|      - | 10449 | ``		 * `DOMElement\|false` and it never wrote that down. */`` |
|      - | 10450 | `		{ "createElementNS",      PH7_MOD_PUBLIC,` |
|      - | 10451 | `		  "?string $namespace, string $qualifiedName, string $value = ''", "",` |
|      - | 10452 | `		  vm_builtin_DOMDocument_createElementNS },` |
|      - | 10453 | ``		/* php declares no return type on this one either: `DOMNode\|false`. */`` |
|      - | 10454 | `		{ "importNode",           PH7_MOD_PUBLIC, "DOMNode $node, bool $deep = false", "",` |
|      - | 10455 | `		  vm_builtin_DOMDocument_importNode },` |
|      - | 10456 | `		{ "adoptNode",            PH7_MOD_PUBLIC, "DOMNode $node", "@DOMNode\|false",` |
|      - | 10457 | `		  vm_builtin_DOMDocument_adoptNode },` |
|      - | 10458 | `		{ "getElementById",       PH7_MOD_PUBLIC, "string $elementId", "@?DOMElement",` |
|      - | 10459 | `		  vm_builtin_DOMDocument_getElementById },` |
|      - | 10460 | `		{ "createAttribute",      PH7_MOD_PUBLIC, "string $localName", "",` |
|      - | 10461 | `		  vm_builtin_DOMDocument_createAttribute },` |
|      - | 10462 | `		{ "createAttributeNS",    PH7_MOD_PUBLIC, "?string $namespace, string $qualifiedName", "",` |
|      - | 10463 | `		  vm_builtin_DOMDocument_createAttributeNS },` |
|      - | 10464 | `		{ "createTextNode",       PH7_MOD_PUBLIC, "string $data", "@DOMText",` |
|      - | 10465 | `		  vm_builtin_DOMDocument_createTextNode },` |
|      - | 10466 | `		{ "createComment",        PH7_MOD_PUBLIC, "string $data", "@DOMComment",` |
|      - | 10467 | `		  vm_builtin_DOMDocument_createComment },` |
|      - | 10468 | `		{ "createCDATASection",   PH7_MOD_PUBLIC, "string $data", "",` |
|      - | 10469 | `		  vm_builtin_DOMDocument_createCDATASection },` |
|      - | 10470 | `		{ "createProcessingInstruction", PH7_MOD_PUBLIC, "string $target, string $data = ''", "",` |
|      - | 10471 | `		  vm_builtin_DOMDocument_createPI },` |
|      - | 10472 | `		{ "createEntityReference", PH7_MOD_PUBLIC, "string $name", "",` |
|      - | 10473 | `		  vm_builtin_DOMDocument_createEntityRef },` |
|      - | 10474 | `		{ "createDocumentFragment", PH7_MOD_PUBLIC, "", "@DOMDocumentFragment",` |
|      - | 10475 | `		  vm_builtin_DOMDocument_createFragment },` |
|      - | 10476 | `		{ "normalizeDocument",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },` |
|      - | 10477 | `		{ "registerNodeClass",    PH7_MOD_PUBLIC, "string $baseClass, ?string $extendedClass",` |
|      - | 10478 | `		  "@true", vm_builtin_DOMDocument_registerNodeClass },` |
|      - | 10479 | `		{ "schemaValidate",       PH7_MOD_PUBLIC, "string $filename, int $flags = 0", "@bool",` |
|      - | 10480 | `		  vm_builtin_DOMDocument_schemaValidate },` |
|      - | 10481 | `		{ "schemaValidateSource", PH7_MOD_PUBLIC, "string $source, int $flags = 0", "@bool",` |
|      - | 10482 | `		  vm_builtin_DOMDocument_schemaValidateSource },` |
|      - | 10483 | `		/* php declares no option word on the RelaxNG pair at all. */` |
|      - | 10484 | `		{ "relaxNGValidate",       PH7_MOD_PUBLIC, "string $filename", "@bool",` |
|      - | 10485 | `		  vm_builtin_DOMDocument_relaxNGValidate },` |
|      - | 10486 | `		{ "relaxNGValidateSource", PH7_MOD_PUBLIC, "string $source", "@bool",` |
|      - | 10487 | `		  vm_builtin_DOMDocument_relaxNGValidateSource },` |
|      - | 10488 | `		{ "validate",             PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 10489 | `		  vm_builtin_DOMDocument_validate },` |
|      - | 10490 | `		{ "xinclude",             PH7_MOD_PUBLIC, "int $options = 0", "@int\|false",` |
|      - | 10491 | `		  vm_builtin_DOMDocument_xinclude },` |
|      - | 10492 | `		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",` |
|      - | 10493 | `		  vm_builtin_Dom_getElementsByTagName },` |
|      - | 10494 | `		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",` |
|      - | 10495 | `		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },` |
|      - | 10496 | `		/* The DOMParentNode three: real (non-tentative) void, one untyped` |
|      - | 10497 | `		 * variadic -- php's own rows, screened inside the body. */` |
|      - | 10498 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|      - | 10499 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|      - | 10500 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|      - | 10501 | `	};` |
|      - | 10502 | `	static const PH7_NativeMethodDef aElemMethod[] = {` |
|      - | 10503 | `		{ "__construct", PH7_MOD_PUBLIC,` |
|      - | 10504 | `		  "string $qualifiedName, ?string $value = null, string $namespace = ''", "",` |
|      - | 10505 | `		  vm_builtin_DOMElement_construct },` |
|      - | 10506 | `		{ "getAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@string",` |
|      - | 10507 | `		  vm_builtin_DOMElement_getAttribute },` |
|      - | 10508 | `		{ "hasAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",` |
|      - | 10509 | `		  vm_builtin_DOMElement_hasAttribute },` |
|      - | 10510 | `		{ "setAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName, string $value", "",` |
|      - | 10511 | `		  vm_builtin_DOMElement_setAttribute },` |
|      - | 10512 | `		{ "removeAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",` |
|      - | 10513 | `		  vm_builtin_DOMElement_removeAttribute },` |
|      - | 10514 | `		{ "getAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@string",` |
|      - | 10515 | `		  vm_builtin_DOMElement_getAttributeNS },` |
|      - | 10516 | `		/* php declares no return type at all on the five that hand out a NODE --` |
|      - | 10517 | `		 * not even a tentative one -- because their answer is a union it never` |
|      - | 10518 | `		 * wrote down. The rows state none either. */` |
|      - | 10519 | `		{ "getAttributeNode",     PH7_MOD_PUBLIC, "string $qualifiedName", "",` |
|      - | 10520 | `		  vm_builtin_DOMElement_getAttributeNode },` |
|      - | 10521 | `		{ "getAttributeNodeNS",   PH7_MOD_PUBLIC, "?string $namespace, string $localName", "",` |
|      - | 10522 | `		  vm_builtin_DOMElement_getAttributeNodeNS },` |
|      - | 10523 | `		{ "setAttributeNode",     PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|      - | 10524 | `		  vm_builtin_DOMElement_setAttributeNode },` |
|      - | 10525 | `		{ "setAttributeNodeNS",   PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|      - | 10526 | `		  vm_builtin_DOMElement_setAttributeNodeNS },` |
|      - | 10527 | `		{ "removeAttributeNode",  PH7_MOD_PUBLIC, "DOMAttr $attr", "",` |
|      - | 10528 | `		  vm_builtin_DOMElement_removeAttributeNode },` |
|      - | 10529 | `		{ "getAttributeNames",    PH7_MOD_PUBLIC, "", "array",` |
|      - | 10530 | `		  vm_builtin_DOMElement_getAttributeNames },` |
|      - | 10531 | `		{ "hasAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@bool",` |
|      - | 10532 | `		  vm_builtin_DOMElement_hasAttributeNS },` |
|      - | 10533 | `		{ "removeAttributeNS",    PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@void",` |
|      - | 10534 | `		  vm_builtin_DOMElement_removeAttributeNS },` |
|      - | 10535 | `		{ "toggleAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName, ?bool $force = null", "bool",` |
|      - | 10536 | `		  vm_builtin_DOMElement_toggleAttribute },` |
|      - | 10537 | `		{ "setIdAttribute",       PH7_MOD_PUBLIC, "string $qualifiedName, bool $isId", "@void",` |
|      - | 10538 | `		  vm_builtin_DOMElement_setIdAttribute },` |
|      - | 10539 | `		{ "setIdAttributeNS",     PH7_MOD_PUBLIC,` |
|      - | 10540 | `		  "string $namespace, string $qualifiedName, bool $isId", "@void",` |
|      - | 10541 | `		  vm_builtin_DOMElement_setIdAttributeNS },` |
|      - | 10542 | `		{ "setIdAttributeNode",   PH7_MOD_PUBLIC, "DOMAttr $attr, bool $isId", "@void",` |
|      - | 10543 | `		  vm_builtin_DOMElement_setIdAttributeNode },` |
|      - | 10544 | `		{ "setAttributeNS",       PH7_MOD_PUBLIC,` |
|      - | 10545 | `		  "?string $namespace, string $qualifiedName, string $value", "@void",` |
|      - | 10546 | `		  vm_builtin_DOMElement_setAttributeNS },` |
|      - | 10547 | `		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",` |
|      - | 10548 | `		  vm_builtin_Dom_getElementsByTagName },` |
|      - | 10549 | `		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",` |
|      - | 10550 | `		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },` |
|      - | 10551 | `		/* The DOMChildNode four, php's order on this class. */` |
|      - | 10552 | `		{ "remove",          PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf },` |
|      - | 10553 | `		{ "before",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_before },` |
|      - | 10554 | `		{ "after",           PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_after },` |
|      - | 10555 | `		{ "replaceWith",     PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceWith },` |
|      - | 10556 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|      - | 10557 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|      - | 10558 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|      - | 10559 | `		/* The 8.3 pair. php declares the first's return as a plain ?DOMElement` |
|      - | 10560 | `		 * and the second's as a real void. */` |
|      - | 10561 | `		{ "insertAdjacentElement", PH7_MOD_PUBLIC, "string $where, DOMElement $element",` |
|      - | 10562 | `		  "?DOMElement", vm_builtin_DOMElement_insertAdjacentElement },` |
|      - | 10563 | `		{ "insertAdjacentText",    PH7_MOD_PUBLIC, "string $where, string $data", "void",` |
|      - | 10564 | `		  vm_builtin_DOMElement_insertAdjacentText },` |
|      - | 10565 | `	};` |
|      - | 10566 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|      - | 10567 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name, string $value = ''", "",` |
|      - | 10568 | `		  vm_builtin_DOMAttr_construct },` |
|      - | 10569 | `		{ "isId",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMAttr_isId },` |
|      - | 10570 | `	};` |
|      - | 10571 | `	static const PH7_NativeMethodDef aCharMethod[] = {` |
|      - | 10572 | `		/* Every offset and count here is in UTF-8 CHARACTERS, php's unit. */` |
|      - | 10573 | `		{ "appendData",    PH7_MOD_PUBLIC, "string $data", "@true",` |
|      - | 10574 | `		  vm_builtin_DOMCharacterData_appendData },` |
|      - | 10575 | `		{ "substringData", PH7_MOD_PUBLIC, "int $offset, int $count", "",` |
|      - | 10576 | `		  vm_builtin_DOMCharacterData_substringData },` |
|      - | 10577 | `		{ "insertData",    PH7_MOD_PUBLIC, "int $offset, string $data", "@bool",` |
|      - | 10578 | `		  vm_builtin_DOMCharacterData_insertData },` |
|      - | 10579 | `		{ "deleteData",    PH7_MOD_PUBLIC, "int $offset, int $count", "@bool",` |
|      - | 10580 | `		  vm_builtin_DOMCharacterData_deleteData },` |
|      - | 10581 | `		{ "replaceData",   PH7_MOD_PUBLIC, "int $offset, int $count, string $data", "@bool",` |
|      - | 10582 | `		  vm_builtin_DOMCharacterData_replaceData },` |
|      - | 10583 | `		/* The DOMChildNode four, php's order on THIS class -- replaceWith` |
|      - | 10584 | `		 * leads here where DOMElement's list starts at remove. */` |
|      - | 10585 | `		{ "replaceWith", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceWith },` |
|      - | 10586 | `		{ "remove",      PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf },` |
|      - | 10587 | `		{ "before",      PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_before },` |
|      - | 10588 | `		{ "after",       PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_after },` |
|      - | 10589 | `	};` |
|      - | 10590 | `	static const PH7_NativeMethodDef aPiMethod[] = {` |
|      - | 10591 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name, string $value = ''", "",` |
|      - | 10592 | `		  vm_builtin_DOMProcessingInstruction_construct },` |
|      - | 10593 | `	};` |
|      - | 10594 | `	static const PH7_NativeMethodDef aFragMethod[] = {` |
|      - | 10595 | `		{ "__construct", PH7_MOD_PUBLIC, "", "",` |
|      - | 10596 | `		  vm_builtin_DOMDocumentFragment_construct },` |
|      - | 10597 | `		{ "appendXML", PH7_MOD_PUBLIC, "string $data", "@bool",` |
|      - | 10598 | `		  vm_builtin_DOMDocumentFragment_appendXML },` |
|      - | 10599 | `		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },` |
|      - | 10600 | `		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },` |
|      - | 10601 | `		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },` |
|      - | 10602 | `	};` |
|      - | 10603 | `	static const PH7_NativeMethodDef aTextMethod[] = {` |
|      - | 10604 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data = ''", "",` |
|      - | 10605 | `		  vm_builtin_DOMText_construct },` |
|      - | 10606 | `		{ "splitText", PH7_MOD_PUBLIC, "int $offset", "", vm_builtin_DOMText_splitText },` |
|      - | 10607 | `		/* php's 8.x rename and the name it renamed, one body. */` |
|      - | 10608 | `		{ "isWhitespaceInElementContent", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 10609 | `		  vm_builtin_DOMText_isWhitespace },` |
|      - | 10610 | `		{ "isElementContentWhitespace",   PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 10611 | `		  vm_builtin_DOMText_isWhitespace },` |
|      - | 10612 | `	};` |
|      - | 10613 | `	static const PH7_NativeMethodDef aEntRefMethod[] = {` |
|      - | 10614 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|      - | 10615 | `		  vm_builtin_DOMEntityReference_construct },` |
|      - | 10616 | `	};` |
|      - | 10617 | `	/* The DTD half declares no method of its own at all -- php's whole` |
|      - | 10618 | `	 * DOMDocumentType surface is properties over DOMNode's method list. */` |
|      - | 10619 | `	/* php's factory class: three ordinary instance methods and no state. */` |
|      - | 10620 | `	static const PH7_NativeMethodDef aImplMethod[] = {` |
|      - | 10621 | `		{ "createDocumentType", PH7_MOD_PUBLIC,` |
|      - | 10622 | `		  "string $qualifiedName, string $publicId = '', string $systemId = ''", "",` |
|      - | 10623 | `		  vm_builtin_DOMImplementation_createDocumentType },` |
|      - | 10624 | `		{ "createDocument", PH7_MOD_PUBLIC,` |
|      - | 10625 | `		  "?string $namespace = null, string $qualifiedName = '', "` |
|      - | 10626 | `		  "?DOMDocumentType $doctype = null", "@DOMDocument",` |
|      - | 10627 | `		  vm_builtin_DOMImplementation_createDocument },` |
|      - | 10628 | `		{ "hasFeature", PH7_MOD_PUBLIC, "string $feature, string $version", "@bool",` |
|      - | 10629 | `		  vm_builtin_DOMImplementation_hasFeature },` |
|      - | 10630 | `	};` |
|      - | 10631 | `	/* The comment and CDATA constructors -- the only method either class` |
|      - | 10632 | `	 * declares of its own; php's CDATA data is REQUIRED where the other two` |
|      - | 10633 | `	 * default. */` |
|      - | 10634 | `	static const PH7_NativeMethodDef aCommentMethod[] = {` |
|      - | 10635 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data = ''", "",` |
|      - | 10636 | `		  vm_builtin_DOMComment_construct },` |
|      - | 10637 | `	};` |
|      - | 10638 | `	static const PH7_NativeMethodDef aCdataMethod[] = {` |
|      - | 10639 | `		{ "__construct", PH7_MOD_PUBLIC, "string $data", "",` |
|      - | 10640 | `		  vm_builtin_DOMCdataSection_construct },` |
|      - | 10641 | `	};` |
|      - | 10642 | `	/* DOMNodeList and DOMNamedNodeMap share a slot layout: what a live view is OF` |
|      - | 10643 | `	 * ($__owner), the document to wrap results against ($__doc), and -- for the` |
|      - | 10644 | `	 * two node-list kinds -- the tag name or the frozen snapshot. */` |
|      - | 10645 | `	static const PH7_NativePropDef aListProp[] = {` |
|      - | 10646 | `		/* php's one declared property on either class, and virtual there too. */` |
|      - | 10647 | `		DOM_VPROP("length","int"),` |
|      - | 10648 | `		{ DNL_KIND,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|      - | 10649 | `		{ DOM_DOC,       PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|      - | 10650 | `		{ DNL_OWNER,     PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|      - | 10651 | `		{ DNL_NAME,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 10652 | `		/* ...and, for the namespace-aware lookup, the URI beside the name. */` |
|      - | 10653 | `		{ DNL_URI,       PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 10654 | `		/* The cached node snapshot, missed by the 2 Aug hidden-slot sweep exactly as` |
|      - | 10655 | `		 * Closure's three were: php presents no property on either class this table` |
|      - | 10656 | ``		 * declares (DOMNodeList, DOMNamedNodeMap), and `__snap` was on var_dump,`` |
|      - | 10657 | `		 * (array), get_object_vars, foreach, json_encode and Reflection. */` |
|      - | 10658 | `		{ DNL_SNAP_SLOT, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|      - | 10659 | `	};` |
|      - | 10660 | `	static const PH7_NativeMethodDef aListMethod[] = {` |
|      - | 10661 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNodeList_count },` |
|      - | 10662 | `		{ "item",        PH7_MOD_PUBLIC, "int $index", "", vm_builtin_DOMNodeList_item },` |
|      - | 10663 | `		{ "getIterator", PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },` |
|      - | 10664 | `	};` |
|      - | 10665 | `	static const PH7_NativeMethodDef aMapMethod[] = {` |
|      - | 10666 | `		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNamedNodeMap_count },` |
|      - | 10667 | `		{ "item",         PH7_MOD_PUBLIC, "int $index", "@?DOMNode", vm_builtin_DOMNamedNodeMap_item },` |
|      - | 10668 | `		{ "getNamedItem", PH7_MOD_PUBLIC, "string $qualifiedName", "@?DOMNode",` |
|      - | 10669 | `		  vm_builtin_DOMNamedNodeMap_getNamedItem },` |
|      - | 10670 | `		{ "getNamedItemNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@?DOMNode",` |
|      - | 10671 | `		  vm_builtin_DOMNamedNodeMap_getNamedItemNS },` |
|      - | 10672 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },` |
|      - | 10673 | `	};` |
|      - | 10674 | `	/* The declaration itself, the document it belongs to, and the element that` |
|      - | 10675 | `	 * MAKES it -- php's parentNode/parentElement. */` |
|      - | 10676 | `	static const PH7_NativePropDef aNsNodeProp[] = {` |
|      - | 10677 | `		/* php's ten, in its order: a namespace declaration is not a DOMNode there,` |
|      - | 10678 | `		 * so the class states its own subset rather than inheriting one. */` |
|      - | 10679 | `		DOM_VPROP("nodeName","string"),` |
|      - | 10680 | `		DOM_VPROP("nodeValue","?string"),` |
|      - | 10681 | `		DOM_VPROP("nodeType","int"),` |
|      - | 10682 | `		DOM_VPROP("prefix","string"),` |
|      - | 10683 | `		DOM_VPROP("localName","?string"),` |
|      - | 10684 | `		DOM_VPROP("namespaceURI","?string"),` |
|      - | 10685 | `		DOM_VPROP("isConnected","bool"),` |
|      - | 10686 | `		DOM_VPROP("ownerDocument","?DOMDocument"),` |
|      - | 10687 | `		DOM_VPROP("parentNode","?DOMNode"),` |
|      - | 10688 | `		DOM_VPROP("parentElement","?DOMElement"),` |
|      - | 10689 | `		{ DOM_RES,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10690 | `		{ DOM_DOC,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10691 | `		{ DOM_NS_OWNER, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10692 | `	};` |
|      - | 10693 | `	static const PH7_NativeMethodDef aNsNodeMethod[] = {` |
|      - | 10694 | `		{ "__sleep",  PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },` |
|      - | 10695 | `		{ "__wakeup", PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },` |
|      - | 10696 | `	};` |
|      - | 10697 | `	static const PH7_NativePropDef aXPathProp[] = {` |
|      - | 10698 | ``		/* php models both as VIRTUAL: `document` is read-only because its handler`` |
|      - | 10699 | ``		 * has no writer -- not because the slot is `readonly`, which is why php's`` |
|      - | 10700 | `		 * isReadOnly() answers false and its modifiers are 513. The values live in` |
|      - | 10701 | `		 * the two hidden slots below. */` |
|      - | 10702 | `		DOM_VPROP("document","DOMDocument"),` |
|      - | 10703 | `		DOM_VPROP("registerNodeNamespaces","bool"),` |
|      - | 10704 | `		{ XP_DOC,    PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10705 | `		{ XP_NSDEF,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 },` |
|      - | 10706 | `		{ XP_NSREG,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10707 | `		{ XP_FNMODE, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN,` |
|      - | 10708 | `		  { 0, 0, PH7_NATIVE_VAL_INT, XP_MODE_NONE, 0, 0.0 }, 0 },` |
|      - | 10709 | `		{ XP_FNREG,  PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10710 | `		{ XP_NSFN,   PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 10711 | `	};` |
|      - | 10712 | `	static const PH7_NativeMethodDef aXPathMethod[] = {` |
|      - | 10713 | `		{ "__construct", PH7_MOD_PUBLIC, "DOMDocument $document, bool $registerNodeNS = true", "",` |
|      - | 10714 | `		  vm_builtin_DOMXPath_construct },` |
|      - | 10715 | `		{ "query",       PH7_MOD_PUBLIC,` |
|      - | 10716 | `		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",` |
|      - | 10717 | `		  vm_builtin_DOMXPath_query },` |
|      - | 10718 | `		{ "evaluate",    PH7_MOD_PUBLIC,` |
|      - | 10719 | `		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",` |
|      - | 10720 | `		  vm_builtin_DOMXPath_evaluate },` |
|      - | 10721 | `		{ "registerNamespace", PH7_MOD_PUBLIC, "string $prefix, string $namespace", "@bool",` |
|      - | 10722 | `		  vm_builtin_DOMXPath_registerNamespace },` |
|      - | 10723 | `		{ "registerPhpFunctions", PH7_MOD_PUBLIC, "array\|string\|null $restrict = null", "@void",` |
|      - | 10724 | `		  vm_builtin_DOMXPath_registerPhpFunctions },` |
|      - | 10725 | `		{ "registerPhpFunctionNS", PH7_MOD_PUBLIC,` |
|      - | 10726 | `		  "string $namespaceURI, string $name, callable $callable", "void",` |
|      - | 10727 | `		  vm_builtin_DOMXPath_registerPhpFunctionNS },` |
|      - | 10728 | `		{ "quote", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $str", "string",` |
|      - | 10729 | `		  vm_builtin_DOMXPath_quote },` |
|      - | 10730 | `	};` |
|      - | 10731 | `	/* Bases before subclasses: PH7_InstallNativeClasses declares the whole table` |
|      - | 10732 | `	 * before touching a method, but PH7_ClassInherit still needs the parent to` |
|      - | 10733 | `	 * exist when the child's row is declared. */` |
|      - | 10734 | `	/* php refuses to serialize a NODE class, and its refusal is the soft kind: the` |
|      - | 10735 | `	 * deny handler sits behind the __serialize()/__sleep() lookup, so a subclass that` |
|      - | 10736 | `	 * declares either one is serialized normally and the sentence says so. DOMXPath's` |
|      - | 10737 | `	 * is the HARD kind — a subclass declaring __serialize() is refused there too — and` |
|      - | 10738 | ``	 * DOMNodeList/DOMNamedNodeMap are not refused at all (`0:{}`), which is what they`` |
|      - | 10739 | `	 * became once serialize() stopped emitting the hidden slot. Restating the flag on` |
|      - | 10740 | `	 * every row is rule 29: a native subclass does not inherit its parent's. */` |
|      - | 10741 | `	/* php's 8.0 insertion interfaces: three untyped-variadic void methods on` |
|      - | 10742 | `	 * the parent side (the child side is DOMChildNode below).  A class row` |
|      - | 10743 | `	 * declares its methods before the implement phase runs, so nothing is` |
|      - | 10744 | `	 * stubbed abstract. */` |
|      - | 10745 | `	static const PH7_NativeMethodDef aParentNodeIf[] = {` |
|      - | 10746 | `		{ "append",          PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10747 | `		{ "prepend",         PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10748 | `		{ "replaceChildren", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10749 | `	};` |
|      - | 10750 | `	static const PH7_NativeMethodDef aChildNodeIf[] = {` |
|      - | 10751 | `		{ "remove",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "", "void", 0 },` |
|      - | 10752 | `		{ "before",      PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10753 | `		{ "after",       PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10754 | `		{ "replaceWith", PH7_MOD_PUBLIC\|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },` |
|      - | 10755 | `	};` |
|      - | 10756 | `	/* The remaining classes' OWN declarations, each php's list in php's order.` |
|      - | 10757 | `	 * They add no storage: every row is virtual, so these tables exist only to put` |
|      - | 10758 | `	 * the names on the class -- for Reflection, for property_exists(), and for the` |
|      - | 10759 | `	 * debug table above to walk. */` |
|      - | 10760 | `	static const PH7_NativePropDef aElemProp[] = {` |
|      - | 10761 | `		DOM_VPROP("tagName","string"),` |
|      - | 10762 | `		DOM_VPROP("className","string"),` |
|      - | 10763 | `		DOM_VPROP("id","string"),` |
|      - | 10764 | `		DOM_VPROP("schemaTypeInfo","mixed"),` |
|      - | 10765 | `		DOM_PARENT_VPROPS,` |
|      - | 10766 | `		DOM_CHILD_VPROPS` |
|      - | 10767 | `	};` |
|      - | 10768 | `	static const PH7_NativePropDef aAttrProp[] = {` |
|      - | 10769 | `		DOM_VPROP("name","string"),` |
|      - | 10770 | `		DOM_VPROP("specified","bool"),` |
|      - | 10771 | `		DOM_VPROP("value","string"),` |
|      - | 10772 | `		DOM_VPROP("ownerElement","?DOMElement"),` |
|      - | 10773 | `		DOM_VPROP("schemaTypeInfo","mixed")` |
|      - | 10774 | `	};` |
|      - | 10775 | `	static const PH7_NativePropDef aCharProp[] = {` |
|      - | 10776 | `		DOM_VPROP("data","string"),` |
|      - | 10777 | `		DOM_VPROP("length","int"),` |
|      - | 10778 | `		DOM_CHILD_VPROPS` |
|      - | 10779 | `	};` |
|      - | 10780 | `	static const PH7_NativePropDef aTextProp[] = {` |
|      - | 10781 | `		DOM_VPROP("wholeText","string")` |
|      - | 10782 | `	};` |
|      - | 10783 | `	static const PH7_NativePropDef aPiProp[] = {` |
|      - | 10784 | `		DOM_VPROP("target","string"),` |
|      - | 10785 | `		DOM_VPROP("data","string")` |
|      - | 10786 | `	};` |
|      - | 10787 | `	static const PH7_NativePropDef aFragProp[] = { DOM_PARENT_VPROPS };` |
|      - | 10788 | `	static const PH7_NativePropDef aDocTypeProp[] = {` |
|      - | 10789 | `		DOM_VPROP("name","string"),` |
|      - | 10790 | `		DOM_VPROP("entities","DOMNamedNodeMap"),` |
|      - | 10791 | `		DOM_VPROP("notations","DOMNamedNodeMap"),` |
|      - | 10792 | `		DOM_VPROP("publicId","string"),` |
|      - | 10793 | `		DOM_VPROP("systemId","string"),` |
|      - | 10794 | `		DOM_VPROP("internalSubset","?string")` |
|      - | 10795 | `	};` |
|      - | 10796 | `	static const PH7_NativePropDef aEntityProp[] = {` |
|      - | 10797 | `		DOM_VPROP("publicId","?string"),` |
|      - | 10798 | `		DOM_VPROP("systemId","?string"),` |
|      - | 10799 | `		DOM_VPROP("notationName","?string"),` |
|      - | 10800 | `		DOM_VPROP("actualEncoding","?string"),` |
|      - | 10801 | `		DOM_VPROP("encoding","?string"),` |
|      - | 10802 | `		DOM_VPROP("version","?string")` |
|      - | 10803 | `	};` |
|      - | 10804 | ``	/* php's pair here are plain `string`, unlike DOMEntity's same-named `?string`. */`` |
|      - | 10805 | `	static const PH7_NativePropDef aNotationProp[] = {` |
|      - | 10806 | `		DOM_VPROP("publicId","string"),` |
|      - | 10807 | `		DOM_VPROP("systemId","string")` |
|      - | 10808 | `	};` |
|      - | 10809 | ``	/* php's DOMException REDECLARES Exception's `$code` as a PUBLIC untyped slot --`` |
|      - | 10810 | ``	 * which is why `(array)$e` and `get_object_vars($e)` show a plain `code` there`` |
|      - | 10811 | `	 * where every other exception mangles it, and why Reflection reports it as` |
|      - | 10812 | `	 * DOMException's own with modifiers 1. The instance keeps the position the BASE` |
|      - | 10813 | `	 * declared it in, so var_dump still prints it third. */` |
|      - | 10814 | `	static const PH7_NativePropDef aExcProp[] = {` |
|      - | 10815 | `		{ "code", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }` |
|      - | 10816 | `	};` |
|      - | 10817 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 10818 | `		{ "DOMException", "Exception", 0, PH7_CLASS_FINAL, 0, 0, 0, 0,` |
|      - | 10819 | `		  aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },` |
|      - | 10820 | `		{ "DOMParentNode", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 10821 | `		  aParentNodeIf, SX_ARRAYSIZE(aParentNodeIf), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 10822 | `		{ "DOMChildNode", 0, 0, PH7_CLASS_INTERFACE,` |
|      - | 10823 | `		  aChildNodeIf, SX_ARRAYSIZE(aChildNodeIf), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 10824 | `		{ "DOMNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10825 | `		  aNodeMethod, SX_ARRAYSIZE(aNodeMethod), aNodeConst, SX_ARRAYSIZE(aNodeConst),` |
|      - | 10826 | `		  aNodeProp, SX_ARRAYSIZE(aNodeProp), 0, 0, DomPresent },` |
|      - | 10827 | `		{ "DOMDocument", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10828 | `		  aDocMethod, SX_ARRAYSIZE(aDocMethod), 0, 0, aDocProp, SX_ARRAYSIZE(aDocProp),` |
|      - | 10829 | `		  DomDocRelease, 0, DomPresent },` |
|      - | 10830 | `		{ "DOMElement", "DOMNode", "DOMParentNode,DOMChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10831 | `		  aElemMethod, SX_ARRAYSIZE(aElemMethod), 0, 0, aElemProp, SX_ARRAYSIZE(aElemProp),` |
|      - | 10832 | `		  0, 0, DomPresent },` |
|      - | 10833 | `		{ "DOMAttr", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10834 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), 0, 0, aAttrProp, SX_ARRAYSIZE(aAttrProp),` |
|      - | 10835 | `		  0, 0, DomPresent },` |
|      - | 10836 | `		{ "DOMCharacterData", "DOMNode", "DOMChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10837 | `		  aCharMethod, SX_ARRAYSIZE(aCharMethod), 0, 0, aCharProp, SX_ARRAYSIZE(aCharProp),` |
|      - | 10838 | `		  0, 0, DomPresent },` |
|      - | 10839 | `		{ "DOMText", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10840 | `		  aTextMethod, SX_ARRAYSIZE(aTextMethod), 0, 0, aTextProp, SX_ARRAYSIZE(aTextProp),` |
|      - | 10841 | `		  0, 0, DomPresent },` |
|      - | 10842 | `		{ "DOMComment", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10843 | `		  aCommentMethod, SX_ARRAYSIZE(aCommentMethod), 0, 0, 0, 0, 0, 0, DomPresent },` |
|      - | 10844 | `		{ "DOMCdataSection", "DOMText", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10845 | `		  aCdataMethod, SX_ARRAYSIZE(aCdataMethod), 0, 0, 0, 0, 0, 0, DomPresent },` |
|      - | 10846 | ``		/* php declares the PI under DOMNode (its `data` is its own property, not`` |
|      - | 10847 | `		 * DOMCharacterData's), the fragment and the entity reference plainly. */` |
|      - | 10848 | `		{ "DOMProcessingInstruction", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10849 | `		  aPiMethod, SX_ARRAYSIZE(aPiMethod), 0, 0, aPiProp, SX_ARRAYSIZE(aPiProp),` |
|      - | 10850 | `		  0, 0, DomPresent },` |
|      - | 10851 | `		{ "DOMDocumentFragment", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10852 | `		  aFragMethod, SX_ARRAYSIZE(aFragMethod), 0, 0, aFragProp, SX_ARRAYSIZE(aFragProp),` |
|      - | 10853 | `		  0, 0, DomPresent },` |
|      - | 10854 | `		{ "DOMEntityReference", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10855 | `		  aEntRefMethod, SX_ARRAYSIZE(aEntRefMethod), 0, 0, 0, 0, 0, 0, DomPresent },` |
|      - | 10856 | `		/* The DTD trio state no method of their own -- every name php declares on` |
|      - | 10857 | `		 * them is a property, and the class's handler answers it. */` |
|      - | 10858 | `		{ "DOMDocumentType", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10859 | `		  0, 0, 0, 0, aDocTypeProp,` |
|      - | 10860 | `		  SX_ARRAYSIZE(aDocTypeProp), 0, 0, DomPresent },` |
|      - | 10861 | `		{ "DOMEntity", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10862 | `		  0, 0, 0, 0, aEntityProp,` |
|      - | 10863 | `		  SX_ARRAYSIZE(aEntityProp), 0, 0, DomPresent },` |
|      - | 10864 | `		{ "DOMImplementation", 0, 0, 0,` |
|      - | 10865 | `		  aImplMethod, SX_ARRAYSIZE(aImplMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 10866 | `		{ "DOMNotation", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10867 | `		  0, 0, 0, 0, aNotationProp,` |
|      - | 10868 | `		  SX_ARRAYSIZE(aNotationProp), 0, 0, DomPresent },` |
|      - | 10869 | `		/* php's own two: IteratorAggregate (NOT Iterator -- the chunk had the` |
|      - | 10870 | `		 * list carry its own cursor) and Countable. */` |
|      - | 10871 | `		{ "DOMNodeList", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,` |
|      - | 10872 | `		  aListMethod, SX_ARRAYSIZE(aListMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),` |
|      - | 10873 | `		  0, &sDomListIterVtab, DomPresent },` |
|      - | 10874 | `		{ "DOMNamedNodeMap", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,` |
|      - | 10875 | `		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),` |
|      - | 10876 | `		  0, &sDomMapIterVtab, DomPresent },` |
|      - | 10877 | `		/* php's own: a class of its OWN, with no parent at all -- a namespace` |
|      - | 10878 | ``		 * declaration is not a DOMNode there, and `$ns instanceof DOMNode` is`` |
|      - | 10879 | `		 * false. Its refusal to serialize is the same soft kind the node` |
|      - | 10880 | `		 * classes carry. */` |
|      - | 10881 | `		{ "DOMNameSpaceNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|      - | 10882 | `		  aNsNodeMethod, SX_ARRAYSIZE(aNsNodeMethod), 0, 0,` |
|      - | 10883 | `		  aNsNodeProp, SX_ARRAYSIZE(aNsNodeProp), 0, 0, DomPresent },` |
|      - | 10884 | `		{ "DOMXPath", 0, 0, PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOCLONE,` |
|      - | 10885 | `		  aXPathMethod, SX_ARRAYSIZE(aXPathMethod), 0, 0, aXPathProp, SX_ARRAYSIZE(aXPathProp),` |
|      - | 10886 | `		  0, 0, DomPresent },` |
|      - | 10887 | `	};` |
|   6726 | 10888 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|   6726 | 10889 | `	if( rc == SXRET_OK ){` |
|      - | 10890 | `		/* The clone hook (ph7_class::xClone, php's clone_obj): stated on every` |
|      - | 10891 | `		 * node class -- rule 29, a hook is per-row and never inherited between` |
|      - | 10892 | `		 * native rows -- and assigned HERE because PH7_NativeClassSpec carries` |
|      - | 10893 | `		 * no field for it. The document's copies the whole document; a user` |
|      - | 10894 | `		 * subclass reaches the nearest ancestor's hook through the engine's` |
|      - | 10895 | `		 * chain walk, php's handler inheritance. */` |
|      - | 10896 | `		static const char * const azNodeClone[] = {` |
|      - | 10897 | `			"DOMNode", "DOMElement", "DOMAttr", "DOMCharacterData", "DOMText",` |
|      - | 10898 | `			"DOMComment", "DOMCdataSection", "DOMProcessingInstruction",` |
|      - | 10899 | `			"DOMDocumentFragment", "DOMEntityReference", "DOMDocumentType"` |
|      - | 10900 | `		};` |
|      - | 10901 | `		sxu32 n;` |
|      - | 10902 | `		ph7_class *pClass;` |
|  80657 | 10903 | `		for( n = 0 ; n < SX_ARRAYSIZE(azNodeClone) ; ++n ){` |
| 110852 | 10904 | `			pClass = PH7_VmExtractClass(&(*pVm),azNodeClone[n],` |
|  73931 | 10905 | `				(sxu32)SyStrlen(azNodeClone[n]),FALSE,0);` |
|  73936 | 10906 | `			if( pClass ){` |
|  73936 | 10907 | `				pClass->xClone = DomInstanceClone;` |
|  36916 | 10908 | `			}` |
|  36921 | 10909 | `		}` |
|   6726 | 10910 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMDocument",sizeof("DOMDocument")-1,FALSE,0);` |
|   6726 | 10911 | `		if( pClass ){` |
|   6726 | 10912 | `			pClass->xClone = DomInstanceCloneDoc;` |
|   3356 | 10913 | `		}` |
|      - | 10914 | `		/* The dimension handlers (ph7_class::xDim, php's read_dimension /` |
|      - | 10915 | `		 * has_dimension), assigned here for the same reason the clone hook is:` |
|      - | 10916 | `		 * PH7_NativeClassSpec carries no field for them, and php's own two` |
|      - | 10917 | `		 * classes wear them without declaring ArrayAccess. */` |
|   6726 | 10918 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMNodeList",sizeof("DOMNodeList")-1,FALSE,0);` |
|   6726 | 10919 | `		if( pClass ){` |
|   6726 | 10920 | `			pClass->xDim = DomListDim;` |
|   3356 | 10921 | `		}` |
|   6726 | 10922 | `		pClass = PH7_VmExtractClass(&(*pVm),"DOMNamedNodeMap",sizeof("DOMNamedNodeMap")-1,FALSE,0);` |
|   6726 | 10923 | `		if( pClass ){` |
|   6726 | 10924 | `			pClass->xDim = DomMapDim;` |
|   3356 | 10925 | `		}` |
|      - | 10926 | `		/* The property handlers (ph7_class::xProp, php's read_property /` |
|      - | 10927 | `		 * has_property / write_property), assigned for the same reason. One per` |
|      - | 10928 | `		 * ROOT: the engine walks the base chain for the hook exactly as php's` |
|      - | 10929 | `		 * handlers are inherited, and the hook then picks the per-class table off` |
|      - | 10930 | `		 * aDomProp[] -- so a subclass of DOMElement reaches DOMElement's. */` |
|      - | 10931 | `		{` |
|      - | 10932 | `			static const char * const azPropRoot[] = {` |
|      - | 10933 | `				"DOMNode", "DOMNodeList", "DOMNamedNodeMap", "DOMNameSpaceNode", "DOMXPath"` |
|      - | 10934 | `			};` |
|  40331 | 10935 | `			for( n = 0 ; n < SX_ARRAYSIZE(azPropRoot) ; ++n ){` |
|  33610 | 10936 | `				PH7_NativeClassInstallPropHook(&(*pVm),azPropRoot[n],DomPropHook);` |
|  16785 | 10937 | `			}` |
|      - | 10938 | `		}` |
|   3356 | 10939 | `	}` |
|   6726 | 10940 | `	return rc;` |
|      5 | 10941 | `}` |
|      - | 10942 |  |
|      - | 10943 | `#else` |
|      - | 10944 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - | 10945 | `typedef int vm_dom_unused;` |
|      - | 10946 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - | 10947 |  |
