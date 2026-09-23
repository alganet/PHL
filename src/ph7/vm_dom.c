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
#include <libxml/xpath.h>
#include <libxml/xmlschemas.h>

/*
 * ext/dom on libxml2: the DOM classes, declared and bodied in C.
 *
 * Architecture (see also vm_libxml.c): DOMNode and its subclasses are native
 * classes (oo_native.c) whose methods ARE the C below.  Every instance holds
 * two slots -- $__res, a phl_domnode resource {phl_xmldoc*, xmlNodePtr}, and
 * $__doc, the owning DOMDocument wrapper.  There is no PHP layer left in the
 * node tree: what used to be a prelude class over ~30 global __dom_* thunks is
 * one C body per method, so the thunks stopped being globally visible names.
 *
 * Node identity: php guarantees $doc->documentElement === $doc->
 * documentElement.  Every wrap goes through DomWrap(), which keys a
 * per-document cache ($doc->__nodes) by the node POINTER, so the same
 * underlying node always yields the same object.  The cache owns the
 * wrappers, which is why DomWrap hands back a BORROWED instance: it stays
 * alive as long as its document does.  (The old shape allocated a fresh
 * phl_domnode on every navigation step even when the cache then threw the
 * result away; only a genuine cache MISS allocates one now.)
 *
 * Tree surgery (append/insert/replace/remove) is done with manual pointer
 * splicing instead of xmlAddChild: xmlAddChild MERGES adjacent text nodes
 * and frees the merged-away node, which would dangle any PHP wrapper (and
 * violates DOM semantics, which php follows -- appendChild never merges).
 * Unlinked nodes are parked on the owning phl_xmldoc's orphan set so they
 * are freed with the document at VM reset/release.
 */

/* One native method body. Its receiver's node is DomThisNode(pCtx); apArg is
 * php's own argument list, already screened against the declared signature. */
#define DOM_METHOD(NAME) static int NAME(ph7_context *pCtx,int nArg,ph7_value **apArg)

/* The two slots every wrapper carries, and the document's identity cache. */
#define DOM_RES   "__res"
#define DOM_DOC   "__doc"
#define DOM_NODES "__nodes"

/* Property names are byte-exact in php, and every name that reaches here is
 * NUL-terminated (ph7_value_to_string null-appends). */
static int DomNameIs(const char *zName,const char *zWant)
{
	sxu32 n = (sxu32)SyStrlen(zWant);
	return SyStrlen(zName) == n && SyStrncmp(zName,zWant,n) == 0;
}
/* The handle behind an instance's $__res, or NULL for anything else. */
static phl_domnode * DomResOf(ph7_class_instance *pObj)
{
	ph7_value *pVal = pObj ? PH7_NativeAttr(pObj,DOM_RES) : 0;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (phl_domnode *)pVal->x.pOther;
}
/* The receiver of a native method, and the two things every body wants from it. */
static phl_domnode * DomThisNode(ph7_context *pCtx)
{
	return DomResOf(PH7_ContextThis(pCtx));
}
static ph7_class_instance * DomThisDoc(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	return pThis ? PH7_NativeAttrObj(pThis,DOM_DOC) : 0;
}
/* A fresh handle onto one node of pShell's tree. Freed with the VM allocator. */
static phl_domnode * DomNewRes(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode)
{
	phl_domnode *pWrap = (phl_domnode *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(phl_domnode));
	if( pWrap ){
		pWrap->pShell = pShell;
		pWrap->pNode = pNode;
	}
	return pWrap;
}
/* Store a handle in an instance's $__res slot. */
static void DomSetRes(ph7_vm *pVm,ph7_class_instance *pObj,phl_domnode *pRes)
{
	ph7_value sVal;
	PH7_MemObjInit(&(*pVm),&sVal);
	sVal.x.pOther = pRes;
	sVal.iFlags = MEMOBJ_RES;
	PH7_NativeSetProp(&(*pVm),pObj,DOM_RES,sizeof(DOM_RES)-1,&sVal);
}
/*
 * The document's identity cache, materialized and separated from any copy that
 * shares it. Same three moves a native class always needs to own an array slot
 * (WeakMap's WmStore is the other one).
 */
static ph7_hashmap * DomCache(ph7_vm *pVm,ph7_class_instance *pDoc)
{
	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NODES) : 0;
	if( pSlot == 0 ){
		return 0;
	}
	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){
		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){
			return 0;
		}
	}
	return PH7_HashmapCowSeparate(&(*pVm),pSlot);
}
/* php's class for a node type. Anything else is a plain DOMNode, as before. */
static const char * DomClassOfKind(int iKind)
{
	switch( iKind ){
	case XML_ELEMENT_NODE:       return "DOMElement";
	case XML_ATTRIBUTE_NODE:     return "DOMAttr";
	case XML_TEXT_NODE:          return "DOMText";
	case XML_CDATA_SECTION_NODE: return "DOMCdataSection";
	case XML_COMMENT_NODE:       return "DOMComment";
	default:                     return "DOMNode";
	}
}
/*
 * The wrapper object for one node of pDoc's tree -- the same one every time,
 * which is what makes `$doc->documentElement === $doc->documentElement` true.
 *
 * BORROWED: the cache owns the returned instance. A caller that hands it to PHP
 * goes through DomResultWrap (ph7_result_value takes its own reference); a
 * caller that stores it uses PH7_NativeSetAttrObj, which does the same. Neither
 * unrefs.
 */
