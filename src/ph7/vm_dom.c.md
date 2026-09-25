# src/ph7/vm_dom.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1028/1188 lines (86.53%)

[Root index](../../index.md) | [Directory index](index.md)

| Hits | Line | Source |
| ---: | ---: | :--- |
|    - |    1 | `/**` |
|    - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|    - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|    - |    4 | ` */` |
|    - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|    - |    6 | `#include "ph7int.h"` |
|    - |    7 | `#include <libxml/parser.h>` |
|    - |    8 | `#include <libxml/tree.h>` |
|    - |    9 | `#include <libxml/c14n.h>` |
|    - |   10 | `#include <libxml/xmlsave.h>` |
|    - |   11 | `#include <libxml/xpath.h>` |
|    - |   12 | `#include <libxml/xmlschemas.h>` |
|    - |   13 |  |
|    - |   14 | `/*` |
|    - |   15 | ` * ext/dom on libxml2: the DOM classes, declared and bodied in C.` |
|    - |   16 | ` *` |
|    - |   17 | ` * Architecture (see also vm_libxml.c): DOMNode and its subclasses are native` |
|    - |   18 | ` * classes (oo_native.c) whose methods ARE the C below.  Every instance holds` |
|    - |   19 | ` * two slots -- $__res, a phl_domnode resource {phl_xmldoc*, xmlNodePtr}, and` |
|    - |   20 | ` * $__doc, the owning DOMDocument wrapper.  There is no PHP layer left in the` |
|    - |   21 | ` * node tree: what used to be a prelude class over ~30 global __dom_* thunks is` |
|    - |   22 | ` * one C body per method, so the thunks stopped being globally visible names.` |
|    - |   23 | ` *` |
|    - |   24 | ` * Node identity: php guarantees $doc->documentElement === $doc->` |
|    - |   25 | ` * documentElement.  Every wrap goes through DomWrap(), which keys a` |
|    - |   26 | ` * per-document cache ($doc->__nodes) by the node POINTER, so the same` |
|    - |   27 | ` * underlying node always yields the same object.  The cache owns the` |
|    - |   28 | ` * wrappers, which is why DomWrap hands back a BORROWED instance: it stays` |
|    - |   29 | ` * alive as long as its document does.  (The old shape allocated a fresh` |
|    - |   30 | ` * phl_domnode on every navigation step even when the cache then threw the` |
|    - |   31 | ` * result away; only a genuine cache MISS allocates one now.)` |
|    - |   32 | ` *` |
|    - |   33 | ` * Tree surgery (append/insert/replace/remove) is done with manual pointer` |
|    - |   34 | ` * splicing instead of xmlAddChild: xmlAddChild MERGES adjacent text nodes` |
|    - |   35 | ` * and frees the merged-away node, which would dangle any PHP wrapper (and` |
|    - |   36 | ` * violates DOM semantics, which php follows -- appendChild never merges).` |
|    - |   37 | ` * Unlinked nodes are parked on the owning phl_xmldoc's orphan set so they` |
|    - |   38 | ` * are freed with the document at VM reset/release.` |
|    - |   39 | ` */` |
|    - |   40 |  |
|    - |   41 | `/* One native method body. Its receiver's node is DomThisNode(pCtx); apArg is` |
|    - |   42 | ` * php's own argument list, already screened against the declared signature. */` |
|    - |   43 | `#define DOM_METHOD(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|    - |   44 |  |
|    - |   45 | `/* The two slots every wrapper carries, and the document's identity cache. */` |
|    - |   46 | `#define DOM_RES   "__res"` |
|    - |   47 | `#define DOM_DOC   "__doc"` |
|    - |   48 | `#define DOM_NODES "__nodes"` |
|    - |   49 |  |
|    - |   50 | `/* Property names are byte-exact in php, and every name that reaches here is` |
|    - |   51 | ` * NUL-terminated (ph7_value_to_string null-appends). */` |
| 1152 |   52 | `static int DomNameIs(const char *zName,const char *zWant)` |
|    1 |   53 | `{` |
| 1153 |   54 | `	sxu32 n = (sxu32)SyStrlen(zWant);` |
| 1153 |   55 | `	return SyStrlen(zName) == n && SyStrncmp(zName,zWant,n) == 0;` |
|    1 |   56 | `}` |
|    - |   57 | `/* The handle behind an instance's $__res, or NULL for anything else. */` |
|  630 |   58 | `static phl_domnode * DomResOf(ph7_class_instance *pObj)` |
|    1 |   59 | `{` |
|  631 |   60 | `	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,DOM_RES) : 0;` |
|  631 |   61 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_RES) == 0 ){` |
|  ! 0 |   62 | `		return 0;` |
|    - |   63 | `	}` |
|  631 |   64 | `	return (phl_domnode *)pVal->x.pOther;` |
|  316 |   65 | `}` |
|    - |   66 | `/* The receiver of a native method, and the two things every body wants from it. */` |
|  396 |   67 | `static phl_domnode * DomThisNode(ph7_context *pCtx)` |
|    1 |   68 | `{` |
|  397 |   69 | `	return DomResOf(PH7_ContextThis(pCtx));` |
|    1 |   70 | `}` |
|  286 |   71 | `static ph7_class_instance * DomThisDoc(ph7_context *pCtx)` |
|    1 |   72 | `{` |
|  287 |   73 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  287 |   74 | `	return pThis ? PH7_NativeAttrObj(pThis,DOM_DOC) : 0;` |
|    1 |   75 | `}` |
|    - |   76 | `/* A fresh handle onto one node of pShell's tree. Freed with the VM allocator. */` |
|  282 |   77 | `static phl_domnode * DomNewRes(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode)` |
|    1 |   78 | `{` |
|  283 |   79 | `	phl_domnode *pWrap = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|  283 |   80 | `	if( pWrap ){` |
|  283 |   81 | `		pWrap->pShell = pShell;` |
|  283 |   82 | `		pWrap->pNode = pNode;` |
|  141 |   83 | `	}` |
|  283 |   84 | `	return pWrap;` |
|    1 |   85 | `}` |
|    - |   86 | `/* Store a handle in an instance's $__res slot. */` |
|  228 |   87 | `static void DomSetRes(ph7_vm *pVm,ph7_class_instance *pObj,phl_domnode *pRes)` |
|    1 |   88 | `{` |
|    - |   89 | `	ph7_value sVal;` |
|  229 |   90 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|  229 |   91 | `	sVal.x.pOther = pRes;` |
|  229 |   92 | `	sVal.iFlags = MEMOBJ_RES;` |
|  229 |   93 | `	PH7_NativeSetProp(&(*pVm),pObj,DOM_RES,sizeof(DOM_RES)-1,&sVal);` |
|  229 |   94 | `}` |
|    - |   95 | `/*` |
|    - |   96 | ` * The document's identity cache, materialized and separated from any copy that` |
|    - |   97 | ` * shares it. Same three moves a native class always needs to own an array slot` |
|    - |   98 | ` * (WeakMap's WmStore is the other one).` |
|    - |   99 | ` */` |
|  224 |  100 | `static ph7_hashmap * DomCache(ph7_vm *pVm,ph7_class_instance *pDoc)` |
|    1 |  101 | `{` |
|  225 |  102 | `	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NODES) : 0;` |
|  225 |  103 | `	if( pSlot == 0 ){` |
|  ! 0 |  104 | `		return 0;` |
|    - |  105 | `	}` |
|  225 |  106 | `	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 |  107 | `		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){` |
|  ! 0 |  108 | `			return 0;` |
|    - |  109 | `		}` |
|  ! 0 |  110 | `	}` |
|  225 |  111 | `	return PH7_HashmapCowSeparate(&(*pVm),pSlot);` |
|  113 |  112 | `}` |
|    - |  113 | `/* php's class for a node type. Anything else is a plain DOMNode, as before. */` |
|  116 |  114 | `static const char * DomClassOfKind(int iKind)` |
|    1 |  115 | `{` |
|  117 |  116 | `	switch( iKind ){` |
|   91 |  117 | `	case XML_ELEMENT_NODE:       return "DOMElement";` |
|   13 |  118 | `	case XML_ATTRIBUTE_NODE:     return "DOMAttr";` |
|    7 |  119 | `	case XML_TEXT_NODE:          return "DOMText";` |
|    5 |  120 | `	case XML_CDATA_SECTION_NODE: return "DOMCdataSection";` |
|    5 |  121 | `	case XML_COMMENT_NODE:       return "DOMComment";` |
|  ! 0 |  122 | `	default:                     return "DOMNode";` |
|    - |  123 | `	}` |
|   59 |  124 | `}` |
|    - |  125 | `/*` |
|    - |  126 | ` * The wrapper object for one node of pDoc's tree -- the same one every time,` |
|    - |  127 | `` * which is what makes `$doc->documentElement === $doc->documentElement` true.`` |
|    - |  128 | ` *` |
|    - |  129 | ` * BORROWED: the cache owns the returned instance. A caller that hands it to PHP` |
|    - |  130 | ` * goes through DomResultWrap (ph7_result_value takes its own reference); a` |
|    - |  131 | ` * caller that stores it uses PH7_NativeSetAttrObj, which does the same. Neither` |
|    - |  132 | ` * unrefs.` |
|    - |  133 | ` */` |
|  240 |  134 | `static ph7_class_instance * DomWrap(ph7_vm *pVm,ph7_class_instance *pDoc,` |
|    - |  135 | `	phl_xmldoc *pShell,xmlNodePtr pNode)` |
|    1 |  136 | `{` |
|    - |  137 | `	ph7_hashmap *pCache;` |
|  241 |  138 | `	ph7_hashmap_node *pEntry = 0;` |
|    - |  139 | `	ph7_class_instance *pObj;` |
|    - |  140 | `	ph7_class *pClass;` |
|    - |  141 | `	phl_domnode *pRes;` |
|    - |  142 | `	const char *zClass;` |
|    - |  143 | `	ph7_value sKey,sVal;` |
|  241 |  144 | `	if( pNode == 0 \|\| pDoc == 0 ){` |
|   15 |  145 | `		return 0;` |
|    - |  146 | `	}` |
|  227 |  147 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|    - |  148 | `		/* The document is its own wrapper: php answers the SAME DOMDocument. */` |
|    3 |  149 | `		return pDoc;` |
|    - |  150 | `	}` |
|  225 |  151 | `	pCache = DomCache(&(*pVm),pDoc);` |
|  225 |  152 | `	if( pCache == 0 ){` |
|  ! 0 |  153 | `		return 0;` |
|    - |  154 | `	}` |
|  225 |  155 | `	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);` |
|  225 |  156 | `	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){` |
|  109 |  157 | `		ph7_value *pHit = HashmapExtractNodeValue(pEntry);` |
|  109 |  158 | `		if( pHit && (pHit->iFlags & MEMOBJ_OBJ) ){` |
|  109 |  159 | `			PH7_MemObjRelease(&sKey);` |
|  109 |  160 | `			return (ph7_class_instance *)pHit->x.pOther;` |
|    - |  161 | `		}` |
|  ! 0 |  162 | `	}` |
|  117 |  163 | `	zClass = DomClassOfKind((int)pNode->type);` |
|  117 |  164 | `	pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|  117 |  165 | `	pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|  117 |  166 | `	pRes = pObj ? DomNewRes(&(*pVm),pShell,pNode) : 0;` |
|  117 |  167 | `	if( pRes == 0 ){` |
|  ! 0 |  168 | `		if( pObj ){` |
|  ! 0 |  169 | `			PH7_ClassInstanceUnref(pObj);` |
|  ! 0 |  170 | `		}` |
|  ! 0 |  171 | `		PH7_MemObjRelease(&sKey);` |
|  ! 0 |  172 | `		return 0;` |
|    - |  173 | `	}` |
|  117 |  174 | `	DomSetRes(&(*pVm),pObj,pRes);` |
|  117 |  175 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|  117 |  176 | `	PH7_MemObjInit(&(*pVm),&sVal);` |
|  117 |  177 | `	sVal.x.pOther = pObj;` |
|  117 |  178 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|  117 |  179 | `	PH7_HashmapInsert(pCache,&sKey,&sVal);   /* takes the cache's reference */` |
|  117 |  180 | `	PH7_MemObjRelease(&sKey);` |
|  117 |  181 | `	PH7_ClassInstanceUnref(pObj);            /* ...and the cache is now the owner */` |
|  117 |  182 | `	return pObj;` |
|  121 |  183 | `}` |
|    - |  184 | `/* Answer a borrowed instance (or NULL) from a native method. */` |
|  166 |  185 | `static int DomResultWrap(ph7_context *pCtx,ph7_class_instance *pObj)` |
|    1 |  186 | `{` |
|    - |  187 | `	ph7_value sRes;` |
|  167 |  188 | `	if( pObj == 0 ){` |
|    7 |  189 | `		ph7_result_null(pCtx);` |
|    7 |  190 | `		return PH7_OK;` |
|    - |  191 | `	}` |
|  161 |  192 | `	PH7_MemObjInit(pCtx->pVm,&sRes);` |
|  161 |  193 | `	sRes.x.pOther = pObj;` |
|  161 |  194 | `	sRes.iFlags = MEMOBJ_OBJ;` |
|  161 |  195 | `	ph7_result_value(pCtx,&sRes);   /* takes its own reference */` |
|  161 |  196 | `	return PH7_OK;` |
|   84 |  197 | `}` |
|    - |  198 | `/* The common tail: wrap a node of the RECEIVER's document and answer it. */` |
|  126 |  199 | `static int DomResultNodeOf(ph7_context *pCtx,phl_domnode *pNd,xmlNodePtr pNode)` |
|    1 |  200 | `{` |
|  127 |  201 | `	if( pNd == 0 \|\| pNode == 0 ){` |
|    3 |  202 | `		ph7_result_null(pCtx);` |
|    3 |  203 | `		return PH7_OK;` |
|    - |  204 | `	}` |
|  125 |  205 | `	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNode));` |
|   64 |  206 | `}` |
|    - |  207 | `/* The phl_domnode behind a DOMNode-typed ARGUMENT (already screened by ZPP). */` |
|   74 |  208 | `static phl_domnode * DomObjArg(ph7_value *pVal)` |
|    1 |  209 | `{` |
|   75 |  210 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_OBJ) == 0 ){` |
|  ! 0 |  211 | `		return 0;` |
|    - |  212 | `	}` |
|   75 |  213 | `	return DomResOf((ph7_class_instance *)pVal->x.pOther);` |
|   38 |  214 | `}` |
|    - |  215 | `/* Orphan bookkeeping: nodes not linked into their tree but still owned */` |
|   28 |  216 | `static void DomOrphanAdd(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|    1 |  217 | `{` |
|   29 |  218 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|    - |  219 | `	sxu32 n;` |
|   59 |  220 | `	for( n = 0 ; n < SySetUsed(&pShell->aOrphans) ; ++n ){` |
|   31 |  221 | `		if( apOrphan[n] == pNode ){` |
|  ! 0 |  222 | `			return;` |
|    - |  223 | `		}` |
|   16 |  224 | `	}` |
|   29 |  225 | `	SySetPut(&pShell->aOrphans,(const void *)&pNode);` |
|   15 |  226 | `}` |
|   12 |  227 | `static void DomOrphanRemove(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|    1 |  228 | `{` |
|   13 |  229 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|   13 |  230 | `	sxu32 n,nUsed = SySetUsed(&pShell->aOrphans);` |
|   29 |  231 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|   29 |  232 | `		if( apOrphan[n] == pNode ){` |
|   13 |  233 | `			apOrphan[n] = apOrphan[nUsed-1];` |
|   13 |  234 | `			SySetTruncate(&pShell->aOrphans,nUsed-1);` |
|   13 |  235 | `			return;` |
|    - |  236 | `		}` |
|    9 |  237 | `	}` |
|    7 |  238 | `}` |
|    - |  239 | `/* Detach a node from wherever it is (tree or orphan set) prior to linking */` |
|   14 |  240 | `static void DomDetach(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|    1 |  241 | `{` |
|   15 |  242 | `	if( pNode->parent ){` |
|    3 |  243 | `		xmlUnlinkNode(pNode);` |
|    2 |  244 | `	}else{` |
|   13 |  245 | `		DomOrphanRemove(pShell,pNode);` |
|    - |  246 | `	}` |
|   15 |  247 | `}` |
|    - |  248 | `/* Raw child-list splicing (no text-node merging -- DOM/php semantics) */` |
|   10 |  249 | `static void DomLinkLast(xmlNodePtr pParent,xmlNodePtr pChild)` |
|    1 |  250 | `{` |
|   11 |  251 | `	pChild->parent = pParent;` |
|   11 |  252 | `	pChild->next = 0;` |
|   11 |  253 | `	if( pParent->last ){` |
|    9 |  254 | `		pParent->last->next = pChild;` |
|    9 |  255 | `		pChild->prev = pParent->last;` |
|    5 |  256 | `	}else{` |
|    3 |  257 | `		pParent->children = pChild;` |
|    3 |  258 | `		pChild->prev = 0;` |
|    - |  259 | `	}` |
|   11 |  260 | `	pParent->last = pChild;` |
|   11 |  261 | `}` |
|    4 |  262 | `static void DomLinkBefore(xmlNodePtr pParent,xmlNodePtr pChild,xmlNodePtr pRef)` |
|    1 |  263 | `{` |
|    5 |  264 | `	pChild->parent = pParent;` |
|    5 |  265 | `	pChild->next = pRef;` |
|    5 |  266 | `	pChild->prev = pRef->prev;` |
|    5 |  267 | `	if( pRef->prev ){` |
|    3 |  268 | `		pRef->prev->next = pChild;` |
|    2 |  269 | `	}else{` |
|    3 |  270 | `		pParent->children = pChild;` |
|    - |  271 | `	}` |
|    5 |  272 | `	pRef->prev = pChild;` |
|    5 |  273 | `}` |
|    - |  274 |  |
|    - |  275 | `/* ===== Node introspection: the readers behind __get ===== */` |
|    - |  276 |  |
|    - |  277 | `/* php's nodeName rules */` |
|   26 |  278 | `static void DomNodeName(ph7_context *pCtx,xmlNodePtr pNode)` |
|    1 |  279 | `{` |
|   27 |  280 | `	if( pNode == 0 ){` |
|  ! 0 |  281 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  282 | `		return;` |
|    - |  283 | `	}` |
|   27 |  284 | `	switch( pNode->type ){` |
|    7 |  285 | `	case XML_TEXT_NODE:          ph7_result_string(pCtx,"#text",(int)sizeof("#text")-1); break;` |
|    3 |  286 | `	case XML_CDATA_SECTION_NODE: ph7_result_string(pCtx,"#cdata-section",(int)sizeof("#cdata-section")-1); break;` |
|    3 |  287 | `	case XML_COMMENT_NODE:       ph7_result_string(pCtx,"#comment",(int)sizeof("#comment")-1); break;` |
|    1 |  288 | `	case XML_HTML_DOCUMENT_NODE:` |
|    3 |  289 | `	case XML_DOCUMENT_NODE:      ph7_result_string(pCtx,"#document",(int)sizeof("#document")-1); break;` |
|  ! 0 |  290 | `	case XML_DOCUMENT_FRAG_NODE: ph7_result_string(pCtx,"#document-fragment",(int)sizeof("#document-fragment")-1); break;` |
|    7 |  291 | `	default:` |
|   14 |  292 | `		if( (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE)` |
|   15 |  293 | `			&& pNode->ns && pNode->ns->prefix ){` |
|  ! 0 |  294 | `			ph7_result_string_format(pCtx,"%s:%s",(const char *)pNode->ns->prefix,(const char *)pNode->name);` |
|  ! 0 |  295 | `		}else{` |
|   15 |  296 | `			ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|    - |  297 | `		}` |
|   14 |  298 | `		break;` |
|    - |  299 | `	}` |
|   14 |  300 | `}` |
|    - |  301 | `/* php's nodeValue: NULL for a document, the text content otherwise */` |
|   30 |  302 | `static void DomNodeValue(ph7_context *pCtx,xmlNodePtr pNode)` |
|    1 |  303 | `{` |
|    - |  304 | `	xmlChar *zContent;` |
|   30 |  305 | `	if( pNode == 0 \|\| pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE` |
|   29 |  306 | `		\|\| pNode->type == XML_DOCUMENT_TYPE_NODE ){` |
|    3 |  307 | `		ph7_result_null(pCtx);` |
|    3 |  308 | `		return;` |
|    - |  309 | `	}` |
|   29 |  310 | `	zContent = xmlNodeGetContent(pNode);` |
|   29 |  311 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|   29 |  312 | `	if( zContent ){` |
|   29 |  313 | `		xmlFree(zContent);` |
|   14 |  314 | `	}` |
|   16 |  315 | `}` |
|    - |  316 | `/* php's textContent: the same walk, but a document answers its text too */` |
|    6 |  317 | `static void DomTextContent(ph7_context *pCtx,xmlNodePtr pNode)` |
|    1 |  318 | `{` |
|    7 |  319 | `	xmlChar *zContent = pNode ? xmlNodeGetContent(pNode) : 0;` |
|    7 |  320 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|    7 |  321 | `	if( zContent ){` |
|    7 |  322 | `		xmlFree(zContent);` |
|    3 |  323 | `	}` |
|    7 |  324 | `}` |
|    - |  325 | `/* The two child counts childNodes->length and childElementCount read. */` |
|    6 |  326 | `static int DomChildCount(xmlNodePtr pNode,int bElementsOnly)` |
|    1 |  327 | `{` |
|    7 |  328 | `	xmlNodePtr pChild = pNode ? pNode->children : 0;` |
|    7 |  329 | `	int iCount = 0;` |
|   29 |  330 | `	for( ; pChild ; pChild = pChild->next ){` |
|   23 |  331 | `		if( !bElementsOnly \|\| pChild->type == XML_ELEMENT_NODE ){` |
|   17 |  332 | `			iCount++;` |
|    8 |  333 | `		}` |
|   12 |  334 | `	}` |
|    7 |  335 | `	return iCount;` |
|    1 |  336 | `}` |
|   18 |  337 | `static xmlNodePtr DomChildAt(xmlNodePtr pNode,int iWant)` |
|    1 |  338 | `{` |
|   19 |  339 | `	xmlNodePtr pChild = pNode ? pNode->children : 0;` |
|   51 |  340 | `	for( ; pChild && iWant > 0 ; pChild = pChild->next ){` |
|   33 |  341 | `		iWant--;` |
|   17 |  342 | `	}` |
|   19 |  343 | `	return pChild;` |
|    1 |  344 | `}` |
|    - |  345 |  |
|    - |  346 | `/* ===== Tree surgery: DOMNode's four mutators ===== */` |
|    - |  347 |  |
|    - |  348 | `/*` |
|    - |  349 | ` * php's refusal taxonomy for linking pChild under pParent, or NULL when the` |
|    - |  350 | ` * link is allowed. The chunk collapsed all of it into one message per method,` |
|    - |  351 | ` * which cost more than a wording: nothing rejected making a node its own` |
|    - |  352 | `` * DESCENDANT, so `$a->firstChild->appendChild($a)` spliced a CYCLE into the`` |
|    - |  353 | ` * tree and every later walk of it ran away.` |
|    - |  354 | ` */` |
|   28 |  355 | `static const char * DomLinkRefusal(xmlNodePtr pParent,xmlNodePtr pChild)` |
|    1 |  356 | `{` |
|    - |  357 | `	xmlNodePtr p;` |
|   29 |  358 | `	if( pParent->doc != pChild->doc ){` |
|    5 |  359 | `		return "Wrong Document Error";` |
|    - |  360 | `	}` |
|    - |  361 | `	/* Walking UP from the parent also catches pChild == pParent. */` |
|   77 |  362 | `	for( p = pParent ; p ; p = p->parent ){` |
|   59 |  363 | `		if( p == pChild ){` |
|    7 |  364 | `			return "Hierarchy Request Error";` |
|    - |  365 | `		}` |
|   27 |  366 | `	}` |
|   19 |  367 | `	return 0;` |
|   15 |  368 | `}` |
|    - |  369 | `/*` |
|    - |  370 | ` * DOMNode::appendChild(DOMNode $node): DOMNode` |
|    - |  371 | ` *` |
|    - |  372 | `` * Note the argument reaches C already screened -- `$n->appendChild(1)` is a`` |
|    - |  373 | ``  * TypeError from the declared `DOMNode $node`, where the chunk read `->__res` `` |
|    - |  374 | ` * off an int and warned.` |
|    - |  375 | ` */` |
|   16 |  376 | `DOM_METHOD(vm_builtin_DOMNode_appendChild)` |
|    1 |  377 | `{` |
|   17 |  378 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|   17 |  379 | `	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|    - |  380 | `	const char *zErr;` |
|   17 |  381 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|  ! 0 |  382 | `		return PH7_VmThrowException(pCtx,"DOMException","Wrong Document Error");` |
|    - |  383 | `	}` |
|   17 |  384 | `	zErr = DomLinkRefusal((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|   17 |  385 | `	if( zErr ){` |
|    7 |  386 | `		return PH7_VmThrowException(pCtx,"DOMException","%s",zErr);` |
|    - |  387 | `	}` |
|   11 |  388 | `	DomDetach(pChd->pShell,(xmlNodePtr)pChd->pNode);` |
|   11 |  389 | `	DomLinkLast((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);` |
|   11 |  390 | `	ph7_result_value(pCtx,apArg[0]);` |
|   11 |  391 | `	return PH7_OK;` |
|    9 |  392 | `}` |
|    - |  393 | `/* DOMNode::insertBefore(DOMNode $node, ?DOMNode $child = null): DOMNode --` |
|    - |  394 | ` * a reference node that is not a child of the receiver is Not Found. */` |
|    6 |  395 | `DOM_METHOD(vm_builtin_DOMNode_insertBefore)` |
|    1 |  396 | `{` |
|    7 |  397 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    7 |  398 | `	phl_domnode *pNew = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|    7 |  399 | `	phl_domnode *pRef = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;` |
|    - |  400 | `	xmlNodePtr pParent,pChild,pAnchor;` |
|    - |  401 | `	const char *zErr;` |
|    7 |  402 | `	if( pPar == 0 \|\| pNew == 0 ){` |
|  ! 0 |  403 | `		return PH7_VmThrowException(pCtx,"DOMException","Not Found Error");` |
|    - |  404 | `	}` |
|    7 |  405 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|    7 |  406 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|    7 |  407 | `	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;` |
|    7 |  408 | `	zErr = DomLinkRefusal(pParent,pChild);` |
|    7 |  409 | `	if( zErr == 0 && pAnchor && pAnchor->parent != pParent ){` |
|    3 |  410 | `		zErr = "Not Found Error";` |
|    1 |  411 | `	}` |
|    7 |  412 | `	if( zErr ){` |
|    5 |  413 | `		return PH7_VmThrowException(pCtx,"DOMException","%s",zErr);` |
|    - |  414 | `	}` |
|    3 |  415 | `	DomDetach(pNew->pShell,pChild);` |
|    3 |  416 | `	if( pAnchor ){` |
|    3 |  417 | `		DomLinkBefore(pParent,pChild,pAnchor);` |
|    2 |  418 | `	}else{` |
|  ! 0 |  419 | `		DomLinkLast(pParent,pChild);` |
|    - |  420 | `	}` |
|    3 |  421 | `	ph7_result_value(pCtx,apArg[0]);` |
|    3 |  422 | `	return PH7_OK;` |
|    4 |  423 | `}` |
|    - |  424 | `/* DOMNode::removeChild(DOMNode $child): DOMNode */` |
|   14 |  425 | `DOM_METHOD(vm_builtin_DOMNode_removeChild)` |
|    1 |  426 | `{` |
|   15 |  427 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|   15 |  428 | `	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|    - |  429 | `	xmlNodePtr pChild;` |
|   15 |  430 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|  ! 0 |  431 | `		return PH7_VmThrowException(pCtx,"DOMException","Not Found Error");` |
|    - |  432 | `	}` |
|   15 |  433 | `	pChild = (xmlNodePtr)pChd->pNode;` |
|   15 |  434 | `	if( pChild->parent != (xmlNodePtr)pPar->pNode ){` |
|    5 |  435 | `		return PH7_VmThrowException(pCtx,"DOMException","Not Found Error");` |
|    - |  436 | `	}` |
|   11 |  437 | `	xmlUnlinkNode(pChild);` |
|   11 |  438 | `	DomOrphanAdd(pChd->pShell,pChild);` |
|   11 |  439 | `	ph7_result_value(pCtx,apArg[0]);` |
|   11 |  440 | `	return PH7_OK;` |
|    8 |  441 | `}` |
|    - |  442 | `/* DOMNode::replaceChild(DOMNode $node, DOMNode $child): DOMNode -- answers the` |
|    - |  443 | ` * node it replaced, which is the SECOND argument. */` |
|    6 |  444 | `DOM_METHOD(vm_builtin_DOMNode_replaceChild)` |
|    1 |  445 | `{` |
|    7 |  446 | `	phl_domnode *pPar = DomThisNode(pCtx);` |
|    7 |  447 | `	phl_domnode *pNew = nArg > 1 ? DomObjArg(apArg[0]) : 0;` |
|    7 |  448 | `	phl_domnode *pOld = nArg > 1 ? DomObjArg(apArg[1]) : 0;` |
|    - |  449 | `	xmlNodePtr pParent,pChild,pVictim;` |
|    - |  450 | `	const char *zErr;` |
|    7 |  451 | `	if( pPar == 0 \|\| pNew == 0 \|\| pOld == 0 ){` |
|  ! 0 |  452 | `		return PH7_VmThrowException(pCtx,"DOMException","Not Found Error");` |
|    - |  453 | `	}` |
|    7 |  454 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|    7 |  455 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|    7 |  456 | `	pVictim = (xmlNodePtr)pOld->pNode;` |
|    7 |  457 | `	zErr = DomLinkRefusal(pParent,pChild);` |
|    7 |  458 | `	if( zErr == 0 && pVictim->parent != pParent ){` |
|    3 |  459 | `		zErr = "Not Found Error";` |
|    1 |  460 | `	}` |
|    7 |  461 | `	if( zErr ){` |
|    5 |  462 | `		return PH7_VmThrowException(pCtx,"DOMException","%s",zErr);` |
|    - |  463 | `	}` |
|    3 |  464 | `	if( pChild != pVictim ){` |
|    3 |  465 | `		DomDetach(pNew->pShell,pChild);` |
|    3 |  466 | `		DomLinkBefore(pParent,pChild,pVictim);` |
|    3 |  467 | `		xmlUnlinkNode(pVictim);` |
|    3 |  468 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|    1 |  469 | `	}` |
|    3 |  470 | `	ph7_result_value(pCtx,apArg[1]);` |
|    3 |  471 | `	return PH7_OK;` |
|    4 |  472 | `}` |
|    - |  473 | `/* DOMNode::hasChildNodes(): bool / hasAttributes(): bool / getLineNo(): int */` |
|  ! 0 |  474 | `DOM_METHOD(vm_builtin_DOMNode_hasChildNodes)` |
|  ! 0 |  475 | `{` |
|  ! 0 |  476 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|  ! 0 |  477 | `	SXUNUSED(nArg);` |
|  ! 0 |  478 | `	SXUNUSED(apArg);` |
|  ! 0 |  479 | `	ph7_result_bool(pCtx,pNd && DomChildCount((xmlNodePtr)pNd->pNode,0) > 0);` |
|  ! 0 |  480 | `	return PH7_OK;` |
|  ! 0 |  481 | `}` |
|    - |  482 | `/* The attribute list of an element (empty for anything else). */` |
|   22 |  483 | `static xmlAttrPtr DomAttrList(xmlNodePtr pNode)` |
|    1 |  484 | `{` |
|   23 |  485 | `	return (pNode && pNode->type == XML_ELEMENT_NODE) ? pNode->properties : 0;` |
|    1 |  486 | `}` |
|    6 |  487 | `static int DomAttrCount(xmlNodePtr pNode)` |
|    1 |  488 | `{` |
|    7 |  489 | `	xmlAttrPtr pAttr = DomAttrList(pNode);` |
|    7 |  490 | `	int iCount = 0;` |
|   21 |  491 | `	for( ; pAttr ; pAttr = pAttr->next ){` |
|   15 |  492 | `		iCount++;` |
|    8 |  493 | `	}` |
|    7 |  494 | `	return iCount;` |
|    1 |  495 | `}` |
|   16 |  496 | `static xmlAttrPtr DomAttrAt(xmlNodePtr pNode,int iWant)` |
|    1 |  497 | `{` |
|   17 |  498 | `	xmlAttrPtr pAttr = DomAttrList(pNode);` |
|   29 |  499 | `	for( ; pAttr && iWant > 0 ; pAttr = pAttr->next ){` |
|   13 |  500 | `		iWant--;` |
|    7 |  501 | `	}` |
|   17 |  502 | `	return pAttr;` |
|    1 |  503 | `}` |
|  ! 0 |  504 | `DOM_METHOD(vm_builtin_DOMNode_hasAttributes)` |
|  ! 0 |  505 | `{` |
|  ! 0 |  506 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|  ! 0 |  507 | `	SXUNUSED(nArg);` |
|  ! 0 |  508 | `	SXUNUSED(apArg);` |
|  ! 0 |  509 | `	ph7_result_bool(pCtx,pNd && DomAttrCount((xmlNodePtr)pNd->pNode) > 0);` |
|  ! 0 |  510 | `	return PH7_OK;` |
|  ! 0 |  511 | `}` |
|    6 |  512 | `DOM_METHOD(vm_builtin_DOMNode_getLineNo)` |
|    1 |  513 | `{` |
|    7 |  514 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    3 |  515 | `	SXUNUSED(nArg);` |
|    3 |  516 | `	SXUNUSED(apArg);` |
|    7 |  517 | `	ph7_result_int64(pCtx,pNd ? (ph7_int64)xmlGetLineNo((xmlNodePtr)pNd->pNode) : 0);` |
|    7 |  518 | `	return PH7_OK;` |
|    1 |  519 | `}` |
|    - |  520 | `/* DOMNode::isSameNode(DOMNode $otherNode): bool -- pointer identity, which is` |
|    - |  521 | ` * also the identity the wrapper cache keys on. */` |
|    2 |  522 | `DOM_METHOD(vm_builtin_DOMNode_isSameNode)` |
|    1 |  523 | `{` |
|    3 |  524 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    3 |  525 | `	phl_domnode *pOther = nArg > 0 ? DomObjArg(apArg[0]) : 0;` |
|    3 |  526 | `	ph7_result_bool(pCtx,pNd != 0 && pOther != 0 && pNd->pNode == pOther->pNode);` |
|    3 |  527 | `	return PH7_OK;` |
|    1 |  528 | `}` |
|    - |  529 |  |
|    - |  530 | `/* ===== Element attributes ===== */` |
|    - |  531 |  |
|    - |  532 | `/* DOMElement::getAttribute(string $qualifiedName): string -- "" when absent */` |
|   20 |  533 | `DOM_METHOD(vm_builtin_DOMElement_getAttribute)` |
|    1 |  534 | `{` |
|   21 |  535 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   21 |  536 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|   21 |  537 | `	xmlChar *zVal = pNd ? xmlGetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;` |
|   21 |  538 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|   21 |  539 | `	if( zVal ){` |
|   19 |  540 | `		xmlFree(zVal);` |
|    9 |  541 | `	}` |
|   21 |  542 | `	return PH7_OK;` |
|    1 |  543 | `}` |
|    - |  544 | `/* DOMElement::hasAttribute(string $qualifiedName): bool */` |
|    4 |  545 | `DOM_METHOD(vm_builtin_DOMElement_hasAttribute)` |
|    1 |  546 | `{` |
|    5 |  547 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    5 |  548 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    5 |  549 | `	ph7_result_bool(pCtx,pNd && xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) != 0);` |
|    5 |  550 | `	return PH7_OK;` |
|    1 |  551 | `}` |
|    - |  552 | `/* DOMElement::setAttribute(string $qualifiedName, string $value): DOMAttr -- php` |
|    - |  553 | ` * answers the attribute NODE it wrote, so the write is followed by a wrap. */` |
|    4 |  554 | `DOM_METHOD(vm_builtin_DOMElement_setAttribute)` |
|    1 |  555 | `{` |
|    5 |  556 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    5 |  557 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";` |
|    5 |  558 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    - |  559 | `	xmlAttrPtr pAttr;` |
|    5 |  560 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|  ! 0 |  561 | `		return PH7_VmThrowException(pCtx,"DOMException","Invalid Character Error");` |
|    - |  562 | `	}` |
|    5 |  563 | `	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);` |
|    5 |  564 | `	pAttr = xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName);` |
|    5 |  565 | `	if( pAttr == 0 \|\| pAttr->type != XML_ATTRIBUTE_NODE ){` |
|  ! 0 |  566 | `		ph7_result_null(pCtx);` |
|  ! 0 |  567 | `		return PH7_OK;` |
|    - |  568 | `	}` |
|    5 |  569 | `	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);` |
|    3 |  570 | `}` |
|    - |  571 | `/* DOMElement::removeAttribute(string $qualifiedName): bool */` |
|    4 |  572 | `DOM_METHOD(vm_builtin_DOMElement_removeAttribute)` |
|    1 |  573 | `{` |
|    5 |  574 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    5 |  575 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    5 |  576 | `	xmlAttrPtr pAttr = pNd ? xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;` |
|    5 |  577 | `	if( pAttr == 0 \|\| pAttr->type != XML_ATTRIBUTE_NODE ){` |
|    - |  578 | `		/* Absent (or a DTD default): php returns false */` |
|    3 |  579 | `		ph7_result_bool(pCtx,0);` |
|    3 |  580 | `		return PH7_OK;` |
|    - |  581 | `	}` |
|    3 |  582 | `	xmlRemoveProp(pAttr);` |
|    3 |  583 | `	ph7_result_bool(pCtx,1);` |
|    3 |  584 | `	return PH7_OK;` |
|    3 |  585 | `}` |
|    - |  586 | `/* DOMElement::getAttributeNS(?string $namespace, string $localName): string */` |
|    4 |  587 | `DOM_METHOD(vm_builtin_DOMElement_getAttributeNS)` |
|    1 |  588 | `{` |
|    5 |  589 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    5 |  590 | `	const char *zUri = (nArg > 1 && !ph7_value_is_null(apArg[0])) ? ph7_value_to_string(apArg[0],0) : "";` |
|    5 |  591 | `	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    5 |  592 | `	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,(const xmlChar *)zUri) : 0;` |
|    5 |  593 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|    5 |  594 | `	if( zVal ){` |
|    3 |  595 | `		xmlFree(zVal);` |
|    1 |  596 | `	}` |
|    5 |  597 | `	return PH7_OK;` |
|    1 |  598 | `}` |
|    - |  599 | `/* DOMElement::setAttributeNS(?string $namespace, string $qualifiedName, string $value): void */` |
|    2 |  600 | `DOM_METHOD(vm_builtin_DOMElement_setAttributeNS)` |
|    1 |  601 | `{` |
|    3 |  602 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    3 |  603 | `	const char *zUri = (nArg > 2 && !ph7_value_is_null(apArg[0])) ? ph7_value_to_string(apArg[0],0) : "";` |
|    3 |  604 | `	const char *zQname = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|    3 |  605 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|    3 |  606 | `	sxu32 nColon = 0;` |
|    - |  607 | `	xmlNodePtr pNode;` |
|    - |  608 | `	xmlNsPtr pNs;` |
|    3 |  609 | `	if( pNd == 0 \|\| zQname[0] == 0 ){` |
|  ! 0 |  610 | `		return PH7_VmThrowException(pCtx,"DOMException","Namespace Error");` |
|    - |  611 | `	}` |
|    3 |  612 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|    3 |  613 | `	if( SyByteFind(zQname,SyStrlen(zQname),':',&nColon) == SXRET_OK ){` |
|    - |  614 | `		/* Prefixed: find (or declare on this element) the namespace */` |
|    - |  615 | `		char zPrefix[128];` |
|    3 |  616 | `		if( nColon >= sizeof(zPrefix) ){` |
|  ! 0 |  617 | `			return PH7_VmThrowException(pCtx,"DOMException","Namespace Error");` |
|    - |  618 | `		}` |
|    3 |  619 | `		SyMemcpy(zQname,zPrefix,nColon);` |
|    3 |  620 | `		zPrefix[nColon] = 0;` |
|    3 |  621 | `		pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);` |
|    3 |  622 | `		if( pNs == 0 ){` |
|    3 |  623 | `			pNs = xmlNewNs(pNode,(const xmlChar *)zUri,(const xmlChar *)zPrefix);` |
|    1 |  624 | `		}` |
|    3 |  625 | `		if( pNs == 0 ){` |
|  ! 0 |  626 | `			return PH7_VmThrowException(pCtx,"DOMException","Namespace Error");` |
|    - |  627 | `		}` |
|    3 |  628 | `		xmlSetNsProp(pNode,pNs,(const xmlChar *)(zQname+nColon+1),(const xmlChar *)zVal);` |
|    2 |  629 | `	}else{` |
|  ! 0 |  630 | `		pNs = zUri[0] ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;` |
|  ! 0 |  631 | `		xmlSetNsProp(pNode,pNs,(const xmlChar *)zQname,(const xmlChar *)zVal);` |
|    - |  632 | `	}` |
|    3 |  633 | `	return PH7_OK;` |
|    2 |  634 | `}` |
|    - |  635 |  |
|    - |  636 | `/* ===== getElementsByTagName (live) ===== */` |
|    - |  637 |  |
|    - |  638 | `/* Document-order successor within pRoot's subtree (pRoot excluded) */` |
|  144 |  639 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot)` |
|    1 |  640 | `{` |
|  145 |  641 | `	if( pCur->children ){` |
|   69 |  642 | `		return pCur->children;` |
|    - |  643 | `	}` |
|  123 |  644 | `	while( pCur && pCur != pRoot ){` |
|   99 |  645 | `		if( pCur->next ){` |
|   53 |  646 | `			return pCur->next;` |
|    - |  647 | `		}` |
|   47 |  648 | `		pCur = pCur->parent;` |
|    1 |  649 | `	}` |
|   25 |  650 | `	return 0;` |
|   73 |  651 | `}` |
|    - |  652 | `/* Length-carrying: the name comes from a declared string SLOT, whose bytes are` |
|    - |  653 | ` * NOT NUL-terminated (PH7_NativeAttrStr borrows the blob as-is). */` |
|  184 |  654 | `static int DomGebtnMatch(xmlNodePtr pNode,const char *zName,int nName)` |
|    1 |  655 | `{` |
|  185 |  656 | `	if( pNode->type != XML_ELEMENT_NODE ){` |
|  ! 0 |  657 | `		return 0;` |
|    - |  658 | `	}` |
|  185 |  659 | `	if( nName == 1 && zName[0] == '*' ){` |
|    3 |  660 | `		return 1;` |
|    - |  661 | `	}` |
|  183 |  662 | `	if( pNode->name == 0 ){` |
|  ! 0 |  663 | `		return 0;` |
|    - |  664 | `	}` |
|  263 |  665 | `	return (int)SyStrlen((const char *)pNode->name) == nName` |
|  182 |  666 | `		&& SyMemcmp((const void *)pNode->name,(const void *)zName,(sxu32)nName) == 0;` |
|   93 |  667 | `}` |
|    - |  668 | `/* The list is LIVE: nothing is snapshotted, both queries re-walk the subtree` |
|    - |  669 | ` * every time DOMNodeList asks. Passing iWant < 0 counts instead of indexing. */` |
|   64 |  670 | `static xmlNodePtr DomGebtnWalk(xmlNodePtr pRoot,const char *zName,int nName,int iWant,int *pnCount)` |
|    1 |  671 | `{` |
|   65 |  672 | `	xmlNodePtr pCur = pRoot ? pRoot->children : 0;` |
|   65 |  673 | `	int iCount = 0;` |
|  209 |  674 | `	while( pCur ){` |
|  185 |  675 | `		if( DomGebtnMatch(pCur,zName,nName) ){` |
|  107 |  676 | `			if( iWant >= 0 && iCount == iWant ){` |
|   41 |  677 | `				return pCur;` |
|    - |  678 | `			}` |
|   67 |  679 | `			iCount++;` |
|   33 |  680 | `		}` |
|  145 |  681 | `		pCur = DomWalkNext(pCur,pRoot);` |
|    1 |  682 | `	}` |
|   25 |  683 | `	if( pnCount ){` |
|   17 |  684 | `		*pnCount = iCount;` |
|    8 |  685 | `	}` |
|   25 |  686 | `	return 0;` |
|   33 |  687 | `}` |
|    - |  688 |  |
|    - |  689 | `/* ===== DOMDocument ===== */` |
|    - |  690 |  |
|    - |  691 | `/*` |
|    - |  692 | ` * DOMDocument::__construct(string $version = '1.0', string $encoding = '')` |
|    - |  693 | ` *` |
|    - |  694 | `` * The chunk reached the document through `parent::__construct(__dom_doc_new(..))`;`` |
|    - |  695 | ` * a native constructor writes its own two slots, and $__doc is the document` |
|    - |  696 | ` * ITSELF (php's ownerDocument is null on a document, which __get answers).` |
|    - |  697 | ` */` |
|   62 |  698 | `DOM_METHOD(vm_builtin_DOMDocument_construct)` |
|    1 |  699 | `{` |
|   63 |  700 | `	ph7_vm *pVm = pCtx->pVm;` |
|   63 |  701 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   63 |  702 | `	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";` |
|   63 |  703 | `	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|    - |  704 | `	xmlDocPtr pDoc;` |
|    - |  705 | `	phl_xmldoc *pShell;` |
|    - |  706 | `	phl_domnode *pRes;` |
|   63 |  707 | `	if( pThis == 0 ){` |
|  ! 0 |  708 | `		return PH7_OK;` |
|    - |  709 | `	}` |
|   63 |  710 | `	pDoc = xmlNewDoc((const xmlChar *)(zVersion[0] ? zVersion : "1.0"));` |
|   63 |  711 | `	if( pDoc == 0 ){` |
|  ! 0 |  712 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  713 | `	}` |
|   63 |  714 | `	if( zEncoding[0] ){` |
|  ! 0 |  715 | `		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);` |
|  ! 0 |  716 | `	}` |
|   63 |  717 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|   63 |  718 | `	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|   63 |  719 | `	if( pRes == 0 ){` |
|  ! 0 |  720 | `		if( pShell == 0 ){` |
|  ! 0 |  721 | `			xmlFreeDoc(pDoc);` |
|  ! 0 |  722 | `		}` |
|  ! 0 |  723 | `		return PH7_ContextMemoryError(pCtx);` |
|    - |  724 | `	}` |
|   63 |  725 | `	DomSetRes(pVm,pThis,pRes);` |
|   63 |  726 | `	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);` |
|   63 |  727 | `	return PH7_OK;` |
|   32 |  728 | `}` |
|    - |  729 | `/* DOMDocument::loadXML(string $source, int $options = 0): bool -- the receiver` |
|    - |  730 | ` * is REPOINTED at a new tree, so its identity cache is dropped with it. */` |
|   60 |  731 | `DOM_METHOD(vm_builtin_DOMDocument_loadXML)` |
|    1 |  732 | `{` |
|   61 |  733 | `	ph7_vm *pVm = pCtx->pVm;` |
|   61 |  734 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   61 |  735 | `	int nLen = 0;` |
|   61 |  736 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";` |
|   61 |  737 | `	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|    - |  738 | `	xmlDocPtr pDoc;` |
|    - |  739 | `	phl_xmldoc *pShell;` |
|    - |  740 | `	phl_domnode *pRes;` |
|    - |  741 | `	ph7_value *pNodes;` |
|    - |  742 | `	sxu32 nMark;` |
|   61 |  743 | `	if( pThis == 0 ){` |
|  ! 0 |  744 | `		return PH7_OK;` |
|    - |  745 | `	}` |
|   61 |  746 | `	if( nLen < 1 ){` |
|    3 |  747 | `		return PH7_VmThrowException(pCtx,"ValueError",` |
|    - |  748 | `			"DOMDocument::loadXML(): Argument #1 ($source) must not be empty");` |
|    - |  749 | `	}` |
|   59 |  750 | `	if( !PH7_NativeAttrTruthy(pThis,"preserveWhiteSpace") ){` |
|   15 |  751 | `		iOpts \|= XML_PARSE_NOBLANKS;` |
|    7 |  752 | `	}` |
|   59 |  753 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   59 |  754 | `	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);` |
|   59 |  755 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::loadXML");` |
|   59 |  756 | `	pShell = pDoc ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;` |
|   59 |  757 | `	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;` |
|   59 |  758 | `	if( pRes == 0 ){` |
|    9 |  759 | `		if( pDoc && pShell == 0 ){` |
|  ! 0 |  760 | `			xmlFreeDoc(pDoc);` |
|  ! 0 |  761 | `		}` |
|    9 |  762 | `		ph7_result_bool(pCtx,0);` |
|    9 |  763 | `		return PH7_OK;` |
|    - |  764 | `	}` |
|   51 |  765 | `	DomSetRes(pVm,pThis,pRes);` |
|   51 |  766 | `	pNodes = PH7_NativeAttr(pThis,DOM_NODES);` |
|   51 |  767 | `	if( pNodes ){` |
|    - |  768 | `		/* Every wrapper into the OLD tree is stale: start a fresh cache. */` |
|   51 |  769 | `		PH7_MemObjRelease(pNodes);` |
|   51 |  770 | `		PH7_MemObjToHashmap(pNodes);` |
|   25 |  771 | `	}` |
|   51 |  772 | `	ph7_result_bool(pCtx,1);` |
|   51 |  773 | `	return PH7_OK;` |
|   31 |  774 | `}` |
|    - |  775 | `/* DOMDocument::saveXML(?DOMNode $node = null, int $options = 0): string\|false */` |
|   24 |  776 | `DOM_METHOD(vm_builtin_DOMDocument_saveXML)` |
|    1 |  777 | `{` |
|   25 |  778 | `	ph7_vm *pVm = pCtx->pVm;` |
|   25 |  779 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   25 |  780 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|   25 |  781 | `	phl_domnode *pTgt = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;` |
|   25 |  782 | `	int bFormat = pThis && PH7_NativeAttrTruthy(pThis,"formatOutput");` |
|    - |  783 | `	xmlDocPtr pDoc;` |
|    - |  784 | `	sxu32 nMark;` |
|   25 |  785 | `	if( pDocNd == 0 ){` |
|  ! 0 |  786 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 |  787 | `		return PH7_OK;` |
|    - |  788 | `	}` |
|   25 |  789 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|   25 |  790 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   32 |  791 | `	if( pTgt == 0 \|\| pTgt->pNode == pDocNd->pNode ){` |
|   15 |  792 | `		xmlChar *zOut = 0;` |
|   15 |  793 | `		int nOut = 0;` |
|   15 |  794 | `		xmlDocDumpFormatMemory(pDoc,&zOut,&nOut,bFormat ? 1 : 0);` |
|   15 |  795 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|   15 |  796 | `		if( zOut == 0 ){` |
|  ! 0 |  797 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  798 | `			return PH7_OK;` |
|    - |  799 | `		}` |
|   15 |  800 | `		ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|   15 |  801 | `		xmlFree(zOut);` |
|    8 |  802 | `	}else{` |
|   11 |  803 | `		xmlBufferPtr pBuf = xmlBufferCreate();` |
|    - |  804 | `		int rc;` |
|   11 |  805 | `		if( pBuf == 0 ){` |
|  ! 0 |  806 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|  ! 0 |  807 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  808 | `			return PH7_OK;` |
|    - |  809 | `		}` |
|   11 |  810 | `		rc = xmlNodeDump(pBuf,pDoc,(xmlNodePtr)pTgt->pNode,0,bFormat ? 1 : 0);` |
|   11 |  811 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|   11 |  812 | `		if( rc < 0 ){` |
|  ! 0 |  813 | `			xmlBufferFree(pBuf);` |
|  ! 0 |  814 | `			ph7_result_bool(pCtx,0);` |
|  ! 0 |  815 | `			return PH7_OK;` |
|    - |  816 | `		}` |
|   11 |  817 | `		ph7_result_string(pCtx,(const char *)xmlBufferContent(pBuf),(int)xmlBufferLength(pBuf));` |
|   11 |  818 | `		xmlBufferFree(pBuf);` |
|    - |  819 | `	}` |
|   25 |  820 | `	return PH7_OK;` |
|   13 |  821 | `}` |
|    - |  822 | `/*` |
|    - |  823 | ` * The four DOMDocument::create* methods, which differ only in the node kind` |
|    - |  824 | ` * they ask libxml for. Fresh nodes start as orphans, so a node that is created` |
|    - |  825 | ` * and never appended is still freed with its document.` |
|    - |  826 | ` */` |
|   16 |  827 | `static int DomDocCreate(ph7_context *pCtx,int iKind,const char *zName,const char *zVal,int nVal)` |
|    1 |  828 | `{` |
|   17 |  829 | `	ph7_vm *pVm = pCtx->pVm;` |
|   17 |  830 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|    - |  831 | `	xmlDocPtr pDoc;` |
|   17 |  832 | `	xmlNodePtr pNode = 0;` |
|    - |  833 | `	sxu32 nMark;` |
|   17 |  834 | `	if( pDocNd == 0 ){` |
|  ! 0 |  835 | `		return PH7_VmThrowException(pCtx,"DOMException","Invalid Character Error");` |
|    - |  836 | `	}` |
|   17 |  837 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|   17 |  838 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   17 |  839 | `	switch( iKind ){` |
|    5 |  840 | `	case XML_ELEMENT_NODE:` |
|   11 |  841 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|  ! 0 |  842 | `			break; /* Invalid Character Error */` |
|    - |  843 | `		}` |
|    - |  844 | `		/* php passes the value through xmlNewDocNode, which entity-parses` |
|    - |  845 | `		 * it (quirk preserved: bad entities warn and drop the content). */` |
|   11 |  846 | `		pNode = xmlNewDocNode(pDoc,0,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|   11 |  847 | `		break;` |
|    1 |  848 | `	case XML_TEXT_NODE:` |
|    3 |  849 | `		pNode = xmlNewDocText(pDoc,(const xmlChar *)zVal);` |
|    3 |  850 | `		break;` |
|    1 |  851 | `	case XML_CDATA_SECTION_NODE:` |
|    3 |  852 | `		pNode = xmlNewCDataBlock(pDoc,(const xmlChar *)zVal,nVal);` |
|    3 |  853 | `		break;` |
|    1 |  854 | `	case XML_COMMENT_NODE:` |
|    3 |  855 | `		pNode = xmlNewDocComment(pDoc,(const xmlChar *)zVal);` |
|    2 |  856 | `		break;` |
|    - |  857 | `	}` |
|   25 |  858 | `	PH7_LibxmlCaptureEnd(pVm,nMark,` |
|    8 |  859 | `		iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");` |
|   17 |  860 | `	if( pNode == 0 ){` |
|  ! 0 |  861 | `		if( iKind == XML_ELEMENT_NODE ){` |
|    - |  862 | `			/* Only createElement can be handed a name libxml refuses. */` |
|  ! 0 |  863 | `			return PH7_VmThrowException(pCtx,"DOMException","Invalid Character Error");` |
|    - |  864 | `		}` |
|  ! 0 |  865 | `		ph7_result_null(pCtx);` |
|  ! 0 |  866 | `		return PH7_OK;` |
|    - |  867 | `	}` |
|   17 |  868 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|   17 |  869 | `	return DomResultNodeOf(pCtx,pDocNd,pNode);` |
|    9 |  870 | `}` |
|    - |  871 | `/* DOMDocument::createElement(string $localName, string $value = ''): DOMElement */` |
|   10 |  872 | `DOM_METHOD(vm_builtin_DOMDocument_createElement)` |
|    1 |  873 | `{` |
|   11 |  874 | `	int nVal = 0;` |
|   11 |  875 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|   11 |  876 | `	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";` |
|   11 |  877 | `	return DomDocCreate(pCtx,XML_ELEMENT_NODE,zName,zVal,nVal);` |
|    1 |  878 | `}` |
|    - |  879 | `/* DOMDocument::createTextNode / createComment / createCDATASection(string $data) */` |
|    6 |  880 | `static int DomDocCreateData(ph7_context *pCtx,int iKind,int nArg,ph7_value **apArg)` |
|    1 |  881 | `{` |
|    7 |  882 | `	int nVal = 0;` |
|    7 |  883 | `	const char *zVal = nArg > 0 ? ph7_value_to_string(apArg[0],&nVal) : "";` |
|    7 |  884 | `	return DomDocCreate(pCtx,iKind,"",zVal,nVal);` |
|    1 |  885 | `}` |
|    2 |  886 | `DOM_METHOD(vm_builtin_DOMDocument_createTextNode)` |
|    1 |  887 | `{` |
|    3 |  888 | `	return DomDocCreateData(pCtx,XML_TEXT_NODE,nArg,apArg);` |
|    1 |  889 | `}` |
|    2 |  890 | `DOM_METHOD(vm_builtin_DOMDocument_createComment)` |
|    1 |  891 | `{` |
|    3 |  892 | `	return DomDocCreateData(pCtx,XML_COMMENT_NODE,nArg,apArg);` |
|    1 |  893 | `}` |
|    2 |  894 | `DOM_METHOD(vm_builtin_DOMDocument_createCDATASection)` |
|    1 |  895 | `{` |
|    3 |  896 | `	return DomDocCreateData(pCtx,XML_CDATA_SECTION_NODE,nArg,apArg);` |
|    1 |  897 | `}` |
|    - |  898 | `/* DOMDocument::normalizeDocument(): void -- merge adjacent text nodes.  Merged-` |
|    - |  899 | ` * away siblings are PARKED as orphans, never freed, so any PHP wrapper to` |
|    - |  900 | ` * them stays valid (they just become empty orphans). */` |
|   24 |  901 | `static void DomNormalizeTree(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|    1 |  902 | `{` |
|   25 |  903 | `	xmlNodePtr pChild = pNode->children;` |
|   49 |  904 | `	while( pChild ){` |
|   25 |  905 | `		if( pChild->type == XML_TEXT_NODE ){` |
|    7 |  906 | `			while( pChild->next && pChild->next->type == XML_TEXT_NODE ){` |
|  ! 0 |  907 | `				xmlNodePtr pNext = pChild->next;` |
|  ! 0 |  908 | `				if( pNext->content ){` |
|  ! 0 |  909 | `					xmlNodeAddContent(pChild,pNext->content);` |
|  ! 0 |  910 | `				}` |
|  ! 0 |  911 | `				xmlUnlinkNode(pNext);` |
|  ! 0 |  912 | `				DomOrphanAdd(pShell,pNext);` |
|  ! 0 |  913 | `			}` |
|   22 |  914 | `		}else if( pChild->type == XML_ELEMENT_NODE ){` |
|   19 |  915 | `			DomNormalizeTree(pShell,pChild);` |
|    9 |  916 | `		}` |
|   25 |  917 | `		pChild = pChild->next;` |
|    1 |  918 | `	}` |
|   25 |  919 | `}` |
|    6 |  920 | `DOM_METHOD(vm_builtin_DOMDocument_normalizeDocument)` |
|    1 |  921 | `{` |
|    7 |  922 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|    3 |  923 | `	SXUNUSED(nArg);` |
|    3 |  924 | `	SXUNUSED(apArg);` |
|    7 |  925 | `	if( pNd ){` |
|    7 |  926 | `		DomNormalizeTree(pNd->pShell,(xmlNodePtr)pNd->pNode);` |
|    3 |  927 | `	}` |
|    7 |  928 | `	return PH7_OK;` |
|    1 |  929 | `}` |
|    - |  930 |  |
|    - |  931 | `/* ===== C14N ===== */` |
|    - |  932 |  |
|    - |  933 | `/* Visibility callback: keep only the target's subtree (attrs/ns follow` |
|    - |  934 | ` * their owning element) -- the same shape php's ext/dom uses. */` |
|   10 |  935 | `static int DomC14NIsVisible(void *pUserData,xmlNodePtr pNode,xmlNodePtr pParent)` |
|    1 |  936 | `{` |
|   11 |  937 | `	xmlNodePtr pTarget = (xmlNodePtr)pUserData;` |
|    - |  938 | `	xmlNodePtr p;` |
|   11 |  939 | `	if( pNode->type == XML_NAMESPACE_DECL ){` |
|  ! 0 |  940 | `		p = pParent;` |
|   11 |  941 | `	}else if( pNode->type == XML_ATTRIBUTE_NODE ){` |
|    5 |  942 | `		p = pNode->parent;` |
|    3 |  943 | `	}else{` |
|    7 |  944 | `		p = pNode;` |
|    - |  945 | `	}` |
|   27 |  946 | `	while( p ){` |
|   19 |  947 | `		if( p == pTarget ){` |
|    3 |  948 | `			return 1;` |
|    - |  949 | `		}` |
|   17 |  950 | `		p = p->parent;` |
|    1 |  951 | `	}` |
|    9 |  952 | `	return 0;` |
|    6 |  953 | `}` |
|    - |  954 | `/*` |
|    - |  955 | ` * DOMNode::C14N(bool $exclusive = false, bool $withComments = false,` |
|    - |  956 | ` *               ?array $xpath = null, ?array $nsPrefixes = null): string\|false` |
|    - |  957 | ` *` |
|    - |  958 | ` * "" on canonicalization failure (php returns an empty string for` |
|    - |  959 | ` * empty/unserializable input). The four parameters are php's; PHL canonicalizes` |
|    - |  960 | ` * inclusive-with-comments only, exactly as the chunk did -- they are declared so` |
|    - |  961 | ` * the arity and types are php's, not so the body branches on them.` |
|    - |  962 | ` */` |
|   12 |  963 | `DOM_METHOD(vm_builtin_DOMNode_C14N)` |
|    1 |  964 | `{` |
|   13 |  965 | `	ph7_vm *pVm = pCtx->pVm;` |
|   13 |  966 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   13 |  967 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    - |  968 | `	sxu32 nMark;` |
|    6 |  969 | `	SXUNUSED(nArg);` |
|    6 |  970 | `	SXUNUSED(apArg);` |
|   13 |  971 | `	if( pNode == 0 \|\| pNode->doc == 0 ){` |
|  ! 0 |  972 | `		ph7_result_string(pCtx,"",0);` |
|  ! 0 |  973 | `		return PH7_OK;` |
|    - |  974 | `	}` |
|   13 |  975 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   18 |  976 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|   11 |  977 | `		xmlChar *zOut = 0;` |
|   11 |  978 | `		int nOut = xmlC14NDocDumpMemory((xmlDocPtr)pNode,0,XML_C14N_1_0,0,0,&zOut);` |
|   11 |  979 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::C14N");` |
|   11 |  980 | `		if( nOut < 0 \|\| zOut == 0 ){` |
|  ! 0 |  981 | `			ph7_result_string(pCtx,"",0);` |
|  ! 0 |  982 | `		}else{` |
|   11 |  983 | `			ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|    - |  984 | `		}` |
|   11 |  985 | `		if( zOut ){` |
|   11 |  986 | `			xmlFree(zOut);` |
|    5 |  987 | `		}` |
|    6 |  988 | `	}else{` |
|    3 |  989 | `		xmlOutputBufferPtr pOut = xmlAllocOutputBuffer(0);` |
|    3 |  990 | `		int rc = -1;` |
|    3 |  991 | `		if( pOut ){` |
|    3 |  992 | `			rc = xmlC14NExecute(pNode->doc,DomC14NIsVisible,pNode,XML_C14N_1_0,0,0,pOut);` |
|    1 |  993 | `		}` |
|    3 |  994 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::C14N");` |
|    3 |  995 | `		if( pOut == 0 \|\| rc < 0 ){` |
|  ! 0 |  996 | `			ph7_result_string(pCtx,"",0);` |
|  ! 0 |  997 | `		}else{` |
|    4 |  998 | `			ph7_result_string(pCtx,(const char *)xmlOutputBufferGetContent(pOut),` |
|    2 |  999 | `				(int)xmlOutputBufferGetSize(pOut));` |
|    - | 1000 | `		}` |
|    3 | 1001 | `		if( pOut ){` |
|    3 | 1002 | `			xmlOutputBufferClose(pOut);` |
|    1 | 1003 | `		}` |
|    - | 1004 | `	}` |
|   13 | 1005 | `	return PH7_OK;` |
|    7 | 1006 | `}` |
|    - | 1007 |  |
|    - | 1008 | `/* ===== DOMNodeList and DOMNamedNodeMap ===== */` |
|    - | 1009 |  |
|    - | 1010 | `/*` |
|    - | 1011 | ` * A node list is one of three things, and which one it is decides both count()` |
|    - | 1012 | ` * and item(). Two of the three are LIVE views (they re-walk the tree on every` |
|    - | 1013 | ` * question, which is what makes getElementsByTagName track mutations); the third` |
|    - | 1014 | ` * is the document-order snapshot DOMXPath::query froze.` |
|    - | 1015 | ` */` |
|    - | 1016 | `#define DNL_CHILD 0   /* $node->childNodes */` |
|    - | 1017 | `#define DNL_GEBTN 1   /* getElementsByTagName($name) */` |
|    - | 1018 | `#define DNL_SNAP  2   /* DOMXPath::query() */` |
|    - | 1019 | `#define DNL_KIND  "__kind"` |
|    - | 1020 | `#define DNL_OWNER "__owner"` |
|    - | 1021 | `#define DNL_NAME  "__name"` |
|    - | 1022 | `#define DNL_SNAP_SLOT "__snap"` |
|    - | 1023 |  |
|    - | 1024 | `/* The node a live list is a view OF. */` |
|  110 | 1025 | `static phl_domnode * DomListOwner(ph7_class_instance *pList)` |
|    1 | 1026 | `{` |
|  111 | 1027 | `	return DomResOf(PH7_NativeAttrObj(pList,DNL_OWNER));` |
|    1 | 1028 | `}` |
|   66 | 1029 | `static ph7_hashmap * DomListSnap(ph7_class_instance *pList)` |
|    1 | 1030 | `{` |
|   67 | 1031 | `	ph7_value *pVal = PH7_NativeAttr(pList,DNL_SNAP_SLOT);` |
|   67 | 1032 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|  ! 0 | 1033 | `		return 0;` |
|    - | 1034 | `	}` |
|   67 | 1035 | `	return (ph7_hashmap *)pVal->x.pOther;` |
|   34 | 1036 | `}` |
|   48 | 1037 | `static int DomListCount(ph7_class_instance *pList)` |
|    1 | 1038 | `{` |
|    - | 1039 | `	phl_domnode *pOwner;` |
|    - | 1040 | `	const char *zName;` |
|   49 | 1041 | `	int nName,iCount = 0;` |
|   49 | 1042 | `	if( pList == 0 ){` |
|  ! 0 | 1043 | `		return 0;` |
|    - | 1044 | `	}` |
|   49 | 1045 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){` |
|   29 | 1046 | `		ph7_hashmap *pMap = DomListSnap(pList);` |
|   29 | 1047 | `		return pMap ? (int)pMap->nEntry : 0;` |
|    - | 1048 | `	}` |
|   21 | 1049 | `	pOwner = DomListOwner(pList);` |
|   21 | 1050 | `	if( pOwner == 0 ){` |
|  ! 0 | 1051 | `		return 0;` |
|    - | 1052 | `	}` |
|   21 | 1053 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){` |
|    5 | 1054 | `		return DomChildCount((xmlNodePtr)pOwner->pNode,0);` |
|    - | 1055 | `	}` |
|   17 | 1056 | `	PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);` |
|   17 | 1057 | `	DomGebtnWalk((xmlNodePtr)pOwner->pNode,zName,nName,-1,&iCount);` |
|   17 | 1058 | `	return iCount;` |
|   25 | 1059 | `}` |
|    - | 1060 | `/* The wrapper at one index, or NULL past the end. BORROWED, like every wrap. */` |
|  104 | 1061 | `static ph7_class_instance * DomListItem(ph7_vm *pVm,ph7_class_instance *pList,int iIndex)` |
|    1 | 1062 | `{` |
|    - | 1063 | `	ph7_class_instance *pDoc;` |
|    - | 1064 | `	phl_domnode *pOwner;` |
|  105 | 1065 | `	xmlNodePtr pNode = 0;` |
|    - | 1066 | `	const char *zName;` |
|    - | 1067 | `	int nName;` |
|  105 | 1068 | `	if( pList == 0 \|\| iIndex < 0 ){` |
|  ! 0 | 1069 | `		return 0;` |
|    - | 1070 | `	}` |
|  105 | 1071 | `	pDoc = PH7_NativeAttrObj(pList,DOM_DOC);` |
|  105 | 1072 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){` |
|   39 | 1073 | `		ph7_hashmap *pMap = DomListSnap(pList);` |
|   39 | 1074 | `		ph7_hashmap_node *pEntry = 0;` |
|    - | 1075 | `		ph7_value sKey,*pHit;` |
|    - | 1076 | `		phl_domnode *pRes;` |
|   39 | 1077 | `		if( pMap == 0 ){` |
|  ! 0 | 1078 | `			return 0;` |
|    - | 1079 | `		}` |
|   39 | 1080 | `		PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)iIndex);` |
|   39 | 1081 | `		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){` |
|    7 | 1082 | `			pEntry = 0;` |
|    3 | 1083 | `		}` |
|   39 | 1084 | `		PH7_MemObjRelease(&sKey);` |
|   39 | 1085 | `		pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;` |
|   39 | 1086 | `		pRes = (pHit && (pHit->iFlags & MEMOBJ_RES)) ? (phl_domnode *)pHit->x.pOther : 0;` |
|   39 | 1087 | `		return pRes ? DomWrap(&(*pVm),pDoc,pRes->pShell,(xmlNodePtr)pRes->pNode) : 0;` |
|    - | 1088 | `	}` |
|   67 | 1089 | `	pOwner = DomListOwner(pList);` |
|   67 | 1090 | `	if( pOwner == 0 ){` |
|  ! 0 | 1091 | `		return 0;` |
|    - | 1092 | `	}` |
|   67 | 1093 | `	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){` |
|   19 | 1094 | `		pNode = DomChildAt((xmlNodePtr)pOwner->pNode,iIndex);` |
|   10 | 1095 | `	}else{` |
|   49 | 1096 | `		PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);` |
|   49 | 1097 | `		pNode = DomGebtnWalk((xmlNodePtr)pOwner->pNode,zName,nName,iIndex,0);` |
|    - | 1098 | `	}` |
|   67 | 1099 | `	return DomWrap(&(*pVm),pDoc,pOwner->pShell,pNode);` |
|   53 | 1100 | `}` |
|    - | 1101 | `/*` |
|    - | 1102 | ` * Build one. pOwnerObj is the node the live view is of (NULL for a snapshot),` |
|    - | 1103 | ` * pSnap the frozen list (NULL otherwise). The caller owns the reference.` |
|    - | 1104 | ` */` |
|   88 | 1105 | `static ph7_class_instance * DomNewCollection(ph7_vm *pVm,const char *zClass,` |
|    - | 1106 | `	ph7_class_instance *pDoc,int iKind,ph7_class_instance *pOwnerObj,` |
|    - | 1107 | `	const char *zName,ph7_value *pSnap)` |
|    1 | 1108 | `{` |
|   89 | 1109 | `	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);` |
|   89 | 1110 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;` |
|   89 | 1111 | `	if( pObj == 0 ){` |
|  ! 0 | 1112 | `		return 0;` |
|    - | 1113 | `	}` |
|   89 | 1114 | `	PH7_NativeSetAttrInt(&(*pVm),pObj,DNL_KIND,iKind);` |
|   89 | 1115 | `	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);` |
|   89 | 1116 | `	if( pOwnerObj ){` |
|   49 | 1117 | `		PH7_NativeSetAttrObj(&(*pVm),pObj,DNL_OWNER,pOwnerObj);` |
|   24 | 1118 | `	}` |
|   89 | 1119 | `	if( zName ){` |
|   23 | 1120 | `		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_NAME,zName,(int)SyStrlen(zName));` |
|   11 | 1121 | `	}` |
|   89 | 1122 | `	if( pSnap ){` |
|   41 | 1123 | `		ph7_value *pSlot = PH7_NativeAttr(pObj,DNL_SNAP_SLOT);` |
|   41 | 1124 | `		if( pSlot ){` |
|   41 | 1125 | `			PH7_MemObjStore(pSnap,pSlot);` |
|   20 | 1126 | `		}` |
|   20 | 1127 | `	}` |
|   89 | 1128 | `	return pObj;` |
|   45 | 1129 | `}` |
|    - | 1130 | `/* DOMNodeList::count(): int and ::item(int $index): ?DOMNode */` |
|    4 | 1131 | `DOM_METHOD(vm_builtin_DOMNodeList_count)` |
|    1 | 1132 | `{` |
|    2 | 1133 | `	SXUNUSED(nArg);` |
|    2 | 1134 | `	SXUNUSED(apArg);` |
|    5 | 1135 | `	ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));` |
|    5 | 1136 | `	return PH7_OK;` |
|    1 | 1137 | `}` |
|   30 | 1138 | `DOM_METHOD(vm_builtin_DOMNodeList_item)` |
|    1 | 1139 | `{` |
|   31 | 1140 | `	int iIndex = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|   31 | 1141 | `	return DomResultWrap(pCtx,DomListItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));` |
|    1 | 1142 | `}` |
|    - | 1143 | ``/* DOMNodeList::__get($name) -- php exposes `length` as a virtual property. */`` |
|   44 | 1144 | `DOM_METHOD(vm_builtin_DOMNodeList_get)` |
|    1 | 1145 | `{` |
|   45 | 1146 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|   45 | 1147 | `	if( DomNameIs(zName,"length") ){` |
|   45 | 1148 | `		ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));` |
|   23 | 1149 | `	}else{` |
|  ! 0 | 1150 | `		ph7_result_null(pCtx);` |
|    - | 1151 | `	}` |
|   45 | 1152 | `	return PH7_OK;` |
|    1 | 1153 | `}` |
|    - | 1154 | `/*` |
|    - | 1155 | ` * DOMNamedNodeMap: an element's attributes, keyed by name.` |
|    - | 1156 | ` *` |
|    - | 1157 | ` * It shares DOMNodeList's slots (the owner element in $__owner) but walks the` |
|    - | 1158 | ` * attribute list rather than the child list, so it gets its own two readers.` |
|    - | 1159 | ` */` |
|   16 | 1160 | `static ph7_class_instance * DomMapItem(ph7_vm *pVm,ph7_class_instance *pMap,int iIndex)` |
|    1 | 1161 | `{` |
|   17 | 1162 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|    - | 1163 | `	xmlAttrPtr pAttr;` |
|   17 | 1164 | `	if( pOwner == 0 \|\| iIndex < 0 ){` |
|  ! 0 | 1165 | `		return 0;` |
|    - | 1166 | `	}` |
|   17 | 1167 | `	pAttr = DomAttrAt((xmlNodePtr)pOwner->pNode,iIndex);` |
|   17 | 1168 | `	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,(xmlNodePtr)pAttr);` |
|    9 | 1169 | `}` |
|    6 | 1170 | `static int DomMapCount(ph7_class_instance *pMap)` |
|    1 | 1171 | `{` |
|    7 | 1172 | `	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;` |
|    7 | 1173 | `	return pOwner ? DomAttrCount((xmlNodePtr)pOwner->pNode) : 0;` |
|    1 | 1174 | `}` |
|    2 | 1175 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_count)` |
|    1 | 1176 | `{` |
|    1 | 1177 | `	SXUNUSED(nArg);` |
|    1 | 1178 | `	SXUNUSED(apArg);` |
|    3 | 1179 | `	ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));` |
|    3 | 1180 | `	return PH7_OK;` |
|    1 | 1181 | `}` |
|    6 | 1182 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_item)` |
|    1 | 1183 | `{` |
|    7 | 1184 | `	int iIndex = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;` |
|    7 | 1185 | `	return DomResultWrap(pCtx,DomMapItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));` |
|    1 | 1186 | `}` |
|    - | 1187 | `/* DOMNamedNodeMap::getNamedItem(string $qualifiedName): ?DOMAttr */` |
|    2 | 1188 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItem)` |
|    1 | 1189 | `{` |
|    3 | 1190 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    3 | 1191 | `	phl_domnode *pOwner = pThis ? DomListOwner(pThis) : 0;` |
|    3 | 1192 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    3 | 1193 | `	xmlAttrPtr pAttr = pOwner ? xmlHasProp((xmlNodePtr)pOwner->pNode,(const xmlChar *)zName) : 0;` |
|    3 | 1194 | `	if( pAttr == 0 \|\| pAttr->type != XML_ATTRIBUTE_NODE ){` |
|  ! 0 | 1195 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1196 | `		return PH7_OK;` |
|    - | 1197 | `	}` |
|    4 | 1198 | `	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,PH7_NativeAttrObj(pThis,DOM_DOC),` |
|    1 | 1199 | `		pOwner->pShell,(xmlNodePtr)pAttr));` |
|    2 | 1200 | `}` |
|    4 | 1201 | `DOM_METHOD(vm_builtin_DOMNamedNodeMap_get)` |
|    1 | 1202 | `{` |
|    5 | 1203 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    5 | 1204 | `	if( DomNameIs(zName,"length") ){` |
|    5 | 1205 | `		ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));` |
|    3 | 1206 | `	}else{` |
|  ! 0 | 1207 | `		ph7_result_null(pCtx);` |
|    - | 1208 | `	}` |
|    5 | 1209 | `	return PH7_OK;` |
|    1 | 1210 | `}` |
|    - | 1211 | `/*` |
|    - | 1212 | ` * Both collections are IteratorAggregates, as php's are -- the chunk made` |
|    - | 1213 | ``  * DOMNodeList an `Iterator` with its own cursor (so `$list instanceof Iterator` `` |
|    - | 1214 | ` * was true where php says false) and gave DOMNamedNodeMap no iteration at all,` |
|    - | 1215 | `` * which meant `foreach ($el->attributes as $a)` walked the map's own private`` |
|    - | 1216 | ` * slots instead of the attributes.` |
|    - | 1217 | ` *` |
|    - | 1218 | ` * The cursor lives in the shared InternalIterator (oo_native.c); a vtable states` |
|    - | 1219 | ` * only how to REACH a position. DOMNodeList keys by index, DOMNamedNodeMap by` |
|    - | 1220 | ` * attribute name, which is what php answers for each.` |
|    - | 1221 | ` */` |
|   84 | 1222 | `static void DomIterSettle(ph7_vm *pVm,ph7_class_instance *pIt,int bNamed)` |
|    1 | 1223 | `{` |
|   85 | 1224 | `	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);` |
|   85 | 1225 | `	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);` |
|    - | 1226 | `	ph7_class_instance *pCur;` |
|   85 | 1227 | `	pCur = bNamed ? DomMapItem(&(*pVm),pSrc,(int)iPos) : DomListItem(&(*pVm),pSrc,(int)iPos);` |
|   85 | 1228 | `	if( pCur == 0 ){` |
|   17 | 1229 | `		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);` |
|   17 | 1230 | `		return;` |
|    - | 1231 | `	}` |
|   69 | 1232 | `	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pCur);  /* borrowed: no unref */` |
|   69 | 1233 | `	if( bNamed ){` |
|    9 | 1234 | `		phl_domnode *pNd = DomResOf(pCur);` |
|    9 | 1235 | `		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    9 | 1236 | `		const char *zKey = (pNode && pNode->name) ? (const char *)pNode->name : "";` |
|    9 | 1237 | `		PH7_NativeSetAttrStr(&(*pVm),pIt,PH7_NATIVE_IT_KEY,zKey,(int)SyStrlen(zKey));` |
|    5 | 1238 | `	}else{` |
|   61 | 1239 | `		PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);` |
|    - | 1240 | `	}` |
|   69 | 1241 | `	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);` |
|   43 | 1242 | `}` |
|   34 | 1243 | `static void DomListRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 1244 | `{` |
|   35 | 1245 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|   35 | 1246 | `	DomIterSettle(&(*pVm),pIt,0);` |
|   35 | 1247 | `}` |
|   40 | 1248 | `static void DomListNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 1249 | `{` |
|   61 | 1250 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|   40 | 1251 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|   41 | 1252 | `	DomIterSettle(&(*pVm),pIt,0);` |
|   41 | 1253 | `}` |
|    6 | 1254 | `static void DomMapRewind(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 1255 | `{` |
|    7 | 1256 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);` |
|    7 | 1257 | `	DomIterSettle(&(*pVm),pIt,1);` |
|    7 | 1258 | `}` |
|    4 | 1259 | `static void DomMapNext(ph7_vm *pVm,ph7_class_instance *pIt)` |
|    1 | 1260 | `{` |
|    7 | 1261 | `	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,` |
|    4 | 1262 | `		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);` |
|    5 | 1263 | `	DomIterSettle(&(*pVm),pIt,1);` |
|    5 | 1264 | `}` |
|    - | 1265 | `static const PH7_NativeIterVtab sDomListIterVtab = { DomListRewind, DomListNext };` |
|    - | 1266 | `static const PH7_NativeIterVtab sDomMapIterVtab  = { DomMapRewind,  DomMapNext };` |
|    - | 1267 | `/* Both getIterator()s: a fresh InternalIterator per call, as php's are. */` |
|   24 | 1268 | `DOM_METHOD(vm_builtin_Dom_getIterator)` |
|    1 | 1269 | `{` |
|   25 | 1270 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    - | 1271 | `	ph7_class_instance *pIt;` |
|   12 | 1272 | `	SXUNUSED(nArg);` |
|   12 | 1273 | `	SXUNUSED(apArg);` |
|   25 | 1274 | `	if( pThis == 0 ){` |
|  ! 0 | 1275 | `		return PH7_OK;` |
|    - | 1276 | `	}` |
|   25 | 1277 | `	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);` |
|   25 | 1278 | `	if( pIt == 0 ){` |
|  ! 0 | 1279 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1280 | `	}` |
|   25 | 1281 | `	PH7_NativeResultObject(pCtx,pIt);` |
|   25 | 1282 | `	return PH7_OK;` |
|   13 | 1283 | `}` |
|    - | 1284 |  |
|    - | 1285 | `/* ===== DOMXPath ===== */` |
|    - | 1286 |  |
|    - | 1287 | `/*` |
|    - | 1288 | ` * DOMXPath::query(string $expression, ?DOMNode $contextNode = null,` |
|    - | 1289 | ` *                 bool $registerNodeNS = true): DOMNodeList\|false` |
|    - | 1290 | ` *` |
|    - | 1291 | ` * The nodeset is frozen into a document-order snapshot (php's query() is not` |
|    - | 1292 | ` * live) and handed to a DOMNodeList of kind DNL_SNAP. false on an invalid` |
|    - | 1293 | ` * expression or a non-nodeset result, which is php's contract.` |
|    - | 1294 | ` */` |
|   42 | 1295 | `DOM_METHOD(vm_builtin_DOMXPath_query)` |
|    1 | 1296 | `{` |
|   43 | 1297 | `	ph7_vm *pVm = pCtx->pVm;` |
|   43 | 1298 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   43 | 1299 | `	ph7_class_instance *pDoc = pThis ? PH7_NativeAttrObj(pThis,"document") : 0;` |
|   43 | 1300 | `	phl_domnode *pDocNd = DomResOf(pDoc);` |
|   43 | 1301 | `	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|   43 | 1302 | `	phl_domnode *pCtxNd = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;` |
|    - | 1303 | `	xmlXPathContextPtr pXCtx;` |
|    - | 1304 | `	xmlXPathObjectPtr pObj;` |
|    - | 1305 | `	ph7_class_instance *pList;` |
|    - | 1306 | `	ph7_value *pSnap;` |
|    - | 1307 | `	sxu32 nMark;` |
|   43 | 1308 | `	if( pDocNd == 0 ){` |
|  ! 0 | 1309 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1310 | `		return PH7_OK;` |
|    - | 1311 | `	}` |
|   43 | 1312 | `	pXCtx = xmlXPathNewContext((xmlDocPtr)pDocNd->pNode);` |
|   43 | 1313 | `	if( pXCtx == 0 ){` |
|  ! 0 | 1314 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1315 | `		return PH7_OK;` |
|    - | 1316 | `	}` |
|    - | 1317 | `	/* With no explicit context node php evaluates relative expressions` |
|    - | 1318 | `	 * against the document ELEMENT (so query('file') matches a child of` |
|    - | 1319 | `	 * the root), not the document node -- match that. */` |
|   43 | 1320 | `	if( pCtxNd ){` |
|   11 | 1321 | `		pXCtx->node = (xmlNodePtr)pCtxNd->pNode;` |
|    6 | 1322 | `	}else{` |
|   33 | 1323 | `		pXCtx->node = xmlDocGetRootElement((xmlDocPtr)pDocNd->pNode);` |
|    - | 1324 | `	}` |
|   43 | 1325 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|   43 | 1326 | `	pObj = xmlXPathEvalExpression((const xmlChar *)zExpr,pXCtx);` |
|   43 | 1327 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMXPath::query");` |
|   43 | 1328 | `	if( pObj == 0 \|\| pObj->type != XPATH_NODESET ){` |
|    3 | 1329 | `		if( pObj ){` |
|  ! 0 | 1330 | `			xmlXPathFreeObject(pObj);` |
|  ! 0 | 1331 | `		}` |
|    3 | 1332 | `		xmlXPathFreeContext(pXCtx);` |
|    3 | 1333 | `		ph7_result_bool(pCtx,0);` |
|    3 | 1334 | `		return PH7_OK;` |
|    - | 1335 | `	}` |
|   41 | 1336 | `	pSnap = ph7_context_new_array(pCtx);` |
|   41 | 1337 | `	if( pSnap == 0 ){` |
|  ! 0 | 1338 | `		xmlXPathFreeObject(pObj);` |
|  ! 0 | 1339 | `		xmlXPathFreeContext(pXCtx);` |
|  ! 0 | 1340 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1341 | `	}` |
|   41 | 1342 | `	if( pObj->nodesetval ){` |
|    - | 1343 | `		int i;` |
|   95 | 1344 | `		for( i = 0 ; i < pObj->nodesetval->nodeNr ; i++ ){` |
|   55 | 1345 | `			xmlNodePtr pNode = pObj->nodesetval->nodeTab[i];` |
|    - | 1346 | `			phl_domnode *pWrap;` |
|    - | 1347 | `			ph7_value *pRes;` |
|   55 | 1348 | `			if( pNode == 0 \|\| pNode->type == XML_NAMESPACE_DECL ){` |
|  ! 0 | 1349 | `				continue; /* namespace pseudo-nodes are not exposed */` |
|    - | 1350 | `			}` |
|   55 | 1351 | `			pWrap = DomNewRes(pVm,pDocNd->pShell,pNode);` |
|   55 | 1352 | `			pRes = ph7_context_new_scalar(pCtx);` |
|   55 | 1353 | `			if( pWrap == 0 \|\| pRes == 0 ){` |
|  ! 0 | 1354 | `				break;` |
|    - | 1355 | `			}` |
|   55 | 1356 | `			ph7_value_resource(pRes,pWrap);` |
|   55 | 1357 | `			ph7_array_add_elem(pSnap,0,pRes);` |
|   28 | 1358 | `		}` |
|   20 | 1359 | `	}` |
|   41 | 1360 | `	xmlXPathFreeObject(pObj);` |
|   41 | 1361 | `	xmlXPathFreeContext(pXCtx);` |
|   41 | 1362 | `	pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_SNAP,0,0,pSnap);` |
|   41 | 1363 | `	if( pList == 0 ){` |
|  ! 0 | 1364 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1365 | `	}` |
|   41 | 1366 | `	PH7_NativeResultObject(pCtx,pList);` |
|   41 | 1367 | `	return PH7_OK;` |
|   22 | 1368 | `}` |
|    - | 1369 | `/* DOMXPath::__construct(DOMDocument $document, bool $registerNodeNS = true) */` |
|   10 | 1370 | `DOM_METHOD(vm_builtin_DOMXPath_construct)` |
|    1 | 1371 | `{` |
|   11 | 1372 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   11 | 1373 | `	if( pThis && nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){` |
|   16 | 1374 | `		PH7_NativeSetAttrObj(pCtx->pVm,pThis,"document",` |
|   10 | 1375 | `			(ph7_class_instance *)apArg[0]->x.pOther);` |
|    5 | 1376 | `	}` |
|   11 | 1377 | `	return PH7_OK;` |
|    1 | 1378 | `}` |
|    - | 1379 |  |
|    - | 1380 | `/* ===== Schema validation ===== */` |
|    - | 1381 |  |
|    - | 1382 | `/* Schema parser/validator diagnostics: forward onto the shared per-VM queue` |
|    - | 1383 | ` * via PH7_LibxmlQueueError, exactly like the global structured handler. */` |
|    - | 1384 | `#if LIBXML_VERSION >= 21200` |
|    2 | 1385 | `static void DomSchemaErr(void *pUserData,const xmlError *pErr)` |
|    - | 1386 | `#else` |
|    2 | 1387 | `static void DomSchemaErr(void *pUserData,xmlErrorPtr pErr)` |
|    - | 1388 | `#endif` |
|    1 | 1389 | `{` |
|    5 | 1390 | `	if( pErr == 0 ){` |
|  ! 0 | 1391 | `		return;` |
|    - | 1392 | `	}` |
|    7 | 1393 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|    4 | 1394 | `		pErr->int2,pErr->message,pErr->file);` |
|    3 | 1395 | `}` |
|    - | 1396 | `/* DOMDocument::schemaValidateSource(string $source, int $flags = 0): bool */` |
|    4 | 1397 | `DOM_METHOD(vm_builtin_DOMDocument_schemaValidateSource)` |
|    1 | 1398 | `{` |
|    5 | 1399 | `	ph7_vm *pVm = pCtx->pVm;` |
|    5 | 1400 | `	phl_domnode *pDocNd = DomThisNode(pCtx);` |
|    5 | 1401 | `	int nXsd = 0;` |
|    5 | 1402 | `	const char *zXsd = nArg > 0 ? ph7_value_to_string(apArg[0],&nXsd) : "";` |
|    - | 1403 | `	xmlSchemaParserCtxtPtr pParser;` |
|    - | 1404 | `	xmlSchemaPtr pSchema;` |
|    - | 1405 | `	xmlSchemaValidCtxtPtr pValid;` |
|    - | 1406 | `	int rc;` |
|    - | 1407 | `	sxu32 nMark;` |
|    5 | 1408 | `	if( pDocNd == 0 \|\| nXsd < 1 ){` |
|  ! 0 | 1409 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1410 | `		return PH7_OK;` |
|    - | 1411 | `	}` |
|    5 | 1412 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|    5 | 1413 | `	pParser = xmlSchemaNewMemParserCtxt(zXsd,nXsd);` |
|    5 | 1414 | `	if( pParser == 0 ){` |
|  ! 0 | 1415 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");` |
|  ! 0 | 1416 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1417 | `		return PH7_OK;` |
|    - | 1418 | `	}` |
|    5 | 1419 | `	xmlSchemaSetParserStructuredErrors(pParser,DomSchemaErr,pVm);` |
|    5 | 1420 | `	pSchema = xmlSchemaParse(pParser);` |
|    5 | 1421 | `	xmlSchemaFreeParserCtxt(pParser);` |
|    5 | 1422 | `	if( pSchema == 0 ){` |
|  ! 0 | 1423 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");` |
|    - | 1424 | `		/* php raises "Invalid Schema" and returns false */` |
|  ! 0 | 1425 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"DOMDocument::schemaValidateSource(): Invalid Schema");` |
|  ! 0 | 1426 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1427 | `		return PH7_OK;` |
|    - | 1428 | `	}` |
|    5 | 1429 | `	pValid = xmlSchemaNewValidCtxt(pSchema);` |
|    5 | 1430 | `	if( pValid == 0 ){` |
|  ! 0 | 1431 | `		xmlSchemaFree(pSchema);` |
|  ! 0 | 1432 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");` |
|  ! 0 | 1433 | `		ph7_result_bool(pCtx,0);` |
|  ! 0 | 1434 | `		return PH7_OK;` |
|    - | 1435 | `	}` |
|    5 | 1436 | `	xmlSchemaSetValidStructuredErrors(pValid,DomSchemaErr,pVm);` |
|    5 | 1437 | `	rc = xmlSchemaValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);` |
|    5 | 1438 | `	xmlSchemaFreeValidCtxt(pValid);` |
|    5 | 1439 | `	xmlSchemaFree(pSchema);` |
|    5 | 1440 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");` |
|    5 | 1441 | `	ph7_result_bool(pCtx,rc == 0);` |
|    5 | 1442 | `	return PH7_OK;` |
|    3 | 1443 | `}` |
|    - | 1444 |  |
|    - | 1445 |  |
|    - | 1446 | `/* ===== The shared __get dispatch ===== */` |
|    - | 1447 |  |
|    - | 1448 | `/*` |
|    - | 1449 | ` * DOMNode's virtual properties.` |
|    - | 1450 | ` *` |
|    - | 1451 | ` * php exposes these through property handlers on the class; PHL answers them` |
|    - | 1452 | ` * from __get, as the chunk did. Returns 1 when it recognised the name, so a` |
|    - | 1453 | ` * subclass's own __get can state its extras and then defer here -- which is` |
|    - | 1454 | `` * what `parent::__get($name)` did.`` |
|    - | 1455 | ` */` |
|  140 | 1456 | `static int DomNodeProp(ph7_context *pCtx,const char *zName)` |
|    1 | 1457 | `{` |
|  141 | 1458 | `	ph7_vm *pVm = pCtx->pVm;` |
|  141 | 1459 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|  141 | 1460 | `	ph7_class_instance *pDoc = DomThisDoc(pCtx);` |
|  141 | 1461 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|  141 | 1462 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|  211 | 1463 | `	int bIsDoc = pNode && (pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE);` |
|  141 | 1464 | `	if( DomNameIs(zName,"nodeName") ){` |
|   23 | 1465 | `		DomNodeName(pCtx,pNode);` |
|  130 | 1466 | `	}else if( DomNameIs(zName,"nodeValue") ){` |
|   15 | 1467 | `		DomNodeValue(pCtx,pNode);` |
|  112 | 1468 | `	}else if( DomNameIs(zName,"nodeType") ){` |
|   15 | 1469 | `		ph7_result_int(pCtx,pNode ? (int)pNode->type : 0);` |
|   98 | 1470 | `	}else if( DomNameIs(zName,"textContent") ){` |
|    7 | 1471 | `		DomTextContent(pCtx,pNode);` |
|   88 | 1472 | `	}else if( DomNameIs(zName,"parentNode") ){` |
|   15 | 1473 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);` |
|   78 | 1474 | `	}else if( DomNameIs(zName,"firstChild") ){` |
|   31 | 1475 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->children : 0);` |
|   56 | 1476 | `	}else if( DomNameIs(zName,"lastChild") ){` |
|    5 | 1477 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->last : 0);` |
|   39 | 1478 | `	}else if( DomNameIs(zName,"nextSibling") ){` |
|    3 | 1479 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->next : 0);` |
|   36 | 1480 | `	}else if( DomNameIs(zName,"previousSibling") ){` |
|    3 | 1481 | `		DomResultNodeOf(pCtx,pNd,pNode ? pNode->prev : 0);` |
|   34 | 1482 | `	}else if( DomNameIs(zName,"ownerDocument") ){` |
|    - | 1483 | `		/* A document has no owner document, which is also why DomWrap answers` |
|    - | 1484 | `		 * the document itself rather than a second wrapper for it. */` |
|    5 | 1485 | `		DomResultWrap(pCtx,bIsDoc ? 0 : pDoc);` |
|   31 | 1486 | `	}else if( DomNameIs(zName,"childElementCount") ){` |
|    3 | 1487 | `		ph7_result_int(pCtx,DomChildCount(pNode,1));` |
|   28 | 1488 | `	}else if( DomNameIs(zName,"childNodes") ){` |
|   13 | 1489 | `		ph7_class_instance *pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_CHILD,pThis,0,0);` |
|   13 | 1490 | `		if( pList == 0 ){` |
|  ! 0 | 1491 | `			return -1;` |
|    - | 1492 | `		}` |
|   13 | 1493 | `		PH7_NativeResultObject(pCtx,pList);` |
|   21 | 1494 | `	}else if( DomNameIs(zName,"attributes") ){` |
|    - | 1495 | `		/* php: NULL for anything that is not an element. */` |
|   15 | 1496 | `		if( pNode == 0 \|\| pNode->type != XML_ELEMENT_NODE ){` |
|  ! 0 | 1497 | `			ph7_result_null(pCtx);` |
|  ! 0 | 1498 | `		}else{` |
|   15 | 1499 | `			ph7_class_instance *pMap = DomNewCollection(pVm,"DOMNamedNodeMap",pDoc,DNL_CHILD,pThis,0,0);` |
|   15 | 1500 | `			if( pMap == 0 ){` |
|  ! 0 | 1501 | `				return -1;` |
|    - | 1502 | `			}` |
|   15 | 1503 | `			PH7_NativeResultObject(pCtx,pMap);` |
|    - | 1504 | `		}` |
|    8 | 1505 | `	}else{` |
|  ! 0 | 1506 | `		return 0;` |
|    - | 1507 | `	}` |
|  141 | 1508 | `	return 1;` |
|   71 | 1509 | `}` |
|    - | 1510 | `/* The argument every __get body reads. */` |
|  216 | 1511 | `static const char * DomGetName(int nArg,ph7_value **apArg)` |
|    1 | 1512 | `{` |
|  217 | 1513 | `	return nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    1 | 1514 | `}` |
|  ! 0 | 1515 | `DOM_METHOD(vm_builtin_DOMNode_get)` |
|  ! 0 | 1516 | `{` |
|  ! 0 | 1517 | `	if( DomNodeProp(pCtx,DomGetName(nArg,apArg)) == 0 ){` |
|  ! 0 | 1518 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1519 | `	}` |
|  ! 0 | 1520 | `	return PH7_OK;` |
|  ! 0 | 1521 | `}` |
|    - | 1522 | `/* DOMDocument adds documentElement. */` |
|   64 | 1523 | `DOM_METHOD(vm_builtin_DOMDocument_get)` |
|    1 | 1524 | `{` |
|   65 | 1525 | `	const char *zName = DomGetName(nArg,apArg);` |
|    - | 1526 | `	phl_domnode *pNd;` |
|   65 | 1527 | `	if( DomNameIs(zName,"documentElement") ){` |
|   55 | 1528 | `		pNd = DomThisNode(pCtx);` |
|   55 | 1529 | `		return DomResultNodeOf(pCtx,pNd,pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0);` |
|    - | 1530 | `	}` |
|   11 | 1531 | `	if( DomNodeProp(pCtx,zName) == 0 ){` |
|  ! 0 | 1532 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1533 | `	}` |
|   11 | 1534 | `	return PH7_OK;` |
|   33 | 1535 | `}` |
|    - | 1536 | `/* DOMElement adds tagName. */` |
|  102 | 1537 | `DOM_METHOD(vm_builtin_DOMElement_get)` |
|    1 | 1538 | `{` |
|  103 | 1539 | `	const char *zName = DomGetName(nArg,apArg);` |
|    - | 1540 | `	phl_domnode *pNd;` |
|  103 | 1541 | `	if( DomNameIs(zName,"tagName") ){` |
|    3 | 1542 | `		pNd = DomThisNode(pCtx);` |
|    3 | 1543 | `		DomNodeName(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|    3 | 1544 | `		return PH7_OK;` |
|    - | 1545 | `	}` |
|  101 | 1546 | `	if( DomNodeProp(pCtx,zName) == 0 ){` |
|  ! 0 | 1547 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1548 | `	}` |
|  101 | 1549 | `	return PH7_OK;` |
|   52 | 1550 | `}` |
|    - | 1551 | `/* DOMAttr adds name/value/ownerElement. */` |
|   16 | 1552 | `DOM_METHOD(vm_builtin_DOMAttr_get)` |
|    1 | 1553 | `{` |
|   17 | 1554 | `	const char *zName = DomGetName(nArg,apArg);` |
|   17 | 1555 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   17 | 1556 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|   17 | 1557 | `	if( DomNameIs(zName,"name") ){` |
|    3 | 1558 | `		DomNodeName(pCtx,pNode);` |
|    3 | 1559 | `		return PH7_OK;` |
|    - | 1560 | `	}` |
|   15 | 1561 | `	if( DomNameIs(zName,"value") ){` |
|   11 | 1562 | `		DomNodeValue(pCtx,pNode);` |
|   11 | 1563 | `		return PH7_OK;` |
|    - | 1564 | `	}` |
|    5 | 1565 | `	if( DomNameIs(zName,"ownerElement") ){` |
|  ! 0 | 1566 | `		return DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);` |
|    - | 1567 | `	}` |
|    5 | 1568 | `	if( DomNodeProp(pCtx,zName) == 0 ){` |
|  ! 0 | 1569 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1570 | `	}` |
|    5 | 1571 | `	return PH7_OK;` |
|    9 | 1572 | `}` |
|    - | 1573 | `/* DOMCharacterData adds data/length; DOMText adds wholeText on top of those. */` |
|   34 | 1574 | `static int DomCharDataProp(ph7_context *pCtx,const char *zName)` |
|    1 | 1575 | `{` |
|   35 | 1576 | `	phl_domnode *pNd = DomThisNode(pCtx);` |
|   35 | 1577 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    - | 1578 | `	xmlChar *zContent;` |
|   35 | 1579 | `	if( DomNameIs(zName,"data") ){` |
|    7 | 1580 | `		DomNodeValue(pCtx,pNode);` |
|    7 | 1581 | `		return 1;` |
|    - | 1582 | `	}` |
|   29 | 1583 | `	if( DomNameIs(zName,"length") ){` |
|    - | 1584 | `		/* php's length is the BYTE length of the data, which is what strlen()` |
|    - | 1585 | `		 * of the chunk's nodeValue measured. */` |
|    3 | 1586 | `		zContent = pNode ? xmlNodeGetContent(pNode) : 0;` |
|    3 | 1587 | `		ph7_result_int(pCtx,zContent ? (int)SyStrlen((const char *)zContent) : 0);` |
|    3 | 1588 | `		if( zContent ){` |
|    3 | 1589 | `			xmlFree(zContent);` |
|    1 | 1590 | `		}` |
|    3 | 1591 | `		return 1;` |
|    - | 1592 | `	}` |
|   27 | 1593 | `	return 0;` |
|   18 | 1594 | `}` |
|    8 | 1595 | `DOM_METHOD(vm_builtin_DOMCharacterData_get)` |
|    1 | 1596 | `{` |
|    9 | 1597 | `	const char *zName = DomGetName(nArg,apArg);` |
|    9 | 1598 | `	if( DomCharDataProp(pCtx,zName) == 0 && DomNodeProp(pCtx,zName) == 0 ){` |
|  ! 0 | 1599 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1600 | `	}` |
|    9 | 1601 | `	return PH7_OK;` |
|    1 | 1602 | `}` |
|   26 | 1603 | `DOM_METHOD(vm_builtin_DOMText_get)` |
|    1 | 1604 | `{` |
|   27 | 1605 | `	const char *zName = DomGetName(nArg,apArg);` |
|    - | 1606 | `	phl_domnode *pNd;` |
|   27 | 1607 | `	if( DomNameIs(zName,"wholeText") ){` |
|  ! 0 | 1608 | `		pNd = DomThisNode(pCtx);` |
|  ! 0 | 1609 | `		DomNodeValue(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);` |
|  ! 0 | 1610 | `		return PH7_OK;` |
|    - | 1611 | `	}` |
|   27 | 1612 | `	if( DomCharDataProp(pCtx,zName) == 0 && DomNodeProp(pCtx,zName) == 0 ){` |
|  ! 0 | 1613 | `		ph7_result_null(pCtx);` |
|  ! 0 | 1614 | `	}` |
|   27 | 1615 | `	return PH7_OK;` |
|   14 | 1616 | `}` |
|    - | 1617 | `/* DOMDocument::getElementsByTagName / DOMElement::getElementsByTagName --` |
|    - | 1618 | ` * php declares it on those two, not on DOMNode, so both specs name it. */` |
|   22 | 1619 | `DOM_METHOD(vm_builtin_Dom_getElementsByTagName)` |
|    1 | 1620 | `{` |
|   23 | 1621 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   23 | 1622 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";` |
|    - | 1623 | `	ph7_class_instance *pList;` |
|   23 | 1624 | `	if( pThis == 0 ){` |
|  ! 0 | 1625 | `		return PH7_OK;` |
|    - | 1626 | `	}` |
|   23 | 1627 | `	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTN,pThis,zName,0);` |
|   23 | 1628 | `	if( pList == 0 ){` |
|  ! 0 | 1629 | `		return PH7_ContextMemoryError(pCtx);` |
|    - | 1630 | `	}` |
|   23 | 1631 | `	PH7_NativeResultObject(pCtx,pList);` |
|   23 | 1632 | `	return PH7_OK;` |
|   12 | 1633 | `}` |
|    - | 1634 |  |
|    - | 1635 | `/*` |
|    - | 1636 | ` * Install the DOM library: every class declared from C, no embedded chunk and` |
|    - | 1637 | ` * no globally visible thunk left.  Called from PH7_VmInit inside the` |
|    - | 1638 | ` * bCompilingBuiltin window, after PH7_VmInstallLibxml (the capture plumbing must` |
|    - | 1639 | ` * exist) and after the Reflection install (DOMException needs Exception).` |
|    - | 1640 | ` */` |
| 5146 | 1641 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)` |
|    5 | 1642 | `{` |
|    - | 1643 | `	/* The two slots every wrapper carries. They were public in the chunk and stay` |
|    - | 1644 | `	 * public: hiding them is the per-class debug-info hook's job (§7.4 (e)), which` |
|    - | 1645 | `	 * DateTime, XMLWriter, Fiber, Generator and WeakReference all wait on too. */` |
|    - | 1646 | `	static const PH7_NativePropDef aNodeProp[] = {` |
|    - | 1647 | `		{ DOM_RES, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 1648 | `		{ DOM_DOC, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 1649 | `	};` |
|    - | 1650 | ``	/* php's own signatures. Declaring `DOMNode $node` is what makes`` |
|    - | 1651 | ``	 * `$n->appendChild(1)` the TypeError php raises instead of a warning from`` |
|    - | 1652 | `	 * reading ->__res off an int. */` |
|    - | 1653 | `	static const PH7_NativeMethodDef aNodeMethod[] = {` |
|    - | 1654 | `		{ "appendChild",    PH7_MOD_PUBLIC, "DOMNode $node", "", vm_builtin_DOMNode_appendChild },` |
|    - | 1655 | `		{ "insertBefore",   PH7_MOD_PUBLIC, "DOMNode $node, ?DOMNode $child = null", "",` |
|    - | 1656 | `		  vm_builtin_DOMNode_insertBefore },` |
|    - | 1657 | `		{ "removeChild",    PH7_MOD_PUBLIC, "DOMNode $child", "", vm_builtin_DOMNode_removeChild },` |
|    - | 1658 | `		{ "replaceChild",   PH7_MOD_PUBLIC, "DOMNode $node, DOMNode $child", "",` |
|    - | 1659 | `		  vm_builtin_DOMNode_replaceChild },` |
|    - | 1660 | `		{ "hasChildNodes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasChildNodes },` |
|    - | 1661 | `		{ "hasAttributes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasAttributes },` |
|    - | 1662 | `		{ "isSameNode",     PH7_MOD_PUBLIC, "DOMNode $otherNode", "@bool", vm_builtin_DOMNode_isSameNode },` |
|    - | 1663 | `		{ "getLineNo",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNode_getLineNo },` |
|    - | 1664 | `		{ "C14N",           PH7_MOD_PUBLIC,` |
|    - | 1665 | `		  "bool $exclusive = false, bool $withComments = false, ?array $xpath = null, "` |
|    - | 1666 | `		  "?array $nsPrefixes = null", "@string\|false", vm_builtin_DOMNode_C14N },` |
|    - | 1667 | `		{ "__get",          PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNode_get },` |
|    - | 1668 | `	};` |
|    - | 1669 | `	static const PH7_NativePropDef aDocProp[] = {` |
|    - | 1670 | `		/* php models both as VIRTUAL hooked properties reading libxml state, so it` |
|    - | 1671 | `		 * reports no default; PHL's are real slots and keep theirs, or a read before` |
|    - | 1672 | `		 * the first write would raise where php answers the parser's current value.` |
|    - | 1673 | `		 * The TYPE is what the row can state exactly (PLAN §7.4 for the virtual half). */` |
|    - | 1674 | `		{ "preserveWhiteSpace", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },` |
|    - | 1675 | `		{ "formatOutput",       PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },` |
|    - | 1676 | `		/* The identity cache DomWrap keys by node pointer. */` |
|    - | 1677 | `		{ DOM_NODES,            PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|    - | 1678 | `	};` |
|    - | 1679 | `	static const PH7_NativeMethodDef aDocMethod[] = {` |
|    - | 1680 | `		{ "__construct",          PH7_MOD_PUBLIC, "string $version = '1.0', string $encoding = ''", "",` |
|    - | 1681 | `		  vm_builtin_DOMDocument_construct },` |
|    - | 1682 | `		{ "loadXML",              PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",` |
|    - | 1683 | `		  vm_builtin_DOMDocument_loadXML },` |
|    - | 1684 | `		{ "saveXML",              PH7_MOD_PUBLIC, "?DOMNode $node = null, int $options = 0", "@string\|false",` |
|    - | 1685 | `		  vm_builtin_DOMDocument_saveXML },` |
|    - | 1686 | `		{ "createElement",        PH7_MOD_PUBLIC, "string $localName, string $value = ''", "",` |
|    - | 1687 | `		  vm_builtin_DOMDocument_createElement },` |
|    - | 1688 | `		{ "createTextNode",       PH7_MOD_PUBLIC, "string $data", "@DOMText",` |
|    - | 1689 | `		  vm_builtin_DOMDocument_createTextNode },` |
|    - | 1690 | `		{ "createComment",        PH7_MOD_PUBLIC, "string $data", "@DOMComment",` |
|    - | 1691 | `		  vm_builtin_DOMDocument_createComment },` |
|    - | 1692 | `		{ "createCDATASection",   PH7_MOD_PUBLIC, "string $data", "",` |
|    - | 1693 | `		  vm_builtin_DOMDocument_createCDATASection },` |
|    - | 1694 | `		{ "normalizeDocument",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },` |
|    - | 1695 | `		{ "schemaValidateSource", PH7_MOD_PUBLIC, "string $source, int $flags = 0", "@bool",` |
|    - | 1696 | `		  vm_builtin_DOMDocument_schemaValidateSource },` |
|    - | 1697 | `		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",` |
|    - | 1698 | `		  vm_builtin_Dom_getElementsByTagName },` |
|    - | 1699 | `		{ "__get",                PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMDocument_get },` |
|    - | 1700 | `	};` |
|    - | 1701 | `	static const PH7_NativeMethodDef aElemMethod[] = {` |
|    - | 1702 | `		{ "getAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@string",` |
|    - | 1703 | `		  vm_builtin_DOMElement_getAttribute },` |
|    - | 1704 | `		{ "hasAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",` |
|    - | 1705 | `		  vm_builtin_DOMElement_hasAttribute },` |
|    - | 1706 | `		{ "setAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName, string $value", "",` |
|    - | 1707 | `		  vm_builtin_DOMElement_setAttribute },` |
|    - | 1708 | `		{ "removeAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",` |
|    - | 1709 | `		  vm_builtin_DOMElement_removeAttribute },` |
|    - | 1710 | `		{ "getAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@string",` |
|    - | 1711 | `		  vm_builtin_DOMElement_getAttributeNS },` |
|    - | 1712 | `		{ "setAttributeNS",       PH7_MOD_PUBLIC,` |
|    - | 1713 | `		  "?string $namespace, string $qualifiedName, string $value", "@void",` |
|    - | 1714 | `		  vm_builtin_DOMElement_setAttributeNS },` |
|    - | 1715 | `		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",` |
|    - | 1716 | `		  vm_builtin_Dom_getElementsByTagName },` |
|    - | 1717 | `		{ "__get",                PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMElement_get },` |
|    - | 1718 | `	};` |
|    - | 1719 | `	static const PH7_NativeMethodDef aAttrMethod[] = {` |
|    - | 1720 | `		{ "__get", PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMAttr_get },` |
|    - | 1721 | `	};` |
|    - | 1722 | `	static const PH7_NativeMethodDef aCharMethod[] = {` |
|    - | 1723 | `		{ "__get", PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMCharacterData_get },` |
|    - | 1724 | `	};` |
|    - | 1725 | `	static const PH7_NativeMethodDef aTextMethod[] = {` |
|    - | 1726 | `		{ "__get", PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMText_get },` |
|    - | 1727 | `	};` |
|    - | 1728 | `	/* DOMNodeList and DOMNamedNodeMap share a slot layout: what a live view is OF` |
|    - | 1729 | `	 * ($__owner), the document to wrap results against ($__doc), and -- for the` |
|    - | 1730 | `	 * two node-list kinds -- the tag name or the frozen snapshot. */` |
|    - | 1731 | `	static const PH7_NativePropDef aListProp[] = {` |
|    - | 1732 | `		{ DNL_KIND,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },` |
|    - | 1733 | `		{ DOM_DOC,       PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|    - | 1734 | `		{ DNL_OWNER,     PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|    - | 1735 | `		{ DNL_NAME,      PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|    - | 1736 | `		/* The cached node snapshot, missed by the 2 Aug hidden-slot sweep exactly as` |
|    - | 1737 | `		 * Closure's three were: php presents no property on either class this table` |
|    - | 1738 | ``		 * declares (DOMNodeList, DOMNamedNodeMap), and `__snap` was on var_dump,`` |
|    - | 1739 | `		 * (array), get_object_vars, foreach, json_encode and Reflection. */` |
|    - | 1740 | `		{ DNL_SNAP_SLOT, PH7_MOD_PUBLIC\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },` |
|    - | 1741 | `	};` |
|    - | 1742 | `	static const PH7_NativeMethodDef aListMethod[] = {` |
|    - | 1743 | `		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNodeList_count },` |
|    - | 1744 | `		{ "item",        PH7_MOD_PUBLIC, "int $index", "", vm_builtin_DOMNodeList_item },` |
|    - | 1745 | `		{ "getIterator", PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },` |
|    - | 1746 | `		{ "__get",       PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNodeList_get },` |
|    - | 1747 | `	};` |
|    - | 1748 | `	static const PH7_NativeMethodDef aMapMethod[] = {` |
|    - | 1749 | `		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNamedNodeMap_count },` |
|    - | 1750 | `		{ "item",         PH7_MOD_PUBLIC, "int $index", "@?DOMNode", vm_builtin_DOMNamedNodeMap_item },` |
|    - | 1751 | `		{ "getNamedItem", PH7_MOD_PUBLIC, "string $qualifiedName", "@?DOMNode",` |
|    - | 1752 | `		  vm_builtin_DOMNamedNodeMap_getNamedItem },` |
|    - | 1753 | `		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },` |
|    - | 1754 | `		{ "__get",        PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNamedNodeMap_get },` |
|    - | 1755 | `	};` |
|    - | 1756 | `	static const PH7_NativePropDef aXPathProp[] = {` |
|    - | 1757 | `		/* Written by the constructor, which is why the slot can carry php's` |
|    - | 1758 | `		 * non-nullable type with no default at all. */` |
|    - | 1759 | `		{ "document", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "DOMDocument" },` |
|    - | 1760 | `	};` |
|    - | 1761 | `	static const PH7_NativeMethodDef aXPathMethod[] = {` |
|    - | 1762 | `		{ "__construct", PH7_MOD_PUBLIC, "DOMDocument $document, bool $registerNodeNS = true", "",` |
|    - | 1763 | `		  vm_builtin_DOMXPath_construct },` |
|    - | 1764 | `		{ "query",       PH7_MOD_PUBLIC,` |
|    - | 1765 | `		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",` |
|    - | 1766 | `		  vm_builtin_DOMXPath_query },` |
|    - | 1767 | `	};` |
|    - | 1768 | `	/* Bases before subclasses: PH7_InstallNativeClasses declares the whole table` |
|    - | 1769 | `	 * before touching a method, but PH7_ClassInherit still needs the parent to` |
|    - | 1770 | `	 * exist when the child's row is declared. */` |
|    - | 1771 | `	/* php refuses to serialize a NODE class, and its refusal is the soft kind: the` |
|    - | 1772 | `	 * deny handler sits behind the __serialize()/__sleep() lookup, so a subclass that` |
|    - | 1773 | `	 * declares either one is serialized normally and the sentence says so. DOMXPath's` |
|    - | 1774 | `	 * is the HARD kind — a subclass declaring __serialize() is refused there too — and` |
|    - | 1775 | ``	 * DOMNodeList/DOMNamedNodeMap are not refused at all (`0:{}`), which is what they`` |
|    - | 1776 | `	 * became once serialize() stopped emitting the hidden slot. Restating the flag on` |
|    - | 1777 | `	 * every row is rule 29: a native subclass does not inherit its parent's. */` |
|    - | 1778 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|    - | 1779 | `		{ "DOMException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|    - | 1780 | `		{ "DOMNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|    - | 1781 | `		  aNodeMethod, SX_ARRAYSIZE(aNodeMethod), 0, 0, aNodeProp, SX_ARRAYSIZE(aNodeProp), 0, 0, 0 },` |
|    - | 1782 | `		{ "DOMDocument", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|    - | 1783 | `		  aDocMethod, SX_ARRAYSIZE(aDocMethod), 0, 0, aDocProp, SX_ARRAYSIZE(aDocProp), 0, 0, 0 },` |
|    - | 1784 | `		{ "DOMElement", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|    - | 1785 | `		  aElemMethod, SX_ARRAYSIZE(aElemMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|    - | 1786 | `		{ "DOMAttr", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|    - | 1787 | `		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|    - | 1788 | `		{ "DOMCharacterData", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|    - | 1789 | `		  aCharMethod, SX_ARRAYSIZE(aCharMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|    - | 1790 | `		{ "DOMText", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|    - | 1791 | `		  aTextMethod, SX_ARRAYSIZE(aTextMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|    - | 1792 | `		{ "DOMComment", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|    - | 1793 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|    - | 1794 | `		{ "DOMCdataSection", "DOMText", 0, PH7_CLASS_NOSERIALIZE_SUBOK,` |
|    - | 1795 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|    - | 1796 | `		/* php's own two: IteratorAggregate (NOT Iterator -- the chunk had the` |
|    - | 1797 | `		 * list carry its own cursor) and Countable. */` |
|    - | 1798 | `		{ "DOMNodeList", 0, "IteratorAggregate,Countable", 0,` |
|    - | 1799 | `		  aListMethod, SX_ARRAYSIZE(aListMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),` |
|    - | 1800 | `		  0, &sDomListIterVtab, 0 },` |
|    - | 1801 | `		{ "DOMNamedNodeMap", 0, "IteratorAggregate,Countable", 0,` |
|    - | 1802 | `		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),` |
|    - | 1803 | `		  0, &sDomMapIterVtab, 0 },` |
|    - | 1804 | `		{ "DOMXPath", 0, 0, PH7_CLASS_NOSERIALIZE,` |
|    - | 1805 | `		  aXPathMethod, SX_ARRAYSIZE(aXPathMethod), 0, 0, aXPathProp, SX_ARRAYSIZE(aXPathProp), 0, 0, 0 },` |
|    - | 1806 | `	};` |
| 5151 | 1807 | `	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    5 | 1808 | `}` |
|    - | 1809 |  |
|    - | 1810 | `#else` |
|    - | 1811 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|    - | 1812 | `typedef int vm_dom_unused;` |
|    - | 1813 | `#endif /* PH7_ENABLE_LIBXML */` |
|    - | 1814 |  |
