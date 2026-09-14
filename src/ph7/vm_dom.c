/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifdef PH7_ENABLE_LIBXML
#include "ph7int.h"
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/c14n.h>
#include <libxml/xmlsave.h>

/*
 * ext/dom on libxml2: __dom_* native thunks + the DOM class prelude.
 *
 * Architecture (see also vm_libxml.c): the PHP-visible DOM classes are
 * compiled from the zDomLib prelude chunks below and hold two private
 * props -- $__res, a phl_domnode resource {phl_xmldoc*, xmlNodePtr}, and
 * $__doc, the owning DOMDocument wrapper.  All tree work happens in the
 * __dom_* thunks; the prelude does dispatch, identity mapping and the
 * php-facing signatures.
 *
 * Node identity: php guarantees $doc->documentElement === $doc->
 * documentElement.  Every wrap goes through __phl_dom_wrap() which keys
 * a per-document cache ($doc->__nodes) by __dom_node_id() (the pointer
 * value), so the same underlying node always yields the same object.
 *
 * Tree surgery (append/insert/replace/remove) is done with manual pointer
 * splicing instead of xmlAddChild: xmlAddChild MERGES adjacent text nodes
 * and frees the merged-away node, which would dangle any PHP wrapper (and
 * violates DOM semantics, which php follows -- appendChild never merges).
 * Unlinked nodes are parked on the owning phl_xmldoc's orphan set so they
 * are freed with the document at VM reset/release.
 */

#define DOM_THUNK(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)

/* Extract a phl_domnode from a thunk argument (NULL if not a resource) */
static phl_domnode * DomNodeArg(ph7_value *pVal)
{
	if( pVal == 0 || !ph7_value_is_resource(pVal) ){
		return 0;
	}
	return (phl_domnode *)ph7_value_to_resource(pVal);
}
/* Return a (possibly NULL) xmlNode as a fresh phl_domnode resource */
static int DomResultNode(ph7_context *pCtx,phl_xmldoc *pShell,void *pNode)
{
	phl_domnode *pWrap;
	if( pNode == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pWrap = (phl_domnode *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,sizeof(phl_domnode));
	if( pWrap == 0 ){
		ph7_context_throw_error(pCtx,PH7_CTX_ERR,"PH7 is running out of memory");
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pWrap->pShell = pShell;
	pWrap->pNode = pNode;
	ph7_result_resource(pCtx,pWrap);
	return PH7_OK;
}
/* Orphan bookkeeping: nodes not linked into their tree but still owned */
static void DomOrphanAdd(phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pShell->aOrphans) ; ++n ){
		if( apOrphan[n] == pNode ){
			return;
		}
	}
	SySetPut(&pShell->aOrphans,(const void *)&pNode);
}
static void DomOrphanRemove(phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNodePtr *apOrphan = (xmlNodePtr *)SySetBasePtr(&pShell->aOrphans);
	sxu32 n,nUsed = SySetUsed(&pShell->aOrphans);
	for( n = 0 ; n < nUsed ; ++n ){
		if( apOrphan[n] == pNode ){
			apOrphan[n] = apOrphan[nUsed-1];
			SySetTruncate(&pShell->aOrphans,nUsed-1);
			return;
		}
	}
}
/* Detach a node from wherever it is (tree or orphan set) prior to linking */
static void DomDetach(phl_xmldoc *pShell,xmlNodePtr pNode)
{
	if( pNode->parent ){
		xmlUnlinkNode(pNode);
	}else{
		DomOrphanRemove(pShell,pNode);
	}
}
/* Raw child-list splicing (no text-node merging -- DOM/php semantics) */
static void DomLinkLast(xmlNodePtr pParent,xmlNodePtr pChild)
{
	pChild->parent = pParent;
	pChild->next = 0;
	if( pParent->last ){
		pParent->last->next = pChild;
		pChild->prev = pParent->last;
	}else{
		pParent->children = pChild;
		pChild->prev = 0;
	}
	pParent->last = pChild;
}
static void DomLinkBefore(xmlNodePtr pParent,xmlNodePtr pChild,xmlNodePtr pRef)
{
	pChild->parent = pParent;
	pChild->next = pRef;
	pChild->prev = pRef->prev;
	if( pRef->prev ){
		pRef->prev->next = pChild;
	}else{
		pParent->children = pChild;
	}
	pRef->prev = pChild;
}

/* ===== Node introspection thunks ===== */