static ph7_class_instance * DomWrap(ph7_vm *pVm,ph7_class_instance *pDoc,
	phl_xmldoc *pShell,xmlNodePtr pNode)
{
	ph7_hashmap *pCache;
	ph7_hashmap_node *pEntry = 0;
	ph7_class_instance *pObj;
	ph7_class *pClass;
	phl_domnode *pRes;
	const char *zClass;
	ph7_value sKey,sVal;
	if( pNode == 0 || pDoc == 0 ){
		return 0;
	}
	if( pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE ){
		/* The document is its own wrapper: php answers the SAME DOMDocument. */
		return pDoc;
	}
	pCache = DomCache(&(*pVm),pDoc);
	if( pCache == 0 ){
		return 0;
	}
	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);
	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){
		ph7_value *pHit = HashmapExtractNodeValue(pEntry);
		if( pHit && (pHit->iFlags & MEMOBJ_OBJ) ){
			PH7_MemObjRelease(&sKey);
			return (ph7_class_instance *)pHit->x.pOther;
		}
	}
	zClass = DomClassOfKind((int)pNode->type);
	pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;
	pRes = pObj ? DomNewRes(&(*pVm),pShell,pNode) : 0;
	if( pRes == 0 ){
		if( pObj ){
			PH7_ClassInstanceUnref(pObj);
		}
		PH7_MemObjRelease(&sKey);
		return 0;
	}
	DomSetRes(&(*pVm),pObj,pRes);
	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);
	PH7_MemObjInit(&(*pVm),&sVal);
	sVal.x.pOther = pObj;
	sVal.iFlags = MEMOBJ_OBJ;
	PH7_HashmapInsert(pCache,&sKey,&sVal);   /* takes the cache's reference */
	PH7_MemObjRelease(&sKey);
	PH7_ClassInstanceUnref(pObj);            /* ...and the cache is now the owner */
	return pObj;
}
/* Answer a borrowed instance (or NULL) from a native method. */
static int DomResultWrap(ph7_context *pCtx,ph7_class_instance *pObj)
{
	ph7_value sRes;
	if( pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm,&sRes);
	sRes.x.pOther = pObj;
	sRes.iFlags = MEMOBJ_OBJ;
	ph7_result_value(pCtx,&sRes);   /* takes its own reference */
	return PH7_OK;
}
/* The common tail: wrap a node of the RECEIVER's document and answer it. */
static int DomResultNodeOf(ph7_context *pCtx,phl_domnode *pNd,xmlNodePtr pNode)
{
	if( pNd == 0 || pNode == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNode));
}
/* The phl_domnode behind a DOMNode-typed ARGUMENT (already screened by ZPP). */
static phl_domnode * DomObjArg(ph7_value *pVal)
{
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	return DomResOf((ph7_class_instance *)pVal->x.pOther);
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

/* ===== Node introspection: the readers behind __get ===== */

/* php's nodeName rules */
static void DomNodeName(ph7_context *pCtx,xmlNodePtr pNode)
{
	if( pNode == 0 ){
		ph7_result_string(pCtx,"",0);
		return;
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
}
/* php's nodeValue: NULL for a document, the text content otherwise */
static void DomNodeValue(ph7_context *pCtx,xmlNodePtr pNode)
{
	xmlChar *zContent;
	if( pNode == 0 || pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE
		|| pNode->type == XML_DOCUMENT_TYPE_NODE ){
		ph7_result_null(pCtx);
		return;
	}
	zContent = xmlNodeGetContent(pNode);
	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);
	if( zContent ){
		xmlFree(zContent);
	}
}
/* php's textContent: the same walk, but a document answers its text too */
static void DomTextContent(ph7_context *pCtx,xmlNodePtr pNode)
{
	xmlChar *zContent = pNode ? xmlNodeGetContent(pNode) : 0;
	ph7_result_string(pCtx,zContent ? (const char *)zContent : "",-1);
	if( zContent ){
		xmlFree(zContent);
	}
}
/* The two child counts childNodes->length and childElementCount read. */
static int DomChildCount(xmlNodePtr pNode,int bElementsOnly)
{
	xmlNodePtr pChild = pNode ? pNode->children : 0;
	int iCount = 0;
	for( ; pChild ; pChild = pChild->next ){
		if( !bElementsOnly || pChild->type == XML_ELEMENT_NODE ){
			iCount++;
		}
	}
	return iCount;
}
static xmlNodePtr DomChildAt(xmlNodePtr pNode,int iWant)
{
	xmlNodePtr pChild = pNode ? pNode->children : 0;
	for( ; pChild && iWant > 0 ; pChild = pChild->next ){
		iWant--;
	}
	return pChild;
}

/* ===== Tree surgery: DOMNode's four mutators ===== */

/*
 * php's refusal taxonomy for linking pChild under pParent, or NULL when the
 * link is allowed. The chunk collapsed all of it into one message per method,
 * which cost more than a wording: nothing rejected making a node its own
 * DESCENDANT, so `$a->firstChild->appendChild($a)` spliced a CYCLE into the
 * tree and every later walk of it ran away.
 */
static const char * DomLinkRefusal(xmlNodePtr pParent,xmlNodePtr pChild)
{
	xmlNodePtr p;
	if( pParent->doc != pChild->doc ){
		return "Wrong Document Error";
	}
	/* Walking UP from the parent also catches pChild == pParent. */
	for( p = pParent ; p ; p = p->parent ){
		if( p == pChild ){
			return "Hierarchy Request Error";
		}
	}
	return 0;
}
/*
 * DOMNode::appendChild(DOMNode $node): DOMNode
 *
 * Note the argument reaches C already screened -- `$n->appendChild(1)` is a
 * TypeError from the declared `DOMNode $node`, where the chunk read `->__res`
 * off an int and warned.
 */
DOM_METHOD(vm_builtin_DOMNode_appendChild)
{
	phl_domnode *pPar = DomThisNode(pCtx);
	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;
	const char *zErr;
	if( pPar == 0 || pChd == 0 ){
		return PH7_VmThrowException(pCtx,"DOMException","Wrong Document Error");
	}
	zErr = DomLinkRefusal((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);
	if( zErr ){
		return PH7_VmThrowException(pCtx,"DOMException","%s",zErr);
	}
	DomDetach(pChd->pShell,(xmlNodePtr)pChd->pNode);
	DomLinkLast((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);
	ph7_result_value(pCtx,apArg[0]);
	return PH7_OK;
}
/* DOMNode::insertBefore(DOMNode $node, ?DOMNode $child = null): DOMNode --
 * a reference node that is not a child of the receiver is Not Found. */
DOM_METHOD(vm_builtin_DOMNode_insertBefore)
{
	phl_domnode *pPar = DomThisNode(pCtx);
	phl_domnode *pNew = nArg > 0 ? DomObjArg(apArg[0]) : 0;
	phl_domnode *pRef = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;
	xmlNodePtr pParent,pChild,pAnchor;
	const char *zErr;
	if( pPar == 0 || pNew == 0 ){
		return PH7_VmThrowException(pCtx,"DOMException","Not Found Error");
	}
	pParent = (xmlNodePtr)pPar->pNode;
	pChild = (xmlNodePtr)pNew->pNode;
	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;
	zErr = DomLinkRefusal(pParent,pChild);
	if( zErr == 0 && pAnchor && pAnchor->parent != pParent ){
		zErr = "Not Found Error";
	}
	if( zErr ){
		return PH7_VmThrowException(pCtx,"DOMException","%s",zErr);
	}
	DomDetach(pNew->pShell,pChild);
	if( pAnchor ){
		DomLinkBefore(pParent,pChild,pAnchor);
	}else{
		DomLinkLast(pParent,pChild);
	}
	ph7_result_value(pCtx,apArg[0]);
	return PH7_OK;
}
/* DOMNode::removeChild(DOMNode $child): DOMNode */
DOM_METHOD(vm_builtin_DOMNode_removeChild)
{
	phl_domnode *pPar = DomThisNode(pCtx);
	phl_domnode *pChd = nArg > 0 ? DomObjArg(apArg[0]) : 0;
	xmlNodePtr pChild;
	if( pPar == 0 || pChd == 0 ){
		return PH7_VmThrowException(pCtx,"DOMException","Not Found Error");
	}
	pChild = (xmlNodePtr)pChd->pNode;
	if( pChild->parent != (xmlNodePtr)pPar->pNode ){
		return PH7_VmThrowException(pCtx,"DOMException","Not Found Error");
	}
	xmlUnlinkNode(pChild);
	DomOrphanAdd(pChd->pShell,pChild);
	ph7_result_value(pCtx,apArg[0]);
	return PH7_OK;
}
/* DOMNode::replaceChild(DOMNode $node, DOMNode $child): DOMNode -- answers the
 * node it replaced, which is the SECOND argument. */
DOM_METHOD(vm_builtin_DOMNode_replaceChild)
{
	phl_domnode *pPar = DomThisNode(pCtx);
	phl_domnode *pNew = nArg > 1 ? DomObjArg(apArg[0]) : 0;
	phl_domnode *pOld = nArg > 1 ? DomObjArg(apArg[1]) : 0;
	xmlNodePtr pParent,pChild,pVictim;
	const char *zErr;
	if( pPar == 0 || pNew == 0 || pOld == 0 ){
		return PH7_VmThrowException(pCtx,"DOMException","Not Found Error");
	}
	pParent = (xmlNodePtr)pPar->pNode;
	pChild = (xmlNodePtr)pNew->pNode;
	pVictim = (xmlNodePtr)pOld->pNode;
	zErr = DomLinkRefusal(pParent,pChild);
	if( zErr == 0 && pVictim->parent != pParent ){
		zErr = "Not Found Error";
	}
	if( zErr ){
		return PH7_VmThrowException(pCtx,"DOMException","%s",zErr);
	}
	if( pChild != pVictim ){
		DomDetach(pNew->pShell,pChild);
		DomLinkBefore(pParent,pChild,pVictim);
		xmlUnlinkNode(pVictim);
		DomOrphanAdd(pOld->pShell,pVictim);
	}
	ph7_result_value(pCtx,apArg[1]);
	return PH7_OK;
}
/* DOMNode::hasChildNodes(): bool / hasAttributes(): bool / getLineNo(): int */
DOM_METHOD(vm_builtin_DOMNode_hasChildNodes)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,pNd && DomChildCount((xmlNodePtr)pNd->pNode,0) > 0);
	return PH7_OK;
}
/* The attribute list of an element (empty for anything else). */
static xmlAttrPtr DomAttrList(xmlNodePtr pNode)
{
	return (pNode && pNode->type == XML_ELEMENT_NODE) ? pNode->properties : 0;
}
static int DomAttrCount(xmlNodePtr pNode)
{
	xmlAttrPtr pAttr = DomAttrList(pNode);
	int iCount = 0;
	for( ; pAttr ; pAttr = pAttr->next ){
		iCount++;
	}
	return iCount;
}
static xmlAttrPtr DomAttrAt(xmlNodePtr pNode,int iWant)
{
	xmlAttrPtr pAttr = DomAttrList(pNode);
	for( ; pAttr && iWant > 0 ; pAttr = pAttr->next ){
		iWant--;
	}
	return pAttr;
}
DOM_METHOD(vm_builtin_DOMNode_hasAttributes)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,pNd && DomAttrCount((xmlNodePtr)pNd->pNode) > 0);
	return PH7_OK;
}
DOM_METHOD(vm_builtin_DOMNode_getLineNo)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx,pNd ? (ph7_int64)xmlGetLineNo((xmlNodePtr)pNd->pNode) : 0);
	return PH7_OK;
}
/* DOMNode::isSameNode(DOMNode $otherNode): bool -- pointer identity, which is
 * also the identity the wrapper cache keys on. */
