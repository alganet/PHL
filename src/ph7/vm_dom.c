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
#include <libxml/xpathInternals.h>
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
	case XML_PI_NODE:            return "DOMProcessingInstruction";
	case XML_DOCUMENT_FRAG_NODE: return "DOMDocumentFragment";
	case XML_ENTITY_REF_NODE:    return "DOMEntityReference";
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
/*
 * The three names a namespaced document reads on every node.
 *
 * `namespaceURI` and `localName` are php's `?string` -- null for a node that
 * cannot carry a name in a namespace at all (a text node, a comment, a PI, the
 * document itself) -- while `prefix` is a plain `string` that answers "" there,
 * which is why one of the three cannot be derived from the other two.
 *
 * All three are gated on the node KIND before anything is read, which is not
 * only php's rule but a memory-safety one: only element and attribute nodes are
 * xmlNode/xmlAttr-shaped, and `->ns` on an xmlDoc aliases its `compression`
 * int, on an xmlDtd its notation table.  Reading it there and dereferencing the
 * result is a SIGSEGV out of `$doc->namespaceURI` -- an ordinary property read.
 */
static int DomHasNsSlot(xmlNodePtr pNode)
{
	return pNode != 0
		&& (pNode->type == XML_ELEMENT_NODE || pNode->type == XML_ATTRIBUTE_NODE);
}
static void DomNamespaceUri(ph7_context *pCtx,xmlNodePtr pNode)
{
	if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->href ){
		ph7_result_string(pCtx,(const char *)pNode->ns->href,-1);
	}else{
		ph7_result_null(pCtx);
	}
}
static void DomPrefix(ph7_context *pCtx,xmlNodePtr pNode)
{
	if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->prefix ){
		ph7_result_string(pCtx,(const char *)pNode->ns->prefix,-1);
	}else{
		ph7_result_string(pCtx,"",0);
	}
}
static void DomLocalName(ph7_context *pCtx,xmlNodePtr pNode)
{
	if( DomHasNsSlot(pNode) ){
		ph7_result_string(pCtx,pNode->name ? (const char *)pNode->name : "",-1);
	}else{
		ph7_result_null(pCtx);
	}
}
/* php's `isConnected`: is the node's root the DOCUMENT? A node built by a
 * create* factory carries the document as its `ownerDocument` from birth, so
 * that property cannot answer this and a program testing it reads true for a
 * node it has not appended yet. */
static int DomIsConnected(xmlNodePtr pNode)
{
	xmlNodePtr pRoot = pNode;
	if( pRoot == 0 ){
		return 0;
	}
	while( pRoot->parent ){
		pRoot = pRoot->parent;
	}
	return pRoot->type == XML_DOCUMENT_NODE || pRoot->type == XML_HTML_DOCUMENT_NODE;
}
/*
 * php's nodeValue, which is null for every node kind that has no value of its
 * own -- the document, a doctype, a fragment, an entity DECLARATION and an
 * entity REFERENCE all answer null, where `textContent` on the same node walks
 * its children and answers a string. (The element case is php's own
 * convenience: DOM says an element has no node value.)
 */