/* int __dom_node_id(res) -- identity-map key (the node pointer) */
DOM_THUNK(vm_builtin_dom_node_id)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	if( pNd == 0 ){
		ph7_result_int64(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx,(ph7_int64)(sxuptr)pNd->pNode);
	return PH7_OK;
}
/* int __dom_node_kind(res) -- the XML_*_NODE type */
DOM_THUNK(vm_builtin_dom_node_kind)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	ph7_result_int(pCtx,pNode ? (int)pNode->type : 0);
	return PH7_OK;
}
/* string __dom_node_name(res) -- php nodeName rules */
DOM_THUNK(vm_builtin_dom_node_name)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pNode == 0 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	switch( pNode->type ){
	case XML_TEXT_NODE:          ph7_result_string(pCtx,"#text",(int)sizeof("#text")-1); break;
	case XML_CDATA_SECTION_NODE: ph7_result_string(pCtx,"#cdata-section",(int)sizeof("#cdata-section")-1); break;
	case XML_COMMENT_NODE:       ph7_result_string(pCtx,"#comment",(int)sizeof("#comment")-1); break;
	case XML_HTML_DOCUMENT_NODE:
	case XML_DOCUMENT_NODE:      ph7_result_string(pCtx,"#document",(int)sizeof("#document")-1); break;
	case XML_DOCUMENT_FRAG_NODE: ph7_result_string(pCtx,"#document-fragment",(int)sizeof("#document-fragment")-1); break;
	default:
		if( (pNode->type == XML_ELEMENT_NODE || pNode->type == XML_ATTRIBUTE_NODE)
			&& pNode->ns && pNode->ns->prefix ){
			ph7_result_string_format(pCtx,"%s:%s",(const char *)pNode->ns->prefix,(const char *)pNode->name);
		}else{
			ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);
		}
		break;
	}
	return PH7_OK;
}
/* ?string __dom_node_value(res) -- php nodeValue (NULL for documents) */
DOM_THUNK(vm_builtin_dom_node_value)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlChar *zContent;
	if( pNode == 0 || pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE
		|| pNode->type == XML_DOCUMENT_TYPE_NODE ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zContent = xmlNodeGetContent(pNode);
	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);
	if( zContent ){
		xmlFree(zContent);
	}
	return PH7_OK;
}
/* string __dom_node_text_content(res) */
DOM_THUNK(vm_builtin_dom_node_text_content)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlChar *zContent = pNode ? xmlNodeGetContent(pNode) : 0;
	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);
	if( zContent ){
		xmlFree(zContent);
	}
	return PH7_OK;
}
/* int __dom_node_line_no(res) */
DOM_THUNK(vm_builtin_dom_node_line_no)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	ph7_result_int64(pCtx,pNode ? (ph7_int64)xmlGetLineNo(pNode) : 0);
	return PH7_OK;
}
/* Navigation: parent/first/last/next/prev share one worker */
static int DomNavigate(ph7_context *pCtx,int nArg,ph7_value **apArg,int iDir)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlNodePtr pOut = 0;
	if( pNode ){
		switch( iDir ){
		case 0: pOut = pNode->parent; break;
		case 1: pOut = pNode->children; break;
		case 2: pOut = pNode->last; break;
		case 3: pOut = pNode->next; break;
		case 4: pOut = pNode->prev; break;
		}
	}
	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pOut);
}
DOM_THUNK(vm_builtin_dom_node_parent){ return DomNavigate(pCtx,nArg,apArg,0); }
DOM_THUNK(vm_builtin_dom_node_first){ return DomNavigate(pCtx,nArg,apArg,1); }
DOM_THUNK(vm_builtin_dom_node_last){ return DomNavigate(pCtx,nArg,apArg,2); }
DOM_THUNK(vm_builtin_dom_node_next){ return DomNavigate(pCtx,nArg,apArg,3); }
DOM_THUNK(vm_builtin_dom_node_prev){ return DomNavigate(pCtx,nArg,apArg,4); }
/* int __dom_node_child_count(res) / ?res __dom_node_child_at(res,i) */
DOM_THUNK(vm_builtin_dom_node_child_count)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;
	int iCount = 0;
	for( ; pChild ; pChild = pChild->next ){
		iCount++;
	}
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
DOM_THUNK(vm_builtin_dom_node_child_at)
{
	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	int iWant = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;
	for( ; pChild && iWant > 0 ; pChild = pChild->next ){
		iWant--;
	}
	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pChild);
}
/* int __dom_node_elem_child_count(res) -- childElementCount */
DOM_THUNK(vm_builtin_dom_node_elem_child_count)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pChild = pNd ? ((xmlNodePtr)pNd->pNode)->children : 0;
	int iCount = 0;
	for( ; pChild ; pChild = pChild->next ){
		if( pChild->type == XML_ELEMENT_NODE ){
			iCount++;
		}
	}
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}

/* ===== Tree surgery thunks ===== */