DOM_METHOD(vm_builtin_DOMNode_isSameNode)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	phl_domnode *pOther = nArg > 0 ? DomObjArg(apArg[0]) : 0;
	ph7_result_bool(pCtx,pNd != 0 && pOther != 0 && pNd->pNode == pOther->pNode);
	return PH7_OK;
}

/* ===== Element attributes ===== */

/* DOMElement::getAttribute(string $qualifiedName): string -- "" when absent */
DOM_METHOD(vm_builtin_DOMElement_getAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlChar *zVal = pNd ? xmlGetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) : 0;
	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);
	if( zVal ){
		xmlFree(zVal);
	}
	return PH7_OK;
}
/* DOMElement::hasAttribute(string $qualifiedName): bool */
DOM_METHOD(vm_builtin_DOMElement_hasAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	ph7_result_bool(pCtx,pNd && xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName) != 0);
	return PH7_OK;
}
/* DOMElement::setAttribute(string $qualifiedName, string $value): DOMAttr -- php
 * answers the attribute NODE it wrote, so the write is followed by a wrap. */
DOM_METHOD(vm_builtin_DOMElement_setAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";
	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlAttrPtr pAttr;
	if( pNd == 0 || xmlValidateName((const xmlChar *)zName,0) != 0 ){
		return PH7_VmThrowException(pCtx,"DOMException","Invalid Character Error");
	}
	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);
	pAttr = xmlHasProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName);
	if( pAttr == 0 || pAttr->type != XML_ATTRIBUTE_NODE ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);
}
/* DOMElement::removeAttribute(string $qualifiedName): bool */
DOM_METHOD(vm_builtin_DOMElement_removeAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
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
/* DOMElement::getAttributeNS(?string $namespace, string $localName): string */
DOM_METHOD(vm_builtin_DOMElement_getAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zUri = (nArg > 1 && !ph7_value_is_null(apArg[0])) ? ph7_value_to_string(apArg[0],0) : "";
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,(const xmlChar *)zUri) : 0;
	ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);
	if( zVal ){
		xmlFree(zVal);
	}
	return PH7_OK;
}
/* DOMElement::setAttributeNS(?string $namespace, string $qualifiedName, string $value): void */
DOM_METHOD(vm_builtin_DOMElement_setAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zUri = (nArg > 2 && !ph7_value_is_null(apArg[0])) ? ph7_value_to_string(apArg[0],0) : "";
	const char *zQname = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";
	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";
	sxu32 nColon = 0;
	xmlNodePtr pNode;
	xmlNsPtr pNs;
	if( pNd == 0 || zQname[0] == 0 ){
		return PH7_VmThrowException(pCtx,"DOMException","Namespace Error");
	}
	pNode = (xmlNodePtr)pNd->pNode;
	if( SyByteFind(zQname,SyStrlen(zQname),':',&nColon) == SXRET_OK ){
		/* Prefixed: find (or declare on this element) the namespace */
		char zPrefix[128];
		if( nColon >= sizeof(zPrefix) ){
			return PH7_VmThrowException(pCtx,"DOMException","Namespace Error");
		}
		SyMemcpy(zQname,zPrefix,nColon);
		zPrefix[nColon] = 0;
		pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);
		if( pNs == 0 ){
			pNs = xmlNewNs(pNode,(const xmlChar *)zUri,(const xmlChar *)zPrefix);
		}
		if( pNs == 0 ){
			return PH7_VmThrowException(pCtx,"DOMException","Namespace Error");
		}
		xmlSetNsProp(pNode,pNs,(const xmlChar *)(zQname+nColon+1),(const xmlChar *)zVal);
	}else{
		pNs = zUri[0] ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;
		xmlSetNsProp(pNode,pNs,(const xmlChar *)zQname,(const xmlChar *)zVal);
	}
	return PH7_OK;
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
/* Length-carrying: the name comes from a declared string SLOT, whose bytes are
 * NOT NUL-terminated (PH7_NativeAttrStr borrows the blob as-is). */
static int DomGebtnMatch(xmlNodePtr pNode,const char *zName,int nName)
{
	if( pNode->type != XML_ELEMENT_NODE ){
		return 0;
	}
	if( nName == 1 && zName[0] == '*' ){
		return 1;
	}
	if( pNode->name == 0 ){
		return 0;
	}
	return (int)SyStrlen((const char *)pNode->name) == nName
		&& SyMemcmp((const void *)pNode->name,(const void *)zName,(sxu32)nName) == 0;
}
/* The list is LIVE: nothing is snapshotted, both queries re-walk the subtree
 * every time DOMNodeList asks. Passing iWant < 0 counts instead of indexing. */
static xmlNodePtr DomGebtnWalk(xmlNodePtr pRoot,const char *zName,int nName,int iWant,int *pnCount)
{
	xmlNodePtr pCur = pRoot ? pRoot->children : 0;
	int iCount = 0;
	while( pCur ){
		if( DomGebtnMatch(pCur,zName,nName) ){
			if( iWant >= 0 && iCount == iWant ){
				return pCur;
			}
			iCount++;
		}
		pCur = DomWalkNext(pCur,pRoot);
	}
	if( pnCount ){
		*pnCount = iCount;
	}
	return 0;
}

