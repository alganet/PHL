# src/ph7/vm_dom.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 577/663 lines (87.03%)

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
|      - |   11 |  |
|      - |   12 | `/*` |
|      - |   13 | ` * ext/dom on libxml2: __dom_* native thunks + the DOM class prelude.` |
|      - |   14 | ` *` |
|      - |   15 | ` * Architecture (see also vm_libxml.c): the PHP-visible DOM classes are` |
|      - |   16 | ` * compiled from the zDomLib prelude chunks below and hold two private` |
|      - |   17 | ` * props -- $__res, a phl_domnode resource {phl_xmldoc*, xmlNodePtr}, and` |
|      - |   18 | ` * $__doc, the owning DOMDocument wrapper.  All tree work happens in the` |
|      - |   19 | ` * __dom_* thunks; the prelude does dispatch, identity mapping and the` |
|      - |   20 | ` * php-facing signatures.` |
|      - |   21 | ` *` |
|      - |   22 | ` * Node identity: php guarantees $doc->documentElement === $doc->` |
|      - |   23 | ` * documentElement.  Every wrap goes through __phl_dom_wrap() which keys` |
|      - |   24 | ` * a per-document cache ($doc->__nodes) by __dom_node_id() (the pointer` |
|      - |   25 | ` * value), so the same underlying node always yields the same object.` |
|      - |   26 | ` *` |
|      - |   27 | ` * Tree surgery (append/insert/replace/remove) is done with manual pointer` |
|      - |   28 | ` * splicing instead of xmlAddChild: xmlAddChild MERGES adjacent text nodes` |
|      - |   29 | ` * and frees the merged-away node, which would dangle any PHP wrapper (and` |
|      - |   30 | ` * violates DOM semantics, which php follows -- appendChild never merges).` |
|      - |   31 | ` * Unlinked nodes are parked on the owning phl_xmldoc's orphan set so they` |
|      - |   32 | ` * are freed with the document at VM reset/release.` |
|      - |   33 | ` */` |
|      - |   34 |  |
|      - |   35 | `#define DOM_THUNK(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)` |
|      - |   36 |  |
|      - |   37 | `/* Extract a phl_domnode from a thunk argument (NULL if not a resource) */` |
|    620 |   38 | `static phl_domnode * DomNodeArg(ph7_value *pVal)` |
|      1 |   39 | `{` |
|    621 |   40 | `	if( pVal == 0 \|\| !ph7_value_is_resource(pVal) ){` |
|    ! 0 |   41 | `		return 0;` |
|      - |   42 | `	}` |
|    621 |   43 | `	return (phl_domnode *)ph7_value_to_resource(pVal);` |
|    311 |   44 | `}` |
|      - |   45 | `/* Return a (possibly NULL) xmlNode as a fresh phl_domnode resource */` |
|    218 |   46 | `static int DomResultNode(ph7_context *pCtx,phl_xmldoc *pShell,void *pNode)` |
|      1 |   47 | `{` |
|      - |   48 | `	phl_domnode *pWrap;` |
|    219 |   49 | `	if( pNode == 0 ){` |
|      5 |   50 | `		ph7_result_null(pCtx);` |
|      5 |   51 | `		return PH7_OK;` |
|      - |   52 | `	}` |
|    215 |   53 | `	pWrap = (phl_domnode *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_domnode));` |
|    215 |   54 | `	if( pWrap == 0 ){` |
|    ! 0 |   55 | `		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");` |
|    ! 0 |   56 | `		ph7_result_null(pCtx);` |
|    ! 0 |   57 | `		return PH7_OK;` |
|      - |   58 | `	}` |
|    215 |   59 | `	pWrap->pShell = pShell;` |
|    215 |   60 | `	pWrap->pNode = pNode;` |
|    215 |   61 | `	ph7_result_resource(pCtx,pWrap);` |
|    215 |   62 | `	return PH7_OK;` |
|    110 |   63 | `}` |
|      - |   64 | `/* Orphan bookkeeping: nodes not linked into their tree but still owned */` |
|     20 |   65 | `static void DomOrphanAdd(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |   66 | `{` |
|     21 |   67 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|      - |   68 | `	sxu32 n;` |
|     41 |   69 | `	for( n = 0 ; n < SySetUsed(&pShell->aOrphans) ; ++n ){` |
|     21 |   70 | `		if( apOrphan[n] == pNode ){` |
|    ! 0 |   71 | `			return;` |
|      - |   72 | `		}` |
|     11 |   73 | `	}` |
|     21 |   74 | `	SySetPut(&pShell->aOrphans,(const void *)&pNode);` |
|     11 |   75 | `}` |
|     10 |   76 | `static void DomOrphanRemove(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |   77 | `{` |
|     11 |   78 | `	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);` |
|     11 |   79 | `	sxu32 n,nUsed = SySetUsed(&pShell->aOrphans);` |
|     23 |   80 | `	for( n = 0 ; n < nUsed ; ++n ){` |
|     23 |   81 | `		if( apOrphan[n] == pNode ){` |
|     11 |   82 | `			apOrphan[n] = apOrphan[nUsed-1];` |
|     11 |   83 | `			SySetTruncate(&pShell->aOrphans,nUsed-1);` |
|     11 |   84 | `			return;` |
|      - |   85 | `		}` |
|      7 |   86 | `	}` |
|      6 |   87 | `}` |
|      - |   88 | `/* Detach a node from wherever it is (tree or orphan set) prior to linking */` |
|     12 |   89 | `static void DomDetach(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |   90 | `{` |
|     13 |   91 | `	if( pNode->parent ){` |
|      3 |   92 | `		xmlUnlinkNode(pNode);` |
|      2 |   93 | `	}else{` |
|     11 |   94 | `		DomOrphanRemove(pShell,pNode);` |
|      - |   95 | `	}` |
|     13 |   96 | `}` |
|      - |   97 | `/* Raw child-list splicing (no text-node merging -- DOM/php semantics) */` |
|      8 |   98 | `static void DomLinkLast(xmlNodePtr pParent,xmlNodePtr pChild)` |
|      1 |   99 | `{` |
|      9 |  100 | `	pChild->parent = pParent;` |
|      9 |  101 | `	pChild->next = 0;` |
|      9 |  102 | `	if( pParent->last ){` |
|      7 |  103 | `		pParent->last->next = pChild;` |
|      7 |  104 | `		pChild->prev = pParent->last;` |
|      4 |  105 | `	}else{` |
|      3 |  106 | `		pParent->children = pChild;` |
|      3 |  107 | `		pChild->prev = 0;` |
|      - |  108 | `	}` |
|      9 |  109 | `	pParent->last = pChild;` |
|      9 |  110 | `}` |
|      4 |  111 | `static void DomLinkBefore(xmlNodePtr pParent,xmlNodePtr pChild,xmlNodePtr pRef)` |
|      1 |  112 | `{` |
|      5 |  113 | `	pChild->parent = pParent;` |
|      5 |  114 | `	pChild->next = pRef;` |
|      5 |  115 | `	pChild->prev = pRef->prev;` |
|      5 |  116 | `	if( pRef->prev ){` |
|      3 |  117 | `		pRef->prev->next = pChild;` |
|      2 |  118 | `	}else{` |
|      3 |  119 | `		pParent->children = pChild;` |
|      - |  120 | `	}` |
|      5 |  121 | `	pRef->prev = pChild;` |
|      5 |  122 | `}` |
|      - |  123 |  |
|      - |  124 | `/* ===== Node introspection thunks ===== */` |
|      - |  125 |  |
|      - |  126 | `/* int __dom_node_id(res) -- identity-map key (the node pointer) */` |
|    146 |  127 | `DOM_THUNK(vm_builtin_dom_node_id)` |
|      1 |  128 | `{` |
|    147 |  129 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|    147 |  130 | `	if( pNd == 0 ){` |
|    ! 0 |  131 | `		ph7_result_int64(pCtx,0);` |
|    ! 0 |  132 | `		return PH7_OK;` |
|      - |  133 | `	}` |
|    147 |  134 | `	ph7_result_int64(pCtx,(ph7_int64)(sxuptr)pNd->pNode);` |
|    147 |  135 | `	return PH7_OK;` |
|     74 |  136 | `}` |
|      - |  137 | `/* int __dom_node_kind(res) -- the XML_*_NODE type */` |
|     98 |  138 | `DOM_THUNK(vm_builtin_dom_node_kind)` |
|      1 |  139 | `{` |
|     99 |  140 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     99 |  141 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     99 |  142 | `	ph7_result_int(pCtx,pNode ? (int)pNode->type : 0);` |
|     99 |  143 | `	return PH7_OK;` |
|      1 |  144 | `}` |
|      - |  145 | `/* string __dom_node_name(res) -- php nodeName rules */` |
|     28 |  146 | `DOM_THUNK(vm_builtin_dom_node_name)` |
|      1 |  147 | `{` |
|     29 |  148 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     29 |  149 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     29 |  150 | `	if( pNode == 0 ){` |
|    ! 0 |  151 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  152 | `		return PH7_OK;` |
|      - |  153 | `	}` |
|     29 |  154 | `	switch( pNode->type ){` |
|      7 |  155 | `	case XML_TEXT_NODE:          ph7_result_string(pCtx,"#text",(int)sizeof("#text")-1); break;` |
|      3 |  156 | `	case XML_CDATA_SECTION_NODE: ph7_result_string(pCtx,"#cdata-section",(int)sizeof("#cdata-section")-1); break;` |
|      3 |  157 | `	case XML_COMMENT_NODE:       ph7_result_string(pCtx,"#comment",(int)sizeof("#comment")-1); break;` |
|      1 |  158 | `	case XML_HTML_DOCUMENT_NODE:` |
|      3 |  159 | `	case XML_DOCUMENT_NODE:      ph7_result_string(pCtx,"#document",(int)sizeof("#document")-1); break;` |
|    ! 0 |  160 | `	case XML_DOCUMENT_FRAG_NODE: ph7_result_string(pCtx,"#document-fragment",(int)sizeof("#document-fragment")-1); break;` |
|      8 |  161 | `	default:` |
|     16 |  162 | `		if( (pNode->type == XML_ELEMENT_NODE \|\| pNode->type == XML_ATTRIBUTE_NODE)` |
|     17 |  163 | `			&& pNode->ns && pNode->ns->prefix ){` |
|    ! 0 |  164 | `			ph7_result_string_format(pCtx,"%s:%s",(const char *)pNode->ns->prefix,(const char *)pNode->name);` |
|    ! 0 |  165 | `		}else{` |
|     17 |  166 | `			ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);` |
|      - |  167 | `		}` |
|     16 |  168 | `		break;` |
|      - |  169 | `	}` |
|     29 |  170 | `	return PH7_OK;` |
|     15 |  171 | `}` |
|      - |  172 | `/* ?string __dom_node_value(res) -- php nodeValue (NULL for documents) */` |
|     28 |  173 | `DOM_THUNK(vm_builtin_dom_node_value)` |
|      1 |  174 | `{` |
|     29 |  175 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     29 |  176 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  177 | `	xmlChar *zContent;` |
|     28 |  178 | `	if( pNode == 0 \|\| pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE` |
|     27 |  179 | `		\|\| pNode->type == XML_DOCUMENT_TYPE_NODE ){` |
|      3 |  180 | `		ph7_result_null(pCtx);` |
|      3 |  181 | `		return PH7_OK;` |
|      - |  182 | `	}` |
|     27 |  183 | `	zContent = xmlNodeGetContent(pNode);` |
|     27 |  184 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|     27 |  185 | `	if( zContent ){` |
|     27 |  186 | `		xmlFree(zContent);` |
|     13 |  187 | `	}` |
|     27 |  188 | `	return PH7_OK;` |
|     15 |  189 | `}` |
|      - |  190 | `/* string __dom_node_text_content(res) */` |
|      4 |  191 | `DOM_THUNK(vm_builtin_dom_node_text_content)` |
|      1 |  192 | `{` |
|      5 |  193 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  194 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      5 |  195 | `	xmlChar *zContent = pNode ? xmlNodeGetContent(pNode) : 0;` |
|      5 |  196 | `	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);` |
|      5 |  197 | `	if( zContent ){` |
|      5 |  198 | `		xmlFree(zContent);` |
|      2 |  199 | `	}` |
|      5 |  200 | `	return PH7_OK;` |
|      1 |  201 | `}` |
|      - |  202 | `/* int __dom_node_line_no(res) */` |
|    ! 0 |  203 | `DOM_THUNK(vm_builtin_dom_node_line_no)` |
|    ! 0 |  204 | `{` |
|    ! 0 |  205 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|    ! 0 |  206 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|    ! 0 |  207 | `	ph7_result_int64(pCtx,pNode ? (ph7_int64)xmlGetLineNo(pNode) : 0);` |
|    ! 0 |  208 | `	return PH7_OK;` |
|    ! 0 |  209 | `}` |
|      - |  210 | `/* Navigation: parent/first/last/next/prev share one worker */` |
|     40 |  211 | `static int DomNavigate(ph7_context *pCtx,int nArg,ph7_value **apArg,int iDir)` |
|      1 |  212 | `{` |
|     41 |  213 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     41 |  214 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     41 |  215 | `	xmlNodePtr pOut = 0;` |
|     41 |  216 | `	if( pNode ){` |
|     41 |  217 | `		switch( iDir ){` |
|     15 |  218 | `		case 0: pOut = pNode->parent; break;` |
|     21 |  219 | `		case 1: pOut = pNode->children; break;` |
|      3 |  220 | `		case 2: pOut = pNode->last; break;` |
|      3 |  221 | `		case 3: pOut = pNode->next; break;` |
|      3 |  222 | `		case 4: pOut = pNode->prev; break;` |
|      - |  223 | `		}` |
|     20 |  224 | `	}` |
|     41 |  225 | `	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pOut);` |
|      1 |  226 | `}` |
|     15 |  227 | `DOM_THUNK(vm_builtin_dom_node_parent){ return DomNavigate(pCtx,nArg,apArg,0); }` |
|     21 |  228 | `DOM_THUNK(vm_builtin_dom_node_first){ return DomNavigate(pCtx,nArg,apArg,1); }` |
|      3 |  229 | `DOM_THUNK(vm_builtin_dom_node_last){ return DomNavigate(pCtx,nArg,apArg,2); }` |
|      3 |  230 | `DOM_THUNK(vm_builtin_dom_node_next){ return DomNavigate(pCtx,nArg,apArg,3); }` |
|      3 |  231 | `DOM_THUNK(vm_builtin_dom_node_prev){ return DomNavigate(pCtx,nArg,apArg,4); }` |
|      - |  232 | `/* int __dom_node_child_count(res) / ?res __dom_node_child_at(res,i) */` |
|     14 |  233 | `DOM_THUNK(vm_builtin_dom_node_child_count)` |
|      1 |  234 | `{` |
|     15 |  235 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     15 |  236 | `	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;` |
|     15 |  237 | `	int iCount = 0;` |
|     69 |  238 | `	for( ; pChild ; pChild = pChild->next ){` |
|     55 |  239 | `		iCount++;` |
|     28 |  240 | `	}` |
|     15 |  241 | `	ph7_result_int(pCtx,iCount);` |
|     15 |  242 | `	return PH7_OK;` |
|      1 |  243 | `}` |
|     14 |  244 | `DOM_THUNK(vm_builtin_dom_node_child_at)` |
|      1 |  245 | `{` |
|     15 |  246 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     15 |  247 | `	int iWant = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     15 |  248 | `	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;` |
|     39 |  249 | `	for( ; pChild && iWant > 0 ; pChild = pChild->next ){` |
|     25 |  250 | `		iWant--;` |
|     13 |  251 | `	}` |
|     15 |  252 | `	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pChild);` |
|      1 |  253 | `}` |
|      - |  254 | `/* int __dom_node_elem_child_count(res) -- childElementCount */` |
|      2 |  255 | `DOM_THUNK(vm_builtin_dom_node_elem_child_count)` |
|      1 |  256 | `{` |
|      3 |  257 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      3 |  258 | `	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;` |
|      3 |  259 | `	int iCount = 0;` |
|     11 |  260 | `	for( ; pChild ; pChild = pChild->next ){` |
|      9 |  261 | `		if( pChild->type == XML_ELEMENT_NODE ){` |
|      3 |  262 | `			iCount++;` |
|      1 |  263 | `		}` |
|      5 |  264 | `	}` |
|      3 |  265 | `	ph7_result_int(pCtx,iCount);` |
|      3 |  266 | `	return PH7_OK;` |
|      1 |  267 | `}` |
|      - |  268 |  |
|      - |  269 | `/* ===== Tree surgery thunks ===== */` |
|      - |  270 |  |
|      - |  271 | `/* bool __dom_node_append(parentres,childres) */` |
|      8 |  272 | `DOM_THUNK(vm_builtin_dom_node_append)` |
|      1 |  273 | `{` |
|      9 |  274 | `	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      9 |  275 | `	phl_domnode *pChd = nArg > 1 ? DomNodeArg(apArg[1]) : 0;` |
|      - |  276 | `	xmlNodePtr pParent,pChild;` |
|      9 |  277 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|    ! 0 |  278 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  279 | `		return PH7_OK;` |
|      - |  280 | `	}` |
|      9 |  281 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|      9 |  282 | `	pChild = (xmlNodePtr)pChd->pNode;` |
|      9 |  283 | `	if( pParent->doc != pChild->doc \|\| pChild == pParent ){` |
|    ! 0 |  284 | `		ph7_result_bool(pCtx,0); /* Wrong Document Error */` |
|    ! 0 |  285 | `		return PH7_OK;` |
|      - |  286 | `	}` |
|      9 |  287 | `	DomDetach(pChd->pShell,pChild);` |
|      9 |  288 | `	DomLinkLast(pParent,pChild);` |
|      9 |  289 | `	ph7_result_bool(pCtx,1);` |
|      9 |  290 | `	return PH7_OK;` |
|      5 |  291 | `}` |
|      - |  292 | `/* bool __dom_node_insert_before(parentres,newres,?refres) */` |
|      2 |  293 | `DOM_THUNK(vm_builtin_dom_node_insert_before)` |
|      1 |  294 | `{` |
|      3 |  295 | `	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      3 |  296 | `	phl_domnode *pNew = nArg > 1 ? DomNodeArg(apArg[1]) : 0;` |
|      3 |  297 | `	phl_domnode *pRef = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? DomNodeArg(apArg[2]) : 0;` |
|      - |  298 | `	xmlNodePtr pParent,pChild,pAnchor;` |
|      3 |  299 | `	if( pPar == 0 \|\| pNew == 0 ){` |
|    ! 0 |  300 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  301 | `		return PH7_OK;` |
|      - |  302 | `	}` |
|      3 |  303 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|      3 |  304 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|      3 |  305 | `	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;` |
|      2 |  306 | `	if( pParent->doc != pChild->doc \|\| pChild == pParent` |
|      3 |  307 | `		\|\| (pAnchor && pAnchor->parent != pParent) ){` |
|    ! 0 |  308 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  309 | `		return PH7_OK;` |
|      - |  310 | `	}` |
|      3 |  311 | `	DomDetach(pNew->pShell,pChild);` |
|      3 |  312 | `	if( pAnchor ){` |
|      3 |  313 | `		DomLinkBefore(pParent,pChild,pAnchor);` |
|      2 |  314 | `	}else{` |
|    ! 0 |  315 | `		DomLinkLast(pParent,pChild);` |
|      - |  316 | `	}` |
|      3 |  317 | `	ph7_result_bool(pCtx,1);` |
|      3 |  318 | `	return PH7_OK;` |
|      2 |  319 | `}` |
|      - |  320 | `/* bool __dom_node_remove(parentres,childres) */` |
|     10 |  321 | `DOM_THUNK(vm_builtin_dom_node_remove)` |
|      1 |  322 | `{` |
|     11 |  323 | `	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     11 |  324 | `	phl_domnode *pChd = nArg > 1 ? DomNodeArg(apArg[1]) : 0;` |
|      - |  325 | `	xmlNodePtr pChild;` |
|     11 |  326 | `	if( pPar == 0 \|\| pChd == 0 ){` |
|    ! 0 |  327 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  328 | `		return PH7_OK;` |
|      - |  329 | `	}` |
|     11 |  330 | `	pChild = (xmlNodePtr)pChd->pNode;` |
|     11 |  331 | `	if( pChild->parent != (xmlNodePtr)pPar->pNode ){` |
|      3 |  332 | `		ph7_result_bool(pCtx,0); /* Not Found Error */` |
|      3 |  333 | `		return PH7_OK;` |
|      - |  334 | `	}` |
|      9 |  335 | `	xmlUnlinkNode(pChild);` |
|      9 |  336 | `	DomOrphanAdd(pChd->pShell,pChild);` |
|      9 |  337 | `	ph7_result_bool(pCtx,1);` |
|      9 |  338 | `	return PH7_OK;` |
|      6 |  339 | `}` |
|      - |  340 | `/* bool __dom_node_replace(parentres,newres,oldres) */` |
|      2 |  341 | `DOM_THUNK(vm_builtin_dom_node_replace)` |
|      1 |  342 | `{` |
|      3 |  343 | `	phl_domnode *pPar = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|      3 |  344 | `	phl_domnode *pNew = nArg > 2 ? DomNodeArg(apArg[1]) : 0;` |
|      3 |  345 | `	phl_domnode *pOld = nArg > 2 ? DomNodeArg(apArg[2]) : 0;` |
|      - |  346 | `	xmlNodePtr pParent,pChild,pVictim;` |
|      3 |  347 | `	if( pPar == 0 \|\| pNew == 0 \|\| pOld == 0 ){` |
|    ! 0 |  348 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  349 | `		return PH7_OK;` |
|      - |  350 | `	}` |
|      3 |  351 | `	pParent = (xmlNodePtr)pPar->pNode;` |
|      3 |  352 | `	pChild = (xmlNodePtr)pNew->pNode;` |
|      3 |  353 | `	pVictim = (xmlNodePtr)pOld->pNode;` |
|      3 |  354 | `	if( pParent->doc != pChild->doc \|\| pVictim->parent != pParent \|\| pChild == pParent ){` |
|    ! 0 |  355 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  356 | `		return PH7_OK;` |
|      - |  357 | `	}` |
|      3 |  358 | `	if( pChild != pVictim ){` |
|      3 |  359 | `		DomDetach(pNew->pShell,pChild);` |
|      3 |  360 | `		DomLinkBefore(pParent,pChild,pVictim);` |
|      3 |  361 | `		xmlUnlinkNode(pVictim);` |
|      3 |  362 | `		DomOrphanAdd(pOld->pShell,pVictim);` |
|      1 |  363 | `	}` |
|      3 |  364 | `	ph7_result_bool(pCtx,1);` |
|      3 |  365 | `	return PH7_OK;` |
|      2 |  366 | `}` |
|      - |  367 |  |
|      - |  368 | `/* ===== Element attribute thunks ===== */` |
|      - |  369 |  |
|      - |  370 | `/* string __dom_elem_get_attr(res,name) -- "" when absent (php) */` |
|     10 |  371 | `DOM_THUNK(vm_builtin_dom_elem_get_attr)` |
|      1 |  372 | `{` |
|     11 |  373 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     11 |  374 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     11 |  375 | `	xmlChar *zVal = pNd ? xmlGetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;` |
|     11 |  376 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|     11 |  377 | `	if( zVal ){` |
|      9 |  378 | `		xmlFree(zVal);` |
|      4 |  379 | `	}` |
|     11 |  380 | `	return PH7_OK;` |
|      1 |  381 | `}` |
|      - |  382 | `/* bool __dom_elem_has_attr(res,name) */` |
|      4 |  383 | `DOM_THUNK(vm_builtin_dom_elem_has_attr)` |
|      1 |  384 | `{` |
|      5 |  385 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  386 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  387 | `	ph7_result_bool(pCtx,pNd && xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) != 0);` |
|      5 |  388 | `	return PH7_OK;` |
|      1 |  389 | `}` |
|      - |  390 | `/* bool __dom_elem_set_attr(res,name,value) */` |
|      4 |  391 | `DOM_THUNK(vm_builtin_dom_elem_set_attr)` |
|      1 |  392 | `{` |
|      5 |  393 | `	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  394 | `	const char *zName = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  395 | `	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|      5 |  396 | `	if( pNd == 0 \|\| xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|    ! 0 |  397 | `		ph7_result_bool(pCtx,0); /* Invalid Character Error */` |
|    ! 0 |  398 | `		return PH7_OK;` |
|      - |  399 | `	}` |
|      5 |  400 | `	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);` |
|      5 |  401 | `	ph7_result_bool(pCtx,1);` |
|      5 |  402 | `	return PH7_OK;` |
|      3 |  403 | `}` |
|      - |  404 | `/* bool __dom_elem_remove_attr(res,name) */` |
|      4 |  405 | `DOM_THUNK(vm_builtin_dom_elem_remove_attr)` |
|      1 |  406 | `{` |
|      5 |  407 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  408 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  409 | `	xmlAttrPtr pAttr = pNd ? xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;` |
|      5 |  410 | `	if( pAttr == 0 \|\| pAttr->type != XML_ATTRIBUTE_NODE ){` |
|      - |  411 | `		/* Absent (or a DTD default): php returns false */` |
|      3 |  412 | `		ph7_result_bool(pCtx,0);` |
|      3 |  413 | `		return PH7_OK;` |
|      - |  414 | `	}` |
|      3 |  415 | `	xmlRemoveProp(pAttr);` |
|      3 |  416 | `	ph7_result_bool(pCtx,1);` |
|      3 |  417 | `	return PH7_OK;` |
|      3 |  418 | `}` |
|      - |  419 | `/* string __dom_elem_get_attr_ns(res,uri,local) */` |
|      4 |  420 | `DOM_THUNK(vm_builtin_dom_elem_get_attr_ns)` |
|      1 |  421 | `{` |
|      5 |  422 | `	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  423 | `	const char *zUri = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  424 | `	const char *zLocal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";` |
|      5 |  425 | `	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,(const xmlChar *)zUri) : 0;` |
|      5 |  426 | `	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);` |
|      5 |  427 | `	if( zVal ){` |
|      3 |  428 | `		xmlFree(zVal);` |
|      1 |  429 | `	}` |
|      5 |  430 | `	return PH7_OK;` |
|      1 |  431 | `}` |
|      - |  432 | `/* bool __dom_elem_set_attr_ns(res,uri,qname,value) */` |
|      2 |  433 | `DOM_THUNK(vm_builtin_dom_elem_set_attr_ns)` |
|      1 |  434 | `{` |
|      3 |  435 | `	phl_domnode *pNd = nArg > 3 ? DomNodeArg(apArg[0]) : 0;` |
|      3 |  436 | `	const char *zUri = nArg > 3 ? ph7_value_to_string(apArg[1],0) : "";` |
|      3 |  437 | `	const char *zQname = nArg > 3 ? ph7_value_to_string(apArg[2],0) : "";` |
|      3 |  438 | `	const char *zVal = nArg > 3 ? ph7_value_to_string(apArg[3],0) : "";` |
|      3 |  439 | `	sxu32 nColon = 0;` |
|      - |  440 | `	xmlNodePtr pNode;` |
|      - |  441 | `	xmlNsPtr pNs;` |
|      3 |  442 | `	if( pNd == 0 \|\| zQname[0] == 0 ){` |
|    ! 0 |  443 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  444 | `		return PH7_OK;` |
|      - |  445 | `	}` |
|      3 |  446 | `	pNode = (xmlNodePtr)pNd->pNode;` |
|      3 |  447 | `	if( SyByteFind(zQname,SyStrlen(zQname),':',&nColon) == SXRET_OK ){` |
|      - |  448 | `		/* Prefixed: find (or declare on this element) the namespace */` |
|      - |  449 | `		char zPrefix[128];` |
|      3 |  450 | `		if( nColon >= sizeof(zPrefix) ){` |
|    ! 0 |  451 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  452 | `			return PH7_OK;` |
|      - |  453 | `		}` |
|      3 |  454 | `		SyMemcpy(zQname,zPrefix,nColon);` |
|      3 |  455 | `		zPrefix[nColon] = 0;` |
|      3 |  456 | `		pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);` |
|      3 |  457 | `		if( pNs == 0 ){` |
|      3 |  458 | `			pNs = xmlNewNs(pNode,(const xmlChar *)zUri,(const xmlChar *)zPrefix);` |
|      1 |  459 | `		}` |
|      3 |  460 | `		if( pNs == 0 ){` |
|    ! 0 |  461 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  462 | `			return PH7_OK;` |
|      - |  463 | `		}` |
|      3 |  464 | `		xmlSetNsProp(pNode,pNs,(const xmlChar *)(zQname+nColon+1),(const xmlChar *)zVal);` |
|      2 |  465 | `	}else{` |
|    ! 0 |  466 | `		pNs = zUri[0] ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;` |
|    ! 0 |  467 | `		xmlSetNsProp(pNode,pNs,(const xmlChar *)zQname,(const xmlChar *)zVal);` |
|      - |  468 | `	}` |
|      3 |  469 | `	ph7_result_bool(pCtx,1);` |
|      3 |  470 | `	return PH7_OK;` |
|      2 |  471 | `}` |
|      - |  472 | `/* ?res __dom_elem_attr_node(res,name) -- the attribute NODE by name */` |
|      4 |  473 | `DOM_THUNK(vm_builtin_dom_elem_attr_node)` |
|      1 |  474 | `{` |
|      5 |  475 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  476 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      5 |  477 | `	xmlAttrPtr pAttr = pNd ? xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;` |
|      5 |  478 | `	if( pAttr == 0 \|\| pAttr->type != XML_ATTRIBUTE_NODE ){` |
|    ! 0 |  479 | `		ph7_result_null(pCtx);` |
|    ! 0 |  480 | `		return PH7_OK;` |
|      - |  481 | `	}` |
|      5 |  482 | `	return DomResultNode(pCtx,pNd->pShell,(xmlNodePtr)pAttr);` |
|      3 |  483 | `}` |
|      - |  484 | `/* int __dom_elem_attr_count(res) / ?res __dom_elem_attr_at(res,i) */` |
|      4 |  485 | `DOM_THUNK(vm_builtin_dom_elem_attr_count)` |
|      1 |  486 | `{` |
|      5 |  487 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      5 |  488 | `	xmlAttrPtr pAttr = (pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE)` |
|      6 |  489 | `		? ((xmlNodePtr)pNd->pNode)->properties : 0;` |
|      5 |  490 | `	int iCount = 0;` |
|     17 |  491 | `	for( ; pAttr ; pAttr = pAttr->next ){` |
|     13 |  492 | `		iCount++;` |
|      7 |  493 | `	}` |
|      5 |  494 | `	ph7_result_int(pCtx,iCount);` |
|      5 |  495 | `	return PH7_OK;` |
|      1 |  496 | `}` |
|     12 |  497 | `DOM_THUNK(vm_builtin_dom_elem_attr_at)` |
|      1 |  498 | `{` |
|     13 |  499 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     13 |  500 | `	int iWant = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;` |
|     13 |  501 | `	xmlAttrPtr pAttr = (pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE)` |
|     18 |  502 | `		? ((xmlNodePtr)pNd->pNode)->properties : 0;` |
|     25 |  503 | `	for( ; pAttr && iWant > 0 ; pAttr = pAttr->next ){` |
|     13 |  504 | `		iWant--;` |
|      7 |  505 | `	}` |
|     13 |  506 | `	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,(xmlNodePtr)pAttr);` |
|      1 |  507 | `}` |
|      - |  508 |  |
|      - |  509 | `/* ===== getElementsByTagName (live) ===== */` |
|      - |  510 |  |
|      - |  511 | `/* Document-order successor within pRoot's subtree (pRoot excluded) */` |
|    152 |  512 | `static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot)` |
|      1 |  513 | `{` |
|    153 |  514 | `	if( pCur->children ){` |
|     57 |  515 | `		return pCur->children;` |
|      - |  516 | `	}` |
|    155 |  517 | `	while( pCur && pCur != pRoot ){` |
|    125 |  518 | `		if( pCur->next ){` |
|     67 |  519 | `			return pCur->next;` |
|      - |  520 | `		}` |
|     59 |  521 | `		pCur = pCur->parent;` |
|      1 |  522 | `	}` |
|     31 |  523 | `	return 0;` |
|     77 |  524 | `}` |
|    174 |  525 | `static int DomGebtnMatch(xmlNodePtr pNode,const char *zName)` |
|      1 |  526 | `{` |
|    175 |  527 | `	if( pNode->type != XML_ELEMENT_NODE ){` |
|    ! 0 |  528 | `		return 0;` |
|      - |  529 | `	}` |
|    175 |  530 | `	if( zName[0] == '*' && zName[1] == 0 ){` |
|      3 |  531 | `		return 1;` |
|      - |  532 | `	}` |
|    173 |  533 | `	return xmlStrEqual(pNode->name,(const xmlChar *)zName) != 0;` |
|     88 |  534 | `}` |
|      - |  535 | `/* int __dom_gebtn_count(res,name) / ?res __dom_gebtn_at(res,name,i) */` |
|     28 |  536 | `DOM_THUNK(vm_builtin_dom_gebtn_count)` |
|      1 |  537 | `{` |
|     29 |  538 | `	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;` |
|     29 |  539 | `	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|     29 |  540 | `	xmlNodePtr pRoot = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     29 |  541 | `	xmlNodePtr pCur = pRoot ? pRoot->children : 0;` |
|     29 |  542 | `	int iCount = 0;` |
|    127 |  543 | `	while( pCur ){` |
|     99 |  544 | `		if( DomGebtnMatch(pCur,zName) ){` |
|     73 |  545 | `			iCount++;` |
|     36 |  546 | `		}` |
|     99 |  547 | `		pCur = DomWalkNext(pCur,pRoot);` |
|      1 |  548 | `	}` |
|     29 |  549 | `	ph7_result_int(pCtx,iCount);` |
|     29 |  550 | `	return PH7_OK;` |
|      1 |  551 | `}` |
|     24 |  552 | `DOM_THUNK(vm_builtin_dom_gebtn_at)` |
|      1 |  553 | `{` |
|     25 |  554 | `	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|     25 |  555 | `	const char *zName = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";` |
|     25 |  556 | `	int iWant = nArg > 2 ? ph7_value_to_int(apArg[2]) : 0;` |
|     25 |  557 | `	xmlNodePtr pRoot = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|     25 |  558 | `	xmlNodePtr pCur = pRoot ? pRoot->children : 0;` |
|     79 |  559 | `	while( pCur ){` |
|     77 |  560 | `		if( DomGebtnMatch(pCur,zName) ){` |
|     37 |  561 | `			if( iWant == 0 ){` |
|     23 |  562 | `				return DomResultNode(pCtx,pNd->pShell,pCur);` |
|      - |  563 | `			}` |
|     15 |  564 | `			iWant--;` |
|      7 |  565 | `		}` |
|     55 |  566 | `		pCur = DomWalkNext(pCur,pRoot);` |
|      1 |  567 | `	}` |
|      3 |  568 | `	ph7_result_null(pCtx);` |
|      3 |  569 | `	return PH7_OK;` |
|     13 |  570 | `}` |
|      - |  571 |  |
|      - |  572 | `/* ===== Document thunks ===== */` |
|      - |  573 |  |
|      - |  574 | `/* res __dom_doc_new(version,encoding) */` |
|     40 |  575 | `DOM_THUNK(vm_builtin_dom_doc_new)` |
|      1 |  576 | `{` |
|     41 |  577 | `	ph7_vm *pVm = pCtx->pVm;` |
|     41 |  578 | `	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";` |
|     41 |  579 | `	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";` |
|      - |  580 | `	xmlDocPtr pDoc;` |
|      - |  581 | `	phl_xmldoc *pShell;` |
|     41 |  582 | `	pDoc = xmlNewDoc((const xmlChar *)(zVersion[0] ? zVersion : "1.0"));` |
|     41 |  583 | `	if( pDoc == 0 ){` |
|    ! 0 |  584 | `		ph7_result_null(pCtx);` |
|    ! 0 |  585 | `		return PH7_OK;` |
|      - |  586 | `	}` |
|     41 |  587 | `	if( zEncoding[0] ){` |
|    ! 0 |  588 | `		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);` |
|    ! 0 |  589 | `	}` |
|     41 |  590 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|     41 |  591 | `	if( pShell == 0 ){` |
|    ! 0 |  592 | `		xmlFreeDoc(pDoc);` |
|    ! 0 |  593 | `		ph7_result_null(pCtx);` |
|    ! 0 |  594 | `		return PH7_OK;` |
|      - |  595 | `	}` |
|     41 |  596 | `	return DomResultNode(pCtx,pShell,(xmlNodePtr)pDoc);` |
|     21 |  597 | `}` |
|      - |  598 | `/* res\|false __dom_doc_loadxml(source,preserveWS,options) -- a NEW doc resource */` |
|     40 |  599 | `DOM_THUNK(vm_builtin_dom_doc_loadxml)` |
|      1 |  600 | `{` |
|     41 |  601 | `	ph7_vm *pVm = pCtx->pVm;` |
|     41 |  602 | `	int nLen = 0;` |
|     41 |  603 | `	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";` |
|     41 |  604 | `	int bPreserve = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;` |
|     41 |  605 | `	int iOpts = nArg > 2 ? ph7_value_to_int(apArg[2]) : 0;` |
|      - |  606 | `	xmlDocPtr pDoc;` |
|      - |  607 | `	phl_xmldoc *pShell;` |
|      - |  608 | `	sxu32 nMark;` |
|     41 |  609 | `	if( !bPreserve ){` |
|     13 |  610 | `		iOpts \|= XML_PARSE_NOBLANKS;` |
|      6 |  611 | `	}` |
|     41 |  612 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     41 |  613 | `	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);` |
|     41 |  614 | `	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::loadXML");` |
|     41 |  615 | `	if( pDoc == 0 ){` |
|      9 |  616 | `		ph7_result_bool(pCtx,0);` |
|      9 |  617 | `		return PH7_OK;` |
|      - |  618 | `	}` |
|     33 |  619 | `	pShell = PH7_LibxmlNewDoc(pVm,pDoc);` |
|     33 |  620 | `	if( pShell == 0 ){` |
|    ! 0 |  621 | `		xmlFreeDoc(pDoc);` |
|    ! 0 |  622 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  623 | `		return PH7_OK;` |
|      - |  624 | `	}` |
|     33 |  625 | `	return DomResultNode(pCtx,pShell,(xmlNodePtr)pDoc);` |
|     21 |  626 | `}` |
|      - |  627 | `/* ?res __dom_doc_root(docres) -- documentElement */` |
|     44 |  628 | `DOM_THUNK(vm_builtin_dom_doc_root)` |
|      1 |  629 | `{` |
|     45 |  630 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     45 |  631 | `	xmlNodePtr pRoot = pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0;` |
|     45 |  632 | `	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pRoot);` |
|      1 |  633 | `}` |
|      - |  634 | `/* string\|false __dom_doc_savexml(docres,?noderes,format) */` |
|     20 |  635 | `DOM_THUNK(vm_builtin_dom_doc_savexml)` |
|      1 |  636 | `{` |
|     21 |  637 | `	ph7_vm *pVm = pCtx->pVm;` |
|     21 |  638 | `	phl_domnode *pDocNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;` |
|     21 |  639 | `	phl_domnode *pTgt = (nArg > 2 && !ph7_value_is_null(apArg[1])) ? DomNodeArg(apArg[1]) : 0;` |
|     21 |  640 | `	int bFormat = nArg > 2 ? ph7_value_to_bool(apArg[2]) : 0;` |
|      - |  641 | `	xmlDocPtr pDoc;` |
|      - |  642 | `	sxu32 nMark;` |
|     21 |  643 | `	if( pDocNd == 0 ){` |
|    ! 0 |  644 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  645 | `		return PH7_OK;` |
|      - |  646 | `	}` |
|     21 |  647 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|     21 |  648 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     28 |  649 | `	if( pTgt == 0 \|\| pTgt->pNode == pDocNd->pNode ){` |
|     15 |  650 | `		xmlChar *zOut = 0;` |
|     15 |  651 | `		int nOut = 0;` |
|     15 |  652 | `		xmlDocDumpFormatMemory(pDoc,&zOut,&nOut,bFormat ? 1 : 0);` |
|     15 |  653 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|     15 |  654 | `		if( zOut == 0 ){` |
|    ! 0 |  655 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  656 | `			return PH7_OK;` |
|      - |  657 | `		}` |
|     15 |  658 | `		ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|     15 |  659 | `		xmlFree(zOut);` |
|      8 |  660 | `	}else{` |
|      7 |  661 | `		xmlBufferPtr pBuf = xmlBufferCreate();` |
|      - |  662 | `		int rc;` |
|      7 |  663 | `		if( pBuf == 0 ){` |
|    ! 0 |  664 | `			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|    ! 0 |  665 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  666 | `			return PH7_OK;` |
|      - |  667 | `		}` |
|      7 |  668 | `		rc = xmlNodeDump(pBuf,pDoc,(xmlNodePtr)pTgt->pNode,0,bFormat ? 1 : 0);` |
|      7 |  669 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");` |
|      7 |  670 | `		if( rc < 0 ){` |
|    ! 0 |  671 | `			xmlBufferFree(pBuf);` |
|    ! 0 |  672 | `			ph7_result_bool(pCtx,0);` |
|    ! 0 |  673 | `			return PH7_OK;` |
|      - |  674 | `		}` |
|      7 |  675 | `		ph7_result_string(pCtx,(const char *)xmlBufferContent(pBuf),(int)xmlBufferLength(pBuf));` |
|      7 |  676 | `		xmlBufferFree(pBuf);` |
|      - |  677 | `	}` |
|     21 |  678 | `	return PH7_OK;` |
|     11 |  679 | `}` |
|      - |  680 | `/* res\|false __dom_doc_create(docres,kind,name,value) -- kind: 1 element,` |
|      - |  681 | ` * 3 text, 4 cdata, 8 comment.  Fresh nodes start as orphans. */` |
|     10 |  682 | `DOM_THUNK(vm_builtin_dom_doc_create)` |
|      1 |  683 | `{` |
|     11 |  684 | `	ph7_vm *pVm = pCtx->pVm;` |
|     11 |  685 | `	phl_domnode *pDocNd = nArg > 3 ? DomNodeArg(apArg[0]) : 0;` |
|     11 |  686 | `	int iKind = nArg > 3 ? ph7_value_to_int(apArg[1]) : 0;` |
|     11 |  687 | `	const char *zName = nArg > 3 ? ph7_value_to_string(apArg[2],0) : "";` |
|     11 |  688 | `	int nVal = 0;` |
|     11 |  689 | `	const char *zVal = nArg > 3 ? ph7_value_to_string(apArg[3],&nVal) : "";` |
|      - |  690 | `	xmlDocPtr pDoc;` |
|     11 |  691 | `	xmlNodePtr pNode = 0;` |
|      - |  692 | `	sxu32 nMark;` |
|     11 |  693 | `	if( pDocNd == 0 ){` |
|    ! 0 |  694 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  695 | `		return PH7_OK;` |
|      - |  696 | `	}` |
|     11 |  697 | `	pDoc = (xmlDocPtr)pDocNd->pNode;` |
|     11 |  698 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     11 |  699 | `	switch( iKind ){` |
|      2 |  700 | `	case XML_ELEMENT_NODE:` |
|      5 |  701 | `		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){` |
|    ! 0 |  702 | `			break; /* Invalid Character Error */` |
|      - |  703 | `		}` |
|      - |  704 | `		/* php passes the value through xmlNewDocNode, which entity-parses` |
|      - |  705 | `		 * it (quirk preserved: bad entities warn and drop the content). */` |
|      5 |  706 | `		pNode = xmlNewDocNode(pDoc,0,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);` |
|      5 |  707 | `		break;` |
|      1 |  708 | `	case XML_TEXT_NODE:` |
|      3 |  709 | `		pNode = xmlNewDocText(pDoc,(const xmlChar *)zVal);` |
|      3 |  710 | `		break;` |
|      1 |  711 | `	case XML_CDATA_SECTION_NODE:` |
|      3 |  712 | `		pNode = xmlNewCDataBlock(pDoc,(const xmlChar *)zVal,nVal);` |
|      3 |  713 | `		break;` |
|      1 |  714 | `	case XML_COMMENT_NODE:` |
|      3 |  715 | `		pNode = xmlNewDocComment(pDoc,(const xmlChar *)zVal);` |
|      2 |  716 | `		break;` |
|      - |  717 | `	}` |
|     11 |  718 | `	PH7_LibxmlCaptureEnd(pVm,nMark,iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");` |
|     11 |  719 | `	if( pNode == 0 ){` |
|    ! 0 |  720 | `		ph7_result_bool(pCtx,0);` |
|    ! 0 |  721 | `		return PH7_OK;` |
|      - |  722 | `	}` |
|     11 |  723 | `	DomOrphanAdd(pDocNd->pShell,pNode);` |
|     11 |  724 | `	return DomResultNode(pCtx,pDocNd->pShell,pNode);` |
|      6 |  725 | `}` |
|      - |  726 | `/* void __dom_doc_normalize(docres) -- merge adjacent text nodes.  Merged-` |
|      - |  727 | ` * away siblings are PARKED as orphans, never freed, so any PHP wrapper to` |
|      - |  728 | ` * them stays valid (they just become empty orphans). */` |
|     24 |  729 | `static void DomNormalizeTree(phl_xmldoc *pShell,xmlNodePtr pNode)` |
|      1 |  730 | `{` |
|     25 |  731 | `	xmlNodePtr pChild = pNode->children;` |
|     49 |  732 | `	while( pChild ){` |
|     25 |  733 | `		if( pChild->type == XML_TEXT_NODE ){` |
|      7 |  734 | `			while( pChild->next && pChild->next->type == XML_TEXT_NODE ){` |
|    ! 0 |  735 | `				xmlNodePtr pNext = pChild->next;` |
|    ! 0 |  736 | `				if( pNext->content ){` |
|    ! 0 |  737 | `					xmlNodeAddContent(pChild,pNext->content);` |
|    ! 0 |  738 | `				}` |
|    ! 0 |  739 | `				xmlUnlinkNode(pNext);` |
|    ! 0 |  740 | `				DomOrphanAdd(pShell,pNext);` |
|    ! 0 |  741 | `			}` |
|     22 |  742 | `		}else if( pChild->type == XML_ELEMENT_NODE ){` |
|     19 |  743 | `			DomNormalizeTree(pShell,pChild);` |
|      9 |  744 | `		}` |
|     25 |  745 | `		pChild = pChild->next;` |
|      1 |  746 | `	}` |
|     25 |  747 | `}` |
|      6 |  748 | `DOM_THUNK(vm_builtin_dom_doc_normalize)` |
|      1 |  749 | `{` |
|      7 |  750 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|      7 |  751 | `	if( pNd ){` |
|      7 |  752 | `		DomNormalizeTree(pNd->pShell,(xmlNodePtr)pNd->pNode);` |
|      3 |  753 | `	}` |
|      7 |  754 | `	ph7_result_null(pCtx);` |
|      7 |  755 | `	return PH7_OK;` |
|      1 |  756 | `}` |
|      - |  757 |  |
|      - |  758 | `/* ===== C14N ===== */` |
|      - |  759 |  |
|      - |  760 | `/* Visibility callback: keep only the target's subtree (attrs/ns follow` |
|      - |  761 | ` * their owning element) -- the same shape php's ext/dom uses. */` |
|     10 |  762 | `static int DomC14NIsVisible(void *pUserData,xmlNodePtr pNode,xmlNodePtr pParent)` |
|      1 |  763 | `{` |
|     11 |  764 | `	xmlNodePtr pTarget = (xmlNodePtr)pUserData;` |
|      - |  765 | `	xmlNodePtr p;` |
|     11 |  766 | `	if( pNode->type == XML_NAMESPACE_DECL ){` |
|    ! 0 |  767 | `		p = pParent;` |
|     11 |  768 | `	}else if( pNode->type == XML_ATTRIBUTE_NODE ){` |
|      5 |  769 | `		p = pNode->parent;` |
|      3 |  770 | `	}else{` |
|      7 |  771 | `		p = pNode;` |
|      - |  772 | `	}` |
|     27 |  773 | `	while( p ){` |
|     19 |  774 | `		if( p == pTarget ){` |
|      3 |  775 | `			return 1;` |
|      - |  776 | `		}` |
|     17 |  777 | `		p = p->parent;` |
|      1 |  778 | `	}` |
|      9 |  779 | `	return 0;` |
|      6 |  780 | `}` |
|      - |  781 | `/* string __dom_node_c14n(res) -- "" on canonicalization failure (php` |
|      - |  782 | ` * returns an empty string for empty/unserializable input) */` |
|     12 |  783 | `DOM_THUNK(vm_builtin_dom_node_c14n)` |
|      1 |  784 | `{` |
|     13 |  785 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  786 | `	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;` |
|     13 |  787 | `	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;` |
|      - |  788 | `	sxu32 nMark;` |
|     13 |  789 | `	if( pNode == 0 \|\| pNode->doc == 0 ){` |
|    ! 0 |  790 | `		ph7_result_string(pCtx,"",0);` |
|    ! 0 |  791 | `		return PH7_OK;` |
|      - |  792 | `	}` |
|     13 |  793 | `	nMark = PH7_LibxmlCaptureBegin(pVm);` |
|     18 |  794 | `	if( pNode->type == XML_DOCUMENT_NODE \|\| pNode->type == XML_HTML_DOCUMENT_NODE ){` |
|     11 |  795 | `		xmlChar *zOut = 0;` |
|     11 |  796 | `		int nOut = xmlC14NDocDumpMemory((xmlDocPtr)pNode,0,XML_C14N_1_0,0,0,&zOut);` |
|     11 |  797 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::C14N");` |
|     11 |  798 | `		if( nOut < 0 \|\| zOut == 0 ){` |
|    ! 0 |  799 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 |  800 | `		}else{` |
|     11 |  801 | `			ph7_result_string(pCtx,(const char *)zOut,nOut);` |
|      - |  802 | `		}` |
|     11 |  803 | `		if( zOut ){` |
|     11 |  804 | `			xmlFree(zOut);` |
|      5 |  805 | `		}` |
|      6 |  806 | `	}else{` |
|      3 |  807 | `		xmlOutputBufferPtr pOut = xmlAllocOutputBuffer(0);` |
|      3 |  808 | `		int rc = -1;` |
|      3 |  809 | `		if( pOut ){` |
|      3 |  810 | `			rc = xmlC14NExecute(pNode->doc,DomC14NIsVisible,pNode,XML_C14N_1_0,0,0,pOut);` |
|      1 |  811 | `		}` |
|      3 |  812 | `		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::C14N");` |
|      3 |  813 | `		if( pOut == 0 \|\| rc < 0 ){` |
|    ! 0 |  814 | `			ph7_result_string(pCtx,"",0);` |
|    ! 0 |  815 | `		}else{` |
|      4 |  816 | `			ph7_result_string(pCtx,(const char *)xmlOutputBufferGetContent(pOut),` |
|      2 |  817 | `				(int)xmlOutputBufferGetSize(pOut));` |
|      - |  818 | `		}` |
|      3 |  819 | `		if( pOut ){` |
|      3 |  820 | `			xmlOutputBufferClose(pOut);` |
|      1 |  821 | `		}` |
|      - |  822 | `	}` |
|     13 |  823 | `	return PH7_OK;` |
|      7 |  824 | `}` |
|      - |  825 |  |
|      - |  826 | `/* ===== The DOM class prelude ===== */` |
|      - |  827 |  |
|      - |  828 | `static const char zDomLib1[] =` |
|      - |  829 | `	"class DOMException extends Exception {}"` |
|      - |  830 | `	"function __phl_dom_wrap($doc,$res)"` |
|      - |  831 | `	"{"` |
|      - |  832 | `	"  if( $res === null \|\| $res === false ){ return null; }"` |
|      - |  833 | `	"  $id = __dom_node_id($res);"` |
|      - |  834 | `	"  if( isset($doc->__nodes[$id]) ){ return $doc->__nodes[$id]; }"` |
|      - |  835 | `	"  switch( __dom_node_kind($res) ){"` |
|      - |  836 | `	"    case XML_ELEMENT_NODE:       $o = new DOMElement($res,$doc); break;"` |
|      - |  837 | `	"    case XML_ATTRIBUTE_NODE:     $o = new DOMAttr($res,$doc); break;"` |
|      - |  838 | `	"    case XML_TEXT_NODE:          $o = new DOMText($res,$doc); break;"` |
|      - |  839 | `	"    case XML_CDATA_SECTION_NODE: $o = new DOMCdataSection($res,$doc); break;"` |
|      - |  840 | `	"    case XML_COMMENT_NODE:       $o = new DOMComment($res,$doc); break;"` |
|      - |  841 | `	"    case XML_DOCUMENT_NODE:"` |
|      - |  842 | `	"    case XML_HTML_DOCUMENT_NODE: return $doc;"` |
|      - |  843 | `	"    default:                     $o = new DOMNode($res,$doc); break;"` |
|      - |  844 | `	"  }"` |
|      - |  845 | `	"  $doc->__nodes[$id] = $o;"` |
|      - |  846 | `	"  return $o;"` |
|      - |  847 | `	"}"` |
|      - |  848 | `	"class DOMNode"` |
|      - |  849 | `	"{"` |
|      - |  850 | `	"  public $__res;"` |
|      - |  851 | `	"  public $__doc;"` |
|      - |  852 | `	"  function __construct($res = null,$doc = null)"` |
|      - |  853 | `	"  {"` |
|      - |  854 | `	"    $this->__res = $res;"` |
|      - |  855 | `	"    $this->__doc = ($doc === null) ? $this : $doc;"` |
|      - |  856 | `	"  }"` |
|      - |  857 | `	"  function appendChild($node)"` |
|      - |  858 | `	"  {"` |
|      - |  859 | `	"    if( !__dom_node_append($this->__res,$node->__res) ){"` |
|      - |  860 | `	"      throw new DOMException('Wrong Document Error');"` |
|      - |  861 | `	"    }"` |
|      - |  862 | `	"    return $node;"` |
|      - |  863 | `	"  }"` |
|      - |  864 | `	"  function insertBefore($node,$child = null)"` |
|      - |  865 | `	"  {"` |
|      - |  866 | `	"    if( !__dom_node_insert_before($this->__res,$node->__res,$child === null ? null : $child->__res) ){"` |
|      - |  867 | `	"      throw new DOMException('Not Found Error');"` |
|      - |  868 | `	"    }"` |
|      - |  869 | `	"    return $node;"` |
|      - |  870 | `	"  }"` |
|      - |  871 | `	"  function removeChild($child)"` |
|      - |  872 | `	"  {"` |
|      - |  873 | `	"    if( !__dom_node_remove($this->__res,$child->__res) ){"` |
|      - |  874 | `	"      throw new DOMException('Not Found Error');"` |
|      - |  875 | `	"    }"` |
|      - |  876 | `	"    return $child;"` |
|      - |  877 | `	"  }"` |
|      - |  878 | `	"  function replaceChild($node,$child)"` |
|      - |  879 | `	"  {"` |
|      - |  880 | `	"    if( !__dom_node_replace($this->__res,$node->__res,$child->__res) ){"` |
|      - |  881 | `	"      throw new DOMException('Not Found Error');"` |
|      - |  882 | `	"    }"` |
|      - |  883 | `	"    return $child;"` |
|      - |  884 | `	"  }"` |
|      - |  885 | `	"  function hasChildNodes(){ return __dom_node_child_count($this->__res) > 0; }"` |
|      - |  886 | `	"  function hasAttributes(){ return __dom_elem_attr_count($this->__res) > 0; }"` |
|      - |  887 | `	"  function isSameNode($otherNode){ return __dom_node_id($this->__res) === __dom_node_id($otherNode->__res); }"` |
|      - |  888 | `	"  function getLineNo(){ return __dom_node_line_no($this->__res); }"` |
|      - |  889 | `	"  function C14N($exclusive = false,$withComments = false,$xpath = null,$nsPrefixes = null)"` |
|      - |  890 | `	"  {"` |
|      - |  891 | `	"    return __dom_node_c14n($this->__res);"` |
|      - |  892 | `	"  }"` |
|      - |  893 | `	"  function getElementsByTagName($qualifiedName)"` |
|      - |  894 | `	"  {"` |
|      - |  895 | `	"    return new DOMNodeList('gebtn',$this->__doc,$this->__res,(string)$qualifiedName);"` |
|      - |  896 | `	"  }"` |
|      - |  897 | `	"  protected function __nodeProp($name)"` |
|      - |  898 | `	"  {"` |
|      - |  899 | `	"    switch( $name ){"` |
|      - |  900 | `	"      case 'nodeName':     return __dom_node_name($this->__res);"` |
|      - |  901 | `	"      case 'nodeValue':    return __dom_node_value($this->__res);"` |
|      - |  902 | `	"      case 'nodeType':     return __dom_node_kind($this->__res);"` |
|      - |  903 | `	"      case 'parentNode':   return __phl_dom_wrap($this->__doc,__dom_node_parent($this->__res));"` |
|      - |  904 | `	"      case 'firstChild':   return __phl_dom_wrap($this->__doc,__dom_node_first($this->__res));"` |
|      - |  905 | `	"      case 'lastChild':    return __phl_dom_wrap($this->__doc,__dom_node_last($this->__res));"` |
|      - |  906 | `	"      case 'nextSibling':  return __phl_dom_wrap($this->__doc,__dom_node_next($this->__res));"` |
|      - |  907 | `	"      case 'previousSibling': return __phl_dom_wrap($this->__doc,__dom_node_prev($this->__res));"` |
|      - |  908 | `	"      case 'ownerDocument': return ($this instanceof DOMDocument) ? null : $this->__doc;"` |
|      - |  909 | `	"      case 'childNodes':   return new DOMNodeList('child',$this->__doc,$this->__res);"` |
|      - |  910 | `	"      case 'textContent':  return __dom_node_text_content($this->__res);"` |
|      - |  911 | `	"      case 'attributes':"` |
|      - |  912 | `	"        return (__dom_node_kind($this->__res) === XML_ELEMENT_NODE)"` |
|      - |  913 | `	"          ? new DOMNamedNodeMap($this->__doc,$this->__res) : null;"` |
|      - |  914 | `	"      case 'childElementCount': return __dom_node_elem_child_count($this->__res);"` |
|      - |  915 | `	"    }"` |
|      - |  916 | `	"    return null;"` |
|      - |  917 | `	"  }"` |
|      - |  918 | `	"  function __get($name){ return $this->__nodeProp($name); }"` |
|      - |  919 | `	"}";` |
|      - |  920 |  |
|      - |  921 | `static const char zDomLib2[] =` |
|      - |  922 | `	"class DOMDocument extends DOMNode"` |
|      - |  923 | `	"{"` |
|      - |  924 | `	"  public $preserveWhiteSpace = true;"` |
|      - |  925 | `	"  public $formatOutput = false;"` |
|      - |  926 | `	"  public $__nodes = array();"` |
|      - |  927 | `	"  function __construct($version = '1.0',$encoding = '')"` |
|      - |  928 | `	"  {"` |
|      - |  929 | `	"    parent::__construct(__dom_doc_new((string)$version,(string)$encoding),null);"` |
|      - |  930 | `	"  }"` |
|      - |  931 | `	"  function loadXML($source,$options = 0)"` |
|      - |  932 | `	"  {"` |
|      - |  933 | `	"    $source = (string)$source;"` |
|      - |  934 | `	"    if( $source === '' ){"` |
|      - |  935 | `	"      throw new ValueError('DOMDocument::loadXML(): Argument #1 ($source) must not be empty');"` |
|      - |  936 | `	"    }"` |
|      - |  937 | `	"    $r = __dom_doc_loadxml($source,(bool)$this->preserveWhiteSpace,(int)$options);"` |
|      - |  938 | `	"    if( $r === false ){ return false; }"` |
|      - |  939 | `	"    $this->__res = $r;"` |
|      - |  940 | `	"    $this->__nodes = array();"` |
|      - |  941 | `	"    return true;"` |
|      - |  942 | `	"  }"` |
|      - |  943 | `	"  function saveXML($node = null)"` |
|      - |  944 | `	"  {"` |
|      - |  945 | `	"    return __dom_doc_savexml($this->__res,$node === null ? null : $node->__res,(bool)$this->formatOutput);"` |
|      - |  946 | `	"  }"` |
|      - |  947 | `	"  function createElement($localName,$value = '')"` |
|      - |  948 | `	"  {"` |
|      - |  949 | `	"    $r = __dom_doc_create($this->__res,XML_ELEMENT_NODE,(string)$localName,(string)$value);"` |
|      - |  950 | `	"    if( $r === false ){ throw new DOMException('Invalid Character Error'); }"` |
|      - |  951 | `	"    return __phl_dom_wrap($this,$r);"` |
|      - |  952 | `	"  }"` |
|      - |  953 | `	"  function createTextNode($data)"` |
|      - |  954 | `	"  {"` |
|      - |  955 | `	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_TEXT_NODE,'',(string)$data));"` |
|      - |  956 | `	"  }"` |
|      - |  957 | `	"  function createComment($data)"` |
|      - |  958 | `	"  {"` |
|      - |  959 | `	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_COMMENT_NODE,'',(string)$data));"` |
|      - |  960 | `	"  }"` |
|      - |  961 | `	"  function createCDATASection($data)"` |
|      - |  962 | `	"  {"` |
|      - |  963 | `	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_CDATA_SECTION_NODE,'',(string)$data));"` |
|      - |  964 | `	"  }"` |
|      - |  965 | `	"  function normalizeDocument(){ __dom_doc_normalize($this->__res); }"` |
|      - |  966 | `	"  function __get($name)"` |
|      - |  967 | `	"  {"` |
|      - |  968 | `	"    if( $name === 'documentElement' ){"` |
|      - |  969 | `	"      return __phl_dom_wrap($this,__dom_doc_root($this->__res));"` |
|      - |  970 | `	"    }"` |
|      - |  971 | `	"    return $this->__nodeProp($name);"` |
|      - |  972 | `	"  }"` |
|      - |  973 | `	"}"` |
|      - |  974 | `	"class DOMElement extends DOMNode"` |
|      - |  975 | `	"{"` |
|      - |  976 | `	"  function getAttribute($qualifiedName){ return __dom_elem_get_attr($this->__res,(string)$qualifiedName); }"` |
|      - |  977 | `	"  function hasAttribute($qualifiedName){ return __dom_elem_has_attr($this->__res,(string)$qualifiedName); }"` |
|      - |  978 | `	"  function setAttribute($qualifiedName,$value)"` |
|      - |  979 | `	"  {"` |
|      - |  980 | `	"    if( !__dom_elem_set_attr($this->__res,(string)$qualifiedName,(string)$value) ){"` |
|      - |  981 | `	"      throw new DOMException('Invalid Character Error');"` |
|      - |  982 | `	"    }"` |
|      - |  983 | `	"    return __phl_dom_wrap($this->__doc,__dom_elem_attr_node($this->__res,(string)$qualifiedName));"` |
|      - |  984 | `	"  }"` |
|      - |  985 | `	"  function removeAttribute($qualifiedName){ return __dom_elem_remove_attr($this->__res,(string)$qualifiedName); }"` |
|      - |  986 | `	"  function getAttributeNS($namespace,$localName)"` |
|      - |  987 | `	"  {"` |
|      - |  988 | `	"    return __dom_elem_get_attr_ns($this->__res,(string)$namespace,(string)$localName);"` |
|      - |  989 | `	"  }"` |
|      - |  990 | `	"  function setAttributeNS($namespace,$qualifiedName,$value)"` |
|      - |  991 | `	"  {"` |
|      - |  992 | `	"    if( !__dom_elem_set_attr_ns($this->__res,(string)$namespace,(string)$qualifiedName,(string)$value) ){"` |
|      - |  993 | `	"      throw new DOMException('Namespace Error');"` |
|      - |  994 | `	"    }"` |
|      - |  995 | `	"  }"` |
|      - |  996 | `	"  function __get($name)"` |
|      - |  997 | `	"  {"` |
|      - |  998 | `	"    if( $name === 'tagName' ){ return __dom_node_name($this->__res); }"` |
|      - |  999 | `	"    return $this->__nodeProp($name);"` |
|      - | 1000 | `	"  }"` |
|      - | 1001 | `	"}"` |
|      - | 1002 | `	"class DOMAttr extends DOMNode"` |
|      - | 1003 | `	"{"` |
|      - | 1004 | `	"  function __get($name)"` |
|      - | 1005 | `	"  {"` |
|      - | 1006 | `	"    if( $name === 'name' ){ return __dom_node_name($this->__res); }"` |
|      - | 1007 | `	"    if( $name === 'value' ){ return __dom_node_value($this->__res); }"` |
|      - | 1008 | `	"    if( $name === 'ownerElement' ){ return __phl_dom_wrap($this->__doc,__dom_node_parent($this->__res)); }"` |
|      - | 1009 | `	"    return $this->__nodeProp($name);"` |
|      - | 1010 | `	"  }"` |
|      - | 1011 | `	"}"` |
|      - | 1012 | `	"class DOMCharacterData extends DOMNode"` |
|      - | 1013 | `	"{"` |
|      - | 1014 | `	"  function __get($name)"` |
|      - | 1015 | `	"  {"` |
|      - | 1016 | `	"    if( $name === 'data' ){ return __dom_node_value($this->__res); }"` |
|      - | 1017 | `	"    if( $name === 'length' ){ return strlen(__dom_node_value($this->__res)); }"` |
|      - | 1018 | `	"    return $this->__nodeProp($name);"` |
|      - | 1019 | `	"  }"` |
|      - | 1020 | `	"}"` |
|      - | 1021 | `	"class DOMText extends DOMCharacterData"` |
|      - | 1022 | `	"{"` |
|      - | 1023 | `	"  function __get($name)"` |
|      - | 1024 | `	"  {"` |
|      - | 1025 | `	"    if( $name === 'wholeText' ){ return __dom_node_value($this->__res); }"` |
|      - | 1026 | `	"    return parent::__get($name);"` |
|      - | 1027 | `	"  }"` |
|      - | 1028 | `	"}"` |
|      - | 1029 | `	"class DOMComment extends DOMCharacterData {}"` |
|      - | 1030 | `	"class DOMCdataSection extends DOMText {}";` |
|      - | 1031 |  |
|      - | 1032 | `static const char zDomLib3[] =` |
|      - | 1033 | `	"class DOMNodeList implements Iterator, Countable"` |
|      - | 1034 | `	"{"` |
|      - | 1035 | `	"  public $__kind;"` |
|      - | 1036 | `	"  public $__doc;"` |
|      - | 1037 | `	"  public $__owner;"` |
|      - | 1038 | `	"  public $__name;"` |
|      - | 1039 | `	"  public $__snap;"` |
|      - | 1040 | `	"  private $__pos = 0;"` |
|      - | 1041 | `	"  function __construct($kind = null,$doc = null,$owner = null,$name = null,$snap = null)"` |
|      - | 1042 | `	"  {"` |
|      - | 1043 | `	"    $this->__kind = $kind; $this->__doc = $doc; $this->__owner = $owner;"` |
|      - | 1044 | `	"    $this->__name = $name; $this->__snap = $snap;"` |
|      - | 1045 | `	"  }"` |
|      - | 1046 | `	"  function count()"` |
|      - | 1047 | `	"  {"` |
|      - | 1048 | `	"    if( $this->__kind === 'snap' ){ return count($this->__snap); }"` |
|      - | 1049 | `	"    if( $this->__kind === 'child' ){ return __dom_node_child_count($this->__owner); }"` |
|      - | 1050 | `	"    return __dom_gebtn_count($this->__owner,$this->__name);"` |
|      - | 1051 | `	"  }"` |
|      - | 1052 | `	"  function item($index)"` |
|      - | 1053 | `	"  {"` |
|      - | 1054 | `	"    $index = (int)$index;"` |
|      - | 1055 | `	"    if( $index < 0 ){ return null; }"` |
|      - | 1056 | `	"    if( $this->__kind === 'snap' ){"` |
|      - | 1057 | `	"      return isset($this->__snap[$index]) ? __phl_dom_wrap($this->__doc,$this->__snap[$index]) : null;"` |
|      - | 1058 | `	"    }"` |
|      - | 1059 | `	"    if( $this->__kind === 'child' ){"` |
|      - | 1060 | `	"      return __phl_dom_wrap($this->__doc,__dom_node_child_at($this->__owner,$index));"` |
|      - | 1061 | `	"    }"` |
|      - | 1062 | `	"    return __phl_dom_wrap($this->__doc,__dom_gebtn_at($this->__owner,$this->__name,$index));"` |
|      - | 1063 | `	"  }"` |
|      - | 1064 | `	"  function rewind(){ $this->__pos = 0; }"` |
|      - | 1065 | `	"  function valid(){ return $this->__pos < $this->count(); }"` |
|      - | 1066 | `	"  function current(){ return $this->item($this->__pos); }"` |
|      - | 1067 | `	"  function key(){ return $this->__pos; }"` |
|      - | 1068 | `	"  function next(){ $this->__pos++; }"` |
|      - | 1069 | `	"  function __get($name)"` |
|      - | 1070 | `	"  {"` |
|      - | 1071 | `	"    if( $name === 'length' ){ return $this->count(); }"` |
|      - | 1072 | `	"    return null;"` |
|      - | 1073 | `	"  }"` |
|      - | 1074 | `	"}"` |
|      - | 1075 | `	"class DOMNamedNodeMap implements Countable"` |
|      - | 1076 | `	"{"` |
|      - | 1077 | `	"  public $__doc;"` |
|      - | 1078 | `	"  public $__owner;"` |
|      - | 1079 | `	"  function __construct($doc = null,$owner = null){ $this->__doc = $doc; $this->__owner = $owner; }"` |
|      - | 1080 | `	"  function count(){ return __dom_elem_attr_count($this->__owner); }"` |
|      - | 1081 | `	"  function item($index)"` |
|      - | 1082 | `	"  {"` |
|      - | 1083 | `	"    return __phl_dom_wrap($this->__doc,__dom_elem_attr_at($this->__owner,(int)$index));"` |
|      - | 1084 | `	"  }"` |
|      - | 1085 | `	"  function getNamedItem($qualifiedName)"` |
|      - | 1086 | `	"  {"` |
|      - | 1087 | `	"    $n = $this->count();"` |
|      - | 1088 | `	"    for( $i = 0; $i < $n; $i++ ){"` |
|      - | 1089 | `	"      $a = $this->item($i);"` |
|      - | 1090 | `	"      if( $a !== null && $a->name === $qualifiedName ){ return $a; }"` |
|      - | 1091 | `	"    }"` |
|      - | 1092 | `	"    return null;"` |
|      - | 1093 | `	"  }"` |
|      - | 1094 | `	"  function __get($name)"` |
|      - | 1095 | `	"  {"` |
|      - | 1096 | `	"    if( $name === 'length' ){ return $this->count(); }"` |
|      - | 1097 | `	"    return null;"` |
|      - | 1098 | `	"  }"` |
|      - | 1099 | `	"}";` |
|      - | 1100 |  |
|      - | 1101 | `/*` |
|      - | 1102 | ` * Install the DOM library: __dom_* thunks first, then the class chunks.` |
|      - | 1103 | ` * Called from PH7_VmInit inside the bCompilingBuiltin window, after` |
|      - | 1104 | ` * PH7_VmInstallLibxml (the capture plumbing must exist).` |
|      - | 1105 | ` */` |
|   3884 | 1106 | `PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)` |
|      5 | 1107 | `{` |
|      - | 1108 | `	static const struct {` |
|      - | 1109 | `		const char *zName;` |
|      - | 1110 | `		ProchHostFunction xFunc;` |
|      - | 1111 | `	} aFunc[] = {` |
|      - | 1112 | `		{ "__dom_node_id",            vm_builtin_dom_node_id            },` |
|      - | 1113 | `		{ "__dom_node_kind",          vm_builtin_dom_node_kind          },` |
|      - | 1114 | `		{ "__dom_node_name",          vm_builtin_dom_node_name          },` |
|      - | 1115 | `		{ "__dom_node_value",         vm_builtin_dom_node_value         },` |
|      - | 1116 | `		{ "__dom_node_text_content",  vm_builtin_dom_node_text_content  },` |
|      - | 1117 | `		{ "__dom_node_line_no",       vm_builtin_dom_node_line_no       },` |
|      - | 1118 | `		{ "__dom_node_parent",        vm_builtin_dom_node_parent        },` |
|      - | 1119 | `		{ "__dom_node_first",         vm_builtin_dom_node_first         },` |
|      - | 1120 | `		{ "__dom_node_last",          vm_builtin_dom_node_last          },` |
|      - | 1121 | `		{ "__dom_node_next",          vm_builtin_dom_node_next          },` |
|      - | 1122 | `		{ "__dom_node_prev",          vm_builtin_dom_node_prev          },` |
|      - | 1123 | `		{ "__dom_node_child_count",   vm_builtin_dom_node_child_count   },` |
|      - | 1124 | `		{ "__dom_node_child_at",      vm_builtin_dom_node_child_at      },` |
|      - | 1125 | `		{ "__dom_node_elem_child_count", vm_builtin_dom_node_elem_child_count },` |
|      - | 1126 | `		{ "__dom_node_append",        vm_builtin_dom_node_append        },` |
|      - | 1127 | `		{ "__dom_node_insert_before", vm_builtin_dom_node_insert_before },` |
|      - | 1128 | `		{ "__dom_node_remove",        vm_builtin_dom_node_remove        },` |
|      - | 1129 | `		{ "__dom_node_replace",       vm_builtin_dom_node_replace       },` |
|      - | 1130 | `		{ "__dom_node_c14n",          vm_builtin_dom_node_c14n          },` |
|      - | 1131 | `		{ "__dom_elem_get_attr",      vm_builtin_dom_elem_get_attr      },` |
|      - | 1132 | `		{ "__dom_elem_has_attr",      vm_builtin_dom_elem_has_attr      },` |
|      - | 1133 | `		{ "__dom_elem_set_attr",      vm_builtin_dom_elem_set_attr      },` |
|      - | 1134 | `		{ "__dom_elem_remove_attr",   vm_builtin_dom_elem_remove_attr   },` |
|      - | 1135 | `		{ "__dom_elem_get_attr_ns",   vm_builtin_dom_elem_get_attr_ns   },` |
|      - | 1136 | `		{ "__dom_elem_set_attr_ns",   vm_builtin_dom_elem_set_attr_ns   },` |
|      - | 1137 | `		{ "__dom_elem_attr_node",     vm_builtin_dom_elem_attr_node     },` |
|      - | 1138 | `		{ "__dom_elem_attr_count",    vm_builtin_dom_elem_attr_count    },` |
|      - | 1139 | `		{ "__dom_elem_attr_at",       vm_builtin_dom_elem_attr_at       },` |
|      - | 1140 | `		{ "__dom_gebtn_count",        vm_builtin_dom_gebtn_count        },` |
|      - | 1141 | `		{ "__dom_gebtn_at",           vm_builtin_dom_gebtn_at           },` |
|      - | 1142 | `		{ "__dom_doc_new",            vm_builtin_dom_doc_new            },` |
|      - | 1143 | `		{ "__dom_doc_loadxml",        vm_builtin_dom_doc_loadxml        },` |
|      - | 1144 | `		{ "__dom_doc_root",           vm_builtin_dom_doc_root           },` |
|      - | 1145 | `		{ "__dom_doc_savexml",        vm_builtin_dom_doc_savexml        },` |
|      - | 1146 | `		{ "__dom_doc_create",         vm_builtin_dom_doc_create         },` |
|      - | 1147 | `		{ "__dom_doc_normalize",      vm_builtin_dom_doc_normalize      },` |
|      - | 1148 | `	};` |
|      - | 1149 | `	sxu32 n;` |
|      - | 1150 | `	sxi32 rc;` |
| 143713 | 1151 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 139829 | 1152 | `		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);` |
|  69917 | 1153 | `	}` |
|   3889 | 1154 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib1,sizeof(zDomLib1)-1);` |
|   3889 | 1155 | `	if( rc == SXRET_OK ){` |
|   3889 | 1156 | `		rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib2,sizeof(zDomLib2)-1);` |
|   1942 | 1157 | `	}` |
|   3889 | 1158 | `	if( rc == SXRET_OK ){` |
|   3889 | 1159 | `		rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib3,sizeof(zDomLib3)-1);` |
|   1942 | 1160 | `	}` |
|   3889 | 1161 | `	return rc;` |
|      5 | 1162 | `}` |
|      - | 1163 |  |
|      - | 1164 | `#else` |
|      - | 1165 | `/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */` |
|      - | 1166 | `typedef int vm_dom_unused;` |
|      - | 1167 | `#endif /* PH7_ENABLE_LIBXML */` |
|      - | 1168 |  |