static void DomNodeValue(ph7_context *pCtx,xmlNodePtr pNode)
{
	xmlChar *zContent;
	if( pNode == 0 ){
		ph7_result_null(pCtx);
		return;
	}
	switch( pNode->type ){
	case XML_ATTRIBUTE_NODE:
	case XML_TEXT_NODE:
	case XML_ELEMENT_NODE:
	case XML_COMMENT_NODE:
	case XML_CDATA_SECTION_NODE:
	case XML_PI_NODE:
		break;
	default:
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
 * A DOCUMENT FRAGMENT is not linked, it is EMPTIED: php moves its children into
 * the target and answers the FIRST of them (the fragment itself is never a
 * child of anything, which is the whole point of the type -- it is how a
 * program builds a run of nodes and inserts it in one call). An EMPTY one is
 * php's warning plus `false`, not an exception.
 *
 * *ppFirst takes the first node moved, or NULL when the argument was not a
 * fragment at all; the caller then links the node itself.
 */
static int DomIsFragment(xmlNodePtr pNode)
{
	return pNode && pNode->type == XML_DOCUMENT_FRAG_NODE;
}
static int DomFragEmpty(ph7_context *pCtx)
{
	/* The context already qualifies the message with php's `DOMNode::method(): `. */
	ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Document Fragment is empty");
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/* Move every child of pFrag into pParent, before pRef or at the end. */
static xmlNodePtr DomFragMove(phl_xmldoc *pShell,xmlNodePtr pParent,xmlNodePtr pFrag,xmlNodePtr pRef)
{
	xmlNodePtr pFirst = pFrag->children;
	xmlNodePtr pChild = pFirst;
	while( pChild ){
		xmlNodePtr pNext = pChild->next;
		DomDetach(pShell,pChild);
		if( pRef ){
			DomLinkBefore(pParent,pChild,pRef);
		}else{
			DomLinkLast(pParent,pChild);
		}
		pChild = pNext;
	}
	pFrag->children = pFrag->last = 0;
	return pFirst;
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
	if( DomIsFragment((xmlNodePtr)pChd->pNode) ){
		xmlNodePtr pFirst;
		if( ((xmlNodePtr)pChd->pNode)->children == 0 ){
			return DomFragEmpty(pCtx);
		}
		pFirst = DomFragMove(pChd->pShell,(xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode,0);
		return DomResultNodeOf(pCtx,pPar,pFirst);
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
	if( DomIsFragment(pChild) ){
		xmlNodePtr pFirst;
		if( pChild->children == 0 ){
			return DomFragEmpty(pCtx);
		}
		pFirst = DomFragMove(pNew->pShell,pParent,pChild,pAnchor);
		return DomResultNodeOf(pCtx,pPar,pFirst);
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
	if( DomIsFragment(pChild) ){
		/* No empty-fragment refusal here, unlike the other two: php REMOVES the
		 * old child and inserts nothing, and answers it as any replaceChild
		 * does. */
		DomFragMove(pNew->pShell,pParent,pChild,pVictim);
		xmlUnlinkNode(pVictim);
		DomOrphanAdd(pOld->pShell,pVictim);
		ph7_result_value(pCtx,apArg[1]);
		return PH7_OK;
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

/* ===== Position, containment and structural equality ===== */

/* php's DOMNode::DOCUMENT_POSITION_* -- the DOM's own bit values. */
#define DOM_POS_DISCONNECTED 1
#define DOM_POS_PRECEDING    2
#define DOM_POS_FOLLOWING    4
#define DOM_POS_CONTAINS     8
#define DOM_POS_CONTAINED_BY 16
#define DOM_POS_IMPL_SPEC    32

/* The topmost node reachable by parent links -- the DOCUMENT for a node in a
 * tree, and the outermost detached node otherwise. */
static xmlNodePtr DomRootOf(xmlNodePtr pNode)
{
	while( pNode && pNode->parent ){
		pNode = pNode->parent;
	}
	return pNode;
}
/* Is pAnc a STRICT ancestor of pNode? libxml parents an attribute at its
 * element, which is how php answers true for `$el->contains($el->attr)`. */
static int DomIsAncestorOf(xmlNodePtr pAnc,xmlNodePtr pNode)
{
	xmlNodePtr p = pNode ? pNode->parent : 0;
	for( ; p ; p = p->parent ){
		if( p == pAnc ){
			return 1;
		}
	}
	return 0;
}
static int DomDepthOf(xmlNodePtr pNode)
{
	int n = 0;
	for( ; pNode ; pNode = pNode->parent ){
		n++;
	}
	return n;
}
/*
 * Does pA come before pB in document order? Both are distinct nodes of one
 * tree. Lifting each to the depth of the other either lands on the SAME node --
 * one is an ancestor of the other, and an ancestor comes first in a preorder
 * walk -- or, after stepping up in lockstep, on two distinct children of one
 * parent, whose child-list order is the answer.
 *
 * The ancestor case is reachable even though compareDocumentPosition answers
 * CONTAINS/CONTAINED_BY for it: an ATTRIBUTE folds onto its element first, so
 * `$root->attr` against `$child->attr` arrives here as the element PAIR with
 * one of them an ancestor of the other.
 */
static int DomPrecedesInTree(xmlNodePtr pA,xmlNodePtr pB)
{
	int nA = DomDepthOf(pA),nB = DomDepthOf(pB);
	xmlNodePtr pUpA = pA,pUpB = pB,p;
	while( nA > nB ){ pUpA = pUpA->parent; nA--; }
	while( nB > nA ){ pUpB = pUpB->parent; nB--; }
	if( pUpA == pUpB ){
		return pUpA == pA;   /* pA was not lifted: it is the ancestor */
	}
	while( pUpA && pUpB && pUpA->parent != pUpB->parent ){
		pUpA = pUpA->parent;
		pUpB = pUpB->parent;
	}
	for( p = pUpA ? pUpA->prev : 0 ; p ; p = p->prev ){
		if( p == pUpB ){
			return 0;   /* pB is an earlier sibling */
		}
	}
	return 1;
}
/*
 * DOMNode::compareDocumentPosition(DOMNode $other): int
 *
 * The DOM's own algorithm, run with `other` as node1 and the receiver as node2.
 * An ATTRIBUTE is folded onto its element first, which is what makes an
 * attribute answer `CONTAINED_BY|FOLLOWING` against its own element and
 * `IMPLEMENTATION_SPECIFIC` plus the attribute-list order against a sibling
 * attribute -- and what makes it compare as its element against everything else.
 *
 * Two nodes in different trees are DISCONNECTED, and the direction bit there is
 * php's own: the raw node POINTERS, which is the only thing available and which
 * php marks IMPLEMENTATION_SPECIFIC for exactly that reason. The bit is stable
 * and antisymmetric within one process; it is not comparable ACROSS engines,
 * so no test pins it.
 */
DOM_METHOD(vm_builtin_DOMNode_compareDocumentPosition)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	phl_domnode *pOtherNd = nArg > 0 ? DomObjArg(apArg[0]) : 0;
	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;
	xmlNodePtr pNode1,pNode2,pAttr1 = 0,pAttr2 = 0;
	if( pThisNode == 0 || pOther == 0 ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( pThisNode == pOther ){
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	pNode1 = pOther;
	pNode2 = pThisNode;
	if( pNode1->type == XML_ATTRIBUTE_NODE ){
		pAttr1 = pNode1;
		pNode1 = pNode1->parent;
	}
	if( pNode2->type == XML_ATTRIBUTE_NODE ){
		pAttr2 = pNode2;
		pNode2 = pNode2->parent;
		if( pAttr1 && pNode1 && pNode1 == pNode2 ){
			xmlAttrPtr pAttr;
			for( pAttr = pNode2->properties ; pAttr ; pAttr = pAttr->next ){
				if( (xmlNodePtr)pAttr == pAttr1 ){
					ph7_result_int(pCtx,DOM_POS_IMPL_SPEC|DOM_POS_PRECEDING);
					return PH7_OK;
				}
				if( (xmlNodePtr)pAttr == pAttr2 ){
					ph7_result_int(pCtx,DOM_POS_IMPL_SPEC|DOM_POS_FOLLOWING);
					return PH7_OK;
				}
			}
		}
	}
	if( pNode1 == 0 || pNode2 == 0 || DomRootOf(pNode1) != DomRootOf(pNode2) ){
		ph7_result_int(pCtx,DOM_POS_DISCONNECTED|DOM_POS_IMPL_SPEC
			|((sxuptr)pThisNode > (sxuptr)pOther ? DOM_POS_PRECEDING : DOM_POS_FOLLOWING));
		return PH7_OK;
	}
	if( (pAttr1 == 0 && DomIsAncestorOf(pNode1,pNode2))
	 || (pAttr2 != 0 && pNode1 == pNode2) ){
		ph7_result_int(pCtx,DOM_POS_CONTAINS|DOM_POS_PRECEDING);
		return PH7_OK;
	}
	if( (pAttr2 == 0 && DomIsAncestorOf(pNode2,pNode1))
	 || (pAttr1 != 0 && pNode1 == pNode2) ){
		ph7_result_int(pCtx,DOM_POS_CONTAINED_BY|DOM_POS_FOLLOWING);
		return PH7_OK;
	}
	ph7_result_int(pCtx,DomPrecedesInTree(pNode1,pNode2)
		? DOM_POS_PRECEDING : DOM_POS_FOLLOWING);
	return PH7_OK;
}
/* DOMNode::contains(DOMNode|DOMNameSpaceNode|null $other): bool -- INCLUSIVE
 * descendant, so a node contains itself, and (libxml parenting attributes) an
 * element contains its own attributes. */
DOM_METHOD(vm_builtin_DOMNode_contains)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	phl_domnode *pOtherNd = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;
	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;
	ph7_result_bool(pCtx,pThisNode != 0 && pOther != 0
		&& (pThisNode == pOther || DomIsAncestorOf(pThisNode,pOther)));
	return PH7_OK;
}
/* DOMNode::getRootNode(?array $options = null): DOMNode -- php declares the
 * options array (the shadow-DOM `composed` key) and reads nothing from it. */
DOM_METHOD(vm_builtin_DOMNode_getRootNode)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DomResultNodeOf(pCtx,pNd,DomRootOf(pNd ? (xmlNodePtr)pNd->pNode : 0));
}
/*
 * DOMNode::isSupported(string $feature, string $version): bool -- the DOM Level
 * 1 feature test, and php's whole table is two rows: `XML` at 1.0 or 2.0 and
 * `Core` at 1.0 (never `Core` at 2.0). The feature name folds case, the version
 * does not.
 */
DOM_METHOD(vm_builtin_DOMNode_isSupported)
{
	const char *zFeature = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";
	const char *zVersion = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	int bOne = DomNameIs(zVersion,"1.0");
	int bTwo = DomNameIs(zVersion,"2.0");
	int bXml = SyStrlen(zFeature) == 3 && SyStrnicmp(zFeature,"XML",3) == 0;
	int bCore = SyStrlen(zFeature) == 4 && SyStrnicmp(zFeature,"Core",4) == 0;
	ph7_result_bool(pCtx,(bXml && (bOne || bTwo)) || (bCore && bOne));
	return PH7_OK;
}
/*
 * php's structural equality, which is NOT the DOM spec's to the letter.
 *
 * The type has to match, then the per-kind identity: an ELEMENT compares its
 * namespace URI, its PREFIX and its local name (so `p:m` and `q:m` bound to the
 * one URI are NOT equal) plus its attributes as a SET -- same count, and every
 * attribute matched by namespace, local name and value regardless of order. An
 * ATTRIBUTE compares its namespace URI, its name and its value and NOT its
 * prefix, which is the asymmetry no reading of the spec predicts. A PI compares
 * target and data, character data its content, an entity REFERENCE its name.
 * Then the children, in order and in the same number.
 */
static int DomStrEqOrBothNull(const xmlChar *zA,const xmlChar *zB)
{
	if( zA == 0 || zB == 0 ){
		return zA == zB;
	}
	return xmlStrEqual(zA,zB) != 0;
}
static void DomNsHrefOf(xmlNodePtr pNode,const xmlChar **pzHref,const xmlChar **pzPrefix)
{
	*pzHref = (pNode->ns && pNode->ns->href) ? pNode->ns->href : 0;
	*pzPrefix = (pNode->ns && pNode->ns->prefix) ? pNode->ns->prefix : 0;
}
static int DomAttrValueEq(xmlNodePtr pA,xmlNodePtr pB)
{
	xmlChar *zA = xmlNodeGetContent(pA);
	xmlChar *zB = xmlNodeGetContent(pB);
	int bEq = DomStrEqOrBothNull(zA,zB);
	if( zA ){ xmlFree(zA); }
	if( zB ){ xmlFree(zB); }
	return bEq;
}
static int DomAttrSetEqual(xmlNodePtr pA,xmlNodePtr pB)
{
	xmlAttrPtr pOne,pTwo;
	int nA = 0,nB = 0;
	for( pOne = pA->properties ; pOne ; pOne = pOne->next ){ nA++; }
	for( pTwo = pB->properties ; pTwo ; pTwo = pTwo->next ){ nB++; }
	if( nA != nB ){
		return 0;
	}
	for( pOne = pA->properties ; pOne ; pOne = pOne->next ){
		const xmlChar *zHrefA,*zPfxA,*zHrefB,*zPfxB;
		DomNsHrefOf((xmlNodePtr)pOne,&zHrefA,&zPfxA);
		for( pTwo = pB->properties ; pTwo ; pTwo = pTwo->next ){
			DomNsHrefOf((xmlNodePtr)pTwo,&zHrefB,&zPfxB);
			if( DomStrEqOrBothNull(zHrefA,zHrefB)
			 && DomStrEqOrBothNull(pOne->name,pTwo->name)
			 && DomAttrValueEq((xmlNodePtr)pOne,(xmlNodePtr)pTwo) ){
				break;
			}
		}
		if( pTwo == 0 ){
			return 0;
		}
	}
	return 1;
}
static int DomNodesEqual(xmlNodePtr pA,xmlNodePtr pB)
{
	xmlNodePtr pKidA,pKidB;
	const xmlChar *zHrefA,*zPfxA,*zHrefB,*zPfxB;
	if( pA == 0 || pB == 0 ){
		return pA == pB;
	}
	if( pA->type != pB->type ){
		return 0;
	}
	switch( pA->type ){
	case XML_ELEMENT_NODE:
		DomNsHrefOf(pA,&zHrefA,&zPfxA);
		DomNsHrefOf(pB,&zHrefB,&zPfxB);
		if( !DomStrEqOrBothNull(zHrefA,zHrefB) || !DomStrEqOrBothNull(zPfxA,zPfxB)
		 || !DomStrEqOrBothNull(pA->name,pB->name) || !DomAttrSetEqual(pA,pB) ){
			return 0;
		}
		break;
	case XML_ATTRIBUTE_NODE:
		DomNsHrefOf(pA,&zHrefA,&zPfxA);
		DomNsHrefOf(pB,&zHrefB,&zPfxB);
		if( !DomStrEqOrBothNull(zHrefA,zHrefB)
		 || !DomStrEqOrBothNull(pA->name,pB->name)
		 || !DomAttrValueEq(pA,pB) ){
			return 0;
		}
		/* An attribute's value IS its child list; comparing it twice would only
		 * refuse a value split across nodes that reads the same. */
		return 1;
	case XML_PI_NODE:
		if( !DomStrEqOrBothNull(pA->name,pB->name)
		 || !DomStrEqOrBothNull(pA->content,pB->content) ){
			return 0;
		}
		break;
	case XML_TEXT_NODE:
	case XML_CDATA_SECTION_NODE:
	case XML_COMMENT_NODE:
		if( !DomStrEqOrBothNull(pA->content,pB->content) ){
			return 0;
		}
		break;
	case XML_ENTITY_REF_NODE:
		/* The reference's NAME is what a program wrote; its children are the
		 * DECLARATION libxml resolved it to, which is not part of the node. */
		return DomStrEqOrBothNull(pA->name,pB->name);
	default:
		break;
	}
	pKidA = pA->children;
	pKidB = pB->children;
	while( pKidA && pKidB ){
		if( !DomNodesEqual(pKidA,pKidB) ){
			return 0;
		}
		pKidA = pKidA->next;
		pKidB = pKidB->next;
	}
	return pKidA == 0 && pKidB == 0;
}
/* DOMNode::isEqualNode(?DOMNode $otherNode): bool */
DOM_METHOD(vm_builtin_DOMNode_isEqualNode)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	phl_domnode *pOtherNd = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;
	ph7_result_bool(pCtx,pNd != 0 && pOtherNd != 0
		&& DomNodesEqual((xmlNodePtr)pNd->pNode,(xmlNodePtr)pOtherNd->pNode));
	return PH7_OK;
}

/* ===== Copying: cloneNode ===== */

/*
 * Cloning a DOCUMENT is not cloning a node: php builds a SECOND document --
 * its own tree, its own wrapper, its own identity cache -- so the copy's
 * `documentElement` answers the copy as its `ownerDocument` and appending a
 * node of the ORIGINAL into it is the Wrong Document Error it would be between
 * any two documents. Everything else is one xmlDocCopyNode into the SAME tree,
 * parked as an orphan like every other node this file creates.
 *
 * The two parser directives ride along: php's copy answers the receiver's
 * `preserveWhiteSpace` and `formatOutput`, not the class defaults.
 */
static int DomCloneDocument(ph7_context *pCtx,phl_domnode *pNd,int bDeep)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pClass;
	ph7_class_instance *pObj;
	phl_xmldoc *pShell;
	phl_domnode *pRes;
	xmlDocPtr pCopy;
	sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);
	pCopy = xmlCopyDoc((xmlDocPtr)pNd->pNode,bDeep ? 1 : 0);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::cloneNode");
	if( pCopy == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* php answers a plain DOMDocument even when the receiver is a subclass of
	 * one: the copy is built by the extension, not by `new static`. */
	pClass = PH7_VmExtractClass(pVm,"DOMDocument",sizeof("DOMDocument")-1,FALSE,0);
	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
	pShell = pObj ? PH7_LibxmlNewDoc(pVm,pCopy) : 0;
	pRes = pShell ? DomNewRes(pVm,pShell,pCopy) : 0;
	if( pRes == 0 ){
		if( pShell == 0 ){
			xmlFreeDoc(pCopy);   /* not registered: nothing else will free it */
		}
		if( pObj ){
			PH7_ClassInstanceUnref(pObj);
		}
		return PH7_ContextMemoryError(pCtx);
	}
	DomSetRes(pVm,pObj,pRes);
	PH7_NativeSetAttrObj(pVm,pObj,DOM_DOC,pObj);
	if( pThis ){
		PH7_NativeSetAttrBool(pVm,pObj,"preserveWhiteSpace",
			PH7_NativeAttrTruthy(pThis,"preserveWhiteSpace"));
		PH7_NativeSetAttrBool(pVm,pObj,"formatOutput",
			PH7_NativeAttrTruthy(pThis,"formatOutput"));
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * DOMNode::cloneNode(bool $deep = false): DOMNode|false
 *
 * The SHALLOW copy is not libxml's shallow copy: php asks for `extended = 2`,
 * which carries an element's attributes and its own `xmlns` declarations across
 * while leaving the children behind -- so `$el->cloneNode()` is a usable
 * template row, not a bare tag. A deep one is `extended = 1`, and libxml then
 * reconciles whatever namespace the descendants were using onto the copy.
 */
DOM_METHOD(vm_builtin_DOMNode_cloneNode)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	int bDeep = nArg > 0 ? ph7_value_to_bool(apArg[0]) : 0;
	xmlNodePtr pCopy;
	sxu32 nMark;
	if( pNode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE ){
		return DomCloneDocument(pCtx,pNd,bDeep);
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pCopy = xmlDocCopyNode(pNode,pNode->doc,bDeep ? 1 : 2);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMNode::cloneNode");
	if( pCopy == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* An ATTRIBUTE copy comes back in NO namespace: libxml resolves an
	 * attribute's prefix against the element it is being copied ONTO, and there
	 * is no element here. php's clone keeps the namespace, so `p:at="1"` cloned
	 * stays `p:at="1"` rather than turning into `at="1"` -- a silent rename of
	 * the very attribute a namespaced document is keyed on. The copy borrows the
	 * SOURCE's declaration, which is the only thing it can do: an attribute
	 * carries no `nsDef` of its own, and the declaration outlives it (nothing in
	 * this file frees a node before its document). */
	if( pCopy->type == XML_ATTRIBUTE_NODE && pCopy->ns == 0 ){
		pCopy->ns = pNode->ns;
	}
	DomOrphanAdd(pNd->pShell,pCopy);
	return DomResultNodeOf(pCtx,pNd,pCopy);
}

/* ===== Namespaces ===== */

/*
 * Where a namespace lookup starts. php resolves a DOCUMENT to its root element
 * first -- so `$doc->lookupPrefix($uri)` answers what the document element
 * would, and an empty document answers nothing at all -- and starts from the
 * node itself for everything else, because libxml's own search walks up the
 * parent chain (which is how a text node or a PI reaches its element's
 * declarations, and how a detached one reaches none).
 */
static xmlNodePtr DomNsAnchor(xmlNodePtr pNode)
{
	if( pNode && (pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE) ){
		return (xmlNodePtr)xmlDocGetRootElement((xmlDocPtr)pNode);
	}
	return pNode;
}
/* A `?string` argument: its bytes, or NULL for a null one. */
static const char * DomArgStrOrNull(int nArg,ph7_value **apArg,int iArg)
{
	if( iArg >= nArg || ph7_value_is_null(apArg[iArg]) ){
		return 0;
	}
	return ph7_value_to_string(apArg[iArg],0);
}
/* DOMNode::lookupNamespaceURI(?string $prefix): ?string */
DOM_METHOD(vm_builtin_DOMNode_lookupNamespaceURI)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);
	const char *zPrefix = DomArgStrOrNull(nArg,apArg,0);
	xmlNsPtr pNs = pNode ? xmlSearchNs(pNode->doc,pNode,(const xmlChar *)zPrefix) : 0;
	if( pNs && pNs->href ){
		ph7_result_string(pCtx,(const char *)pNs->href,-1);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/* DOMNode::lookupPrefix(string $namespace): ?string -- the DEFAULT namespace has
 * no prefix, so a document whose only declaration is `xmlns="..."` answers null
 * for the very URI lookupNamespaceURI(null) hands back. */
DOM_METHOD(vm_builtin_DOMNode_lookupPrefix)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);
	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlNsPtr pNs = (pNode && zUri[0]) ? xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri) : 0;
	if( pNs && pNs->prefix ){
		ph7_result_string(pCtx,(const char *)pNs->prefix,-1);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/* DOMNode::isDefaultNamespace(string $namespace): bool -- php tests the URI
 * against the default declaration in scope, and answers FALSE for the empty
 * string rather than "this node is in no namespace". */
DOM_METHOD(vm_builtin_DOMNode_isDefaultNamespace)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = DomNsAnchor(pNd ? (xmlNodePtr)pNd->pNode : 0);
	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlNsPtr pNs = (pNode && zUri[0]) ? xmlSearchNs(pNode->doc,pNode,0) : 0;
	ph7_result_bool(pCtx,pNs != 0 && pNs->href != 0
		&& xmlStrEqual(pNs->href,(const xmlChar *)zUri));
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
	case XML_PI_NODE:
		/* php validates the TARGET the same way it validates an element name,
		 * so `createProcessingInstruction('a b')` is Invalid Character Error
		 * rather than a document that will not parse back. */
		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){
			break;
		}
		pNode = xmlNewDocPI(pDoc,(const xmlChar *)zName,nVal ? (const xmlChar *)zVal : 0);
		break;
	case XML_ENTITY_REF_NODE:
		if( xmlValidateName((const xmlChar *)zName,0) != 0 ){
			break;
		}
		pNode = xmlNewReference(pDoc,(const xmlChar *)zName);
		break;
	case XML_DOCUMENT_FRAG_NODE:
		pNode = xmlNewDocFragment(pDoc);
		break;
	}
	PH7_LibxmlCaptureEnd(pVm,nMark,
		iKind == XML_ELEMENT_NODE ? "DOMDocument::createElement" : "DOMDocument::createNode");
	if( pNode == 0 ){
		if( iKind == XML_ELEMENT_NODE || iKind == XML_PI_NODE || iKind == XML_ENTITY_REF_NODE ){
			/* The three factories that take a NAME are the three that can be
			 * handed one libxml refuses. */
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
/* DOMDocument::createProcessingInstruction(string $target, string $data = '')
 * / createEntityReference(string $name) / createDocumentFragment() */
DOM_METHOD(vm_builtin_DOMDocument_createPI)
{
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int nVal = 0;
	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],&nVal) : "";
	return DomDocCreate(pCtx,XML_PI_NODE,zName,zVal,nVal);
}
DOM_METHOD(vm_builtin_DOMDocument_createEntityRef)
{
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	return DomDocCreate(pCtx,XML_ENTITY_REF_NODE,zName,"",0);
}
DOM_METHOD(vm_builtin_DOMDocument_createFragment)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DomDocCreate(pCtx,XML_DOCUMENT_FRAG_NODE,"","",0);
}
DOM_METHOD(vm_builtin_DOMDocument_createComment)
{
	return DomDocCreateData(pCtx,XML_COMMENT_NODE,nArg,apArg);
}
DOM_METHOD(vm_builtin_DOMDocument_createCDATASection)
{
	return DomDocCreateData(pCtx,XML_CDATA_SECTION_NODE,nArg,apArg);
}
/*
 * php's normalization, which both `DOMNode::normalize()` and
 * `DOMDocument::normalizeDocument()` are: adjacent text nodes merge into the
 * FIRST of the run, and a text node left EMPTY is then dropped from the tree
 * entirely -- including one that was empty to begin with, which is what makes
 * `$el->normalize()` the way a program gets rid of the zero-length text nodes an
 * edit leaves behind. Dropping them was the half missing here: a document that
 * had been normalized still serialized `<k></k>` where php writes `<k/>`, and
 * still counted the empty node in `childNodes->length`.
 *
 * Merged-away and dropped siblings are PARKED as orphans, never freed, so any
 * PHP wrapper to them stays valid -- php keeps exactly those alive too, through
 * its own wrapper refcount, and a variable holding one reads its old content and
 * a NULL `parentNode` in both engines.
 *
 * The walk descends into a child ELEMENT and into that element's ATTRIBUTES
 * (an attribute's value is a child text list of its own, and a program that
 * built one in pieces has the same run of nodes to merge). What it does NOT
 * touch is the RECEIVER's own attributes -- php's switch reaches an attribute
 * only through a child element -- so `$el->normalize()` leaves `$el`'s
 * attributes alone while `$el->parentNode->normalize()` normalizes them.
 */
static void DomNormalizeTree(phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNodePtr pChild = pNode->children;
	while( pChild ){
		if( pChild->type == XML_TEXT_NODE ){
			xmlNodePtr pNext;
			while( pChild->next && pChild->next->type == XML_TEXT_NODE ){
				pNext = pChild->next;
				if( pNext->content ){
					xmlNodeAddContent(pChild,pNext->content);
				}
				xmlUnlinkNode(pNext);
				DomOrphanAdd(pShell,pNext);
			}
			if( pChild->content == 0 || pChild->content[0] == 0 ){
				pNext = pChild->next;
				xmlUnlinkNode(pChild);
				DomOrphanAdd(pShell,pChild);
				pChild = pNext;
				continue;
			}
		}else if( pChild->type == XML_ELEMENT_NODE ){
			xmlAttrPtr pAttr;
			DomNormalizeTree(pShell,pChild);
			for( pAttr = pChild->properties ; pAttr ; pAttr = pAttr->next ){
				DomNormalizeTree(pShell,(xmlNodePtr)pAttr);
			}
		}else if( pChild->type == XML_ATTRIBUTE_NODE ){
			/* Unreachable from a tree walk (attributes are not children), but
			 * php's switch states it and a fragment/DTD shape could reach it. */
			DomNormalizeTree(pShell,pChild);
		}
		pChild = pChild->next;
	}
}
/* DOMDocument::normalizeDocument(): void and DOMNode::normalize(): void -- php
 * runs the same walk from the receiver, so the two share one body. */
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
/* DOMNode::getNodePath(): ?string -- the XPath that selects this node, or null
 * for one that is not addressable at all (anything under a fragment). php hands
 * libxml's answer straight back, positional predicate and all. */
DOM_METHOD(vm_builtin_DOMNode_getNodePath)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlChar *zPath = pNd ? xmlGetNodePath((xmlNodePtr)pNd->pNode) : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( zPath == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)zPath,-1);
	xmlFree(zPath);
	return PH7_OK;
}

/* ===== C14N ===== */

/*
 * php canonicalizes a NODE by handing libxml the node SET an XPath produces
 * from it -- `(.//. | .//@* | .//namespace::*)` with the node as context -- and
 * a DOCUMENT by handing it no set at all, which is how a document's top-level
 * comments reach the output where a node's cannot. Running a VISIBILITY
 * callback instead (the shape this file had) is close but not the same: an
 * ATTRIBUTE canonicalizes to its own ` b="2"` under php, where a "keep the
 * target's subtree" callback answers the empty string.
 *
 * All four of php's parameters are read here. `$exclusive` picks Exclusive
 * C14N, `$withComments` keeps comments, `$xpath` REPLACES the default node set
 * with the caller's query (and may register prefixes for it), and `$nsPrefixes`
 * lists the namespace prefixes an exclusive canonicalization must declare even
 * where they are unused. Only the two bools were honoured before, so
 * `C14N(true)` -- the mode every XML-DSig signer asks for -- silently
 * canonicalized inclusively and produced bytes that will not verify.
 */

/* The `namespaces` sub-array of `$xpath`: prefix => URI, string pairs only. */
static int DomC14NRegisterNs(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	xmlXPathContextPtr pXCtx = (xmlXPathContextPtr)pUserData;
	if( pKey && pVal && ph7_value_is_string(pKey) && ph7_value_is_string(pVal) ){
		xmlXPathRegisterNs(pXCtx,(const xmlChar *)ph7_value_to_string(pKey,0),
			(const xmlChar *)ph7_value_to_string(pVal,0));
	}
	return PH7_OK;
}
/*
 * The `$nsPrefixes` list, collected into the NULL-terminated array libxml
 * wants. Non-string entries are skipped, exactly as php skips them.
 *
 * The bytes are COPIED. ph7_array_walk hands its callback a temporary copy of
 * each value and releases it the moment the callback returns, so keeping the
 * pointer leaves a dangling one -- which libxml then compares against real
 * prefixes and matches at random, so `C14N(true,false,null,['u','p'])` declared
 * whichever prefix the freed memory happened to still read as.
 */
typedef struct DomC14NPrefixes DomC14NPrefixes;
struct DomC14NPrefixes {
	SyBlob sPool;    /* the prefix bytes, NUL-terminated one after another */
	SySet aOfs;      /* each prefix's offset into sPool */
	xmlChar **apPrefix;
};
static int DomC14NCollectPrefix(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	DomC14NPrefixes *pList = (DomC14NPrefixes *)pUserData;
	SXUNUSED(pKey);
	if( pVal && ph7_value_is_string(pVal) ){
		sxu32 nOfs = SyBlobLength(&pList->sPool);
		int nByte = 0;
		const char *zVal = ph7_value_to_string(pVal,&nByte);
		SySetPut(&pList->aOfs,(const void *)&nOfs);
		SyBlobAppend(&pList->sPool,zVal,(sxu32)nByte);
		SyBlobAppend(&pList->sPool,"",1);
	}
	return PH7_OK;
}
/*
 * Canonicalize the receiver into *pzOut (xmlFree'd by the caller) and answer
 * its byte count, or -1 when php answers false/"" instead. *pRc carries a
 * refusal php raises before anything is written.
 *
 * iXPathPos is the 1-based position of `$xpath` in the CALLING method's
 * parameter list: 3 on C14N, 4 on C14NFile, and php's messages print it.
 */
static int DomC14NRun(ph7_context *pCtx,int nArg,ph7_value **apArg,int iXPathPos,
	const char *zFn,xmlChar **pzOut,int *pRc)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	int iFirst = iXPathPos - 3;   /* index of $exclusive */
	int bExclusive = nArg > iFirst && ph7_value_to_bool(apArg[iFirst]);
	int bComments = nArg > iFirst+1 && ph7_value_to_bool(apArg[iFirst+1]);
	ph7_value *pXPath = (nArg > iFirst+2 && ph7_value_is_array(apArg[iFirst+2]))
		? apArg[iFirst+2] : 0;
	ph7_value *pPrefixes = (nArg > iFirst+3 && ph7_value_is_array(apArg[iFirst+3]))
		? apArg[iFirst+3] : 0;
	DomC14NPrefixes sPrefixes;
	xmlXPathContextPtr pXCtx = 0;
	xmlXPathObjectPtr pXObj = 0;
	xmlNodeSetPtr pSet = 0;
	sxu32 nMark,n;
	int nOut;
	*pzOut = 0;
	sPrefixes.apPrefix = 0;
	SyBlobInit(&sPrefixes.sPool,&pVm->sAllocator);
	SySetInit(&sPrefixes.aOfs,&pVm->sAllocator,sizeof(sxu32));
	if( pNode == 0 || pNode->doc == 0 ){
		nOut = -1;
		goto done;
	}
	if( pXPath ){
		ph7_value *pQuery = ph7_array_fetch(pXPath,"query",(int)sizeof("query")-1);
		ph7_value *pNs;
		if( pQuery == 0 ){
			*pRc = PH7_VmThrowException(pCtx,"ValueError",
				"%s(): Argument #%d ($xpath) must have a \"query\" key",zFn,iXPathPos);
			nOut = -1;
			goto done;
		}
		if( !ph7_value_is_string(pQuery) ){
			*pRc = PH7_VmThrowException(pCtx,"TypeError",
				"%s(): Argument #%d ($xpath) \"query\" option must be a string, %s given",
				zFn,iXPathPos,ph7_type_name(pQuery));
			nOut = -1;
			goto done;
		}
		pXCtx = xmlXPathNewContext(pNode->doc);
		if( pXCtx == 0 ){
			nOut = -1;
			goto done;
		}
		pXCtx->node = pNode;
		pNs = ph7_array_fetch(pXPath,"namespaces",(int)sizeof("namespaces")-1);
		if( pNs && ph7_value_is_array(pNs) ){
			ph7_array_walk(pNs,DomC14NRegisterNs,pXCtx);
		}
		nMark = PH7_LibxmlCaptureBegin(pVm);
		pXObj = xmlXPathEvalExpression((const xmlChar *)ph7_value_to_string(pQuery,0),pXCtx);
		/* php lets libxml's own complaint out first ("Invalid expression"), THEN
		 * raises its refusal, so the queue is flushed rather than dropped. */
		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
		pXCtx->node = 0;
	}else if( pNode->type != XML_DOCUMENT_NODE ){
		pXCtx = xmlXPathNewContext(pNode->doc);
		if( pXCtx == 0 ){
			nOut = -1;
			goto done;
		}
		pXCtx->node = pNode;
		nMark = PH7_LibxmlCaptureBegin(pVm);
		pXObj = xmlXPathEvalExpression(
			(const xmlChar *)"(.//. | .//@* | .//namespace::*)",pXCtx);
		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
		pXCtx->node = 0;
	}
	if( pXCtx ){
		if( pXObj == 0 || pXObj->type != XPATH_NODESET ){
			*pRc = PH7_VmThrowException(pCtx,"Error","XPath query did not return a nodeset");
			nOut = -1;
			goto done;
		}
		pSet = pXObj->nodesetval;
	}
	/* php reads `$nsPrefixes` only AFTER the query has been resolved, so a bad
	 * query's refusal reaches the caller with no notice in front of it. */
	if( pPrefixes ){
		if( bExclusive ){
			ph7_array_walk(pPrefixes,DomC14NCollectPrefix,&sPrefixes);
			n = SySetUsed(&sPrefixes.aOfs);
			if( n > 0 ){
				sPrefixes.apPrefix = (xmlChar **)SyMemBackendAlloc(&pVm->sAllocator,
					(sxu32)((n+1)*sizeof(xmlChar *)));
				if( sPrefixes.apPrefix == 0 ){
					nOut = -1;
					goto done;
				}
				/* Offsets, not pointers, until the pool has stopped growing. */
				for( n = 0 ; n < SySetUsed(&sPrefixes.aOfs) ; ++n ){
					sPrefixes.apPrefix[n] = (xmlChar *)SyBlobData(&sPrefixes.sPool)
						+ ((sxu32 *)SySetBasePtr(&sPrefixes.aOfs))[n];
				}
				sPrefixes.apPrefix[n] = 0;
			}
		}else{
			/* php's E_NOTICE, and the list is then ignored outright. */
			ph7_context_throw_error(pCtx,PH7_CTX_NOTICE,
				"Inclusive namespace prefixes only allowed in exclusive mode.");
		}
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	nOut = xmlC14NDocDumpMemory(pNode->doc,pSet,
		bExclusive ? XML_C14N_EXCLUSIVE_1_0 : XML_C14N_1_0,
		sPrefixes.apPrefix,bComments,pzOut);
	PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
	if( nOut < 0 && *pzOut ){
		xmlFree(*pzOut);
		*pzOut = 0;
	}
done:
	if( pXObj ){
		xmlXPathFreeObject(pXObj);
	}
	if( pXCtx ){
		xmlXPathFreeContext(pXCtx);
	}
	if( sPrefixes.apPrefix ){
		SyMemBackendFree(&pVm->sAllocator,(void *)sPrefixes.apPrefix);
	}
	SyBlobRelease(&sPrefixes.sPool);
	SySetRelease(&sPrefixes.aOfs);
	return nOut;
}
/*
 * DOMNode::C14N(bool $exclusive = false, bool $withComments = false,
 *               ?array $xpath = null, ?array $nsPrefixes = null): string|false
 *
 * The empty string on canonicalization failure, which is what php answers for
 * a detached node or a fragment: nothing of it is visible from the document.
 */
DOM_METHOD(vm_builtin_DOMNode_C14N)
{
	xmlChar *zOut = 0;
	int rc = PH7_OK;
	int nOut = DomC14NRun(pCtx,nArg,apArg,3,"DOMNode::C14N",&zOut,&rc);
	if( rc != PH7_OK ){
		return rc;
	}
	if( nOut < 0 ){
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)zOut,nOut);
	xmlFree(zOut);
	return PH7_OK;
}
/*
 * DOMNode::C14NFile(string $uri, bool $exclusive = false,
 *                   bool $withComments = false, ?array $xpath = null,
 *                   ?array $nsPrefixes = null): int|false
 *
 * The same canonicalization written to a destination instead of answered, and
 * the byte count rather than the bytes. The destination goes through the stream
 * layer -- php's libxml I/O is wired to php's streams, so `php://stdout` and a
 * userland wrapper are both valid here -- which is also where php's
 * "Failed to open stream" warning comes from.
 */
DOM_METHOD(vm_builtin_DOMNode_C14NFile)
{
	ph7_vm *pVm = pCtx->pVm;
	const ph7_io_stream *pStream;
	void *pHandle;
	xmlChar *zOut = 0;
	const char *zFile;
	int nFile = 0,nOut,rc = PH7_OK;
	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";
	if( nFile != (int)SyStrlen(zFile) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMNode::C14NFile(): Argument #1 ($uri) must not contain any null bytes");
	}
	if( nFile < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError","Path must not be empty");
	}
	nOut = DomC14NRun(pCtx,nArg,apArg,4,"DOMNode::C14NFile",&zOut,&rc);
	if( rc != PH7_OK ){
		return rc;
	}
	if( nOut < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);
	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,
		PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,
		ph7_function_name(pCtx)) : 0;
	if( pHandle == 0 ){
		xmlFree(zOut);
		VfsThrowOpenWarning(pCtx,zFile);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){
		nOut = -1;
	}
	PH7_StreamCloseHandle(pStream,pHandle);
	xmlFree(zOut);
	if( nOut < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_int(pCtx,nOut);
	return PH7_OK;
}
/*
 * DOMNode::__sleep(): array and DOMNode::__wakeup(): void
 *
 * php declares both on DOMNode and both do one thing: refuse. They are the
 * MECHANISM behind the refusal, not decoration -- `serialize()` finds `__sleep`
 * and `unserialize()` calls `__wakeup`, which is why a subclass that declares
 * its own escapes both. Without them, PHL refused serialize() from its own deny
 * handler (same sentence) but UNSERIALIZE went through in silence and handed
 * back a DOM object with no node behind it, which then answered nothing for
 * every property a program read off it.
 */
DOM_METHOD(vm_builtin_DOMNode_sleep)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx,"Exception",
		"Serialization of '%z' is not allowed, unless serialization methods "
		"are implemented in a subclass",&pThis->pClass->sName);
}
DOM_METHOD(vm_builtin_DOMNode_wakeup)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx,"Exception",
		"Unserialization of '%z' is not allowed, unless unserialization methods "
		"are implemented in a subclass",&pThis->pClass->sName);
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
/* php exposes `length` on both collections as a virtual property; the
 * DOM_PROP_ACCESSORS pair below turns each recognizer into __get + __isset. */