/* ===== DOMDocument ===== */

/*
 * DOMDocument::__construct(string $version = '1.0', string $encoding = '')
 *
 * The chunk reached the document through `parent::__construct(__dom_doc_new(..))`;
 * a native constructor writes its own two slots, and $__doc is the document
 * ITSELF (php's ownerDocument is null on a document, which __get answers).
 */
DOM_METHOD(vm_builtin_DOMDocument_construct)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";
	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlDocPtr pDoc;
	phl_xmldoc *pShell;
	phl_domnode *pRes;
	if( pThis == 0 ){
		return PH7_OK;
	}
	pDoc = xmlNewDoc((const xmlChar *)(zVersion[0] ? zVersion : "1.0"));
	if( pDoc == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( zEncoding[0] ){
		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);
	}
	pShell = PH7_LibxmlNewDoc(pVm,pDoc);
	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;
	if( pRes == 0 ){
		if( pShell == 0 ){
			xmlFreeDoc(pDoc);
		}
		return PH7_ContextMemoryError(pCtx);
	}
	DomSetRes(pVm,pThis,pRes);
	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);
	return PH7_OK;
}
/* DOMDocument::loadXML(string $source, int $options = 0): bool -- the receiver
 * is REPOINTED at a new tree, so its identity cache is dropped with it. */
DOM_METHOD(vm_builtin_DOMDocument_loadXML)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nLen = 0;
	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	xmlDocPtr pDoc;
	phl_xmldoc *pShell;
	phl_domnode *pRes;
	ph7_value *pNodes;
	sxu32 nMark;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( nLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMDocument::loadXML(): Argument #1 ($source) must not be empty");
	}
	if( !PH7_NativeAttrTruthy(pThis,"preserveWhiteSpace") ){
		iOpts |= XML_PARSE_NOBLANKS;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::loadXML");
	pShell = pDoc ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;
	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;
	if( pRes == 0 ){
		if( pDoc && pShell == 0 ){
			xmlFreeDoc(pDoc);
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	DomSetRes(pVm,pThis,pRes);
	pNodes = PH7_NativeAttr(pThis,DOM_NODES);
	if( pNodes ){
		/* Every wrapper into the OLD tree is stale: start a fresh cache. */
		PH7_MemObjRelease(pNodes);
		PH7_MemObjToHashmap(pNodes);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* DOMDocument::saveXML(?DOMNode $node = null, int $options = 0): string|false */
DOM_METHOD(vm_builtin_DOMDocument_saveXML)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pDocNd = DomThisNode(pCtx);
	phl_domnode *pTgt = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;
	int bFormat = pThis && PH7_NativeAttrTruthy(pThis,"formatOutput");
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
/*
 * The four DOMDocument::create* methods, which differ only in the node kind
 * they ask libxml for. Fresh nodes start as orphans, so a node that is created
 * and never appended is still freed with its document.
 */
static int DomDocCreate(ph7_context *pCtx,int iKind,const char *zName,const char *zVal,int nVal)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = DomThisNode(pCtx);
	xmlDocPtr pDoc;
	xmlNodePtr pNode = 0;
	sxu32 nMark;
	if( pDocNd == 0 ){
		return PH7_VmThrowException(pCtx,"DOMException","Invalid Character Error");
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
	PH7_LibxmlCaptureEnd(pVm,nMark,
		iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");
	if( pNode == 0 ){
		if( iKind == XML_ELEMENT_NODE ){
			/* Only createElement can be handed a name libxml refuses. */
			return PH7_VmThrowException(pCtx,"DOMException","Invalid Character Error");
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	DomOrphanAdd(pDocNd->pShell,pNode);
	return DomResultNodeOf(pCtx,pDocNd,pNode);
}
/* DOMDocument::createElement(string $localName, string $value = ''): DOMElement */
DOM_METHOD(vm_builtin_DOMDocument_createElement)
{
	int nVal = 0;
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";
	return DomDocCreate(pCtx,XML_ELEMENT_NODE,zName,zVal,nVal);
}
/* DOMDocument::createTextNode / createComment / createCDATASection(string $data) */
static int DomDocCreateData(ph7_context *pCtx,int iKind,int nArg,ph7_value **apArg)
{
	int nVal = 0;
	const char *zVal = nArg > 0 ? ph7_value_to_string(apArg[0],&nVal) : "";
	return DomDocCreate(pCtx,iKind,"",zVal,nVal);
}
DOM_METHOD(vm_builtin_DOMDocument_createTextNode)
{
	return DomDocCreateData(pCtx,XML_TEXT_NODE,nArg,apArg);
}
DOM_METHOD(vm_builtin_DOMDocument_createComment)
{
	return DomDocCreateData(pCtx,XML_COMMENT_NODE,nArg,apArg);
}
DOM_METHOD(vm_builtin_DOMDocument_createCDATASection)
{
	return DomDocCreateData(pCtx,XML_CDATA_SECTION_NODE,nArg,apArg);
}
/* DOMDocument::normalizeDocument(): void -- merge adjacent text nodes.  Merged-
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
DOM_METHOD(vm_builtin_DOMDocument_normalizeDocument)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pNd ){
		DomNormalizeTree(pNd->pShell,(xmlNodePtr)pNd->pNode);
	}
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
/*
 * DOMNode::C14N(bool $exclusive = false, bool $withComments = false,
 *               ?array $xpath = null, ?array $nsPrefixes = null): string|false
 *
 * "" on canonicalization failure (php returns an empty string for
 * empty/unserializable input). The four parameters are php's; PHL canonicalizes
 * inclusive-with-comments only, exactly as the chunk did -- they are declared so
 * the arity and types are php's, not so the body branches on them.
 */
DOM_METHOD(vm_builtin_DOMNode_C14N)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	sxu32 nMark;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
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

/* ===== DOMNodeList and DOMNamedNodeMap ===== */

/*
 * A node list is one of three things, and which one it is decides both count()
 * and item(). Two of the three are LIVE views (they re-walk the tree on every
 * question, which is what makes getElementsByTagName track mutations); the third
 * is the document-order snapshot DOMXPath::query froze.
 */
#define DNL_CHILD 0   /* $node->childNodes */
#define DNL_GEBTN 1   /* getElementsByTagName($name) */
#define DNL_SNAP  2   /* DOMXPath::query() */
#define DNL_KIND  "__kind"
#define DNL_OWNER "__owner"
#define DNL_NAME  "__name"
#define DNL_SNAP_SLOT "__snap"

/* The node a live list is a view OF. */
static phl_domnode * DomListOwner(ph7_class_instance *pList)
{
	return DomResOf(PH7_NativeAttrObj(pList,DNL_OWNER));
}
static ph7_hashmap * DomListSnap(ph7_class_instance *pList)
{
	ph7_value *pVal = PH7_NativeAttr(pList,DNL_SNAP_SLOT);
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	return (ph7_hashmap *)pVal->x.pOther;
}
static int DomListCount(ph7_class_instance *pList)
{
	phl_domnode *pOwner;
	const char *zName;
	int nName,iCount = 0;
	if( pList == 0 ){
		return 0;
	}
	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){
		ph7_hashmap *pMap = DomListSnap(pList);
		return pMap ? (int)pMap->nEntry : 0;
	}
	pOwner = DomListOwner(pList);
	if( pOwner == 0 ){
		return 0;
	}
	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){
		return DomChildCount((xmlNodePtr)pOwner->pNode,0);
	}
	PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);
	DomGebtnWalk((xmlNodePtr)pOwner->pNode,zName,nName,-1,&iCount);
	return iCount;
}
/* The wrapper at one index, or NULL past the end. BORROWED, like every wrap. */
static ph7_class_instance * DomListItem(ph7_vm *pVm,ph7_class_instance *pList,int iIndex)
{
	ph7_class_instance *pDoc;
	phl_domnode *pOwner;
	xmlNodePtr pNode = 0;
	const char *zName;
	int nName;
	if( pList == 0 || iIndex < 0 ){
		return 0;
	}
	pDoc = PH7_NativeAttrObj(pList,DOM_DOC);
	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_SNAP ){
		ph7_hashmap *pMap = DomListSnap(pList);
		ph7_hashmap_node *pEntry = 0;
		ph7_value sKey,*pHit;
		phl_domnode *pRes;
		if( pMap == 0 ){
			return 0;
		}
		PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)iIndex);
		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){
			pEntry = 0;
		}
		PH7_MemObjRelease(&sKey);
		pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;
		pRes = (pHit && (pHit->iFlags & MEMOBJ_RES)) ? (phl_domnode *)pHit->x.pOther : 0;
		return pRes ? DomWrap(&(*pVm),pDoc,pRes->pShell,(xmlNodePtr)pRes->pNode) : 0;
	}
	pOwner = DomListOwner(pList);
	if( pOwner == 0 ){
		return 0;
	}
	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_CHILD ){
		pNode = DomChildAt((xmlNodePtr)pOwner->pNode,iIndex);
	}else{
		PH7_NativeAttrStr(pList,DNL_NAME,&zName,&nName);
		pNode = DomGebtnWalk((xmlNodePtr)pOwner->pNode,zName,nName,iIndex,0);
	}
	return DomWrap(&(*pVm),pDoc,pOwner->pShell,pNode);
}
/*
 * Build one. pOwnerObj is the node the live view is of (NULL for a snapshot),
 * pSnap the frozen list (NULL otherwise). The caller owns the reference.
 */
