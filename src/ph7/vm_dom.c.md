# src/ph7/vm_dom.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 656/762 lines (86.09%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#ifdef PH7_ENABLE_LIBXML` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `#include <libxml/parser.h>` |
|      - |    8 | `#include <libxml/tree.h>` |
|      - |    9 | `#include <libxml/c14n.h>` |
|      - |   10 | `#include <libxml/xmlsave.h>` |
|      - |   11 | `#include <libxml/xpath.h>` |
|      - |   12 | `#include <libxml/xmlschemas.h>` |
|      - |   13 |  |
|      - |   14 | `/*` |
|      - |   15 | ` * ext/dom on libxml2: __dom_* native thunks + the DOM class prelude.` |
|      - |   16 | ` *` |
|      - |   17 | ` * Architecture (see also vm_libxml.c): the PHP-visible DOM classes are` |
|      - |   18 | ` * compiled from the zDomLib prelude chunks below and hold two private` |
|      - |   19 | ` * props -- $__res, a phl_domnode resource {phl_xmldoc*, xmlNodePtr}, and` |
|      - |   20 | ` * $__doc, the owning DOMDocument wrapper.  All tree work happens in the` |
|      - |   21 | ` * __dom_* thunks; the prelude does dispatch, identity mapping and the` |
|      - |   22 | ` * php-facing signatures.` |
|      - |   23 | ` *` |
|      - |   24 | ` * Node identity: php guarantees $doc->documentElement === $doc->` |
|      - |   25 | ` * documentElement.  Every wrap goes through __phl_dom_wrap() which keys` |
|      - |   26 | ` * a per-document cache ($doc->__nodes) by __dom_node_id() (the pointer` |
|      - |   27 | ` * value), so the same underlying node always yields the same object.` |
|      - |   28 | ` *` |
|      - |   29 | ` * Tree surgery (append/insert/replace/remove) is done with manual pointer` |
|      - |   30 | ` * splicing instead of xmlAddChild: xmlAddChild MERGES adjacent text nodes` |
|      - |   31 | ` * and frees the merged-away node, which would dangle any PHP wrapper (and` |
|      - |   32 | ` * violates DOM semantics, which php follows -- appendChild never merges).` |
|      - |   33 | ` * Unlinked nodes are parked on the owning phl_xmldoc's orphan set so they` |
|      - |   34 | ` * are freed with the document at VM reset/release.` |
|      - |   35 | ` */` |
|      - |   36 |  |
|      - |   37 | `#define DOM_THUNK(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |   38 |  |
|      - |   39 | `/* Extract a phl_domnode from a thunk argument (NULL if not a resource) */` |
|    738 |   40 | `static phl_domnode * DomNodeArg(ph7_value *pVal)` |
|      1 |   41 | `{` |
|    739 |   42 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|    ! 0 |   43 | `		return 0;` |
|      - |   44 | `	}` |
|    739 |   45 | `	return (phl_domnode *)ph7_value_to_resource(pVal);` |
|    370 |   46 | `}` |
|      - |   47 | `/* Return a (possibly NULL) xmlNode as a fresh phl_domnode resource */` |
|    238 |   48 | `static int DomResultNode(ph7_context *pCtx,phl_xmldoc *pShell,void *pNode)` |
|      1 |   49 | `{` |
|      - |   50 | `	phl_domnode *pWrap;` |
|    239 |   51 | `	if( pNode == 0 ){` |
|      5 |   52 | `		ph7_result_null(pCtx);` |
|      5 |   53 | `		return PH7_OK;` |
|      - |   54 | `	}` |
|    235 |   55 | `	pWrap = (phl_domnode *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_domnode));` |
|    235 |   56 | `	if( pWrap == 0 ){` |
|    ! 0 |   57 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |   58 | `		ph7_result_null(pCtx);` |
|    ! 0 |   59 | `		return PH7_OK;` |
|      - |   60 | `	}` |
|    235 |   61 | `	pWrap->pShell = pShell;` |
|    235 |   62 | `	pWrap->pNode = pNode;` |
|    235 |   63 | `	ph7_result_resource(pCtx,pWrap);` |
|    235 |   64 | `	return PH7_OK;` |
|    120 |   65 | `}` |
|      - |   66 | `/* Orphan bookkeeping: nodes not linked into their tree but still owned */` |
|     20 |   67 | `static void DomOrphanAdd(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |   68 | `{` |
|     21 |   69 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|      - |   70 | `	sxu32 n;` |
|     41 |   71 | `	for( n = 0 ; n < SySetUsed(&pShell->aOrphans) ; ++n ){` |
|     21 |   72 | `		if( apOrphan[n] == pNode ){` |
|    ! 0 |   73 | `			return;` |
|      - |   74 | `		}` |
|     11 |   75 | `	}` |
|     21 |   76 | `	SySetPut(&pShell->aOrphans,(const void *)&pNode);` |
|     11 |   77 | `}` |
|     10 |   78 | `static void DomOrphanRemove(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |   79 | `{` |
|     11 |   80 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|     11 |   81 | `	sxu32 n,nUsed = SySetUsed(&pShell->aOrphans);` |
|     23 |   82 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|     23 |   83 | `		if( apOrphan[n] == pNode ){` |
|     11 |   84 | `			apOrphan[n] = apOrphan[nUsed-1];` |
|     11 |   85 | `			SySetTruncate(&pShell->aOrphans,nUsed-1);` |
|     11 |   86 | `			return;` |
|      - |   87 | `		}` |
|      7 |   88 | `	}` |
|      6 |   89 | `}` |
|      - |   90 | `/* Detach a node from wherever it is (tree or orphan set) prior to linking */` |
|     12 |   91 | `static void DomDetach(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |   92 | `{` |
|     13 |   93 | `	if( pNode->parent ){` |
|      3 |   94 | `		xmlUnlinkNode(pNode);` |
|      2 |   95 | `	}else{` |
|     11 |   96 | `		DomOrphanRemove(pShell,pNode);` |
|      - |   97 | `	}` |
|     13 |   98 | `}` |
|      - |   99 | `/* Raw child-list splicing (no text-node merging -- DOM/php semantics) */` |
|      8 |  100 | `static void DomLinkLast(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      1 |  101 | `{` |
|      9 |  102 | `	pChild->parent = pParent;` |
|      9 |  103 | `	pChild->next = 0;` |
|      9 |  104 | `	if( pParent->last ){` |
|      7 |  105 | `		pParent->last->next = pChild;` |
|      7 |  106 | `		pChild->prev = pParent->last;` |
|      4 |  107 | `	}else{` |
|      3 |  108 | `		pParent->children = pChild;` |
|      3 |  109 | `		pChild->prev = 0;` |
|      - |  110 | `	}` |
|      9 |  111 | `	pParent->last = pChild;` |
|      9 |  112 | `}` |
|      4 |  113 | `static void DomLinkBefore(xmlNodePtr pParent,xmlNodePtr pChild,xmlNodePtr pRef)` |
|      1 |  114 | `{` |
|      5 |  115 | `	pChild->parent = pParent;` |
|      5 |  116 | `	pChild->next = pRef;` |
|      5 |  117 | `	pChild->prev = pRef->prev;` |
|      5 |  118 | `	if( pRef->prev ){` |
|      3 |  119 | `		pRef->prev->next = pChild;` |
|      2 |  120 | `	}else{` |
|      3 |  121 | `		pParent->children = pChild;` |
|      - |  122 | `	}` |
|      5 |  123 | `	pRef->prev = pChild;` |
|      5 |  124 | `}` |
|      - |  125 |  |
|      - |  126 | `/* ===== Node introspection thunks ===== */` |
|      - |  127 |  |
|      - |  128 | `/* int __dom_node_id(res) -- identity-map key (the node pointer) */` |
|    172 |  129 | `DOM_THUNK(vm_builtin_dom_node_id)` |
|      1 |  130 | `{` |
|    173 |  131 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|    173 |  132 | `	if( pNd == 0 ){` |
|    ! 0 |  133 | `		ph7_result_int64(pCtx,0);` |
|    ! 0 |  134 | `		return PH7_OK;` |
|      - |  135 | `	}` |
|    173 |  136 | `	ph7_result_int64(pCtx,(ph7_int64)(sxuptr)pNd->pNode);` |
|    173 |  137 | `	return PH7_OK;` |
|     87 |  138 | `}` |
|      - |  139 | `/* int __dom_node_kind(res) -- the XML_*_NODE type */` |
|    116 |  140 | `DOM_THUNK(vm_builtin_dom_node_kind)` |
|      1 |  141 | `{` |
|    117 |  142 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|    117 |  143 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    117 |  144 | `	ph7_result_int(pCtx,pNode ? (int)pNode->type : 0);` |
|    117 |  145 | `	return PH7_OK;` |
|      1 |  146 | `}` |
|      - |  147 | `/* string __dom_node_name(res) -- php nodeName rules */` |
|     28 |  148 | `DOM_THUNK(vm_builtin_dom_node_name)` |
|      1 |  149 | `{` |
|     29 |  150 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     29 |  151 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     29 |  152 | `	if( pNode == 0 ){` |
|    ! 0 |  153 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  154 | `		return PH7_OK;` |
|      - |  155 | `	}` |
|     29 |  156 | `	switch( pNode->type ){` |
|      7 |  157 | `	case XML_TEXT_NODE:          ph7_result_string(pCtx,"#text",(int)sizeof("#text")-1); break;` |
|      3 |  158 | `	case XML_CDATA_SECTION_NODE: ph7_result_string(pCtx,"#cdata-section",(int)sizeof("#cdata-section")-1); break;` |
|      3 |  159 | `	case XML_COMMENT_NODE:       ph7_result_string(pCtx,"#comment",(int)sizeof("#comment")-1); break;` |
|      1 |  160 | `	case XML_HTML_DOCUMENT_NODE:` |
|      3 |  161 | `	case XML_DOCUMENT_NODE:      ph7_result_string(pCtx,"#document",(int)sizeof("#document")-1); break;` |
|    ! 0 |  162 | `	case XML_DOCUMENT_FRAG_NODE: ph7_result_string(pCtx,"#document-fragment",(int)sizeof("#document-fragment")-1); break;` |
|      8 |  163 | `	default:` |
|     16 |  164 | `		if( (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE)` |
|     17 |  165 | `			&& pNode->ns && pNode->ns->prefix ){` |
|    ! 0 |  166 | `			ph7_result_string_format(pCtx,"%s:%s",(const char *)pNode->ns->prefix,(const char *)pNode->name);` |
|    ! 0 |  167 | `		}else{` |
|     17 |  168 | `			ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|      - |  169 | `		}` |
|     16 |  170 | `		break;` |
|      - |  171 | `	}` |
|     29 |  172 | `	return PH7_OK;` |
|     15 |  173 | `}` |
|      - |  174 | `/* ?string __dom_node_value(res) -- php nodeValue (NULL for documents) */` |
|     28 |  175 | `DOM_THUNK(vm_builtin_dom_node_value)` |
|      1 |  176 | `{` |
|     29 |  177 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     29 |  178 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  179 | `	xmlChar *zContent;` |
|     28 |  180 | `	if( pNode == 0 \|\| pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE` |
|     27 |  181 | `		\|\| pNode->type == XML_DOCUMENT_TYPE_NODE ){` |
|      3 |  182 | `		ph7_result_null(pCtx);` |
|      3 |  183 | `		return PH7_OK;` |
|      - |  184 | `	}` |
|     27 |  185 | `	zContent = xmlNodeGetContent(pNode);` |
|     27 |  186 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|     27 |  187 | `	if( zContent ){` |
|     27 |  188 | `		xmlFree(zContent);` |
|     13 |  189 | `	}` |
|     27 |  190 | `	return PH7_OK;` |
|     15 |  191 | `}` |
|      - |  192 | `/* string __dom_node_text_content(res) */` |
|      6 |  193 | `DOM_THUNK(vm_builtin_dom_node_text_content)` |
|      1 |  194 | `{` |
|      7 |  195 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      7 |  196 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      7 |  197 | `	xmlChar *zContent = pNode ? xmlNodeGetContent(pNode) : 0;` |
|      7 |  198 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|      7 |  199 | `	if( zContent ){` |
|      7 |  200 | `		xmlFree(zContent);` |
|      3 |  201 | `	}` |
|      7 |  202 | `	return PH7_OK;` |
|      1 |  203 | `}` |
|      - |  204 | `/* int __dom_node_line_no(res) */` |
|      6 |  205 | `DOM_THUNK(vm_builtin_dom_node_line_no)` |
|      1 |  206 | `{` |
|      7 |  207 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      7 |  208 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      7 |  209 | `	ph7_result_int64(pCtx,pNode ? (ph7_int64)xmlGetLineNo(pNode) : 0);` |
|      7 |  210 | `	return PH7_OK;` |
|      1 |  211 | `}` |
|      - |  212 | `/* Navigation: parent/first/last/next/prev share one worker */` |
|     40 |  213 | `static int DomNavigate(ph7_context *pCtx,int nArg,ph7_value **apArg,int iDir)` |
|      1 |  214 | `{` |
|     41 |  215 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     41 |  216 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     41 |  217 | `	xmlNodePtr pOut = 0;` |
|     41 |  218 | `	if( pNode ){` |
|     41 |  219 | `		switch( iDir ){` |
|     15 |  220 | `		case 0: pOut = pNode->parent; break;` |
|     21 |  221 | `		case 1: pOut = pNode->children; break;` |
|      3 |  222 | `		case 2: pOut = pNode->last; break;` |
|      3 |  223 | `		case 3: pOut = pNode->next; break;` |
|      3 |  224 | `		case 4: pOut = pNode->prev; break;` |
|      - |  225 | `		}` |
|     20 |  226 | `	}` |
|     41 |  227 | `	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pOut);` |
|      1 |  228 | `}` |
|     15 |  229 | `DOM_THUNK(vm_builtin_dom_node_parent){ return DomNavigate(pCtx,nArg,apArg,0); }` |
|     21 |  230 | `DOM_THUNK(vm_builtin_dom_node_first){ return DomNavigate(pCtx,nArg,apArg,1); }` |
|      3 |  231 | `DOM_THUNK(vm_builtin_dom_node_last){ return DomNavigate(pCtx,nArg,apArg,2); }` |
|      3 |  232 | `DOM_THUNK(vm_builtin_dom_node_next){ return DomNavigate(pCtx,nArg,apArg,3); }` |
|      3 |  233 | `DOM_THUNK(vm_builtin_dom_node_prev){ return DomNavigate(pCtx,nArg,apArg,4); }` |
|      - |  234 | `/* int __dom_node_child_count(res) / ?res __dom_node_child_at(res,i) */` |
|     14 |  235 | `DOM_THUNK(vm_builtin_dom_node_child_count)` |
|      1 |  236 | `{` |
|     15 |  237 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     15 |  238 | `	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;` |
|     15 |  239 | `	int iCount = 0;` |
|     69 |  240 | `	for( ; pChild ; pChild = pChild->next ){` |
|     55 |  241 | `		iCount++;` |
|     28 |  242 | `	}` |
|     15 |  243 | `	ph7_result_int(pCtx,iCount);` |
|     15 |  244 | `	return PH7_OK;` |
|      1 |  245 | `}` |
|     14 |  246 | `DOM_THUNK(vm_builtin_dom_node_child_at)` |
|      1 |  247 | `{` |
|     15 |  248 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     15 |  249 | `	int iWant = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     15 |  250 | `	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;` |
|     39 |  251 | `	for( ; pChild && iWant > 0 ; pChild = pChild->next ){` |
|     25 |  252 | `		iWant--;` |
|     13 |  253 | `	}` |
|     15 |  254 | `	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pChild);` |
|      1 |  255 | `}` |
|      - |  256 | `/* int __dom_node_elem_child_count(res) -- childElementCount */` |
|      2 |  257 | `DOM_THUNK(vm_builtin_dom_node_elem_child_count)` |
|      1 |  258 | `{` |
|      3 |  259 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      3 |  260 | `	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;` |
|      3 |  261 | `	int iCount = 0;` |
|     11 |  262 | `	for( ; pChild ; pChild = pChild->next ){` |
|      9 |  263 | `		if( pChild->type == XML_ELEMENT_NODE ){` |
|      3 |  264 | `			iCount++;` |
|      1 |  265 | `		}` |
|      5 |  266 | `	}` |
|      3 |  267 | `	ph7_result_int(pCtx,iCount);` |
|      3 |  268 | `	return PH7_OK;` |
|      1 |  269 | `}` |
|      - |  270 |  |
|      - |  271 | `/* ===== Tree surgery thunks ===== */` |
|      - |  272 |  |
|      - |  273 | `/* bool __dom_node_append(parentres,childres) */` |
|      8 |  274 | `DOM_THUNK(vm_builtin_dom_node_append)` |
|      1 |  275 | `{` |
|      9 |  276 | `	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      9 |  277 | `	phl_domnode *pChd = nArg > 1 ? DomNodeArg(apArg[1]) : 0;` |
|      - |  278 | `	xmlNodePtr pParent,pChild;` |
|      9 |  279 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|    ! 0 |  280 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  281 | `		return PH7_OK;` |
|      - |  282 | `	}` |
|      9 |  283 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|      9 |  284 | `	pChild = (xmlNodePtr)pChd->pNode;` |
|      9 |  285 | `	if( pParent->doc != pChild->doc \|\| pChild == pParent ){` |
|    ! 0 |  286 | `		ph7_result_bool(pCtx,0); /* Wrong Document Error */` |
|    ! 0 |  287 | `		return PH7_OK;` |
|      - |  288 | `	}` |
|      9 |  289 | `	DomDetach(pChd->pShell,pChild);` |
|      9 |  290 | `	DomLinkLast(pParent,pChild);` |
|      9 |  291 | `	ph7_result_bool(pCtx,1);` |
|      9 |  292 | `	return PH7_OK;` |
|      5 |  293 | `}` |
|      - |  294 | `/* bool __dom_node_insert_before(parentres,newres,?refres) */` |
|      2 |  295 | `DOM_THUNK(vm_builtin_dom_node_insert_before)` |
|      1 |  296 | `{` |
|      3 |  297 | `	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      3 |  298 | `	phl_domnode *pNew = nArg > 1 ? DomNodeArg(apArg[1]) : 0;` |
|      3 |  299 | `	phl_domnode *pRef = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? DomNodeArg(apArg[2]) : 0;` |
|      - |  300 | `	xmlNodePtr pParent,pChild,pAnchor;` |
|      3 |  301 | `	if( pPar == 0 \|\| pNew == 0 ){` |
|    ! 0 |  302 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  303 | `		return PH7_OK;` |
|      - |  304 | `	}` |
|      3 |  305 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|      3 |  306 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|      3 |  307 | `	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;` |
|      2 |  308 | `	if( pParent->doc != pChild->doc \|\| pChild == pParent` |
|      3 |  309 | `		\|\| (pAnchor && pAnchor->parent != pParent) ){` |
|    ! 0 |  310 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  311 | `		return PH7_OK;` |
|      - |  312 | `	}` |
|      3 |  313 | `	DomDetach(pNew->pShell,pChild);` |
|      3 |  314 | `	if( pAnchor ){` |
|      3 |  315 | `		DomLinkBefore(pParent,pChild,pAnchor);` |
|      2 |  316 | `	}else{` |
|    ! 0 |  317 | `		DomLinkLast(pParent,pChild);` |
|      - |  318 | `	}` |
|      3 |  319 | `	ph7_result_bool(pCtx,1);` |
|      3 |  320 | `	return PH7_OK;` |
|      2 |  321 | `}` |
|      - |  322 | `/* bool __dom_node_remove(parentres,childres) */` |
|     10 |  323 | `DOM_THUNK(vm_builtin_dom_node_remove)` |
|      1 |  324 | `{` |
|     11 |  325 | `	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     11 |  326 | `	phl_domnode *pChd = nArg > 1 ? DomNodeArg(apArg[1]) : 0;` |
|      - |  327 | `	xmlNodePtr pChild;` |
|     11 |  328 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|    ! 0 |  329 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  330 | `		return PH7_OK;` |
|      - |  331 | `	}` |
|     11 |  332 | `	pChild = (xmlNodePtr)pChd->pNode;` |
|     11 |  333 | `	if( pChild->parent != (xmlNodePtr)pPar->pNode ){` |
|      3 |  334 | `		ph7_result_bool(pCtx,0); /* Not Found Error */` |
|      3 |  335 | `		return PH7_OK;` |
|      - |  336 | `	}` |
|      9 |  337 | `	xmlUnlinkNode(pChild);` |
|      9 |  338 | `	DomOrphanAdd(pChd->pShell,pChild);` |
|      9 |  339 | `	ph7_result_bool(pCtx,1);` |
|      9 |  340 | `	return PH7_OK;` |
|      6 |  341 | `}` |
|      - |  342 | `/* bool __dom_node_replace(parentres,newres,oldres) */` |
|      2 |  343 | `DOM_THUNK(vm_builtin_dom_node_replace)` |
|      1 |  344 | `{` |
|      3 |  345 | `	phl_domnode *pPar = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|      3 |  346 | `	phl_domnode *pNew = nArg > 2 ? DomNodeArg(apArg[1]) : 0;` |
|      3 |  347 | `	phl_domnode *pOld = nArg > 2 ? DomNodeArg(apArg[2]) : 0;` |
|      - |  348 | `	xmlNodePtr pParent,pChild,pVictim;` |
|      3 |  349 | `	if( pPar == 0 \|\| pNew == 0 \|\| pOld == 0 ){` |
|    ! 0 |  350 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  351 | `		return PH7_OK;` |
|      - |  352 | `	}` |
|      3 |  353 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|      3 |  354 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|      3 |  355 | `	pVictim = (xmlNodePtr)pOld->pNode;` |
|      3 |  356 | `	if( pParent->doc != pChild->doc \|\| pVictim->parent != pParent \|\| pChild == pParent ){` |
|    ! 0 |  357 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  358 | `		return PH7_OK;` |
|      - |  359 | `	}` |
|      3 |  360 | `	if( pChild != pVictim ){` |
|      3 |  361 | `		DomDetach(pNew->pShell,pChild);` |
|      3 |  362 | `		DomLinkBefore(pParent,pChild,pVictim);` |
|      3 |  363 | `		xmlUnlinkNode(pVictim);` |
|      3 |  364 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|      1 |  365 | `	}` |
|      3 |  366 | `	ph7_result_bool(pCtx,1);` |
|      3 |  367 | `	return PH7_OK;` |
|      2 |  368 | `}` |
|      - |  369 |  |
|      - |  370 | `/* ===== Element attribute thunks ===== */` |
|      - |  371 |  |
|      - |  372 | `/* string __dom_elem_get_attr(res,name) -- "" when absent (php) */` |
|     20 |  373 | `DOM_THUNK(vm_builtin_dom_elem_get_attr)` |
|      1 |  374 | `{` |
|     21 |  375 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     21 |  376 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     21 |  377 | `	xmlChar *zVal = pNd ? xmlGetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;` |
|     21 |  378 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|     21 |  379 | `	if( zVal ){` |
|     19 |  380 | `		xmlFree(zVal);` |
|      9 |  381 | `	}` |
|     21 |  382 | `	return PH7_OK;` |
|      1 |  383 | `}` |
|      - |  384 | `/* bool __dom_elem_has_attr(res,name) */` |
|      4 |  385 | `DOM_THUNK(vm_builtin_dom_elem_has_attr)` |
|      1 |  386 | `{` |
|      5 |  387 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  388 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  389 | `	ph7_result_bool(pCtx,pNd && xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) != 0);` |
|      5 |  390 | `	return PH7_OK;` |
|      1 |  391 | `}` |
|      - |  392 | `/* bool __dom_elem_set_attr(res,name,value) */` |
|      4 |  393 | `DOM_THUNK(vm_builtin_dom_elem_set_attr)` |
|      1 |  394 | `{` |
|      5 |  395 | `	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  396 | `	const char *zName = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  397 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|      5 |  398 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|    ! 0 |  399 | `		ph7_result_bool(pCtx,0); /* Invalid Character Error */` |
|    ! 0 |  400 | `		return PH7_OK;` |
|      - |  401 | `	}` |
|      5 |  402 | `	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);` |
|      5 |  403 | `	ph7_result_bool(pCtx,1);` |
|      5 |  404 | `	return PH7_OK;` |
|      3 |  405 | `}` |
|      - |  406 | `/* bool __dom_elem_remove_attr(res,name) */` |
|      4 |  407 | `DOM_THUNK(vm_builtin_dom_elem_remove_attr)` |
|      1 |  408 | `{` |
|      5 |  409 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  410 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  411 | `	xmlAttrPtr pAttr = pNd ? xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;` |
|      5 |  412 | `	if( pAttr == 0 \|\| pAttr->type != XML_ATTRIBUTE_NODE ){` |
|      - |  413 | `		/* Absent (or a DTD default): php returns false */` |
|      3 |  414 | `		ph7_result_bool(pCtx,0);` |
|      3 |  415 | `		return PH7_OK;` |
|      - |  416 | `	}` |
|      3 |  417 | `	xmlRemoveProp(pAttr);` |
|      3 |  418 | `	ph7_result_bool(pCtx,1);` |
|      3 |  419 | `	return PH7_OK;` |
|      3 |  420 | `}` |
|      - |  421 | `/* string __dom_elem_get_attr_ns(res,uri,local) */` |
|      4 |  422 | `DOM_THUNK(vm_builtin_dom_elem_get_attr_ns)` |
|      1 |  423 | `{` |
|      5 |  424 | `	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  425 | `	const char *zUri = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  426 | `	const char *zLocal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|      5 |  427 | `	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,(const xmlChar *)zUri) : 0;` |
|      5 |  428 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|      5 |  429 | `	if( zVal ){` |
|      3 |  430 | `		xmlFree(zVal);` |
|      1 |  431 | `	}` |
|      5 |  432 | `	return PH7_OK;` |
|      1 |  433 | `}` |
|      - |  434 | `/* bool __dom_elem_set_attr_ns(res,uri,qname,value) */` |
|      2 |  435 | `DOM_THUNK(vm_builtin_dom_elem_set_attr_ns)` |
|      1 |  436 | `{` |
|      3 |  437 | `	phl_domnode *pNd = nArg > 3 ? DomNodeArg(apArg[0]) : 0;` |
|      3 |  438 | `	const char *zUri = nArg > 3 ? ph7_value_to_string(apArg[1],0) : "";` |
|      3 |  439 | `	const char *zQname = nArg > 3 ? ph7_value_to_string(apArg[2],0) : "";` |
|      3 |  440 | `	const char *zVal = nArg > 3 ? ph7_value_to_string(apArg[3],0) : "";` |
|      3 |  441 | `	sxu32 nColon = 0;` |
|      - |  442 | `	xmlNodePtr pNode;` |
|      - |  443 | `	xmlNsPtr pNs;` |
|      3 |  444 | `	if( pNd == 0 \|\| zQname[0] == 0 ){` |
|    ! 0 |  445 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  446 | `		return PH7_OK;` |
|      - |  447 | `	}` |
|      3 |  448 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|      3 |  449 | `	if( SyByteFind(zQname,SyStrlen(zQname),':',&nColon) == SXRET_OK ){` |
|      - |  450 | `		/* Prefixed: find (or declare on this element) the namespace */` |
|      - |  451 | `		char zPrefix[128];` |
|      3 |  452 | `		if( nColon >= sizeof(zPrefix) ){` |
|    ! 0 |  453 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  454 | `			return PH7_OK;` |
|      - |  455 | `		}` |
|      3 |  456 | `		SyMemcpy(zQname,zPrefix,nColon);` |
|      3 |  457 | `		zPrefix[nColon] = 0;` |
|      3 |  458 | `		pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);` |
|      3 |  459 | `		if( pNs == 0 ){` |
|      3 |  460 | `			pNs = xmlNewNs(pNode,(const xmlChar *)zUri,(const xmlChar *)zPrefix);` |
|      1 |  461 | `		}` |
|      3 |  462 | `		if( pNs == 0 ){` |
|    ! 0 |  463 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  464 | `			return PH7_OK;` |
|      - |  465 | `		}` |
|      3 |  466 | `		xmlSetNsProp(pNode,pNs,(const xmlChar *)(zQname+nColon+1),(const xmlChar *)zVal);` |
|      2 |  467 | `	}else{` |
|    ! 0 |  468 | `		pNs = zUri[0] ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;` |
|    ! 0 |  469 | `		xmlSetNsProp(pNode,pNs,(const xmlChar *)zQname,(const xmlChar *)zVal);` |
|      - |  470 | `	}` |
|      3 |  471 | `	ph7_result_bool(pCtx,1);` |
|      3 |  472 | `	return PH7_OK;` |
|      2 |  473 | `}` |
|      - |  474 | `/* ?res __dom_elem_attr_node(res,name) -- the attribute NODE by name */` |
|      4 |  475 | `DOM_THUNK(vm_builtin_dom_elem_attr_node)` |
|      1 |  476 | `{` |
|      5 |  477 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  478 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  479 | `	xmlAttrPtr pAttr = pNd ? xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;` |
|      5 |  480 | `	if( pAttr == 0 \|\| pAttr->type != XML_ATTRIBUTE_NODE ){` |
|    ! 0 |  481 | `		ph7_result_null(pCtx);` |
|    ! 0 |  482 | `		return PH7_OK;` |
|      - |  483 | `	}` |
|      5 |  484 | `	return DomResultNode(pCtx,pNd->pShell,(xmlNodePtr)pAttr);` |
|      3 |  485 | `}` |
|      - |  486 | `/* int __dom_elem_attr_count(res) / ?res __dom_elem_attr_at(res,i) */` |
|      4 |  487 | `DOM_THUNK(vm_builtin_dom_elem_attr_count)` |
|      1 |  488 | `{` |
|      5 |  489 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  490 | `	xmlAttrPtr pAttr = (pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE)` |
|      6 |  491 | `		? ((xmlNodePtr)pNd->pNode)->properties : 0;` |
|      5 |  492 | `	int iCount = 0;` |
|     17 |  493 | `	for( ; pAttr ; pAttr = pAttr->next ){` |
|     13 |  494 | `		iCount++;` |
|      7 |  495 | `	}` |
|      5 |  496 | `	ph7_result_int(pCtx,iCount);` |
|      5 |  497 | `	return PH7_OK;` |
|      1 |  498 | `}` |
|     12 |  499 | `DOM_THUNK(vm_builtin_dom_elem_attr_at)` |
|      1 |  500 | `{` |
|     13 |  501 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     13 |  502 | `	int iWant = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     13 |  503 | `	xmlAttrPtr pAttr = (pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE)` |
|     18 |  504 | `		? ((xmlNodePtr)pNd->pNode)->properties : 0;` |
|     25 |  505 | `	for( ; pAttr && iWant > 0 ; pAttr = pAttr->next ){` |
|     13 |  506 | `		iWant--;` |
|      7 |  507 | `	}` |
|     13 |  508 | `	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,(xmlNodePtr)pAttr);` |
|      1 |  509 | `}` |
|      - |  510 |  |
|      - |  511 | `/* ===== getElementsByTagName (live) ===== */` |
|      - |  512 |  |
|      - |  513 | `/* Document-order successor within pRoot's subtree (pRoot excluded) */` |
|    152 |  514 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot)` |
|      1 |  515 | `{` |
|    153 |  516 | `	if( pCur->children ){` |
|     57 |  517 | `		return pCur->children;` |
|      - |  518 | `	}` |
|    155 |  519 | `	while( pCur && pCur != pRoot ){` |
|    125 |  520 | `		if( pCur->next ){` |
|     67 |  521 | `			return pCur->next;` |
|      - |  522 | `		}` |
|     59 |  523 | `		pCur = pCur->parent;` |
|      1 |  524 | `	}` |
|     31 |  525 | `	return 0;` |
|     77 |  526 | `}` |
|    174 |  527 | `static int DomGebtnMatch(xmlNodePtr pNode,const char *zName)` |
|      1 |  528 | `{` |
|    175 |  529 | `	if( pNode->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  530 | `		return 0;` |
|      - |  531 | `	}` |
|    175 |  532 | `	if( zName[0] == '*' && zName[1] == 0 ){` |
|      3 |  533 | `		return 1;` |
|      - |  534 | `	}` |
|    173 |  535 | `	return xmlStrEqual(pNode->name,(const xmlChar *)zName) != 0;` |
|     88 |  536 | `}` |
|      - |  537 | `/* int __dom_gebtn_count(res,name) / ?res __dom_gebtn_at(res,name,i) */` |
|     28 |  538 | `DOM_THUNK(vm_builtin_dom_gebtn_count)` |
|      1 |  539 | `{` |
|     29 |  540 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     29 |  541 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     29 |  542 | `	xmlNodePtr pRoot = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     29 |  543 | `	xmlNodePtr pCur = pRoot ? pRoot->children : 0;` |
|     29 |  544 | `	int iCount = 0;` |
|    127 |  545 | `	while( pCur ){` |
|     99 |  546 | `		if( DomGebtnMatch(pCur,zName) ){` |
|     73 |  547 | `			iCount++;` |
|     36 |  548 | `		}` |
|     99 |  549 | `		pCur = DomWalkNext(pCur,pRoot);` |
|      1 |  550 | `	}` |
|     29 |  551 | `	ph7_result_int(pCtx,iCount);` |
|     29 |  552 | `	return PH7_OK;` |
|      1 |  553 | `}` |
|     24 |  554 | `DOM_THUNK(vm_builtin_dom_gebtn_at)` |
|      1 |  555 | `{` |
|     25 |  556 | `	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|     25 |  557 | `	const char *zName = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|     25 |  558 | `	int iWant = nArg > 2 ? ph7_value_to_int(apArg[2]) : 0;` |
|     25 |  559 | `	xmlNodePtr pRoot = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     25 |  560 | `	xmlNodePtr pCur = pRoot ? pRoot->children : 0;` |
|     79 |  561 | `	while( pCur ){` |
|     77 |  562 | `		if( DomGebtnMatch(pCur,zName) ){` |
|     37 |  563 | `			if( iWant == 0 ){` |
|     23 |  564 | `				return DomResultNode(pCtx,pNd->pShell,pCur);` |
|      - |  565 | `			}` |
|     15 |  566 | `			iWant--;` |
|      7 |  567 | `		}` |
|     55 |  568 | `		pCur = DomWalkNext(pCur,pRoot);` |
|      1 |  569 | `	}` |
|      3 |  570 | `	ph7_result_null(pCtx);` |
|      3 |  571 | `	return PH7_OK;` |
|     13 |  572 | `}` |
|      - |  573 |  |
|      - |  574 | `/* ===== Document thunks ===== */` |
|      - |  575 |  |
|      - |  576 | `/* res __dom_doc_new(version,encoding) */` |
|     50 |  577 | `DOM_THUNK(vm_builtin_dom_doc_new)` |
|      1 |  578 | `{` |
|     51 |  579 | `	ph7_vm *pVm = pCtx->pVm;` |
|     51 |  580 | `	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";` |
|     51 |  581 | `	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  582 | `	xmlDocPtr pDoc;` |
|      - |  583 | `	phl_xmldoc *pShell;` |
|     51 |  584 | `	pDoc = xmlNewDoc((const xmlChar *)(zVersion[0] ? zVersion : "1.0"));` |
|     51 |  585 | `	if( pDoc == 0 ){` |
|    ! 0 |  586 | `		ph7_result_null(pCtx);` |
|    ! 0 |  587 | `		return PH7_OK;` |
|      - |  588 | `	}` |
|     51 |  589 | `	if( zEncoding[0] ){` |
|    ! 0 |  590 | `		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);` |
|    ! 0 |  591 | `	}` |
|     51 |  592 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|     51 |  593 | `	if( pShell == 0 ){` |
|    ! 0 |  594 | `		xmlFreeDoc(pDoc);` |
|    ! 0 |  595 | `		ph7_result_null(pCtx);` |
|    ! 0 |  596 | `		return PH7_OK;` |
|      - |  597 | `	}` |
|     51 |  598 | `	return DomResultNode(pCtx,pShell,(xmlNodePtr)pDoc);` |
|     26 |  599 | `}` |
|      - |  600 | `/* res\|false __dom_doc_loadxml(source,preserveWS,options) -- a NEW doc resource */` |
|     50 |  601 | `DOM_THUNK(vm_builtin_dom_doc_loadxml)` |
|      1 |  602 | `{` |
|     51 |  603 | `	ph7_vm *pVm = pCtx->pVm;` |
|     51 |  604 | `	int nLen = 0;` |
|     51 |  605 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";` |
|     51 |  606 | `	int bPreserve = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|     51 |  607 | `	int iOpts = nArg > 2 ? ph7_value_to_int(apArg[2]) : 0;` |
|      - |  608 | `	xmlDocPtr pDoc;` |
|      - |  609 | `	phl_xmldoc *pShell;` |
|      - |  610 | `	sxu32 nMark;` |
|     51 |  611 | `	if( !bPreserve ){` |
|     15 |  612 | `		iOpts \|= XML_PARSE_NOBLANKS;` |
|      7 |  613 | `	}` |
|     51 |  614 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     51 |  615 | `	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);` |
|     51 |  616 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::loadXML");` |
|     51 |  617 | `	if( pDoc == 0 ){` |
|      9 |  618 | `		ph7_result_bool(pCtx,0);` |
|      9 |  619 | `		return PH7_OK;` |
|      - |  620 | `	}` |
|     43 |  621 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|     43 |  622 | `	if( pShell == 0 ){` |
|    ! 0 |  623 | `		xmlFreeDoc(pDoc);` |
|    ! 0 |  624 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  625 | `		return PH7_OK;` |
|      - |  626 | `	}` |
|     43 |  627 | `	return DomResultNode(pCtx,pShell,(xmlNodePtr)pDoc);` |
|     26 |  628 | `}` |
|      - |  629 | `/* ?res __dom_doc_root(docres) -- documentElement */` |
|     44 |  630 | `DOM_THUNK(vm_builtin_dom_doc_root)` |
|      1 |  631 | `{` |
|     45 |  632 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     45 |  633 | `	xmlNodePtr pRoot = pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0;` |
|     45 |  634 | `	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pRoot);` |
|      1 |  635 | `}` |
|      - |  636 | `/* string\|false __dom_doc_savexml(docres,?noderes,format) */` |
|     20 |  637 | `DOM_THUNK(vm_builtin_dom_doc_savexml)` |
|      1 |  638 | `{` |
|     21 |  639 | `	ph7_vm *pVm = pCtx->pVm;` |
|     21 |  640 | `	phl_domnode *pDocNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|     21 |  641 | `	phl_domnode *pTgt = (nArg > 2 && !ph7_value_is_null(apArg[1])) ? DomNodeArg(apArg[1]) : 0;` |
|     21 |  642 | `	int bFormat = nArg > 2 ? ph7_value_to_bool(apArg[2]) : 0;` |
|      - |  643 | `	xmlDocPtr pDoc;` |
|      - |  644 | `	sxu32 nMark;` |
|     21 |  645 | `	if( pDocNd == 0 ){` |
|    ! 0 |  646 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  647 | `		return PH7_OK;` |
|      - |  648 | `	}` |
|     21 |  649 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|     21 |  650 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     28 |  651 | `	if( pTgt == 0 \|\| pTgt->pNode == pDocNd->pNode ){` |
|     15 |  652 | `		xmlChar *zOut = 0;` |
|     15 |  653 | `		int nOut = 0;` |
|     15 |  654 | `		xmlDocDumpFormatMemory(pDoc,&zOut,&nOut,bFormat ? 1 : 0);` |
|     15 |  655 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|     15 |  656 | `		if( zOut == 0 ){` |
|    ! 0 |  657 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  658 | `			return PH7_OK;` |
|      - |  659 | `		}` |
|     15 |  660 | `		ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|     15 |  661 | `		xmlFree(zOut);` |
|      8 |  662 | `	}else{` |
|      7 |  663 | `		xmlBufferPtr pBuf = xmlBufferCreate();` |
|      - |  664 | `		int rc;` |
|      7 |  665 | `		if( pBuf == 0 ){` |
|    ! 0 |  666 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|    ! 0 |  667 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  668 | `			return PH7_OK;` |
|      - |  669 | `		}` |
|      7 |  670 | `		rc = xmlNodeDump(pBuf,pDoc,(xmlNodePtr)pTgt->pNode,0,bFormat ? 1 : 0);` |
|      7 |  671 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|      7 |  672 | `		if( rc < 0 ){` |
|    ! 0 |  673 | `			xmlBufferFree(pBuf);` |
|    ! 0 |  674 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  675 | `			return PH7_OK;` |
|      - |  676 | `		}` |
|      7 |  677 | `		ph7_result_string(pCtx,(const char *)xmlBufferContent(pBuf),(int)xmlBufferLength(pBuf));` |
|      7 |  678 | `		xmlBufferFree(pBuf);` |
|      - |  679 | `	}` |
|     21 |  680 | `	return PH7_OK;` |
|     11 |  681 | `}` |
|      - |  682 | `/* res\|false __dom_doc_create(docres,kind,name,value) -- kind: 1 element,` |
|      - |  683 | ` * 3 text, 4 cdata, 8 comment.  Fresh nodes start as orphans. */` |
|     10 |  684 | `DOM_THUNK(vm_builtin_dom_doc_create)` |
|      1 |  685 | `{` |
|     11 |  686 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 |  687 | `	phl_domnode *pDocNd = nArg > 3 ? DomNodeArg(apArg[0]) : 0;` |
|     11 |  688 | `	int iKind = nArg > 3 ? ph7_value_to_int(apArg[1]) : 0;` |
|     11 |  689 | `	const char *zName = nArg > 3 ? ph7_value_to_string(apArg[2],0) : "";` |
|     11 |  690 | `	int nVal = 0;` |
|     11 |  691 | `	const char *zVal = nArg > 3 ? ph7_value_to_string(apArg[3],&nVal) : "";` |
|      - |  692 | `	xmlDocPtr pDoc;` |
|     11 |  693 | `	xmlNodePtr pNode = 0;` |
|      - |  694 | `	sxu32 nMark;` |
|     11 |  695 | `	if( pDocNd == 0 ){` |
|    ! 0 |  696 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  697 | `		return PH7_OK;` |
|      - |  698 | `	}` |
|     11 |  699 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|     11 |  700 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     11 |  701 | `	switch( iKind ){` |
|      2 |  702 | `	case XML_ELEMENT_NODE:` |
|      5 |  703 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|    ! 0 |  704 | `			break; /* Invalid Character Error */` |
|      - |  705 | `		}` |
|      - |  706 | `		/* php passes the value through xmlNewDocNode, which entity-parses` |
|      - |  707 | `		 * it (quirk preserved: bad entities warn and drop the content). */` |
|      5 |  708 | `		pNode = xmlNewDocNode(pDoc,0,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|      5 |  709 | `		break;` |
|      1 |  710 | `	case XML_TEXT_NODE:` |
|      3 |  711 | `		pNode = xmlNewDocText(pDoc,(const xmlChar *)zVal);` |
|      3 |  712 | `		break;` |
|      1 |  713 | `	case XML_CDATA_SECTION_NODE:` |
|      3 |  714 | `		pNode = xmlNewCDataBlock(pDoc,(const xmlChar *)zVal,nVal);` |
|      3 |  715 | `		break;` |
|      1 |  716 | `	case XML_COMMENT_NODE:` |
|      3 |  717 | `		pNode = xmlNewDocComment(pDoc,(const xmlChar *)zVal);` |
|      2 |  718 | `		break;` |
|      - |  719 | `	}` |
|     11 |  720 | `	PH7_LibxmlCaptureEnd(pVm,nMark,iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");` |
|     11 |  721 | `	if( pNode == 0 ){` |
|    ! 0 |  722 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  723 | `		return PH7_OK;` |
|      - |  724 | `	}` |
|     11 |  725 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|     11 |  726 | `	return DomResultNode(pCtx,pDocNd->pShell,pNode);` |
|      6 |  727 | `}` |
|      - |  728 | `/* void __dom_doc_normalize(docres) -- merge adjacent text nodes.  Merged-` |
|      - |  729 | ` * away siblings are PARKED as orphans, never freed, so any PHP wrapper to` |
|      - |  730 | ` * them stays valid (they just become empty orphans). */` |
|     24 |  731 | `static void DomNormalizeTree(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |  732 | `{` |
|     25 |  733 | `	xmlNodePtr pChild = pNode->children;` |
|     49 |  734 | `	while( pChild ){` |
|     25 |  735 | `		if( pChild->type == XML_TEXT_NODE ){` |
|      7 |  736 | `			while( pChild->next && pChild->next->type == XML_TEXT_NODE ){` |
|    ! 0 |  737 | `				xmlNodePtr pNext = pChild->next;` |
|    ! 0 |  738 | `				if( pNext->content ){` |
|    ! 0 |  739 | `					xmlNodeAddContent(pChild,pNext->content);` |
|    ! 0 |  740 | `				}` |
|    ! 0 |  741 | `				xmlUnlinkNode(pNext);` |
|    ! 0 |  742 | `				DomOrphanAdd(pShell,pNext);` |
|    ! 0 |  743 | `			}` |
|     22 |  744 | `		}else if( pChild->type == XML_ELEMENT_NODE ){` |
|     19 |  745 | `			DomNormalizeTree(pShell,pChild);` |
|      9 |  746 | `		}` |
|     25 |  747 | `		pChild = pChild->next;` |
|      1 |  748 | `	}` |
|     25 |  749 | `}` |
|      6 |  750 | `DOM_THUNK(vm_builtin_dom_doc_normalize)` |
|      1 |  751 | `{` |
|      7 |  752 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      7 |  753 | `	if( pNd ){` |
|      7 |  754 | `		DomNormalizeTree(pNd->pShell,(xmlNodePtr)pNd->pNode);` |
|      3 |  755 | `	}` |
|      7 |  756 | `	ph7_result_null(pCtx);` |
|      7 |  757 | `	return PH7_OK;` |
|      1 |  758 | `}` |
|      - |  759 |  |
|      - |  760 | `/* ===== C14N ===== */` |
|      - |  761 |  |
|      - |  762 | `/* Visibility callback: keep only the target's subtree (attrs/ns follow` |
|      - |  763 | ` * their owning element) -- the same shape php's ext/dom uses. */` |
|     10 |  764 | `static int DomC14NIsVisible(void *pUserData,xmlNodePtr pNode,xmlNodePtr pParent)` |
|      1 |  765 | `{` |
|     11 |  766 | `	xmlNodePtr pTarget = (xmlNodePtr)pUserData;` |
|      - |  767 | `	xmlNodePtr p;` |
|     11 |  768 | `	if( pNode->type == XML_NAMESPACE_DECL ){` |
|    ! 0 |  769 | `		p = pParent;` |
|     11 |  770 | `	}else if( pNode->type == XML_ATTRIBUTE_NODE ){` |
|      5 |  771 | `		p = pNode->parent;` |
|      3 |  772 | `	}else{` |
|      7 |  773 | `		p = pNode;` |
|      - |  774 | `	}` |
|     27 |  775 | `	while( p ){` |
|     19 |  776 | `		if( p == pTarget ){` |
|      3 |  777 | `			return 1;` |
|      - |  778 | `		}` |
|     17 |  779 | `		p = p->parent;` |
|      1 |  780 | `	}` |
|      9 |  781 | `	return 0;` |
|      6 |  782 | `}` |
|      - |  783 | `/* string __dom_node_c14n(res) -- "" on canonicalization failure (php` |
|      - |  784 | ` * returns an empty string for empty/unserializable input) */` |
|     12 |  785 | `DOM_THUNK(vm_builtin_dom_node_c14n)` |
|      1 |  786 | `{` |
|     13 |  787 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  788 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     13 |  789 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  790 | `	sxu32 nMark;` |
|     13 |  791 | `	if( pNode == 0 \|\| pNode->doc == 0 ){` |
|    ! 0 |  792 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  793 | `		return PH7_OK;` |
|      - |  794 | `	}` |
|     13 |  795 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     18 |  796 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|     11 |  797 | `		xmlChar *zOut = 0;` |
|     11 |  798 | `		int nOut = xmlC14NDocDumpMemory((xmlDocPtr)pNode,0,XML_C14N_1_0,0,0,&zOut);` |
|     11 |  799 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::C14N");` |
|     11 |  800 | `		if( nOut < 0 \|\| zOut == 0 ){` |
|    ! 0 |  801 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 |  802 | `		}else{` |
|     11 |  803 | `			ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|      - |  804 | `		}` |
|     11 |  805 | `		if( zOut ){` |
|     11 |  806 | `			xmlFree(zOut);` |
|      5 |  807 | `		}` |
|      6 |  808 | `	}else{` |
|      3 |  809 | `		xmlOutputBufferPtr pOut = xmlAllocOutputBuffer(0);` |
|      3 |  810 | `		int rc = -1;` |
|      3 |  811 | `		if( pOut ){` |
|      3 |  812 | `			rc = xmlC14NExecute(pNode->doc,DomC14NIsVisible,pNode,XML_C14N_1_0,0,0,pOut);` |
|      1 |  813 | `		}` |
|      3 |  814 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::C14N");` |
|      3 |  815 | `		if( pOut == 0 \|\| rc < 0 ){` |
|    ! 0 |  816 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 |  817 | `		}else{` |
|      4 |  818 | `			ph7_result_string(pCtx,(const char *)xmlOutputBufferGetContent(pOut),` |
|      2 |  819 | `				(int)xmlOutputBufferGetSize(pOut));` |
|      - |  820 | `		}` |
|      3 |  821 | `		if( pOut ){` |
|      3 |  822 | `			xmlOutputBufferClose(pOut);` |
|      1 |  823 | `		}` |
|      - |  824 | `	}` |
|     13 |  825 | `	return PH7_OK;` |
|      7 |  826 | `}` |
|      - |  827 |  |
|      - |  828 | `/* ===== DOMXPath ===== */` |
|      - |  829 |  |
|      - |  830 | `/* array\|false __dom_xpath_query(docres,expr,?ctxnoderes) -- snapshot array` |
|      - |  831 | ` * of node resources in document order, or false on an invalid expression` |
|      - |  832 | ` * or a non-nodeset result (php's DOMXPath::query contract). */` |
|     42 |  833 | `DOM_THUNK(vm_builtin_dom_xpath_query)` |
|      1 |  834 | `{` |
|     43 |  835 | `	ph7_vm *pVm = pCtx->pVm;` |
|     43 |  836 | `	phl_domnode *pDocNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     43 |  837 | `	const char *zExpr = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     43 |  838 | `	phl_domnode *pCtxNd = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? DomNodeArg(apArg[2]) : 0;` |
|      - |  839 | `	xmlXPathContextPtr pXCtx;` |
|      - |  840 | `	xmlXPathObjectPtr pObj;` |
|      - |  841 | `	ph7_value *pList;` |
|      - |  842 | `	sxu32 nMark;` |
|     43 |  843 | `	if( pDocNd == 0 ){` |
|    ! 0 |  844 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  845 | `		return PH7_OK;` |
|      - |  846 | `	}` |
|     43 |  847 | `	pXCtx = xmlXPathNewContext((xmlDocPtr)pDocNd->pNode);` |
|     43 |  848 | `	if( pXCtx == 0 ){` |
|    ! 0 |  849 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  850 | `		return PH7_OK;` |
|      - |  851 | `	}` |
|      - |  852 | `	/* With no explicit context node php evaluates relative expressions` |
|      - |  853 | `	 * against the document ELEMENT (so query('file') matches a child of` |
|      - |  854 | `	 * the root), not the document node -- match that. */` |
|     43 |  855 | `	if( pCtxNd ){` |
|     11 |  856 | `		pXCtx->node = (xmlNodePtr)pCtxNd->pNode;` |
|      6 |  857 | `	}else{` |
|     33 |  858 | `		pXCtx->node = xmlDocGetRootElement((xmlDocPtr)pDocNd->pNode);` |
|      - |  859 | `	}` |
|     43 |  860 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     43 |  861 | `	pObj = xmlXPathEvalExpression((const xmlChar *)zExpr,pXCtx);` |
|     43 |  862 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMXPath::query");` |
|     43 |  863 | `	if( pObj == 0 \|\| pObj->type != XPATH_NODESET ){` |
|      3 |  864 | `		if( pObj ){` |
|    ! 0 |  865 | `			xmlXPathFreeObject(pObj);` |
|    ! 0 |  866 | `		}` |
|      3 |  867 | `		xmlXPathFreeContext(pXCtx);` |
|      3 |  868 | `		ph7_result_bool(pCtx,0);` |
|      3 |  869 | `		return PH7_OK;` |
|      - |  870 | `	}` |
|     41 |  871 | `	pList = ph7_context_new_array(pCtx);` |
|     41 |  872 | `	if( pList == 0 ){` |
|    ! 0 |  873 | `		xmlXPathFreeObject(pObj);` |
|    ! 0 |  874 | `		xmlXPathFreeContext(pXCtx);` |
|    ! 0 |  875 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |  876 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  877 | `		return PH7_OK;` |
|      - |  878 | `	}` |
|     41 |  879 | `	if( pObj->nodesetval ){` |
|      - |  880 | `		int i;` |
|     95 |  881 | `		for( i = 0 ; i < pObj->nodesetval->nodeNr ; i++ ){` |
|     55 |  882 | `			xmlNodePtr pNode = pObj->nodesetval->nodeTab[i];` |
|      - |  883 | `			phl_domnode *pWrap;` |
|      - |  884 | `			ph7_value *pRes;` |
|     55 |  885 | `			if( pNode == 0 \|\| pNode->type == XML_NAMESPACE_DECL ){` |
|    ! 0 |  886 | `				continue; /* namespace pseudo-nodes are not exposed */` |
|      - |  887 | `			}` |
|     55 |  888 | `			pWrap = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));` |
|     55 |  889 | `			pRes = ph7_context_new_scalar(pCtx);` |
|     55 |  890 | `			if( pWrap == 0 \|\| pRes == 0 ){` |
|    ! 0 |  891 | `				break;` |
|      - |  892 | `			}` |
|     55 |  893 | `			pWrap->pShell = pDocNd->pShell;` |
|     55 |  894 | `			pWrap->pNode = pNode;` |
|     55 |  895 | `			ph7_value_resource(pRes,pWrap);` |
|     55 |  896 | `			ph7_array_add_elem(pList,0,pRes);` |
|     28 |  897 | `		}` |
|     20 |  898 | `	}` |
|     41 |  899 | `	xmlXPathFreeObject(pObj);` |
|     41 |  900 | `	xmlXPathFreeContext(pXCtx);` |
|     41 |  901 | `	ph7_result_value(pCtx,pList);` |
|     41 |  902 | `	return PH7_OK;` |
|     22 |  903 | `}` |
|      - |  904 |  |
|      - |  905 | `/* ===== Schema validation ===== */` |
|      - |  906 |  |
|      - |  907 | `/* Schema parser/validator diagnostics: forward onto the shared per-VM queue` |
|      - |  908 | ` * via PH7_LibxmlQueueError, exactly like the global structured handler. */` |
|      - |  909 | `#if LIBXML_VERSION >= 21200` |
|      2 |  910 | `static void DomSchemaErr(void *pUserData,const xmlError *pErr)` |
|      - |  911 | `#else` |
|      2 |  912 | `static void DomSchemaErr(void *pUserData,xmlErrorPtr pErr)` |
|      - |  913 | `#endif` |
|      1 |  914 | `{` |
|      5 |  915 | `	if( pErr == 0 ){` |
|    ! 0 |  916 | `		return;` |
|      - |  917 | `	}` |
|      7 |  918 | `	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,` |
|      4 |  919 | `		pErr->int2,pErr->message,pErr->file);` |
|      3 |  920 | `}` |
|      - |  921 | `/* bool __dom_doc_schema_validate_source(docres,xsdSource) */` |
|      4 |  922 | `DOM_THUNK(vm_builtin_dom_doc_schema_validate_source)` |
|      1 |  923 | `{` |
|      5 |  924 | `	ph7_vm *pVm = pCtx->pVm;` |
|      5 |  925 | `	phl_domnode *pDocNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  926 | `	int nXsd = 0;` |
|      5 |  927 | `	const char *zXsd = nArg > 1 ? ph7_value_to_string(apArg[1],&nXsd) : "";` |
|      - |  928 | `	xmlSchemaParserCtxtPtr pParser;` |
|      - |  929 | `	xmlSchemaPtr pSchema;` |
|      - |  930 | `	xmlSchemaValidCtxtPtr pValid;` |
|      - |  931 | `	int rc;` |
|      - |  932 | `	sxu32 nMark;` |
|      5 |  933 | `	if( pDocNd == 0 \|\| nXsd < 1 ){` |
|    ! 0 |  934 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  935 | `		return PH7_OK;` |
|      - |  936 | `	}` |
|      5 |  937 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|      5 |  938 | `	pParser = xmlSchemaNewMemParserCtxt(zXsd,nXsd);` |
|      5 |  939 | `	if( pParser == 0 ){` |
|    ! 0 |  940 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");` |
|    ! 0 |  941 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  942 | `		return PH7_OK;` |
|      - |  943 | `	}` |
|      5 |  944 | `	xmlSchemaSetParserStructuredErrors(pParser,DomSchemaErr,pVm);` |
|      5 |  945 | `	pSchema = xmlSchemaParse(pParser);` |
|      5 |  946 | `	xmlSchemaFreeParserCtxt(pParser);` |
|      5 |  947 | `	if( pSchema == 0 ){` |
|    ! 0 |  948 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");` |
|      - |  949 | `		/* php raises "Invalid Schema" and returns false */` |
|    ! 0 |  950 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"DOMDocument::schemaValidateSource(): Invalid Schema");` |
|    ! 0 |  951 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  952 | `		return PH7_OK;` |
|      - |  953 | `	}` |
|      5 |  954 | `	pValid = xmlSchemaNewValidCtxt(pSchema);` |
|      5 |  955 | `	if( pValid == 0 ){` |
|    ! 0 |  956 | `		xmlSchemaFree(pSchema);` |
|    ! 0 |  957 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");` |
|    ! 0 |  958 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  959 | `		return PH7_OK;` |
|      - |  960 | `	}` |
|      5 |  961 | `	xmlSchemaSetValidStructuredErrors(pValid,DomSchemaErr,pVm);` |
|      5 |  962 | `	rc = xmlSchemaValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);` |
|      5 |  963 | `	xmlSchemaFreeValidCtxt(pValid);` |
|      5 |  964 | `	xmlSchemaFree(pSchema);` |
|      5 |  965 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");` |
|      5 |  966 | `	ph7_result_bool(pCtx,rc == 0);` |
|      5 |  967 | `	return PH7_OK;` |
|      3 |  968 | `}` |
|      - |  969 |  |
|      - |  970 | `/* ===== The DOM class prelude ===== */` |
|      - |  971 |  |
|      - |  972 | `static const char zDomLib1[] =` |
|      - |  973 | `	"class DOMException extends Exception {}"` |
|      - |  974 | `	"function __phl_dom_wrap($doc,$res)"` |
|      - |  975 | `	"{"` |
|      - |  976 | `	"  if( $res === null \|\| $res === false ){ return null; }"` |
|      - |  977 | `	"  $id = __dom_node_id($res);"` |
|      - |  978 | `	"  if( isset($doc->__nodes[$id]) ){ return $doc->__nodes[$id]; }"` |
|      - |  979 | `	"  switch( __dom_node_kind($res) ){"` |
|      - |  980 | `	"    case XML_ELEMENT_NODE:       $o = new DOMElement($res,$doc); break;"` |
|      - |  981 | `	"    case XML_ATTRIBUTE_NODE:     $o = new DOMAttr($res,$doc); break;"` |
|      - |  982 | `	"    case XML_TEXT_NODE:          $o = new DOMText($res,$doc); break;"` |
|      - |  983 | `	"    case XML_CDATA_SECTION_NODE: $o = new DOMCdataSection($res,$doc); break;"` |
|      - |  984 | `	"    case XML_COMMENT_NODE:       $o = new DOMComment($res,$doc); break;"` |
|      - |  985 | `	"    case XML_DOCUMENT_NODE:"` |
|      - |  986 | `	"    case XML_HTML_DOCUMENT_NODE: return $doc;"` |
|      - |  987 | `	"    default:                     $o = new DOMNode($res,$doc); break;"` |
|      - |  988 | `	"  }"` |
|      - |  989 | `	"  $doc->__nodes[$id] = $o;"` |
|      - |  990 | `	"  return $o;"` |
|      - |  991 | `	"}"` |
|      - |  992 | `	"class DOMNode"` |
|      - |  993 | `	"{"` |
|      - |  994 | `	"  public $__res;"` |
|      - |  995 | `	"  public $__doc;"` |
|      - |  996 | `	"  function __construct($res = null,$doc = null)"` |
|      - |  997 | `	"  {"` |
|      - |  998 | `	"    $this->__res = $res;"` |
|      - |  999 | `	"    $this->__doc = ($doc === null) ? $this : $doc;"` |
|      - | 1000 | `	"  }"` |
|      - | 1001 | `	"  function appendChild($node)"` |
|      - | 1002 | `	"  {"` |
|      - | 1003 | `	"    if( !__dom_node_append($this->__res,$node->__res) ){"` |
|      - | 1004 | `	"      throw new DOMException('Wrong Document Error');"` |
|      - | 1005 | `	"    }"` |
|      - | 1006 | `	"    return $node;"` |
|      - | 1007 | `	"  }"` |
|      - | 1008 | `	"  function insertBefore($node,$child = null)"` |
|      - | 1009 | `	"  {"` |
|      - | 1010 | `	"    if( !__dom_node_insert_before($this->__res,$node->__res,$child === null ? null : $child->__res) ){"` |
|      - | 1011 | `	"      throw new DOMException('Not Found Error');"` |
|      - | 1012 | `	"    }"` |
|      - | 1013 | `	"    return $node;"` |
|      - | 1014 | `	"  }"` |
|      - | 1015 | `	"  function removeChild($child)"` |
|      - | 1016 | `	"  {"` |
|      - | 1017 | `	"    if( !__dom_node_remove($this->__res,$child->__res) ){"` |
|      - | 1018 | `	"      throw new DOMException('Not Found Error');"` |
|      - | 1019 | `	"    }"` |
|      - | 1020 | `	"    return $child;"` |
|      - | 1021 | `	"  }"` |
|      - | 1022 | `	"  function replaceChild($node,$child)"` |
|      - | 1023 | `	"  {"` |
|      - | 1024 | `	"    if( !__dom_node_replace($this->__res,$node->__res,$child->__res) ){"` |
|      - | 1025 | `	"      throw new DOMException('Not Found Error');"` |
|      - | 1026 | `	"    }"` |
|      - | 1027 | `	"    return $child;"` |
|      - | 1028 | `	"  }"` |
|      - | 1029 | `	"  function hasChildNodes(){ return __dom_node_child_count($this->__res) > 0; }"` |
|      - | 1030 | `	"  function hasAttributes(){ return __dom_elem_attr_count($this->__res) > 0; }"` |
|      - | 1031 | `	"  function isSameNode($otherNode){ return __dom_node_id($this->__res) === __dom_node_id($otherNode->__res); }"` |
|      - | 1032 | `	"  function getLineNo(){ return __dom_node_line_no($this->__res); }"` |
|      - | 1033 | `	"  function C14N($exclusive = false,$withComments = false,$xpath = null,$nsPrefixes = null)"` |
|      - | 1034 | `	"  {"` |
|      - | 1035 | `	"    return __dom_node_c14n($this->__res);"` |
|      - | 1036 | `	"  }"` |
|      - | 1037 | `	"  function getElementsByTagName($qualifiedName)"` |
|      - | 1038 | `	"  {"` |
|      - | 1039 | `	"    return new DOMNodeList('gebtn',$this->__doc,$this->__res,(string)$qualifiedName);"` |
|      - | 1040 | `	"  }"` |
|      - | 1041 | `	"  protected function __nodeProp($name)"` |
|      - | 1042 | `	"  {"` |
|      - | 1043 | `	"    switch( $name ){"` |
|      - | 1044 | `	"      case 'nodeName':     return __dom_node_name($this->__res);"` |
|      - | 1045 | `	"      case 'nodeValue':    return __dom_node_value($this->__res);"` |
|      - | 1046 | `	"      case 'nodeType':     return __dom_node_kind($this->__res);"` |
|      - | 1047 | `	"      case 'parentNode':   return __phl_dom_wrap($this->__doc,__dom_node_parent($this->__res));"` |
|      - | 1048 | `	"      case 'firstChild':   return __phl_dom_wrap($this->__doc,__dom_node_first($this->__res));"` |
|      - | 1049 | `	"      case 'lastChild':    return __phl_dom_wrap($this->__doc,__dom_node_last($this->__res));"` |
|      - | 1050 | `	"      case 'nextSibling':  return __phl_dom_wrap($this->__doc,__dom_node_next($this->__res));"` |
|      - | 1051 | `	"      case 'previousSibling': return __phl_dom_wrap($this->__doc,__dom_node_prev($this->__res));"` |
|      - | 1052 | `	"      case 'ownerDocument': return ($this instanceof DOMDocument) ? null : $this->__doc;"` |
|      - | 1053 | `	"      case 'childNodes':   return new DOMNodeList('child',$this->__doc,$this->__res);"` |
|      - | 1054 | `	"      case 'textContent':  return __dom_node_text_content($this->__res);"` |
|      - | 1055 | `	"      case 'attributes':"` |
|      - | 1056 | `	"        return (__dom_node_kind($this->__res) === XML_ELEMENT_NODE)"` |
|      - | 1057 | `	"          ? new DOMNamedNodeMap($this->__doc,$this->__res) : null;"` |
|      - | 1058 | `	"      case 'childElementCount': return __dom_node_elem_child_count($this->__res);"` |
|      - | 1059 | `	"    }"` |
|      - | 1060 | `	"    return null;"` |
|      - | 1061 | `	"  }"` |
|      - | 1062 | `	"  function __get($name){ return $this->__nodeProp($name); }"` |
|      - | 1063 | `	"}";` |
|      - | 1064 |  |
|      - | 1065 | `static const char zDomLib2[] =` |
|      - | 1066 | `	"class DOMDocument extends DOMNode"` |
|      - | 1067 | `	"{"` |
|      - | 1068 | `	"  public $preserveWhiteSpace = true;"` |
|      - | 1069 | `	"  public $formatOutput = false;"` |
|      - | 1070 | `	"  public $__nodes = array();"` |
|      - | 1071 | `	"  function __construct($version = '1.0',$encoding = '')"` |
|      - | 1072 | `	"  {"` |
|      - | 1073 | `	"    parent::__construct(__dom_doc_new((string)$version,(string)$encoding),null);"` |
|      - | 1074 | `	"  }"` |
|      - | 1075 | `	"  function loadXML($source,$options = 0)"` |
|      - | 1076 | `	"  {"` |
|      - | 1077 | `	"    $source = (string)$source;"` |
|      - | 1078 | `	"    if( $source === '' ){"` |
|      - | 1079 | `	"      throw new ValueError('DOMDocument::loadXML(): Argument #1 ($source) must not be empty');"` |
|      - | 1080 | `	"    }"` |
|      - | 1081 | `	"    $r = __dom_doc_loadxml($source,(bool)$this->preserveWhiteSpace,(int)$options);"` |
|      - | 1082 | `	"    if( $r === false ){ return false; }"` |
|      - | 1083 | `	"    $this->__res = $r;"` |
|      - | 1084 | `	"    $this->__nodes = array();"` |
|      - | 1085 | `	"    return true;"` |
|      - | 1086 | `	"  }"` |
|      - | 1087 | `	"  function saveXML($node = null)"` |
|      - | 1088 | `	"  {"` |
|      - | 1089 | `	"    return __dom_doc_savexml($this->__res,$node === null ? null : $node->__res,(bool)$this->formatOutput);"` |
|      - | 1090 | `	"  }"` |
|      - | 1091 | `	"  function createElement($localName,$value = '')"` |
|      - | 1092 | `	"  {"` |
|      - | 1093 | `	"    $r = __dom_doc_create($this->__res,XML_ELEMENT_NODE,(string)$localName,(string)$value);"` |
|      - | 1094 | `	"    if( $r === false ){ throw new DOMException('Invalid Character Error'); }"` |
|      - | 1095 | `	"    return __phl_dom_wrap($this,$r);"` |
|      - | 1096 | `	"  }"` |
|      - | 1097 | `	"  function createTextNode($data)"` |
|      - | 1098 | `	"  {"` |
|      - | 1099 | `	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_TEXT_NODE,'',(string)$data));"` |
|      - | 1100 | `	"  }"` |
|      - | 1101 | `	"  function createComment($data)"` |
|      - | 1102 | `	"  {"` |
|      - | 1103 | `	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_COMMENT_NODE,'',(string)$data));"` |
|      - | 1104 | `	"  }"` |
|      - | 1105 | `	"  function createCDATASection($data)"` |
|      - | 1106 | `	"  {"` |
|      - | 1107 | `	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_CDATA_SECTION_NODE,'',(string)$data));"` |
|      - | 1108 | `	"  }"` |
|      - | 1109 | `	"  function normalizeDocument(){ __dom_doc_normalize($this->__res); }"` |
|      - | 1110 | `	"  function schemaValidateSource($source,$flags = 0)"` |
|      - | 1111 | `	"  {"` |
|      - | 1112 | `	"    return __dom_doc_schema_validate_source($this->__res,(string)$source);"` |
|      - | 1113 | `	"  }"` |
|      - | 1114 | `	"  function __get($name)"` |
|      - | 1115 | `	"  {"` |
|      - | 1116 | `	"    if( $name === 'documentElement' ){"` |
|      - | 1117 | `	"      return __phl_dom_wrap($this,__dom_doc_root($this->__res));"` |
|      - | 1118 | `	"    }"` |
|      - | 1119 | `	"    return $this->__nodeProp($name);"` |
|      - | 1120 | `	"  }"` |
|      - | 1121 | `	"}"` |
|      - | 1122 | `	"class DOMElement extends DOMNode"` |
|      - | 1123 | `	"{"` |
|      - | 1124 | `	"  function getAttribute($qualifiedName){ return __dom_elem_get_attr($this->__res,(string)$qualifiedName); }"` |
|      - | 1125 | `	"  function hasAttribute($qualifiedName){ return __dom_elem_has_attr($this->__res,(string)$qualifiedName); }"` |
|      - | 1126 | `	"  function setAttribute($qualifiedName,$value)"` |
|      - | 1127 | `	"  {"` |
|      - | 1128 | `	"    if( !__dom_elem_set_attr($this->__res,(string)$qualifiedName,(string)$value) ){"` |
|      - | 1129 | `	"      throw new DOMException('Invalid Character Error');"` |
|      - | 1130 | `	"    }"` |
|      - | 1131 | `	"    return __phl_dom_wrap($this->__doc,__dom_elem_attr_node($this->__res,(string)$qualifiedName));"` |
|      - | 1132 | `	"  }"` |
|      - | 1133 | `	"  function removeAttribute($qualifiedName){ return __dom_elem_remove_attr($this->__res,(string)$qualifiedName); }"` |
|      - | 1134 | `	"  function getAttributeNS($namespace,$localName)"` |
|      - | 1135 | `	"  {"` |
|      - | 1136 | `	"    return __dom_elem_get_attr_ns($this->__res,(string)$namespace,(string)$localName);"` |
|      - | 1137 | `	"  }"` |
|      - | 1138 | `	"  function setAttributeNS($namespace,$qualifiedName,$value)"` |
|      - | 1139 | `	"  {"` |
|      - | 1140 | `	"    if( !__dom_elem_set_attr_ns($this->__res,(string)$namespace,(string)$qualifiedName,(string)$value) ){"` |
|      - | 1141 | `	"      throw new DOMException('Namespace Error');"` |
|      - | 1142 | `	"    }"` |
|      - | 1143 | `	"  }"` |
|      - | 1144 | `	"  function __get($name)"` |
|      - | 1145 | `	"  {"` |
|      - | 1146 | `	"    if( $name === 'tagName' ){ return __dom_node_name($this->__res); }"` |
|      - | 1147 | `	"    return $this->__nodeProp($name);"` |
|      - | 1148 | `	"  }"` |
|      - | 1149 | `	"}"` |
|      - | 1150 | `	"class DOMAttr extends DOMNode"` |
|      - | 1151 | `	"{"` |
|      - | 1152 | `	"  function __get($name)"` |
|      - | 1153 | `	"  {"` |
|      - | 1154 | `	"    if( $name === 'name' ){ return __dom_node_name($this->__res); }"` |
|      - | 1155 | `	"    if( $name === 'value' ){ return __dom_node_value($this->__res); }"` |
|      - | 1156 | `	"    if( $name === 'ownerElement' ){ return __phl_dom_wrap($this->__doc,__dom_node_parent($this->__res)); }"` |
|      - | 1157 | `	"    return $this->__nodeProp($name);"` |
|      - | 1158 | `	"  }"` |
|      - | 1159 | `	"}"` |
|      - | 1160 | `	"class DOMCharacterData extends DOMNode"` |
|      - | 1161 | `	"{"` |
|      - | 1162 | `	"  function __get($name)"` |
|      - | 1163 | `	"  {"` |
|      - | 1164 | `	"    if( $name === 'data' ){ return __dom_node_value($this->__res); }"` |
|      - | 1165 | `	"    if( $name === 'length' ){ return strlen(__dom_node_value($this->__res)); }"` |
|      - | 1166 | `	"    return $this->__nodeProp($name);"` |
|      - | 1167 | `	"  }"` |
|      - | 1168 | `	"}"` |
|      - | 1169 | `	"class DOMText extends DOMCharacterData"` |
|      - | 1170 | `	"{"` |
|      - | 1171 | `	"  function __get($name)"` |
|      - | 1172 | `	"  {"` |
|      - | 1173 | `	"    if( $name === 'wholeText' ){ return __dom_node_value($this->__res); }"` |
|      - | 1174 | `	"    return parent::__get($name);"` |
|      - | 1175 | `	"  }"` |
|      - | 1176 | `	"}"` |
|      - | 1177 | `	"class DOMComment extends DOMCharacterData {}"` |
|      - | 1178 | `	"class DOMCdataSection extends DOMText {}";` |
|      - | 1179 |  |
|      - | 1180 | `static const char zDomLib3[] =` |
|      - | 1181 | `	"class DOMNodeList implements Iterator, Countable"` |
|      - | 1182 | `	"{"` |
|      - | 1183 | `	"  public $__kind;"` |
|      - | 1184 | `	"  public $__doc;"` |
|      - | 1185 | `	"  public $__owner;"` |
|      - | 1186 | `	"  public $__name;"` |
|      - | 1187 | `	"  public $__snap;"` |
|      - | 1188 | `	"  private $__pos = 0;"` |
|      - | 1189 | `	"  function __construct($kind = null,$doc = null,$owner = null,$name = null,$snap = null)"` |
|      - | 1190 | `	"  {"` |
|      - | 1191 | `	"    $this->__kind = $kind; $this->__doc = $doc; $this->__owner = $owner;"` |
|      - | 1192 | `	"    $this->__name = $name; $this->__snap = $snap;"` |
|      - | 1193 | `	"  }"` |
|      - | 1194 | `	"  function count()"` |
|      - | 1195 | `	"  {"` |
|      - | 1196 | `	"    if( $this->__kind === 'snap' ){ return count($this->__snap); }"` |
|      - | 1197 | `	"    if( $this->__kind === 'child' ){ return __dom_node_child_count($this->__owner); }"` |
|      - | 1198 | `	"    return __dom_gebtn_count($this->__owner,$this->__name);"` |
|      - | 1199 | `	"  }"` |
|      - | 1200 | `	"  function item($index)"` |
|      - | 1201 | `	"  {"` |
|      - | 1202 | `	"    $index = (int)$index;"` |
|      - | 1203 | `	"    if( $index < 0 ){ return null; }"` |
|      - | 1204 | `	"    if( $this->__kind === 'snap' ){"` |
|      - | 1205 | `	"      return isset($this->__snap[$index]) ? __phl_dom_wrap($this->__doc,$this->__snap[$index]) : null;"` |
|      - | 1206 | `	"    }"` |
|      - | 1207 | `	"    if( $this->__kind === 'child' ){"` |
|      - | 1208 | `	"      return __phl_dom_wrap($this->__doc,__dom_node_child_at($this->__owner,$index));"` |
|      - | 1209 | `	"    }"` |
|      - | 1210 | `	"    return __phl_dom_wrap($this->__doc,__dom_gebtn_at($this->__owner,$this->__name,$index));"` |
|      - | 1211 | `	"  }"` |
|      - | 1212 | `	"  function rewind(){ $this->__pos = 0; }"` |
|      - | 1213 | `	"  function valid(){ return $this->__pos < $this->count(); }"` |
|      - | 1214 | `	"  function current(){ return $this->item($this->__pos); }"` |
|      - | 1215 | `	"  function key(){ return $this->__pos; }"` |
|      - | 1216 | `	"  function next(){ $this->__pos++; }"` |
|      - | 1217 | `	"  function __get($name)"` |
|      - | 1218 | `	"  {"` |
|      - | 1219 | `	"    if( $name === 'length' ){ return $this->count(); }"` |
|      - | 1220 | `	"    return null;"` |
|      - | 1221 | `	"  }"` |
|      - | 1222 | `	"}"` |
|      - | 1223 | `	"class DOMXPath"` |
|      - | 1224 | `	"{"` |
|      - | 1225 | `	"  public $__doc;"` |
|      - | 1226 | `	"  public $document;"` |
|      - | 1227 | `	"  function __construct($document)"` |
|      - | 1228 | `	"  {"` |
|      - | 1229 | `	"    $this->__doc = $document;"` |
|      - | 1230 | `	"    $this->document = $document;"` |
|      - | 1231 | `	"  }"` |
|      - | 1232 | `	"  function query($expression,$contextNode = null,$registerNodeNS = true)"` |
|      - | 1233 | `	"  {"` |
|      - | 1234 | `	"    $ctx = ($contextNode === null) ? null : $contextNode->__res;"` |
|      - | 1235 | `	"    $r = __dom_xpath_query($this->__doc->__res,(string)$expression,$ctx);"` |
|      - | 1236 | `	"    if( $r === false ){ return false; }"` |
|      - | 1237 | `	"    return new DOMNodeList('snap',$this->__doc,null,null,$r);"` |
|      - | 1238 | `	"  }"` |
|      - | 1239 | `	"}"` |
|      - | 1240 | `	"class DOMNamedNodeMap implements Countable"` |
|      - | 1241 | `	"{"` |
|      - | 1242 | `	"  public $__doc;"` |
|      - | 1243 | `	"  public $__owner;"` |
|      - | 1244 | `	"  function __construct($doc = null,$owner = null){ $this->__doc = $doc; $this->__owner = $owner; }"` |
|      - | 1245 | `	"  function count(){ return __dom_elem_attr_count($this->__owner); }"` |
|      - | 1246 | `	"  function item($index)"` |
|      - | 1247 | `	"  {"` |
|      - | 1248 | `	"    return __phl_dom_wrap($this->__doc,__dom_elem_attr_at($this->__owner,(int)$index));"` |
|      - | 1249 | `	"  }"` |
|      - | 1250 | `	"  function getNamedItem($qualifiedName)"` |
|      - | 1251 | `	"  {"` |
|      - | 1252 | `	"    $n = $this->count();"` |
|      - | 1253 | `	"    for( $i = 0; $i < $n; $i++ ){"` |
|      - | 1254 | `	"      $a = $this->item($i);"` |
|      - | 1255 | `	"      if( $a !== null && $a->name === $qualifiedName ){ return $a; }"` |
|      - | 1256 | `	"    }"` |
|      - | 1257 | `	"    return null;"` |
|      - | 1258 | `	"  }"` |
|      - | 1259 | `	"  function __get($name)"` |
|      - | 1260 | `	"  {"` |
|      - | 1261 | `	"    if( $name === 'length' ){ return $this->count(); }"` |
|      - | 1262 | `	"    return null;"` |
|      - | 1263 | `	"  }"` |
|      - | 1264 | `	"}";` |
|      - | 1265 |  |
|      - | 1266 | `/*` |
|      - | 1267 | ` * Install the DOM library: __dom_* thunks first, then the class chunks.` |
|      - | 1268 | ` * Called from PH7_VmInit inside the bCompilingBuiltin window, after` |
|      - | 1269 | ` * PH7_VmInstallLibxml (the capture plumbing must exist).` |
|      - | 1270 | ` */` |
|   4528 | 1271 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)` |
|      5 | 1272 | `{` |
|      - | 1273 | `	static const struct {` |
|      - | 1274 | `		const char *zName;` |
|      - | 1275 | `		ProchHostFunction xFunc;` |
|      - | 1276 | `	} aFunc[] = {` |
|      - | 1277 | `		{ "__dom_node_id",            vm_builtin_dom_node_id            },` |
|      - | 1278 | `		{ "__dom_node_kind",          vm_builtin_dom_node_kind          },` |
|      - | 1279 | `		{ "__dom_node_name",          vm_builtin_dom_node_name          },` |
|      - | 1280 | `		{ "__dom_node_value",         vm_builtin_dom_node_value         },` |
|      - | 1281 | `		{ "__dom_node_text_content",  vm_builtin_dom_node_text_content  },` |
|      - | 1282 | `		{ "__dom_node_line_no",       vm_builtin_dom_node_line_no       },` |
|      - | 1283 | `		{ "__dom_node_parent",        vm_builtin_dom_node_parent        },` |
|      - | 1284 | `		{ "__dom_node_first",         vm_builtin_dom_node_first         },` |
|      - | 1285 | `		{ "__dom_node_last",          vm_builtin_dom_node_last          },` |
|      - | 1286 | `		{ "__dom_node_next",          vm_builtin_dom_node_next          },` |
|      - | 1287 | `		{ "__dom_node_prev",          vm_builtin_dom_node_prev          },` |
|      - | 1288 | `		{ "__dom_node_child_count",   vm_builtin_dom_node_child_count   },` |
|      - | 1289 | `		{ "__dom_node_child_at",      vm_builtin_dom_node_child_at      },` |
|      - | 1290 | `		{ "__dom_node_elem_child_count", vm_builtin_dom_node_elem_child_count },` |
|      - | 1291 | `		{ "__dom_node_append",        vm_builtin_dom_node_append        },` |
|      - | 1292 | `		{ "__dom_node_insert_before", vm_builtin_dom_node_insert_before },` |
|      - | 1293 | `		{ "__dom_node_remove",        vm_builtin_dom_node_remove        },` |
|      - | 1294 | `		{ "__dom_node_replace",       vm_builtin_dom_node_replace       },` |
|      - | 1295 | `		{ "__dom_node_c14n",          vm_builtin_dom_node_c14n          },` |
|      - | 1296 | `		{ "__dom_elem_get_attr",      vm_builtin_dom_elem_get_attr      },` |
|      - | 1297 | `		{ "__dom_elem_has_attr",      vm_builtin_dom_elem_has_attr      },` |
|      - | 1298 | `		{ "__dom_elem_set_attr",      vm_builtin_dom_elem_set_attr      },` |
|      - | 1299 | `		{ "__dom_elem_remove_attr",   vm_builtin_dom_elem_remove_attr   },` |
|      - | 1300 | `		{ "__dom_elem_get_attr_ns",   vm_builtin_dom_elem_get_attr_ns   },` |
|      - | 1301 | `		{ "__dom_elem_set_attr_ns",   vm_builtin_dom_elem_set_attr_ns   },` |
|      - | 1302 | `		{ "__dom_elem_attr_node",     vm_builtin_dom_elem_attr_node     },` |
|      - | 1303 | `		{ "__dom_elem_attr_count",    vm_builtin_dom_elem_attr_count    },` |
|      - | 1304 | `		{ "__dom_elem_attr_at",       vm_builtin_dom_elem_attr_at       },` |
|      - | 1305 | `		{ "__dom_gebtn_count",        vm_builtin_dom_gebtn_count        },` |
|      - | 1306 | `		{ "__dom_gebtn_at",           vm_builtin_dom_gebtn_at           },` |
|      - | 1307 | `		{ "__dom_doc_new",            vm_builtin_dom_doc_new            },` |
|      - | 1308 | `		{ "__dom_doc_loadxml",        vm_builtin_dom_doc_loadxml        },` |
|      - | 1309 | `		{ "__dom_doc_root",           vm_builtin_dom_doc_root           },` |
|      - | 1310 | `		{ "__dom_doc_savexml",        vm_builtin_dom_doc_savexml        },` |
|      - | 1311 | `		{ "__dom_doc_create",         vm_builtin_dom_doc_create         },` |
|      - | 1312 | `		{ "__dom_doc_normalize",      vm_builtin_dom_doc_normalize      },` |
|      - | 1313 | `		{ "__dom_xpath_query",        vm_builtin_dom_xpath_query        },` |
|      - | 1314 | `		{ "__dom_doc_schema_validate_source", vm_builtin_dom_doc_schema_validate_source },` |
|      - | 1315 | `	};` |
|      - | 1316 | `	sxu32 n;` |
|      - | 1317 | `	sxi32 rc;` |
| 176597 | 1318 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 172069 | 1319 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  86037 | 1320 | `	}` |
|   4533 | 1321 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib1,sizeof(zDomLib1)-1);` |
|   4533 | 1322 | `	if( rc == SXRET_OK ){` |
|   4533 | 1323 | `		rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib2,sizeof(zDomLib2)-1);` |
|   2264 | 1324 | `	}` |
|   4533 | 1325 | `	if( rc == SXRET_OK ){` |
|   4533 | 1326 | `		rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib3,sizeof(zDomLib3)-1);` |
|   2264 | 1327 | `	}` |
|   4533 | 1328 | `	return rc;` |
|      5 | 1329 | `}` |
|      - | 1330 |  |
|      - | 1331 | `#else` |
|      - | 1332 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - | 1333 | `typedef int vm_dom_unused;` |
|      - | 1334 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - | 1335 |  |