static int DomListProp(ph7_context *pCtx,const char *zName)
{
	if( DomNameIs(zName,"length") ){
		ph7_result_int(pCtx,DomListCount(PH7_ContextThis(pCtx)));
		return 1;
	}
	return 0;
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
static int DomMapProp(ph7_context *pCtx,const char *zName)
{
	if( DomNameIs(zName,"length") ){
		ph7_result_int(pCtx,DomMapCount(PH7_ContextThis(pCtx)));
		return 1;
	}
	return 0;
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
	}else if( DomNameIs(zName,"parentElement") ){
		/* php's `?DOMElement`: the parent when it IS an element, so a root
		 * element (whose parent is the document) answers null. An ATTRIBUTE
		 * answers its element -- libxml parents an attribute, and php reports
		 * that parent from both this property and `parentNode`. */
		xmlNodePtr pPar = pNode ? pNode->parent : 0;
		DomResultNodeOf(pCtx,pNd,(pPar && pPar->type == XML_ELEMENT_NODE) ? pPar : 0);
	}else if( DomNameIs(zName,"namespaceURI") ){
		DomNamespaceUri(pCtx,pNode);
	}else if( DomNameIs(zName,"prefix") ){
		DomPrefix(pCtx,pNode);
	}else if( DomNameIs(zName,"localName") ){
		DomLocalName(pCtx,pNode);
	}else if( DomNameIs(zName,"isConnected") ){
		ph7_result_bool(pCtx,DomIsConnected(pNode));
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
/*
 * The __get/__isset/__set trio every DOM class carries.
 *
 * Both ride ONE recognizer per class -- the DomProp_X readers below, which
 * answer 1 when the name is a property of that class and have written its
 * value, and 0 when it is not.  php models these as real (virtual) properties,
 * so the 0 case is its `Undefined property` WARNING rather than a silent null,
 * and `isset()` is php's own has_property: the name has to exist AND read back
 * non-null (which is what makes `isset($n->nextSibling)` false on a last child
 * while `isset($n->nodeName)` is true).  Without the __isset half every
 * `isset($doc->documentElement)` and every `$node->attributes ?? []` answered
 * as though the whole surface were absent.
 */
static int DomUndefProp(ph7_context *pCtx,const char *zName)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_result_null(pCtx);
	if( pThis ){
		/* php names the INSTANCE's class, so a userland subclass of DOMElement
		 * is reported under its own name. */
		SyBlob sMsg;
		SyString sName;
		SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
		SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
		SyBlobFormat(&sMsg,"Undefined property: %z::$%z",&pThis->pClass->sName,&sName);
		SyBlobNullAppend(&sMsg);
		PH7_VmThrowError(pCtx->pVm,0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
		SyBlobRelease(&sMsg);
	}
	return PH7_OK;
}
/*
 * The write half. A per-class WRITER answers one of these; the name it does
 * not write is looked up in the class's READER, which decides between php's
 * two refusals -- a property that exists is read-only, one that does not is a
 * dynamic property (deprecated in php 8.2, so §10 rejects it here, which is
 * what the engine's own store path would have said had the class carried no
 * __set at all).
 */
#define DOM_SET_UNKNOWN  0   /* not a property of this class */
#define DOM_SET_DONE     1   /* written, or a refusal already raised into *pRc */
static int DomRefuseWrite(ph7_context *pCtx,const char *zName,int bKnown)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SyString sName;
	if( pThis == 0 ){
		return PH7_OK;
	}
	SyStringInitFromBuf(&sName,zName,SyStrlen(zName));
	return PH7_VmThrowException(pCtx,"Error",
		bKnown ? "Cannot modify readonly property %z::$%z"
		       : "Cannot create dynamic property %z::$%z",
		&pThis->pClass->sName,&sName);
}
#define DOM_PROP_ACCESSORS(CLS,READER,WRITER)                                   \
	DOM_METHOD(vm_builtin_##CLS##_get)                                          \
	{                                                                           \
		const char *zName = DomGetName(nArg,apArg);                             \
		if( READER(pCtx,zName) == 0 ){                                          \
			return DomUndefProp(pCtx,zName);                                    \
		}                                                                       \
		return PH7_OK;                                                          \
	}                                                                           \
	DOM_METHOD(vm_builtin_##CLS##_isset)                                        \
	{                                                                           \
		int bKnown = READER(pCtx,DomGetName(nArg,apArg)) != 0;                  \
		int bNull = (pCtx->pRet->iFlags & MEMOBJ_NULL) != 0;                    \
		ph7_result_bool(pCtx,bKnown && !bNull);                                 \
		return PH7_OK;                                                          \
	}                                                                           \
	DOM_METHOD(vm_builtin_##CLS##_set)                                          \
	{                                                                           \
		const char *zName = DomGetName(nArg,apArg);                             \
		int rc = PH7_OK;                                                        \
		if( WRITER(pCtx,zName,nArg > 1 ? apArg[1] : 0,&rc) == DOM_SET_DONE ){   \
			return rc;                                                          \
		}                                                                       \
		return DomRefuseWrite(pCtx,zName,READER(pCtx,zName) != 0);              \
	}
/* DOMDocument adds documentElement. */
static int DomDocProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd;
	if( DomNameIs(zName,"documentElement") ){
		pNd = DomThisNode(pCtx);
		DomResultNodeOf(pCtx,pNd,pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0);
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
/* DOMElement adds tagName, and the two attribute-backed names php exposes as
 * properties: `className` IS the class attribute and `id` IS the id one, both
 * answering "" when the attribute is absent. */
static int DomElemProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd;
	int bClass = DomNameIs(zName,"className");
	if( DomNameIs(zName,"tagName") ){
		pNd = DomThisNode(pCtx);
		DomNodeName(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);
		return 1;
	}
	if( bClass || DomNameIs(zName,"id") ){
		xmlChar *zVal;
		pNd = DomThisNode(pCtx);
		zVal = pNd ? xmlGetNoNsProp((xmlNodePtr)pNd->pNode,
			(const xmlChar *)(bClass ? "class" : "id")) : 0;
		ph7_result_string(pCtx,zVal ? (const char *)zVal : "",-1);
		if( zVal ){
			xmlFree(zVal);
		}
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
/* DOMAttr adds name/value/ownerElement. */
static int DomAttrProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( DomNameIs(zName,"name") ){
		DomNodeName(pCtx,pNode);
		return 1;
	}
	if( DomNameIs(zName,"value") ){
		DomNodeValue(pCtx,pNode);
		return 1;
	}
	if( DomNameIs(zName,"ownerElement") ){
		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);
		return 1;
	}
	return DomNodeProp(pCtx,zName);
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
static int DomCharProp(ph7_context *pCtx,const char *zName)
{
	if( DomCharDataProp(pCtx,zName) ){
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
static int DomTextProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd;
	if( DomNameIs(zName,"wholeText") ){
		pNd = DomThisNode(pCtx);
		DomNodeValue(pCtx,pNd ? (xmlNodePtr)pNd->pNode : 0);
		return 1;
	}
	return DomCharProp(pCtx,zName);
}
/*
 * php's typed-property store for the DOM's own string-shaped properties: a
 * scalar coerces, null is accepted only where the declared type is nullable
 * (and means the empty string), and an array or an object is a TypeError
 * naming the class that DECLARES the property rather than the one the write
 * went through. The value is coerced through a COPY -- ph7_value_to_string()
 * converts the object it is handed, and that object is the caller's own
 * `$v` in `$node->nodeValue = $v`.
 */
static int DomWriteText(ph7_context *pCtx,const char *zOwner,const char *zProp,
	const char *zType,ph7_value *pVal,SyBlob *pOut,int *pRc)
{
	int bNullable = zType[0] == '?';
	ph7_value sTmp;
	if( pVal == 0
	 || (pVal->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ)) != 0
	 || ((pVal->iFlags & MEMOBJ_NULL) != 0 && !bNullable) ){
		char zBuf[128];
		const char *zGiven = "null";
		if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){
			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;
			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sName);
			zGiven = zBuf;
		}else if( pVal ){
			zGiven = ph7_type_name(pVal);
		}
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"Cannot assign %s to property %s::$%s of type %s",zGiven,zOwner,zProp,zType);
		return 0;
	}
	SyBlobInit(pOut,&pCtx->pVm->sAllocator);
	if( (pVal->iFlags & MEMOBJ_NULL) == 0 ){
		PH7_MemObjInit(pCtx->pVm,&sTmp);
		PH7_MemObjLoad(pVal,&sTmp);
		PH7_MemObjToString(&sTmp);
		SyBlobAppend(pOut,SyBlobData(&sTmp.sBlob),SyBlobLength(&sTmp.sBlob));
		PH7_MemObjRelease(&sTmp);
	}
	SyBlobNullAppend(pOut);
	return 1;
}
/* Has this node ever been handed to PHP? The identity cache is the record. */
static int DomIsWrapped(ph7_vm *pVm,ph7_class_instance *pDoc,xmlNodePtr pNode)
{
	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);
	ph7_hashmap_node *pEntry = 0;
	ph7_value sKey;
	int bHit;
	if( pCache == 0 ){
		return 0;
	}
	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);
	bHit = PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry != 0;
	PH7_MemObjRelease(&sKey);
	return bHit;
}
/*
 * php FREES the subtree a content write replaces -- except the nodes a PHP
 * variable still holds a wrapper for, which it unlinks and keeps as roots of
 * their own detached fragments. That is observable: after
 * `$el->textContent = 'flat'`, a variable holding a grandchild still reads its
 * text and answers NULL for `parentNode`. Parking the subtree whole (which is
 * what this engine must do -- a wrapper's handle is a raw pointer, so nothing
 * here is ever freed before the document is) left every such parent attached,
 * so the same variable answered its old parent's name. Detach exactly the nodes
 * php would have kept: the OUTERMOST wrapped ones, php's own rule, since it
 * stops recursing at a node it is keeping.
 */