static ph7_class_instance * DomNewCollection(ph7_vm *pVm,const char *zClass,
	ph7_class_instance *pDoc,int iKind,ph7_class_instance *pOwnerObj,
	const char *zName,ph7_value *pSnap)
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;
	if( pObj == 0 ){
		return 0;
	}
	PH7_NativeSetAttrInt(&(*pVm),pObj,DNL_KIND,iKind);
	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);
	if( pOwnerObj ){
		PH7_NativeSetAttrObj(&(*pVm),pObj,DNL_OWNER,pOwnerObj);
	}
	if( zName ){
		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_NAME,zName,(int)SyStrlen(zName));
	}
	if( pSnap ){
		ph7_value *pSlot = PH7_NativeAttr(pObj,DNL_SNAP_SLOT);
		if( pSlot ){
			PH7_MemObjStore(pSnap,pSlot);
		}
	}
	return pObj;
}
/* DOMNodeList::count(): int and ::item(int $index): ?DOMNode */
DOM_METHOD(vm_builtin_DOMNodeList_count)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));
	return PH7_OK;
}
DOM_METHOD(vm_builtin_DOMNodeList_item)
{
	int iIndex = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;
	return DomResultWrap(pCtx,DomListItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));
}
/* DOMNodeList::__get($name) -- php exposes `length` as a virtual property. */
DOM_METHOD(vm_builtin_DOMNodeList_get)
{
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	if( DomNameIs(zName,"length") ){
		ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * DOMNamedNodeMap: an element's attributes, keyed by name.
 *
 * It shares DOMNodeList's slots (the owner element in $__owner) but walks the
 * attribute list rather than the child list, so it gets its own two readers.
 */
static ph7_class_instance * DomMapItem(ph7_vm *pVm,ph7_class_instance *pMap,int iIndex)
{
	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;
	xmlAttrPtr pAttr;
	if( pOwner == 0 || iIndex < 0 ){
		return 0;
	}
	pAttr = DomAttrAt((xmlNodePtr)pOwner->pNode,iIndex);
	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,(xmlNodePtr)pAttr);
}
static int DomMapCount(ph7_class_instance *pMap)
{
	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;
	return pOwner ? DomAttrCount((xmlNodePtr)pOwner->pNode) : 0;
}
DOM_METHOD(vm_builtin_DOMNamedNodeMap_count)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));
	return PH7_OK;
}
DOM_METHOD(vm_builtin_DOMNamedNodeMap_item)
{
	int iIndex = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;
	return DomResultWrap(pCtx,DomMapItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));
}
/* DOMNamedNodeMap::getNamedItem(string $qualifiedName): ?DOMAttr */
DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItem)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pOwner = pThis ? DomListOwner(pThis) : 0;
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlAttrPtr pAttr = pOwner ? xmlHasProp((xmlNodePtr)pOwner->pNode,(const xmlChar *)zName) : 0;
	if( pAttr == 0 || pAttr->type != XML_ATTRIBUTE_NODE ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,PH7_NativeAttrObj(pThis,DOM_DOC),
		pOwner->pShell,(xmlNodePtr)pAttr));
}
DOM_METHOD(vm_builtin_DOMNamedNodeMap_get)
{
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	if( DomNameIs(zName,"length") ){
		ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * Both collections are IteratorAggregates, as php's are -- the chunk made
 * DOMNodeList an `Iterator` with its own cursor (so `$list instanceof Iterator`
 * was true where php says false) and gave DOMNamedNodeMap no iteration at all,
 * which meant `foreach ($el->attributes as $a)` walked the map's own private
 * slots instead of the attributes.
 *
 * The cursor lives in the shared InternalIterator (oo_native.c); a vtable states
 * only how to REACH a position. DOMNodeList keys by index, DOMNamedNodeMap by
 * attribute name, which is what php answers for each.
 */
static void DomIterSettle(ph7_vm *pVm,ph7_class_instance *pIt,int bNamed)
{
	ph7_class_instance *pSrc = PH7_NativeAttrObj(pIt,PH7_NATIVE_IT_SRC);
	sxi64 iPos = PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS);
	ph7_class_instance *pCur;
	pCur = bNamed ? DomMapItem(&(*pVm),pSrc,(int)iPos) : DomListItem(&(*pVm),pSrc,(int)iPos);
	if( pCur == 0 ){
		PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,1);
		return;
	}
	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pCur);  /* borrowed: no unref */
	if( bNamed ){
		phl_domnode *pNd = DomResOf(pCur);
		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
		const char *zKey = (pNode && pNode->name) ? (const char *)pNode->name : "";
		PH7_NativeSetAttrStr(&(*pVm),pIt,PH7_NATIVE_IT_KEY,zKey,(int)SyStrlen(zKey));
	}else{
		PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_KEY,iPos);
	}
	PH7_NativeSetAttrBool(&(*pVm),pIt,PH7_NATIVE_IT_DONE,0);
}
static void DomListRewind(ph7_vm *pVm,ph7_class_instance *pIt)
{
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);
	DomIterSettle(&(*pVm),pIt,0);
}
static void DomListNext(ph7_vm *pVm,ph7_class_instance *pIt)
{
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,
		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);
	DomIterSettle(&(*pVm),pIt,0);
}
static void DomMapRewind(ph7_vm *pVm,ph7_class_instance *pIt)
{
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,0);
	DomIterSettle(&(*pVm),pIt,1);
}
static void DomMapNext(ph7_vm *pVm,ph7_class_instance *pIt)
{
	PH7_NativeSetAttrInt(&(*pVm),pIt,PH7_NATIVE_IT_POS,
		PH7_NativeAttrInt(pIt,PH7_NATIVE_IT_POS) + 1);
	DomIterSettle(&(*pVm),pIt,1);
}
static const PH7_NativeIterVtab sDomListIterVtab = { DomListRewind, DomListNext };
static const PH7_NativeIterVtab sDomMapIterVtab  = { DomMapRewind,  DomMapNext };
/* Both getIterator()s: a fresh InternalIterator per call, as php's are. */
DOM_METHOD(vm_builtin_Dom_getIterator)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pIt;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	pIt = PH7_NativeIteratorNew(pCtx->pVm,pThis);
	if( pIt == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pIt);
	return PH7_OK;
}