/* bool __dom_node_append(parentres,childres) */
DOM_THUNK(vm_builtin_dom_node_append)
{
	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	phl_domnode *pChd = nArg > 1 ? DomNodeArg(apArg[1]) : 0;
	xmlNodePtr pParent,pChild;
	if( pPar == 0 || pChd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pParent = (xmlNodePtr)pPar->pNode;
	pChild = (xmlNodePtr)pChd->pNode;
	if( pParent->doc != pChild->doc || pChild == pParent ){
		ph7_result_bool(pCtx,0); /* Wrong Document Error */
		return PH7_OK;
	}
	DomDetach(pChd->pShell,pChild);
	DomLinkLast(pParent,pChild);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool __dom_node_insert_before(parentres,newres,?refres) */
DOM_THUNK(vm_builtin_dom_node_insert_before)
{
	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	phl_domnode *pNew = nArg > 1 ? DomNodeArg(apArg[1]) : 0;
	phl_domnode *pRef = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? DomNodeArg(apArg[2]) : 0;
	xmlNodePtr pParent,pChild,pAnchor;
	if( pPar == 0 || pNew == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pParent = (xmlNodePtr)pPar->pNode;
	pChild = (xmlNodePtr)pNew->pNode;
	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;
	if( pParent->doc != pChild->doc || pChild == pParent
		|| (pAnchor && pAnchor->parent != pParent) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	DomDetach(pNew->pShell,pChild);
	if( pAnchor ){
		DomLinkBefore(pParent,pChild,pAnchor);
	}else{
		DomLinkLast(pParent,pChild);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool __dom_node_remove(parentres,childres) */
DOM_THUNK(vm_builtin_dom_node_remove)
{
	phl_domnode *pPar = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	phl_domnode *pChd = nArg > 1 ? DomNodeArg(apArg[1]) : 0;
	xmlNodePtr pChild;
	if( pPar == 0 || pChd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pChild = (xmlNodePtr)pChd->pNode;
	if( pChild->parent != (xmlNodePtr)pPar->pNode ){
		ph7_result_bool(pCtx,0); /* Not Found Error */
		return PH7_OK;
	}
	xmlUnlinkNode(pChild);
	DomOrphanAdd(pChd->pShell,pChild);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool __dom_node_replace(parentres,newres,oldres) */
DOM_THUNK(vm_builtin_dom_node_replace)
{
	phl_domnode *pPar = nArg > 2 ? DomNodeArg(apArg[0]) : 0;
	phl_domnode *pNew = nArg > 2 ? DomNodeArg(apArg[1]) : 0;
	phl_domnode *pOld = nArg > 2 ? DomNodeArg(apArg[2]) : 0;
	xmlNodePtr pParent,pChild,pVictim;
	if( pPar == 0 || pNew == 0 || pOld == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pParent = (xmlNodePtr)pPar->pNode;
	pChild = (xmlNodePtr)pNew->pNode;
	pVictim = (xmlNodePtr)pOld->pNode;
	if( pParent->doc != pChild->doc || pVictim->parent != pParent || pChild == pParent ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pChild != pVictim ){
		DomDetach(pNew->pShell,pChild);
		DomLinkBefore(pParent,pChild,pVictim);
		xmlUnlinkNode(pVictim);
		DomOrphanAdd(pOld->pShell,pVictim);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* ===== Element attribute thunks ===== */

/* string __dom_elem_get_attr(res,name) -- "" when absent (php) */
DOM_THUNK(vm_builtin_dom_elem_get_attr)
{
	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlChar *zVal = pNd ? xmlGetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;
	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);
	if( zVal ){
		xmlFree(zVal);
	}
	return PH7_OK;
}
/* bool __dom_elem_has_attr(res,name) */
DOM_THUNK(vm_builtin_dom_elem_has_attr)
{
	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	ph7_result_bool(pCtx,pNd && xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) != 0);
	return PH7_OK;
}
/* bool __dom_elem_set_attr(res,name,value) */
DOM_THUNK(vm_builtin_dom_elem_set_attr)
{
	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;
	const char *zName = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";
	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";
	if( pNd == 0 || xmlValidateName((const xmlChar *)zName,0) != 0 ){
		ph7_result_bool(pCtx,0); /* Invalid Character Error */
		return PH7_OK;
	}
	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* bool __dom_elem_remove_attr(res,name) */
DOM_THUNK(vm_builtin_dom_elem_remove_attr)
{
	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlAttrPtr pAttr = pNd ? xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;
	if( pAttr == 0 || pAttr->type != XML_ATTRIBUTE_NODE ){
		/* Absent (or a DTD default): php returns false */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	xmlRemoveProp(pAttr);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* string __dom_elem_get_attr_ns(res,uri,local) */
DOM_THUNK(vm_builtin_dom_elem_get_attr_ns)
{
	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;
	const char *zUri = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";
	const char *zLocal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";
	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,(const xmlChar *)zUri) : 0;
	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);
	if( zVal ){
		xmlFree(zVal);
	}
	return PH7_OK;
}
/* bool __dom_elem_set_attr_ns(res,uri,qname,value) */
DOM_THUNK(vm_builtin_dom_elem_set_attr_ns)
{
	phl_domnode *pNd = nArg > 3 ? DomNodeArg(apArg[0]) : 0;
	const char *zUri = nArg > 3 ? ph7_value_to_string(apArg[1],0) : "";
	const char *zQname = nArg > 3 ? ph7_value_to_string(apArg[2],0) : "";
	const char *zVal = nArg > 3 ? ph7_value_to_string(apArg[3],0) : "";
	sxu32 nColon = 0;
	xmlNodePtr pNode;
	xmlNsPtr pNs;
	if( pNd == 0 || zQname[0] == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pNode = (xmlNodePtr)pNd->pNode;
	if( SyByteFind(zQname,SyStrlen(zQname),':',&nColon) == SXRET_OK ){
		/* Prefixed: find (or declare on this element) the namespace */
		char zPrefix[128];
		if( nColon >= sizeof(zPrefix) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		SyMemcpy(zQname,zPrefix,nColon);
		zPrefix[nColon] = 0;
		pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);
		if( pNs == 0 ){
			pNs = xmlNewNs(pNode,(const xmlChar *)zUri,(const xmlChar *)zPrefix);
		}
		if( pNs == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		xmlSetNsProp(pNode,pNs,(const xmlChar *)(zQname+nColon+1),(const xmlChar *)zVal);
	}else{
		pNs = zUri[0] ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;
		xmlSetNsProp(pNode,pNs,(const xmlChar *)zQname,(const xmlChar *)zVal);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* ?res __dom_elem_attr_node(res,name) -- the attribute NODE by name */
DOM_THUNK(vm_builtin_dom_elem_attr_node)
{
	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlAttrPtr pAttr = pNd ? xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;
	if( pAttr == 0 || pAttr->type != XML_ATTRIBUTE_NODE ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultNode(pCtx,pNd->pShell,(xmlNodePtr)pAttr);
}
/* int __dom_elem_attr_count(res) / ?res __dom_elem_attr_at(res,i) */
DOM_THUNK(vm_builtin_dom_elem_attr_count)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlAttrPtr pAttr = (pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE)
		? ((xmlNodePtr)pNd->pNode)->properties : 0;
	int iCount = 0;
	for( ; pAttr ; pAttr = pAttr->next ){
		iCount++;
	}
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
DOM_THUNK(vm_builtin_dom_elem_attr_at)
{
	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	int iWant = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	xmlAttrPtr pAttr = (pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE)
		? ((xmlNodePtr)pNd->pNode)->properties : 0;
	for( ; pAttr && iWant > 0 ; pAttr = pAttr->next ){
		iWant--;
	}
	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,(xmlNodePtr)pAttr);
}

/* ===== getElementsByTagName (live) ===== */

/* Document-order successor within pRoot's subtree (pRoot excluded) */
static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot)
{
	if( pCur->children ){
		return pCur->children;
	}
	while( pCur && pCur != pRoot ){
		if( pCur->next ){
			return pCur->next;
		}
		pCur = pCur->parent;
	}
	return 0;
}
static int DomGebtnMatch(xmlNodePtr pNode,const char *zName)
{
	if( pNode->type != XML_ELEMENT_NODE ){
		return 0;
	}
	if( zName[0] == '*' && zName[1] == 0 ){
		return 1;
	}
	return xmlStrEqual(pNode->name,(const xmlChar *)zName) != 0;
}
/* int __dom_gebtn_count(res,name) / ?res __dom_gebtn_at(res,name,i) */
DOM_THUNK(vm_builtin_dom_gebtn_count)
{
	phl_domnode *pNd = nArg > 1 ? DomNodeArg(apArg[0]) : 0;
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlNodePtr pRoot = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlNodePtr pCur = pRoot ? pRoot->children : 0;
	int iCount = 0;
	while( pCur ){
		if( DomGebtnMatch(pCur,zName) ){
			iCount++;
		}
		pCur = DomWalkNext(pCur,pRoot);
	}
	ph7_result_int(pCtx,iCount);
	return PH7_OK;
}
DOM_THUNK(vm_builtin_dom_gebtn_at)
{
	phl_domnode *pNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;
	const char *zName = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";
	int iWant = nArg > 2 ? ph7_value_to_int(apArg[2]) : 0;
	xmlNodePtr pRoot = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlNodePtr pCur = pRoot ? pRoot->children : 0;
	while( pCur ){
		if( DomGebtnMatch(pCur,zName) ){
			if( iWant == 0 ){
				return DomResultNode(pCtx,pNd->pShell,pCur);
			}
			iWant--;
		}
		pCur = DomWalkNext(pCur,pRoot);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}

/* ===== Document thunks ===== */

/* res __dom_doc_new(version,encoding) */
DOM_THUNK(vm_builtin_dom_doc_new)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";
	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlDocPtr pDoc;
	phl_xmldoc *pShell;
	pDoc = xmlNewDoc((const xmlChar *)(zVersion[0] ? zVersion : "1.0"));
	if( pDoc == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( zEncoding[0] ){
		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);
	}
	pShell = PH7_LibxmlNewDoc(pVm,pDoc);
	if( pShell == 0 ){
		xmlFreeDoc(pDoc);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultNode(pCtx,pShell,(xmlNodePtr)pDoc);
}
/* res|false __dom_doc_loadxml(source,preserveWS,options) -- a NEW doc resource */
DOM_THUNK(vm_builtin_dom_doc_loadxml)
{
	ph7_vm *pVm = pCtx->pVm;
	int nLen = 0;
	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";
	int bPreserve = nArg > 1 ? ph7_value_to_bool(apArg[1]) : 1;
	int iOpts = nArg > 2 ? ph7_value_to_int(apArg[2]) : 0;
	xmlDocPtr pDoc;
	phl_xmldoc *pShell;
	sxu32 nMark;
	if( !bPreserve ){
		iOpts |= XML_PARSE_NOBLANKS;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::loadXML");
	if( pDoc == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pShell = PH7_LibxmlNewDoc(pVm,pDoc);
	if( pShell == 0 ){
		xmlFreeDoc(pDoc);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return DomResultNode(pCtx,pShell,(xmlNodePtr)pDoc);
}
/* ?res __dom_doc_root(docres) -- documentElement */
DOM_THUNK(vm_builtin_dom_doc_root)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pRoot = pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0;
	return DomResultNode(pCtx,pNd ? pNd->pShell : 0,pRoot);
}
/* string|false __dom_doc_savexml(docres,?noderes,format) */
DOM_THUNK(vm_builtin_dom_doc_savexml)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = nArg > 2 ? DomNodeArg(apArg[0]) : 0;
	phl_domnode *pTgt = (nArg > 2 && !ph7_value_is_null(apArg[1])) ? DomNodeArg(apArg[1]) : 0;
	int bFormat = nArg > 2 ? ph7_value_to_bool(apArg[2]) : 0;
	xmlDocPtr pDoc;
	sxu32 nMark;
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDoc = (xmlDocPtr)pDocNd->pNode;
	nMark = PH7_LibxmlCaptureBegin(pVm);
	if( pTgt == 0 || pTgt->pNode == pDocNd->pNode ){
		xmlChar *zOut = 0;
		int nOut = 0;
		xmlDocDumpFormatMemory(pDoc,&zOut,&nOut,bFormat ? 1 : 0);
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");
		if( zOut == 0 ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_string(pCtx,(const char *)zOut,nOut);
		xmlFree(zOut);
	}else{
		xmlBufferPtr pBuf = xmlBufferCreate();
		int rc;
		if( pBuf == 0 ){
			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		rc = xmlNodeDump(pBuf,pDoc,(xmlNodePtr)pTgt->pNode,0,bFormat ? 1 : 0);
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");
		if( rc < 0 ){
			xmlBufferFree(pBuf);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		ph7_result_string(pCtx,(const char *)xmlBufferContent(pBuf),(int)xmlBufferLength(pBuf));
		xmlBufferFree(pBuf);
	}
	return PH7_OK;
}
/* res|false __dom_doc_create(docres,kind,name,value) -- kind: 1 element,
 * 3 text, 4 cdata, 8 comment.  Fresh nodes start as orphans. */
DOM_THUNK(vm_builtin_dom_doc_create)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = nArg > 3 ? DomNodeArg(apArg[0]) : 0;
	int iKind = nArg > 3 ? ph7_value_to_int(apArg[1]) : 0;
	const char *zName = nArg > 3 ? ph7_value_to_string(apArg[2],0) : "";
	int nVal = 0;
	const char *zVal = nArg > 3 ? ph7_value_to_string(apArg[3],&nVal) : "";
	xmlDocPtr pDoc;
	xmlNodePtr pNode = 0;
	sxu32 nMark;
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDoc = (xmlDocPtr)pDocNd->pNode;
	nMark = PH7_LibxmlCaptureBegin(pVm);
	switch( iKind ){
	case XML_ELEMENT_NODE:
		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){
			break; /* Invalid Character Error */
		}
		/* php passes the value through xmlNewDocNode, which entity-parses
		 * it (quirk preserved: bad entities warn and drop the content). */
		pNode = xmlNewDocNode(pDoc,0,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);
		break;
	case XML_TEXT_NODE:
		pNode = xmlNewDocText(pDoc,(const xmlChar *)zVal);
		break;
	case XML_CDATA_SECTION_NODE:
		pNode = xmlNewCDataBlock(pDoc,(const xmlChar *)zVal,nVal);
		break;
	case XML_COMMENT_NODE:
		pNode = xmlNewDocComment(pDoc,(const xmlChar *)zVal);
		break;
	}
	PH7_LibxmlCaptureEnd(pVm,nMark,iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");
	if( pNode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	DomOrphanAdd(pDocNd->pShell,pNode);
	return DomResultNode(pCtx,pDocNd->pShell,pNode);
}
/* void __dom_doc_normalize(docres) -- merge adjacent text nodes.  Merged-
 * away siblings are PARKED as orphans, never freed, so any PHP wrapper to
 * them stays valid (they just become empty orphans). */
static void DomNormalizeTree(phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNodePtr pChild = pNode->children;
	while( pChild ){
		if( pChild->type == XML_TEXT_NODE ){
			while( pChild->next && pChild->next->type == XML_TEXT_NODE ){
				xmlNodePtr pNext = pChild->next;
				if( pNext->content ){
					xmlNodeAddContent(pChild,pNext->content);
				}
				xmlUnlinkNode(pNext);
				DomOrphanAdd(pShell,pNext);
			}
		}else if( pChild->type == XML_ELEMENT_NODE ){
			DomNormalizeTree(pShell,pChild);
		}
		pChild = pChild->next;
	}
}
DOM_THUNK(vm_builtin_dom_doc_normalize)
{
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	if( pNd ){
		DomNormalizeTree(pNd->pShell,(xmlNodePtr)pNd->pNode);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}

/* ===== C14N ===== */

/* Visibility callback: keep only the target's subtree (attrs/ns follow
 * their owning element) -- the same shape php's ext/dom uses. */
static int DomC14NIsVisible(void *pUserData,xmlNodePtr pNode,xmlNodePtr pParent)
{
	xmlNodePtr pTarget = (xmlNodePtr)pUserData;
	xmlNodePtr p;
	if( pNode->type == XML_NAMESPACE_DECL ){
		p = pParent;
	}else if( pNode->type == XML_ATTRIBUTE_NODE ){
		p = pNode->parent;
	}else{
		p = pNode;
	}
	while( p ){
		if( p == pTarget ){
			return 1;
		}
		p = p->parent;
	}
	return 0;
}
/* string __dom_node_c14n(res) -- "" on canonicalization failure (php
 * returns an empty string for empty/unserializable input) */
DOM_THUNK(vm_builtin_dom_node_c14n)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pNd = nArg > 0 ? DomNodeArg(apArg[0]) : 0;
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	sxu32 nMark;
	if( pNode == 0 || pNode->doc == 0 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	if( pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE ){
		xmlChar *zOut = 0;
		int nOut = xmlC14NDocDumpMemory((xmlDocPtr)pNode,0,XML_C14N_1_0,0,0,&zOut);
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::C14N");
		if( nOut < 0 || zOut == 0 ){
			ph7_result_string(pCtx,"",0);
		}else{
			ph7_result_string(pCtx,(const char *)zOut,nOut);
		}
		if( zOut ){
			xmlFree(zOut);
		}
	}else{
		xmlOutputBufferPtr pOut = xmlAllocOutputBuffer(0);
		int rc = -1;
		if( pOut ){
			rc = xmlC14NExecute(pNode->doc,DomC14NIsVisible,pNode,XML_C14N_1_0,0,0,pOut);
		}
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::C14N");
		if( pOut == 0 || rc < 0 ){
			ph7_result_string(pCtx,"",0);
		}else{
			ph7_result_string(pCtx,(const char *)xmlOutputBufferGetContent(pOut),
				(int)xmlOutputBufferGetSize(pOut));
		}
		if( pOut ){
			xmlOutputBufferClose(pOut);
		}
	}
	return PH7_OK;
}

/* ===== The DOM class prelude ===== */

static const char zDomLib1[] =
	"class DOMException extends Exception {}"
	"function __phl_dom_wrap($doc,$res)"
	"{"
	"  if( $res === null || $res === false ){ return null; }"
	"  $id = __dom_node_id($res);"
	"  if( isset($doc->__nodes[$id]) ){ return $doc->__nodes[$id]; }"
	"  switch( __dom_node_kind($res) ){"
	"    case XML_ELEMENT_NODE:       $o = new DOMElement($res,$doc); break;"
	"    case XML_ATTRIBUTE_NODE:     $o = new DOMAttr($res,$doc); break;"
	"    case XML_TEXT_NODE:          $o = new DOMText($res,$doc); break;"
	"    case XML_CDATA_SECTION_NODE: $o = new DOMCdataSection($res,$doc); break;"
	"    case XML_COMMENT_NODE:       $o = new DOMComment($res,$doc); break;"
	"    case XML_DOCUMENT_NODE:"
	"    case XML_HTML_DOCUMENT_NODE: return $doc;"
	"    default:                     $o = new DOMNode($res,$doc); break;"
	"  }"
	"  $doc->__nodes[$id] = $o;"
	"  return $o;"
	"}"
	"class DOMNode"
	"{"
	"  public $__res;"
	"  public $__doc;"
	"  function __construct($res = null,$doc = null)"
	"  {"
	"    $this->__res = $res;"
	"    $this->__doc = ($doc === null) ? $this : $doc;"
	"  }"
	"  function appendChild($node)"
	"  {"
	"    if( !__dom_node_append($this->__res,$node->__res) ){"
	"      throw new DOMException('Wrong Document Error');"
	"    }"
	"    return $node;"
	"  }"
	"  function insertBefore($node,$child = null)"
	"  {"
	"    if( !__dom_node_insert_before($this->__res,$node->__res,$child === null ? null : $child->__res) ){"
	"      throw new DOMException('Not Found Error');"
	"    }"
	"    return $node;"
	"  }"
	"  function removeChild($child)"
	"  {"
	"    if( !__dom_node_remove($this->__res,$child->__res) ){"
	"      throw new DOMException('Not Found Error');"
	"    }"
	"    return $child;"
	"  }"
	"  function replaceChild($node,$child)"
	"  {"
	"    if( !__dom_node_replace($this->__res,$node->__res,$child->__res) ){"
	"      throw new DOMException('Not Found Error');"
	"    }"
	"    return $child;"
	"  }"
	"  function hasChildNodes(){ return __dom_node_child_count($this->__res) > 0; }"
	"  function hasAttributes(){ return __dom_elem_attr_count($this->__res) > 0; }"
	"  function isSameNode($otherNode){ return __dom_node_id($this->__res) === __dom_node_id($otherNode->__res); }"
	"  function getLineNo(){ return __dom_node_line_no($this->__res); }"
	"  function C14N($exclusive = false,$withComments = false,$xpath = null,$nsPrefixes = null)"
	"  {"
	"    return __dom_node_c14n($this->__res);"
	"  }"
	"  function getElementsByTagName($qualifiedName)"
	"  {"
	"    return new DOMNodeList('gebtn',$this->__doc,$this->__res,(string)$qualifiedName);"
	"  }"
	"  protected function __nodeProp($name)"
	"  {"
	"    switch( $name ){"
	"      case 'nodeName':     return __dom_node_name($this->__res);"
	"      case 'nodeValue':    return __dom_node_value($this->__res);"
	"      case 'nodeType':     return __dom_node_kind($this->__res);"
	"      case 'parentNode':   return __phl_dom_wrap($this->__doc,__dom_node_parent($this->__res));"
	"      case 'firstChild':   return __phl_dom_wrap($this->__doc,__dom_node_first($this->__res));"
	"      case 'lastChild':    return __phl_dom_wrap($this->__doc,__dom_node_last($this->__res));"
	"      case 'nextSibling':  return __phl_dom_wrap($this->__doc,__dom_node_next($this->__res));"
	"      case 'previousSibling': return __phl_dom_wrap($this->__doc,__dom_node_prev($this->__res));"
	"      case 'ownerDocument': return ($this instanceof DOMDocument) ? null : $this->__doc;"
	"      case 'childNodes':   return new DOMNodeList('child',$this->__doc,$this->__res);"
	"      case 'textContent':  return __dom_node_text_content($this->__res);"
	"      case 'attributes':"
	"        return (__dom_node_kind($this->__res) === XML_ELEMENT_NODE)"
	"          ? new DOMNamedNodeMap($this->__doc,$this->__res) : null;"
	"      case 'childElementCount': return __dom_node_elem_child_count($this->__res);"
	"    }"
	"    return null;"
	"  }"
	"  function __get($name){ return $this->__nodeProp($name); }"
	"}";

static const char zDomLib2[] =
	"class DOMDocument extends DOMNode"
	"{"
	"  public $preserveWhiteSpace = true;"
	"  public $formatOutput = false;"
	"  public $__nodes = array();"
	"  function __construct($version = '1.0',$encoding = '')"
	"  {"
	"    parent::__construct(__dom_doc_new((string)$version,(string)$encoding),null);"
	"  }"
	"  function loadXML($source,$options = 0)"
	"  {"
	"    $source = (string)$source;"
	"    if( $source === '' ){"
	"      throw new ValueError('DOMDocument::loadXML(): Argument #1 ($source) must not be empty');"
	"    }"
	"    $r = __dom_doc_loadxml($source,(bool)$this->preserveWhiteSpace,(int)$options);"
	"    if( $r === false ){ return false; }"
	"    $this->__res = $r;"
	"    $this->__nodes = array();"
	"    return true;"
	"  }"
	"  function saveXML($node = null)"
	"  {"
	"    return __dom_doc_savexml($this->__res,$node === null ? null : $node->__res,(bool)$this->formatOutput);"
	"  }"
	"  function createElement($localName,$value = '')"
	"  {"
	"    $r = __dom_doc_create($this->__res,XML_ELEMENT_NODE,(string)$localName,(string)$value);"
	"    if( $r === false ){ throw new DOMException('Invalid Character Error'); }"
	"    return __phl_dom_wrap($this,$r);"
	"  }"
	"  function createTextNode($data)"
	"  {"
	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_TEXT_NODE,'',(string)$data));"
	"  }"
	"  function createComment($data)"
	"  {"
	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_COMMENT_NODE,'',(string)$data));"
	"  }"
	"  function createCDATASection($data)"
	"  {"
	"    return __phl_dom_wrap($this,__dom_doc_create($this->__res,XML_CDATA_SECTION_NODE,'',(string)$data));"
	"  }"
	"  function normalizeDocument(){ __dom_doc_normalize($this->__res); }"
	"  function __get($name)"
	"  {"
	"    if( $name === 'documentElement' ){"
	"      return __phl_dom_wrap($this,__dom_doc_root($this->__res));"
	"    }"
	"    return $this->__nodeProp($name);"
	"  }"
	"}"
	"class DOMElement extends DOMNode"
	"{"
	"  function getAttribute($qualifiedName){ return __dom_elem_get_attr($this->__res,(string)$qualifiedName); }"
	"  function hasAttribute($qualifiedName){ return __dom_elem_has_attr($this->__res,(string)$qualifiedName); }"
	"  function setAttribute($qualifiedName,$value)"
	"  {"
	"    if( !__dom_elem_set_attr($this->__res,(string)$qualifiedName,(string)$value) ){"
	"      throw new DOMException('Invalid Character Error');"
	"    }"
	"    return __phl_dom_wrap($this->__doc,__dom_elem_attr_node($this->__res,(string)$qualifiedName));"
	"  }"
	"  function removeAttribute($qualifiedName){ return __dom_elem_remove_attr($this->__res,(string)$qualifiedName); }"
	"  function getAttributeNS($namespace,$localName)"
	"  {"
	"    return __dom_elem_get_attr_ns($this->__res,(string)$namespace,(string)$localName);"
	"  }"
	"  function setAttributeNS($namespace,$qualifiedName,$value)"
	"  {"
	"    if( !__dom_elem_set_attr_ns($this->__res,(string)$namespace,(string)$qualifiedName,(string)$value) ){"
	"      throw new DOMException('Namespace Error');"
	"    }"
	"  }"
	"  function __get($name)"
	"  {"
	"    if( $name === 'tagName' ){ return __dom_node_name($this->__res); }"
	"    return $this->__nodeProp($name);"
	"  }"
	"}"
	"class DOMAttr extends DOMNode"
	"{"
	"  function __get($name)"
	"  {"
	"    if( $name === 'name' ){ return __dom_node_name($this->__res); }"
	"    if( $name === 'value' ){ return __dom_node_value($this->__res); }"
	"    if( $name === 'ownerElement' ){ return __phl_dom_wrap($this->__doc,__dom_node_parent($this->__res)); }"
	"    return $this->__nodeProp($name);"
	"  }"
	"}"
	"class DOMCharacterData extends DOMNode"
	"{"
	"  function __get($name)"
	"  {"
	"    if( $name === 'data' ){ return __dom_node_value($this->__res); }"
	"    if( $name === 'length' ){ return strlen(__dom_node_value($this->__res)); }"
	"    return $this->__nodeProp($name);"
	"  }"
	"}"
	"class DOMText extends DOMCharacterData"
	"{"
	"  function __get($name)"
	"  {"
	"    if( $name === 'wholeText' ){ return __dom_node_value($this->__res); }"
	"    return parent::__get($name);"
	"  }"
	"}"
	"class DOMComment extends DOMCharacterData {}"
	"class DOMCdataSection extends DOMText {}";

static const char zDomLib3[] =
	"class DOMNodeList implements Iterator, Countable"
	"{"
	"  public $__kind;"
	"  public $__doc;"
	"  public $__owner;"
	"  public $__name;"
	"  public $__snap;"
	"  private $__pos = 0;"
	"  function __construct($kind = null,$doc = null,$owner = null,$name = null,$snap = null)"
	"  {"
	"    $this->__kind = $kind; $this->__doc = $doc; $this->__owner = $owner;"
	"    $this->__name = $name; $this->__snap = $snap;"
	"  }"
	"  function count()"
	"  {"
	"    if( $this->__kind === 'snap' ){ return count($this->__snap); }"
	"    if( $this->__kind === 'child' ){ return __dom_node_child_count($this->__owner); }"
	"    return __dom_gebtn_count($this->__owner,$this->__name);"
	"  }"
	"  function item($index)"
	"  {"
	"    $index = (int)$index;"
	"    if( $index < 0 ){ return null; }"
	"    if( $this->__kind === 'snap' ){"
	"      return isset($this->__snap[$index]) ? __phl_dom_wrap($this->__doc,$this->__snap[$index]) : null;"
	"    }"
	"    if( $this->__kind === 'child' ){"
	"      return __phl_dom_wrap($this->__doc,__dom_node_child_at($this->__owner,$index));"
	"    }"
	"    return __phl_dom_wrap($this->__doc,__dom_gebtn_at($this->__owner,$this->__name,$index));"
	"  }"
	"  function rewind(){ $this->__pos = 0; }"
	"  function valid(){ return $this->__pos < $this->count(); }"
	"  function current(){ return $this->item($this->__pos); }"
	"  function key(){ return $this->__pos; }"
	"  function next(){ $this->__pos++; }"
	"  function __get($name)"
	"  {"
	"    if( $name === 'length' ){ return $this->count(); }"
	"    return null;"
	"  }"
	"}"
	"class DOMNamedNodeMap implements Countable"
	"{"
	"  public $__doc;"
	"  public $__owner;"
	"  function __construct($doc = null,$owner = null){ $this->__doc = $doc; $this->__owner = $owner; }"
	"  function count(){ return __dom_elem_attr_count($this->__owner); }"
	"  function item($index)"
	"  {"
	"    return __phl_dom_wrap($this->__doc,__dom_elem_attr_at($this->__owner,(int)$index));"
	"  }"
	"  function getNamedItem($qualifiedName)"
	"  {"
	"    $n = $this->count();"
	"    for( $i = 0; $i < $n; $i++ ){"
	"      $a = $this->item($i);"
	"      if( $a !== null && $a->name === $qualifiedName ){ return $a; }"
	"    }"
	"    return null;"
	"  }"
	"  function __get($name)"
	"  {"
	"    if( $name === 'length' ){ return $this->count(); }"
	"    return null;"
	"  }"
	"}";

/*
 * Install the DOM library: __dom_* thunks first, then the class chunks.
 * Called from PH7_VmInit inside the bCompilingBuiltin window, after
 * PH7_VmInstallLibxml (the capture plumbing must exist).
 */
PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "__dom_node_id",            vm_builtin_dom_node_id            },
		{ "__dom_node_kind",          vm_builtin_dom_node_kind          },
		{ "__dom_node_name",          vm_builtin_dom_node_name          },
		{ "__dom_node_value",         vm_builtin_dom_node_value         },
		{ "__dom_node_text_content",  vm_builtin_dom_node_text_content  },
		{ "__dom_node_line_no",       vm_builtin_dom_node_line_no       },
		{ "__dom_node_parent",        vm_builtin_dom_node_parent        },
		{ "__dom_node_first",         vm_builtin_dom_node_first         },
		{ "__dom_node_last",          vm_builtin_dom_node_last          },
		{ "__dom_node_next",          vm_builtin_dom_node_next          },
		{ "__dom_node_prev",          vm_builtin_dom_node_prev          },
		{ "__dom_node_child_count",   vm_builtin_dom_node_child_count   },
		{ "__dom_node_child_at",      vm_builtin_dom_node_child_at      },
		{ "__dom_node_elem_child_count", vm_builtin_dom_node_elem_child_count },
		{ "__dom_node_append",        vm_builtin_dom_node_append        },
		{ "__dom_node_insert_before", vm_builtin_dom_node_insert_before },
		{ "__dom_node_remove",        vm_builtin_dom_node_remove        },
		{ "__dom_node_replace",       vm_builtin_dom_node_replace       },
		{ "__dom_node_c14n",          vm_builtin_dom_node_c14n          },
		{ "__dom_elem_get_attr",      vm_builtin_dom_elem_get_attr      },
		{ "__dom_elem_has_attr",      vm_builtin_dom_elem_has_attr      },
		{ "__dom_elem_set_attr",      vm_builtin_dom_elem_set_attr      },
		{ "__dom_elem_remove_attr",   vm_builtin_dom_elem_remove_attr   },
		{ "__dom_elem_get_attr_ns",   vm_builtin_dom_elem_get_attr_ns   },
		{ "__dom_elem_set_attr_ns",   vm_builtin_dom_elem_set_attr_ns   },
		{ "__dom_elem_attr_node",     vm_builtin_dom_elem_attr_node     },
		{ "__dom_elem_attr_count",    vm_builtin_dom_elem_attr_count    },
		{ "__dom_elem_attr_at",       vm_builtin_dom_elem_attr_at       },
		{ "__dom_gebtn_count",        vm_builtin_dom_gebtn_count        },
		{ "__dom_gebtn_at",           vm_builtin_dom_gebtn_at           },
		{ "__dom_doc_new",            vm_builtin_dom_doc_new            },
		{ "__dom_doc_loadxml",        vm_builtin_dom_doc_loadxml        },
		{ "__dom_doc_root",           vm_builtin_dom_doc_root           },
		{ "__dom_doc_savexml",        vm_builtin_dom_doc_savexml        },
		{ "__dom_doc_create",         vm_builtin_dom_doc_create         },
		{ "__dom_doc_normalize",      vm_builtin_dom_doc_normalize      },
	};
	sxu32 n;
	sxi32 rc;
	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){
		ph7_create_function(&(*pVm),aFunc[n].zName,aFunc[n].xFunc,0);
	}
	rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib1,sizeof(zDomLib1)-1);
	if( rc == SXRET_OK ){
		rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib2,sizeof(zDomLib2)-1);
	}
	if( rc == SXRET_OK ){
		rc = PH7_VmEvalBuiltinChunk(&(*pVm),zDomLib3,sizeof(zDomLib3)-1);
	}
	return rc;
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_dom_unused;
#endif /* PH7_ENABLE_LIBXML */