static void DomPartWrapped(ph7_vm *pVm,ph7_class_instance *pDoc,phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNodePtr pChild = pNode ? pNode->children : 0;
	while( pChild ){
		xmlNodePtr pNext = pChild->next;
		if( DomIsWrapped(&(*pVm),pDoc,pChild) ){
			xmlUnlinkNode(pChild);
			DomOrphanAdd(pShell,pChild);
		}else{
			DomPartWrapped(&(*pVm),pDoc,pShell,pChild);
		}
		pChild = pNext;
	}
}
/*
 * Everything a content write has to do before libxml sees it: the node's
 * children go to the document's ORPHAN set rather than being freed, because a
 * PHP variable may still hold a wrapper for one of them and the wrapper's
 * handle is a raw pointer. (php keeps such a node alive through its own
 * wrapper refcount; this engine parks it, exactly as removeChild does.)
 */
static void DomDropChildren(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode)
{
	ph7_class_instance *pDoc = DomThisDoc(pCtx);
	xmlNodePtr pChild = pNode ? pNode->children : 0;
	while( pChild ){
		xmlNodePtr pNext = pChild->next;
		if( !DomIsWrapped(pCtx->pVm,pDoc,pChild) ){
			DomPartWrapped(pCtx->pVm,pDoc,pShell,pChild);
		}
		xmlUnlinkNode(pChild);
		DomOrphanAdd(pShell,pChild);
		pChild = pNext;
	}
}
/*
 * php's two content writes, which are NOT the same write.
 *
 * `nodeValue` is libxml's xmlNodeSetContent, and on an element or an attribute
 * that PARSES entity references: `$el->nodeValue = 'a&b'` is libxml's
 * "unterminated entity reference" and leaves the node EMPTY, while
 * `'a&amp;b'` stores the one character. `textContent` sets one raw text child
 * instead, so the same two strings store what they say. Every other node kind
 * takes its content literally either way.
 */