/* ===== DOMXPath ===== */

/*
 * DOMXPath::query(string $expression, ?DOMNode $contextNode = null,
 *                 bool $registerNodeNS = true): DOMNodeList|false
 *
 * The nodeset is frozen into a document-order snapshot (php's query() is not
 * live) and handed to a DOMNodeList of kind DNL_SNAP. false on an invalid
 * expression or a non-nodeset result, which is php's contract.
 */
DOM_METHOD(vm_builtin_DOMXPath_query)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pDoc = pThis ? PH7_NativeAttrObj(pThis,"document") : 0;
	phl_domnode *pDocNd = DomResOf(pDoc);
	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	phl_domnode *pCtxNd = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;
	xmlXPathContextPtr pXCtx;
	xmlXPathObjectPtr pObj;
	ph7_class_instance *pList;
	ph7_value *pSnap;
	sxu32 nMark;
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pXCtx = xmlXPathNewContext((xmlDocPtr)pDocNd->pNode);
	if( pXCtx == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* With no explicit context node php evaluates relative expressions
	 * against the document ELEMENT (so query('file') matches a child of
	 * the root), not the document node -- match that. */
	if( pCtxNd ){
		pXCtx->node = (xmlNodePtr)pCtxNd->pNode;
	}else{
		pXCtx->node = xmlDocGetRootElement((xmlDocPtr)pDocNd->pNode);
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pObj = xmlXPathEvalExpression((const xmlChar *)zExpr,pXCtx);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMXPath::query");
	if( pObj == 0 || pObj->type != XPATH_NODESET ){
		if( pObj ){
			xmlXPathFreeObject(pObj);
		}
		xmlXPathFreeContext(pXCtx);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pSnap = ph7_context_new_array(pCtx);
	if( pSnap == 0 ){
		xmlXPathFreeObject(pObj);
		xmlXPathFreeContext(pXCtx);
		return PH7_ContextMemoryError(pCtx);
	}
	if( pObj->nodesetval ){
		int i;
		for( i = 0 ; i < pObj->nodesetval->nodeNr ; i++ ){
			xmlNodePtr pNode = pObj->nodesetval->nodeTab[i];
			phl_domnode *pWrap;
			ph7_value *pRes;
			if( pNode == 0 || pNode->type == XML_NAMESPACE_DECL ){
				continue; /* namespace pseudo-nodes are not exposed */
			}
			pWrap = DomNewRes(pVm,pDocNd->pShell,pNode);
			pRes = ph7_context_new_scalar(pCtx);
			if( pWrap == 0 || pRes == 0 ){
				break;
			}
			ph7_value_resource(pRes,pWrap);
			ph7_array_add_elem(pSnap,0,pRes);
		}
	}
	xmlXPathFreeObject(pObj);
	xmlXPathFreeContext(pXCtx);
	pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_SNAP,0,0,pSnap);
	if( pList == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pList);
	return PH7_OK;
}
/* DOMXPath::__construct(DOMDocument $document, bool $registerNodeNS = true) */
DOM_METHOD(vm_builtin_DOMXPath_construct)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis && nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){
		PH7_NativeSetAttrObj(pCtx->pVm,pThis,"document",
			(ph7_class_instance *)apArg[0]->x.pOther);
	}
	return PH7_OK;
}

/* ===== Schema validation ===== */

/* Schema parser/validator diagnostics: forward onto the shared per-VM queue
 * via PH7_LibxmlQueueError, exactly like the global structured handler. */