static void DomSetContent(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode,
	const char *zText,int bParseEntities)
{
	int bTree = pNode->type == XML_ELEMENT_NODE || pNode->type == XML_ATTRIBUTE_NODE;
	xmlNodePtr pText;
	DomDropChildren(pCtx,pShell,pNode);
	if( !bTree || (bParseEntities && zText[0]) ){
		/* The parsing write is the one that can FAIL -- an unterminated entity
		 * reference leaves the node empty and libxml says so. Route that through
		 * the per-VM queue like every other libxml diagnostic here, or it prints
		 * itself on stderr past error_reporting(), past `@`, and past
		 * libxml_get_errors(). */
		SyBlob sFn;
		sxu32 nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);
		xmlNodeSetContent(pNode,(const xmlChar *)zText);
		/* php attributes the warning to the CALLER's scope -- a property write
		 * is not a call, so there is no accessor name to print. */
		SyBlobInit(&sFn,&pCtx->pVm->sAllocator);
		PH7_VmActiveFuncName(pCtx->pVm,&sFn);
		PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,(const char *)SyBlobData(&sFn));
		SyBlobRelease(&sFn);
		return;
	}
	/* One raw text child -- and php leaves one even for the EMPTY string, which
	 * is why `$el->nodeValue = ''` serializes as <r></r> rather than <r/>.
	 * (The entity-parsing write is the exception: a string libxml refuses, like
	 * `'a&b'`, leaves the element with no children at all.) */
	pText = xmlNewDocText(pNode->doc,(const xmlChar *)zText);
	if( pText ){
		DomLinkLast(pNode,pText);
	}
}
/* DOMNode's three writable properties. */
static int DomSetNodeProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	int bValue = DomNameIs(zName,"nodeValue");
	SyBlob sVal;
	if( bValue || DomNameIs(zName,"textContent") ){
		if( DomWriteText(pCtx,"DOMNode",bValue ? "nodeValue" : "textContent",
			bValue ? "?string" : "string",pVal,&sVal,pRc) == 0 ){
			return DOM_SET_DONE;
		}
		/* php leaves a DOCUMENT alone: its nodeValue is null and stays null. */
		if( pNode && pNode->type != XML_DOCUMENT_NODE && pNode->type != XML_HTML_DOCUMENT_NODE ){
			DomSetContent(pCtx,pNd->pShell,pNode,(const char *)SyBlobData(&sVal),bValue);
		}
		SyBlobRelease(&sVal);
		return DOM_SET_DONE;
	}
	if( DomNameIs(zName,"prefix") ){
		xmlNsPtr pNs;
		xmlNodePtr pDecl;
		if( DomWriteText(pCtx,"DOMNode","prefix","string",pVal,&sVal,pRc) == 0 ){
			return DOM_SET_DONE;
		}
		/* Only a node that HAS a namespace can be re-prefixed; php ignores the
		 * write for anything else, including an element in no namespace. */
		if( DomHasNsSlot(pNode) && pNode->ns && pNode->ns->href ){
			const char *zPrefix = (const char *)SyBlobData(&sVal);
			/* An attribute's declaration goes on its ELEMENT. */
			pDecl = pNode->type == XML_ATTRIBUTE_NODE ? pNode->parent : pNode;
			if( DomNameIs(zPrefix,"xml")
			 && !DomNameIs((const char *)pNode->ns->href,
				"http://www.w3.org/XML/1998/namespace") ){
				/* php's reserved-prefix refusal: `xml` may only name ITS namespace. */
				SyBlobRelease(&sVal);
				*pRc = PH7_VmThrowException(pCtx,"DOMException","Namespace Error");
				return DOM_SET_DONE;
			}
			/* php looks only at the declarations THIS node carries -- an
			 * ancestor's is not reused, which is why re-prefixing a child grows
			 * a second `xmlns:q` beside the one its parent already has. */
			for( pNs = pDecl ? pDecl->nsDef : 0 ; pNs ; pNs = pNs->next ){
				const char *zHave = pNs->prefix ? (const char *)pNs->prefix : "";
				if( DomNameIs(zHave,zPrefix) && pNs->href
				 && xmlStrEqual(pNs->href,pNode->ns->href) ){
					break;
				}
			}
			if( pNs == 0 ){
				/* None binds this prefix to the node's own URI: php declares one,
				 * which is how `$el->prefix = ''` grows an `xmlns="..."` on the
				 * element itself -- and how a prefix already bound HERE to another
				 * URI becomes libxml's refusal and php's Namespace Error. */
				pNs = pDecl ? xmlNewNs(pDecl,pNode->ns->href,
					zPrefix[0] ? (const xmlChar *)zPrefix : 0) : 0;
			}
			if( pNs == 0 ){
				SyBlobRelease(&sVal);
				*pRc = PH7_VmThrowException(pCtx,"DOMException","Namespace Error");
				return DOM_SET_DONE;
			}
			xmlSetNs(pNode,pNs);
		}
		SyBlobRelease(&sVal);
		return DOM_SET_DONE;
	}
	return DOM_SET_UNKNOWN;
}
/* DOMElement adds className and id, both of them ATTRIBUTES under the name. */
static int DomSetElemProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	int bClass = DomNameIs(zName,"className");
	SyBlob sVal;
	if( bClass || DomNameIs(zName,"id") ){
		if( DomWriteText(pCtx,"DOMElement",bClass ? "className" : "id","string",
			pVal,&sVal,pRc) == 0 ){
			return DOM_SET_DONE;
		}
		if( pNd ){
			xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)(bClass ? "class" : "id"),
				(const xmlChar *)SyBlobData(&sVal));
		}
		SyBlobRelease(&sVal);
		return DOM_SET_DONE;
	}
	return DomSetNodeProp(pCtx,zName,pVal,pRc);
}
/* DOMAttr::value and DOMCharacterData::data are the node's own content. The
 * attribute's parses entity references, as its `nodeValue` does -- only
 * `textContent` takes an attribute's bytes literally; character data has no
 * parsing write at all, whichever name it is written under. */
static int DomSetContentProp(ph7_context *pCtx,const char *zOwner,const char *zProp,
	const char *zName,ph7_value *pVal,int bParseEntities,int *pRc)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	SyBlob sVal;
	if( !DomNameIs(zName,zProp) ){
		return DOM_SET_UNKNOWN;
	}
	if( DomWriteText(pCtx,zOwner,zProp,"string",pVal,&sVal,pRc) == 0 ){
		return DOM_SET_DONE;
	}
	if( pNd ){
		DomSetContent(pCtx,pNd->pShell,(xmlNodePtr)pNd->pNode,
			(const char *)SyBlobData(&sVal),bParseEntities);
	}
	SyBlobRelease(&sVal);
	return DOM_SET_DONE;
}
static int DomSetAttrProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	int rc = DomSetContentProp(pCtx,"DOMAttr","value",zName,pVal,TRUE,pRc);
	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);
}
static int DomSetCharProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	int rc = DomSetContentProp(pCtx,"DOMCharacterData","data",zName,pVal,FALSE,pRc);
	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);
}
static int DomSetPiProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	int rc = DomSetContentProp(pCtx,"DOMProcessingInstruction","data",zName,pVal,FALSE,pRc);
	return rc != DOM_SET_UNKNOWN ? rc : DomSetNodeProp(pCtx,zName,pVal,pRc);
}
/* The two collections and the document have nothing writable of their own yet;
 * `length` is read-only and the document's own directives are §4's next slice. */