#if LIBXML_VERSION >= 21200
static void DomSchemaErr(void *pUserData,const xmlError *pErr)
#else
static void DomSchemaErr(void *pUserData,xmlErrorPtr pErr)
#endif
{
	if( pErr == 0 ){
		return;
	}
	PH7_LibxmlQueueError((ph7_vm *)pUserData,(int)pErr->level,pErr->code,pErr->line,
		pErr->int2,pErr->message,pErr->file);
}
/* DOMDocument::schemaValidateSource(string $source, int $flags = 0): bool */
DOM_METHOD(vm_builtin_DOMDocument_schemaValidateSource)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = DomThisNode(pCtx);
	int nXsd = 0;
	const char *zXsd = nArg > 0 ? ph7_value_to_string(apArg[0],&nXsd) : "";
	xmlSchemaParserCtxtPtr pParser;
	xmlSchemaPtr pSchema;
	xmlSchemaValidCtxtPtr pValid;
	int rc;
	sxu32 nMark;
	if( pDocNd == 0 || nXsd < 1 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pParser = xmlSchemaNewMemParserCtxt(zXsd,nXsd);
	if( pParser == 0 ){
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	xmlSchemaSetParserStructuredErrors(pParser,DomSchemaErr,pVm);
	pSchema = xmlSchemaParse(pParser);
	xmlSchemaFreeParserCtxt(pParser);
	if( pSchema == 0 ){
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");
		/* php raises "Invalid Schema" and returns false */
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,"DOMDocument::schemaValidateSource(): Invalid Schema");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pValid = xmlSchemaNewValidCtxt(pSchema);
	if( pValid == 0 ){
		xmlSchemaFree(pSchema);
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	xmlSchemaSetValidStructuredErrors(pValid,DomSchemaErr,pVm);
	rc = xmlSchemaValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);
	xmlSchemaFreeValidCtxt(pValid);
	xmlSchemaFree(pSchema);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::schemaValidateSource");
	ph7_result_bool(pCtx,rc == 0);
	return PH7_OK;
}


/* ===== The shared __get dispatch ===== */

/*
 * DOMNode's virtual properties.
 *
 * php exposes these through property handlers on the class; PHL answers them
 * from __get, as the chunk did. Returns 1 when it recognised the name, so a
 * subclass's own __get can state its extras and then defer here -- which is
 * what `parent::__get($name)` did.
 */
static int DomNodeProp(ph7_context *pCtx,const char *zName)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pDoc = DomThisDoc(pCtx);
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	int bIsDoc = pNode && (pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE);
	if( DomNameIs(zName,"nodeName") ){
		DomNodeName(pCtx,pNode);
	}else if( DomNameIs(zName,"nodeValue") ){
		DomNodeValue(pCtx,pNode);
	}else if( DomNameIs(zName,"nodeType") ){
		ph7_result_int(pCtx,pNode ? (int)pNode->type : 0);
	}else if( DomNameIs(zName,"textContent") ){
		DomTextContent(pCtx,pNode);
	}else if( DomNameIs(zName,"parentNode") ){
		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);
	}else if( DomNameIs(zName,"firstChild") ){
		DomResultNodeOf(pCtx,pNd,pNode ? pNode->children : 0);
	}else if( DomNameIs(zName,"lastChild") ){
		DomResultNodeOf(pCtx,pNd,pNode ? pNode->last : 0);
	}else if( DomNameIs(zName,"nextSibling") ){
		DomResultNodeOf(pCtx,pNd,pNode ? pNode->next : 0);
	}else if( DomNameIs(zName,"previousSibling") ){
		DomResultNodeOf(pCtx,pNd,pNode ? pNode->prev : 0);
	}else if( DomNameIs(zName,"ownerDocument") ){
		/* A document has no owner document, which is also why DomWrap answers
		 * the document itself rather than a second wrapper for it. */
		DomResultWrap(pCtx,bIsDoc ? 0 : pDoc);
	}else if( DomNameIs(zName,"childElementCount") ){
		ph7_result_int(pCtx,DomChildCount(pNode,1));
	}else if( DomNameIs(zName,"childNodes") ){
		ph7_class_instance *pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_CHILD,pThis,0,0);
		if( pList == 0 ){
			return -1;
		}
		PH7_NativeResultObject(pCtx,pList);
	}else if( DomNameIs(zName,"attributes") ){
		/* php: NULL for anything that is not an element. */
		if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
			ph7_result_null(pCtx);
		}else{
			ph7_class_instance *pMap = DomNewCollection(pVm,"DOMNamedNodeMap",pDoc,DNL_CHILD,pThis,0,0);
			if( pMap == 0 ){
				return -1;
			}
			PH7_NativeResultObject(pCtx,pMap);
		}
	}else{
		return 0;
	}
	return 1;
}
/* The argument every __get body reads. */
static const char * DomGetName(int nArg,ph7_value **apArg)
{
	return nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
}
DOM_METHOD(vm_builtin_DOMNode_get)
{
	if( DomNodeProp(pCtx,DomGetName(nArg,apArg)) == 0 ){
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/* DOMDocument adds documentElement. */
DOM_METHOD(vm_builtin_DOMDocument_get)
{
	const char *zName = DomGetName(nArg,apArg);
	phl_domnode *pNd;
	if( DomNameIs(zName,"documentElement") ){
		pNd = DomThisNode(pCtx);
		return DomResultNodeOf(pCtx,pNd,pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0);
	}
	if( DomNodeProp(pCtx,zName) == 0 ){
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/* DOMElement adds tagName. */
DOM_METHOD(vm_builtin_DOMElement_get)
{
	const char *zName = DomGetName(nArg,apArg);
	phl_domnode *pNd;
	if( DomNameIs(zName,"tagName") ){
		pNd = DomThisNode(pCtx);
		DomNodeName(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);
		return PH7_OK;
	}
	if( DomNodeProp(pCtx,zName) == 0 ){
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/* DOMAttr adds name/value/ownerElement. */
DOM_METHOD(vm_builtin_DOMAttr_get)
{
	const char *zName = DomGetName(nArg,apArg);
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( DomNameIs(zName,"name") ){
		DomNodeName(pCtx,pNode);
		return PH7_OK;
	}
	if( DomNameIs(zName,"value") ){
		DomNodeValue(pCtx,pNode);
		return PH7_OK;
	}
	if( DomNameIs(zName,"ownerElement") ){
		return DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);
	}
	if( DomNodeProp(pCtx,zName) == 0 ){
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/* DOMCharacterData adds data/length; DOMText adds wholeText on top of those. */
static int DomCharDataProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlChar *zContent;
	if( DomNameIs(zName,"data") ){
		DomNodeValue(pCtx,pNode);
		return 1;
	}
	if( DomNameIs(zName,"length") ){
		/* php's length is the BYTE length of the data, which is what strlen()
		 * of the chunk's nodeValue measured. */
		zContent = pNode ? xmlNodeGetContent(pNode) : 0;
		ph7_result_int(pCtx,zContent ? (int)SyStrlen((const char *)zContent) : 0);
		if( zContent ){
			xmlFree(zContent);
		}
		return 1;
	}
	return 0;
}
DOM_METHOD(vm_builtin_DOMCharacterData_get)
{
	const char *zName = DomGetName(nArg,apArg);
	if( DomCharDataProp(pCtx,zName) == 0 && DomNodeProp(pCtx,zName) == 0 ){
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
DOM_METHOD(vm_builtin_DOMText_get)
{
	const char *zName = DomGetName(nArg,apArg);
	phl_domnode *pNd;
	if( DomNameIs(zName,"wholeText") ){
		pNd = DomThisNode(pCtx);
		DomNodeValue(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);
		return PH7_OK;
	}
	if( DomCharDataProp(pCtx,zName) == 0 && DomNodeProp(pCtx,zName) == 0 ){
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/* DOMDocument::getElementsByTagName / DOMElement::getElementsByTagName --
 * php declares it on those two, not on DOMNode, so both specs name it. */
DOM_METHOD(vm_builtin_Dom_getElementsByTagName)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	ph7_class_instance *pList;
	if( pThis == 0 ){
		return PH7_OK;
	}
	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTN,pThis,zName,0);
	if( pList == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pList);
	return PH7_OK;
}

/*
 * Install the DOM library: every class declared from C, no embedded chunk and
 * no globally visible thunk left.  Called from PH7_VmInit inside the
 * bCompilingBuiltin window, after PH7_VmInstallLibxml (the capture plumbing must
 * exist) and after the Reflection install (DOMException needs Exception).
 */
PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)
{
	/* The two slots every wrapper carries. They were public in the chunk and stay
	 * public: hiding them is the per-class debug-info hook's job (§7.4 (e)), which
	 * DateTime, XMLWriter, Fiber, Generator and WeakReference all wait on too. */
	static const PH7_NativePropDef aNodeProp[] = {
		{ DOM_RES, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_DOC, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	/* php's own signatures. Declaring `DOMNode $node` is what makes
	 * `$n->appendChild(1)` the TypeError php raises instead of a warning from
	 * reading ->__res off an int. */
	static const PH7_NativeMethodDef aNodeMethod[] = {
		{ "appendChild",    PH7_MOD_PUBLIC, "DOMNode $node", "", vm_builtin_DOMNode_appendChild },
		{ "insertBefore",   PH7_MOD_PUBLIC, "DOMNode $node, ?DOMNode $child = null", "",
		  vm_builtin_DOMNode_insertBefore },
		{ "removeChild",    PH7_MOD_PUBLIC, "DOMNode $child", "", vm_builtin_DOMNode_removeChild },
		{ "replaceChild",   PH7_MOD_PUBLIC, "DOMNode $node, DOMNode $child", "",
		  vm_builtin_DOMNode_replaceChild },
		{ "hasChildNodes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasChildNodes },
		{ "hasAttributes",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMNode_hasAttributes },
		{ "isSameNode",     PH7_MOD_PUBLIC, "DOMNode $otherNode", "@bool", vm_builtin_DOMNode_isSameNode },
		{ "getLineNo",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNode_getLineNo },
		{ "C14N",           PH7_MOD_PUBLIC,
		  "bool $exclusive = false, bool $withComments = false, ?array $xpath = null, "
		  "?array $nsPrefixes = null", "@string|false", vm_builtin_DOMNode_C14N },
		{ "__get",          PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNode_get },
	};
	static const PH7_NativePropDef aDocProp[] = {
		/* php models both as VIRTUAL hooked properties reading libxml state, so it
		 * reports no default; PHL's are real slots and keep theirs, or a read before
		 * the first write would raise where php answers the parser's current value.
		 * The TYPE is what the row can state exactly (PLAN §7.4 for the virtual half). */
		{ "preserveWhiteSpace", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },
		{ "formatOutput",       PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },
		/* The identity cache DomWrap keys by node pointer. */
		{ DOM_NODES,            PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aDocMethod[] = {
		{ "__construct",          PH7_MOD_PUBLIC, "string $version = '1.0', string $encoding = ''", "",
		  vm_builtin_DOMDocument_construct },
		{ "loadXML",              PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",
		  vm_builtin_DOMDocument_loadXML },
		{ "saveXML",              PH7_MOD_PUBLIC, "?DOMNode $node = null, int $options = 0", "@string|false",
		  vm_builtin_DOMDocument_saveXML },
		{ "createElement",        PH7_MOD_PUBLIC, "string $localName, string $value = ''", "",
		  vm_builtin_DOMDocument_createElement },
		{ "createTextNode",       PH7_MOD_PUBLIC, "string $data", "@DOMText",
		  vm_builtin_DOMDocument_createTextNode },
		{ "createComment",        PH7_MOD_PUBLIC, "string $data", "@DOMComment",
		  vm_builtin_DOMDocument_createComment },
		{ "createCDATASection",   PH7_MOD_PUBLIC, "string $data", "",
		  vm_builtin_DOMDocument_createCDATASection },
		{ "normalizeDocument",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },
		{ "schemaValidateSource", PH7_MOD_PUBLIC, "string $source, int $flags = 0", "@bool",
		  vm_builtin_DOMDocument_schemaValidateSource },
		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",
		  vm_builtin_Dom_getElementsByTagName },
		{ "__get",                PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMDocument_get },
	};
	static const PH7_NativeMethodDef aElemMethod[] = {
		{ "getAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@string",
		  vm_builtin_DOMElement_getAttribute },
		{ "hasAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",
		  vm_builtin_DOMElement_hasAttribute },
		{ "setAttribute",         PH7_MOD_PUBLIC, "string $qualifiedName, string $value", "",
		  vm_builtin_DOMElement_setAttribute },
		{ "removeAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName", "@bool",
		  vm_builtin_DOMElement_removeAttribute },
		{ "getAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@string",
		  vm_builtin_DOMElement_getAttributeNS },
		{ "setAttributeNS",       PH7_MOD_PUBLIC,
		  "?string $namespace, string $qualifiedName, string $value", "@void",
		  vm_builtin_DOMElement_setAttributeNS },
		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",
		  vm_builtin_Dom_getElementsByTagName },
		{ "__get",                PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMElement_get },
	};
	static const PH7_NativeMethodDef aAttrMethod[] = {
		{ "__get", PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMAttr_get },
	};
	static const PH7_NativeMethodDef aCharMethod[] = {
		{ "__get", PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMCharacterData_get },
	};
	static const PH7_NativeMethodDef aTextMethod[] = {
		{ "__get", PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMText_get },
	};
	/* DOMNodeList and DOMNamedNodeMap share a slot layout: what a live view is OF
	 * ($__owner), the document to wrap results against ($__doc), and -- for the
	 * two node-list kinds -- the tag name or the frozen snapshot. */
	static const PH7_NativePropDef aListProp[] = {
		{ DNL_KIND,      PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0, 0.0 }, 0 },
		{ DOM_DOC,       PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },
		{ DNL_OWNER,     PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },
		{ DNL_NAME,      PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
		/* The cached node snapshot, missed by the 2 Aug hidden-slot sweep exactly as
		 * Closure's three were: php presents no property on either class this table
		 * declares (DOMNodeList, DOMNamedNodeMap), and `__snap` was on var_dump,
		 * (array), get_object_vars, foreach, json_encode and Reflection. */
		{ DNL_SNAP_SLOT, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aListMethod[] = {
		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNodeList_count },
		{ "item",        PH7_MOD_PUBLIC, "int $index", "", vm_builtin_DOMNodeList_item },
		{ "getIterator", PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
		{ "__get",       PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNodeList_get },
	};
	static const PH7_NativeMethodDef aMapMethod[] = {
		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNamedNodeMap_count },
		{ "item",         PH7_MOD_PUBLIC, "int $index", "@?DOMNode", vm_builtin_DOMNamedNodeMap_item },
		{ "getNamedItem", PH7_MOD_PUBLIC, "string $qualifiedName", "@?DOMNode",
		  vm_builtin_DOMNamedNodeMap_getNamedItem },
		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
		{ "__get",        PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNamedNodeMap_get },
	};
	static const PH7_NativePropDef aXPathProp[] = {
		/* Written by the constructor, which is why the slot can carry php's
		 * non-nullable type with no default at all. */
		{ "document", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "DOMDocument" },
	};
	static const PH7_NativeMethodDef aXPathMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "DOMDocument $document, bool $registerNodeNS = true", "",
		  vm_builtin_DOMXPath_construct },
		{ "query",       PH7_MOD_PUBLIC,
		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",
		  vm_builtin_DOMXPath_query },
	};
	/* Bases before subclasses: PH7_InstallNativeClasses declares the whole table
	 * before touching a method, but PH7_ClassInherit still needs the parent to
	 * exist when the child's row is declared. */
	static const PH7_NativeClassSpec aSpec[] = {
		{ "DOMException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMNode", 0, 0, 0,
		  aNodeMethod, SX_ARRAYSIZE(aNodeMethod), 0, 0, aNodeProp, SX_ARRAYSIZE(aNodeProp), 0, 0, 0 },
		{ "DOMDocument", "DOMNode", 0, 0,
		  aDocMethod, SX_ARRAYSIZE(aDocMethod), 0, 0, aDocProp, SX_ARRAYSIZE(aDocProp), 0, 0, 0 },
		{ "DOMElement", "DOMNode", 0, 0,
		  aElemMethod, SX_ARRAYSIZE(aElemMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMAttr", "DOMNode", 0, 0,
		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMCharacterData", "DOMNode", 0, 0,
		  aCharMethod, SX_ARRAYSIZE(aCharMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMText", "DOMCharacterData", 0, 0,
		  aTextMethod, SX_ARRAYSIZE(aTextMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMComment", "DOMCharacterData", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMCdataSection", "DOMText", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		/* php's own two: IteratorAggregate (NOT Iterator -- the chunk had the
		 * list carry its own cursor) and Countable. */
		{ "DOMNodeList", 0, "IteratorAggregate,Countable", 0,
		  aListMethod, SX_ARRAYSIZE(aListMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),
		  0, &sDomListIterVtab, 0 },
		{ "DOMNamedNodeMap", 0, "IteratorAggregate,Countable", 0,
		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),
		  0, &sDomMapIterVtab, 0 },
		{ "DOMXPath", 0, 0, 0,
		  aXPathMethod, SX_ARRAYSIZE(aXPathMethod), 0, 0, aXPathProp, SX_ARRAYSIZE(aXPathProp), 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_dom_unused;
#endif /* PH7_ENABLE_LIBXML */