static int DomSetNothing(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	SXUNUSED(pCtx); SXUNUSED(zName); SXUNUSED(pVal); SXUNUSED(pRc);
	return DOM_SET_UNKNOWN;
}
/* DOMProcessingInstruction adds target (its name) and data (its content) --
 * php declares it under DOMNode, not DOMCharacterData, so the character-data
 * methods are deliberately absent from it. */
static int DomPiProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( DomNameIs(zName,"target") ){
		ph7_result_string(pCtx,(pNode && pNode->name) ? (const char *)pNode->name : "",-1);
		return 1;
	}
	if( DomNameIs(zName,"data") ){
		DomNodeValue(pCtx,pNode);
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
DOM_PROP_ACCESSORS(DOMNodeList,DomListProp,DomSetNothing)
DOM_PROP_ACCESSORS(DOMNamedNodeMap,DomMapProp,DomSetNothing)
DOM_PROP_ACCESSORS(DOMNode,DomNodeProp,DomSetNodeProp)
DOM_PROP_ACCESSORS(DOMDocument,DomDocProp,DomSetNodeProp)
DOM_PROP_ACCESSORS(DOMElement,DomElemProp,DomSetElemProp)
DOM_PROP_ACCESSORS(DOMAttr,DomAttrProp,DomSetAttrProp)
DOM_PROP_ACCESSORS(DOMCharacterData,DomCharProp,DomSetCharProp)
DOM_PROP_ACCESSORS(DOMText,DomTextProp,DomSetCharProp)
DOM_PROP_ACCESSORS(DOMProcessingInstruction,DomPiProp,DomSetPiProp)
/*
 * DOMDocumentFragment::appendXML(string $data): bool
 *
 * php parses the chunk as a well-balanced FRAGMENT (no single root required,
 * bare text allowed) and appends what it produced; anything libxml refuses is
 * `false` with nothing appended.
 */
DOM_METHOD(vm_builtin_DOMDocumentFragment_appendXML)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zXml = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlNodePtr pFrag,pList = 0;
	sxu32 nMark;
	int rc;
	if( pNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pFrag = (xmlNodePtr)pNd->pNode;
	nMark = PH7_LibxmlCaptureBegin(pCtx->pVm);
	rc = xmlParseBalancedChunkMemory(pFrag->doc,0,0,0,(const xmlChar *)zXml,&pList);
	PH7_LibxmlCaptureEnd(pCtx->pVm,nMark,"DOMDocumentFragment::appendXML");
	if( rc != 0 ){
		if( pList ){
			xmlFreeNodeList(pList);
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	while( pList ){
		xmlNodePtr pNext = pList->next;
		pList->next = pList->prev = 0;
		DomLinkLast(pFrag,pList);
		pList = pNext;
	}
	ph7_result_bool(pCtx,1);
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
		/* php declares no return type at all on this one, not even a tentative
		 * one, so the row states none either. */
		{ "cloneNode",      PH7_MOD_PUBLIC, "bool $deep = false", "", vm_builtin_DOMNode_cloneNode },
		/* php runs the same walk normalizeDocument() does, from the receiver. */
		{ "normalize",      PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },
		{ "getNodePath",    PH7_MOD_PUBLIC, "", "@?string", vm_builtin_DOMNode_getNodePath },
		{ "isEqualNode",    PH7_MOD_PUBLIC, "?DOMNode $otherNode", "bool",
		  vm_builtin_DOMNode_isEqualNode },
		{ "isSupported",    PH7_MOD_PUBLIC, "string $feature, string $version", "@bool",
		  vm_builtin_DOMNode_isSupported },
		/* php's declared type names DOMNameSpaceNode, a class PHL does not have;
		 * the row states it anyway so Reflection reports php's, and nothing can
		 * be handed one. (php's own zpp rejects a NON-object here with a
		 * "?object" message instead -- PLAN §7.4, the error-format class.) */
		{ "contains",       PH7_MOD_PUBLIC, "DOMNode|DOMNameSpaceNode|null $other", "bool",
		  vm_builtin_DOMNode_contains },
		{ "getRootNode",    PH7_MOD_PUBLIC, "?array $options = null", "DOMNode",
		  vm_builtin_DOMNode_getRootNode },
		{ "compareDocumentPosition", PH7_MOD_PUBLIC, "DOMNode $other", "int",
		  vm_builtin_DOMNode_compareDocumentPosition },
		{ "getLineNo",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNode_getLineNo },
		{ "C14N",           PH7_MOD_PUBLIC,
		  "bool $exclusive = false, bool $withComments = false, ?array $xpath = null, "
		  "?array $nsPrefixes = null", "@string|false", vm_builtin_DOMNode_C14N },
		{ "C14NFile",       PH7_MOD_PUBLIC,
		  "string $uri, bool $exclusive = false, bool $withComments = false, "
		  "?array $xpath = null, ?array $nsPrefixes = null", "@int|false",
		  vm_builtin_DOMNode_C14NFile },
		/* The refusal MACHINERY, not decoration: serialize() finds __sleep and
		 * unserialize() calls __wakeup, so a subclass declaring either escapes. */
		{ "__sleep",        PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },
		{ "__wakeup",       PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },
		{ "lookupNamespaceURI", PH7_MOD_PUBLIC, "?string $prefix", "@?string",
		  vm_builtin_DOMNode_lookupNamespaceURI },
		{ "lookupPrefix",   PH7_MOD_PUBLIC, "string $namespace", "@?string",
		  vm_builtin_DOMNode_lookupPrefix },
		{ "isDefaultNamespace", PH7_MOD_PUBLIC, "string $namespace", "@bool",
		  vm_builtin_DOMNode_isDefaultNamespace },
		{ "__get",          PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNode_get },
		{ "__isset",        PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNode_isset },
		{ "__set",          PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMNode_set },
	};
	/* php declares the six on DOMNode; every node class inherits them. */
	static const PH7_NativeConstDef aNodeConst[] = {
		{ "DOCUMENT_POSITION_DISCONNECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,
		  DOM_POS_DISCONNECTED, 0, 0.0 },
		{ "DOCUMENT_POSITION_PRECEDING", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,
		  DOM_POS_PRECEDING, 0, 0.0 },
		{ "DOCUMENT_POSITION_FOLLOWING", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,
		  DOM_POS_FOLLOWING, 0, 0.0 },
		{ "DOCUMENT_POSITION_CONTAINS", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,
		  DOM_POS_CONTAINS, 0, 0.0 },
		{ "DOCUMENT_POSITION_CONTAINED_BY", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,
		  DOM_POS_CONTAINED_BY, 0, 0.0 },
		{ "DOCUMENT_POSITION_IMPLEMENTATION_SPECIFIC", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT,
		  DOM_POS_IMPL_SPEC, 0, 0.0 },
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
		{ "createProcessingInstruction", PH7_MOD_PUBLIC, "string $target, string $data = ''", "",
		  vm_builtin_DOMDocument_createPI },
		{ "createEntityReference", PH7_MOD_PUBLIC, "string $name", "",
		  vm_builtin_DOMDocument_createEntityRef },
		{ "createDocumentFragment", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_DOMDocument_createFragment },
		{ "normalizeDocument",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },
		{ "schemaValidateSource", PH7_MOD_PUBLIC, "string $source, int $flags = 0", "@bool",
		  vm_builtin_DOMDocument_schemaValidateSource },
		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",
		  vm_builtin_Dom_getElementsByTagName },
		{ "__get",                PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMDocument_get },
		{ "__isset",              PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMDocument_isset },
		{ "__set",                PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMDocument_set },
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
		{ "__isset",              PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMElement_isset },
		{ "__set",                PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMElement_set },
	};
	static const PH7_NativeMethodDef aAttrMethod[] = {
		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMAttr_get },
		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMAttr_isset },
		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMAttr_set },
	};
	static const PH7_NativeMethodDef aCharMethod[] = {
		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMCharacterData_get },
		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMCharacterData_isset },
		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMCharacterData_set },
	};
	static const PH7_NativeMethodDef aPiMethod[] = {
		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMProcessingInstruction_get },
		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMProcessingInstruction_isset },
		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",
		  vm_builtin_DOMProcessingInstruction_set },
	};
	static const PH7_NativeMethodDef aFragMethod[] = {
		{ "appendXML", PH7_MOD_PUBLIC, "string $data", "@bool",
		  vm_builtin_DOMDocumentFragment_appendXML },
	};
	static const PH7_NativeMethodDef aTextMethod[] = {
		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMText_get },
		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMText_isset },
		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMText_set },
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
		{ "__isset",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNodeList_isset },
		{ "__set",       PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMNodeList_set },
	};
	static const PH7_NativeMethodDef aMapMethod[] = {
		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNamedNodeMap_count },
		{ "item",         PH7_MOD_PUBLIC, "int $index", "@?DOMNode", vm_builtin_DOMNamedNodeMap_item },
		{ "getNamedItem", PH7_MOD_PUBLIC, "string $qualifiedName", "@?DOMNode",
		  vm_builtin_DOMNamedNodeMap_getNamedItem },
		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
		{ "__get",        PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNamedNodeMap_get },
		{ "__isset",      PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNamedNodeMap_isset },
		{ "__set",        PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMNamedNodeMap_set },
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
	/* php refuses to serialize a NODE class, and its refusal is the soft kind: the
	 * deny handler sits behind the __serialize()/__sleep() lookup, so a subclass that
	 * declares either one is serialized normally and the sentence says so. DOMXPath's
	 * is the HARD kind — a subclass declaring __serialize() is refused there too — and
	 * DOMNodeList/DOMNamedNodeMap are not refused at all (`0:{}`), which is what they
	 * became once serialize() stopped emitting the hidden slot. Restating the flag on
	 * every row is rule 29: a native subclass does not inherit its parent's. */
	static const PH7_NativeClassSpec aSpec[] = {
		{ "DOMException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aNodeMethod, SX_ARRAYSIZE(aNodeMethod), aNodeConst, SX_ARRAYSIZE(aNodeConst),
		  aNodeProp, SX_ARRAYSIZE(aNodeProp), 0, 0, 0 },
		{ "DOMDocument", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aDocMethod, SX_ARRAYSIZE(aDocMethod), 0, 0, aDocProp, SX_ARRAYSIZE(aDocProp), 0, 0, 0 },
		{ "DOMElement", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aElemMethod, SX_ARRAYSIZE(aElemMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMAttr", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMCharacterData", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aCharMethod, SX_ARRAYSIZE(aCharMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMText", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aTextMethod, SX_ARRAYSIZE(aTextMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMComment", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMCdataSection", "DOMText", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		/* php declares the PI under DOMNode (its `data` is its own property, not
		 * DOMCharacterData's), the fragment and the entity reference plainly. */
		{ "DOMProcessingInstruction", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aPiMethod, SX_ARRAYSIZE(aPiMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMDocumentFragment", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aFragMethod, SX_ARRAYSIZE(aFragMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMEntityReference", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
		/* php's own two: IteratorAggregate (NOT Iterator -- the chunk had the
		 * list carry its own cursor) and Countable. */
		{ "DOMNodeList", 0, "IteratorAggregate,Countable", 0,
		  aListMethod, SX_ARRAYSIZE(aListMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),
		  0, &sDomListIterVtab, 0 },
		{ "DOMNamedNodeMap", 0, "IteratorAggregate,Countable", 0,
		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),
		  0, &sDomMapIterVtab, 0 },
		{ "DOMXPath", 0, 0, PH7_CLASS_NOSERIALIZE,
		  aXPathMethod, SX_ARRAYSIZE(aXPathMethod), 0, 0, aXPathProp, SX_ARRAYSIZE(aXPathProp), 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_dom_unused;
#endif /* PH7_ENABLE_LIBXML */
