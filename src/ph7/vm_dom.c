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
#include <libxml/encoding.h>
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>

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

/*
 * The DOCUMENT's own directives: php's seven boolean properties, real slots
 * here (as `preserveWhiteSpace` and `formatOutput` already were) because their
 * value is the extension's own state and not a question about the tree.  Four
 * of them are read by every parse and one by every refusal, and a clone of a
 * document carries the whole block across rather than resetting it to the class
 * defaults.
 */
static const char * const azDomDocFlag[] = {
	"preserveWhiteSpace", "formatOutput", "validateOnParse",
	"resolveExternals", "substituteEntities", "recover", "strictErrorChecking"
};

/*
 * php's DOMException carries the DOM level-2 error CODE beside its sentence --
 * `catch (DOMException $e) { if ($e->getCode() === DOM_NOT_FOUND_ERR) ... }` is
 * how a caller tells one refusal from another, and the sentence is only a
 * sentence.  Every throw below states its code; DOM_PHP_ERR (0) is php's own
 * "not a DOM error" and no refusal here uses it.
 */
#define DOM_ERR_INDEX_SIZE     1
#define DOM_ERR_HIERARCHY      3
#define DOM_ERR_WRONG_DOC      4
#define DOM_ERR_INVALID_CHAR   5
#define DOM_ERR_NOT_FOUND      8
#define DOM_ERR_NOT_SUPPORTED  9
#define DOM_ERR_INVALID_STATE 11
#define DOM_ERR_NAMESPACE     14
/* The sentence php prints for each -- so a refusal that travels as a code can
 * be raised from one place. */
static const char * DomErrText(int iCode)
{
	switch( iCode ){
	case DOM_ERR_INDEX_SIZE:   return "Index Size Error";
	case DOM_ERR_HIERARCHY:    return "Hierarchy Request Error";
	case DOM_ERR_WRONG_DOC:    return "Wrong Document Error";
	case DOM_ERR_INVALID_CHAR: return "Invalid Character Error";
	case DOM_ERR_NOT_SUPPORTED: return "Not Supported Error";
	case DOM_ERR_INVALID_STATE: return "Invalid State Error";
	case DOM_ERR_NAMESPACE:    return "Namespace Error";
	default:                   return "Not Found Error";
	}
}
/* Forward: the refusal has to ask the receiver's document for its mode. */
static ph7_class_instance * DomThisDoc(ph7_context *pCtx);
/*
 * A DOM refusal, in whichever of php's TWO modes the document is in.
 *
 * `$doc->strictErrorChecking` (true by default) decides whether a refusal is an
 * exception or a warning: with it off, php raises the SAME sentence as an
 * E_WARNING under the method's own name and the method answers instead of
 * unwinding. The two answers it gives are the two this file needs -- `false`
 * from a method that returns something, and NOTHING from one php declares
 * `void` -- so the mode is one call with the answer as its argument.
 *
 * The flag is document state rather than tree state: it survives a `loadXML()`
 * onto the same object, and a clone carries it. It is read off the RECEIVER's
 * document -- the argument's own is not consulted even when the refusal is
 * about that argument -- with exactly one exception, `adoptNode`, which reads
 * the argument's and is passed it explicitly.
 *
 * And not every refusal consults it at all: php passes a hardcoded "strict" at
 * `setAttribute` and `toggleAttribute`, which throw whatever the flag says.
 * Those call DomThrowAlways.
 */
#define DOM_REFUSE_FALSE 0   /* the method answers false */
#define DOM_REFUSE_VOID  1   /* the method answers nothing (php declares it void) */
static int DomThrowAlways(ph7_context *pCtx,int iCode)
{
	return PH7_VmThrowExceptionCode(pCtx,"DOMException",(sxi32)iCode,"%s",DomErrText(iCode));
}
static int DomThrowFor(ph7_context *pCtx,ph7_class_instance *pDoc,int iCode,int iAnswer)
{
	if( pDoc && !PH7_NativeAttrTruthy(pDoc,"strictErrorChecking") ){
		/* The context prints php's own `Class::method(): ` in front of it. */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,DomErrText(iCode));
		if( iAnswer == DOM_REFUSE_FALSE ){
			ph7_result_bool(pCtx,0);
		}
		return PH7_OK;
	}
	return DomThrowAlways(pCtx,iCode);
}
static int DomThrow(ph7_context *pCtx,int iCode)
{
	return DomThrowFor(pCtx,DomThisDoc(pCtx),iCode,DOM_REFUSE_FALSE);
}
static int DomThrowVoid(ph7_context *pCtx,int iCode)
{
	return DomThrowFor(pCtx,DomThisDoc(pCtx),iCode,DOM_REFUSE_VOID);
}
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
/* ...and the DOCUMENT object it belongs to, which is where its wrapper is
 * cached and what its `ownerDocument` answers. */
static ph7_class_instance * DomObjArgDoc(ph7_value *pVal)
{
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	return PH7_NativeAttrObj((ph7_class_instance *)pVal->x.pOther,DOM_DOC);
}
/* The slot a DOMNameSpaceNode carries beside its own two: the element that
 * MAKES the declaration, which is php's parentNode for one. */
#define DOM_NS_OWNER "__owner"

/* The element a DOMNameSpaceNode argument was found on, or NULL for anything
 * else -- the slot exists on that class alone. */
static phl_domnode * DomNsNodeOwner(ph7_value *pVal)
{
	ph7_class_instance *pObj;
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pObj = (ph7_class_instance *)pVal->x.pOther;
	return DomResOf(PH7_NativeAttrObj(pObj,DOM_NS_OWNER));
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
 * Defined with the namespace machinery below, and declared here because the
 * surgery runs it: a node LINKED into a tree loses the declarations its new
 * scope already makes, and gains the ones its new scope no longer makes.
 * (The namespace section cannot move up because it reads the attribute
 * walker; DomDropChildren below is with the property-write machinery it was
 * built for, and replaceChildren() runs the same wrapper-preserving drop.)
 */
static void DomNsOnInsertEx(xmlNodePtr pNode,int bDeep);
static void DomDropChildren(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pNode);

/*
 * php's refusal taxonomy for linking pChild under pParent, or NULL when the
 * link is allowed. The chunk collapsed all of it into one message per method,
 * which cost more than a wording: nothing rejected making a node its own
 * DESCENDANT, so `$a->firstChild->appendChild($a)` spliced a CYCLE into the
 * tree and every later walk of it ran away.
 */
static int DomLinkRefusal(xmlNodePtr pParent,xmlNodePtr pChild)
{
	xmlNodePtr p;
	if( pParent->doc != pChild->doc ){
		return DOM_ERR_WRONG_DOC;
	}
	/* Walking UP from the parent also catches pChild == pParent. */
	for( p = pParent ; p ; p = p->parent ){
		if( p == pChild ){
			return DOM_ERR_HIERARCHY;
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
		/* php reconciles each node it MOVED, not the fragment they came from --
		 * and through this path it does so DEEPLY (see DomNsOnInsertEx). */
		DomNsOnInsertEx(pChild,1);
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
	int iErr;
	if( pPar == 0 || pChd == 0 ){
		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);
	}
	iErr = DomLinkRefusal((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);
	if( iErr ){
		return DomThrow(pCtx,iErr);
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
	DomNsOnInsertEx((xmlNodePtr)pChd->pNode,0);
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
	int iErr;
	if( pPar == 0 || pNew == 0 ){
		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);
	}
	pParent = (xmlNodePtr)pPar->pNode;
	pChild = (xmlNodePtr)pNew->pNode;
	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;
	iErr = DomLinkRefusal(pParent,pChild);
	if( iErr == 0 && pAnchor && pAnchor->parent != pParent ){
		iErr = DOM_ERR_NOT_FOUND;
	}
	if( iErr ){
		return DomThrow(pCtx,iErr);
	}
	if( pAnchor == pChild ){
		/*
		 * A node cannot be inserted before ITSELF, and linking it anyway made
		 * it its own sibling: `$p->insertBefore($x,$x)` spliced a cycle into
		 * the child list, and the next walk of the tree -- saveXML, a
		 * childNodes count, getNodePath -- never returned.
		 *
		 * php's refusal here is a plain Error with no DOM code, because there
		 * is no DOM error for it, and it comes AFTER the node is detached: the
		 * tree loses the node and the caller is told nothing more.
		 */
		DomDetach(pNew->pShell,pChild);
		DomOrphanAdd(pNew->pShell,pChild);
		return PH7_VmThrowException(pCtx,"Error",
			"Cannot add newnode as the previous sibling of refnode");
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
	DomNsOnInsertEx(pChild,0);
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
		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);
	}
	pChild = (xmlNodePtr)pChd->pNode;
	if( pChild->parent != (xmlNodePtr)pPar->pNode ){
		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);
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
	int iErr;
	if( pPar == 0 || pNew == 0 || pOld == 0 ){
		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);
	}
	pParent = (xmlNodePtr)pPar->pNode;
	pChild = (xmlNodePtr)pNew->pNode;
	pVictim = (xmlNodePtr)pOld->pNode;
	iErr = DomLinkRefusal(pParent,pChild);
	if( iErr == 0 && pVictim->parent != pParent ){
		iErr = DOM_ERR_NOT_FOUND;
	}
	if( iErr ){
		return DomThrow(pCtx,iErr);
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
		DomNsOnInsertEx(pChild,0);
	}
	ph7_result_value(pCtx,apArg[1]);
	return PH7_OK;
}
/* ===== The 8.3 parent/child-node family (DOMParentNode / DOMChildNode) ===== */

/*
 * The name a TypeError prints for a value that is neither a DOMNode nor a
 * string: the CLASS of an object, php's type keyword for anything else --
 * the same rendering DomWriteText uses for a typed property store.
 */
static const char * DomGivenName(ph7_value *pVal,char *zBuf,sxu32 nBuf)
{
	if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){
		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;
		SyBufferFormat(zBuf,nBuf,"%z",&pObj->pClass->sName);
		return zBuf;
	}
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) ){
		return "null";
	}
	return ph7_type_name(pVal);
}
/*
 * php's variadic screen for the 8.0 insertion methods: every argument must be
 * a DOMNode or a STRING (nothing coerces -- an int is refused where an
 * ordinary `string $data` parameter would take it), the WHOLE list is checked
 * before anything else runs, and the TypeError names the position with no
 * parameter name, because many values share the one variadic formal.  The
 * method name it prints is the DECLARING class's (`DOMCharacterData::before()`
 * for a comment), which is what pCtx->pFunc->sName already carries.
 * Answers 0 when every argument passed, non-zero after raising.
 */
static int DomNodesScreen(ph7_context *pCtx,int nArg,ph7_value **apArg)
{
	ph7_class *pNodeCls = PH7_VmExtractClass(pCtx->pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);
	int i;
	for( i = 0 ; i < nArg ; i++ ){
		ph7_value *pVal = apArg[i];
		char zBuf[128];
		if( pVal->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;
			if( pNodeCls && PH7_VmInstanceOf(pObj->pClass,pNodeCls) ){
				continue;
			}
		}else if( pVal->iFlags & MEMOBJ_STRING ){
			continue;
		}
		PH7_VmThrowException(pCtx,"TypeError",
			"%z(): Argument #%d must be of type DOMNode|string, %s given",
			&pCtx->pFunc->sName,i+1,DomGivenName(pVal,zBuf,sizeof(zBuf)));
		return -1;
	}
	return 0;
}
/*
 * php's "convert nodes into a node" (dom_zvals_to_single_node), transcribed
 * with its ONE-argument shortcut: a single node argument is handed through
 * whole -- nothing is unlinked, every check waits for the insertion -- while
 * two or more arguments really are appended one by one into an internal
 * fragment.  The difference is observable twice over.  A refusal DURING that
 * conversion (another document's node, a document, an attribute) leaves every
 * argument already converted DETACHED -- `$b->append($a, $attr)` costs the
 * tree its $a -- and a refusal at the final insertion (the receiver was in
 * the converted set) leaves ALL of them detached, which is how
 * `$b->append($a, $b)` empties <r> of both children where `$r->append($r)`,
 * one argument, moves nothing at all.  The CYCLE is checked only against the
 * conversion fragment (i.e. never fails there), NOT against the receiver --
 * that waits for the insertion step.
 *
 * The converted list is built in pList (xmlNodePtr entries, in order).  Every
 * node it takes is detached and parked in the receiver's orphan set, where a
 * failure leaves it alive for whatever PHP variable still wraps it -- php
 * frees the unwrapped ones instead, which no program can see.  A fragment
 * argument is emptied INTO the list (php unpacks it), so it stays empty even
 * when a later argument is refused.  Answers 0, or non-zero after raising.
 */
static int DomNodesConvert(ph7_context *pCtx,phl_xmldoc *pShell,xmlNodePtr pParent,
	int nArg,ph7_value **apArg,SySet *pList,int *pRc)
{
	int i;
	for( i = 0 ; i < nArg ; i++ ){
		phl_domnode *pNd;
		xmlNodePtr pNode;
		if( (apArg[i]->iFlags & MEMOBJ_OBJ) == 0 ){
			int nLen = 0;
			const char *zText = ph7_value_to_string(apArg[i],&nLen);
			pNode = xmlNewDocTextLen(pParent->doc,(const xmlChar *)zText,nLen);
			if( pNode ){
				DomOrphanAdd(pShell,pNode);
				SySetPut(pList,(const void *)&pNode);
			}
			continue;
		}
		pNd = DomObjArg(apArg[i]);
		if( pNd == 0 ){
			/* A DOMNode-classed object with no node behind it. php's refusal
			 * ignores strictErrorChecking. */
			*pRc = DomThrowAlways(pCtx,DOM_ERR_INVALID_STATE);
			return -1;
		}
		pNode = (xmlNodePtr)pNd->pNode;
		if( pNode->doc != pParent->doc ){
			*pRc = DomThrowVoid(pCtx,DOM_ERR_WRONG_DOC);
			return -1;
		}
		if( pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE
		 || pNode->type == XML_ATTRIBUTE_NODE ){
			*pRc = DomThrowVoid(pCtx,DOM_ERR_HIERARCHY);
			return -1;
		}
		if( DomIsFragment(pNode) ){
			xmlNodePtr pChild = pNode->children;
			while( pChild ){
				xmlNodePtr pNext = pChild->next;
				xmlUnlinkNode(pChild);
				DomOrphanAdd(pShell,pChild);
				SySetPut(pList,(const void *)&pChild);
				pChild = pNext;
			}
			pNode->children = pNode->last = 0;
			continue;
		}
		DomDetach(pNd->pShell,pNode);
		DomOrphanAdd(pShell,pNode);
		SySetPut(pList,(const void *)&pNode);
	}
	return 0;
}
static int DomListHas(SySet *pList,xmlNodePtr pNode)
{
	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(pList) ; ++n ){
		if( apNode[n] == pNode ){
			return 1;
		}
	}
	return 0;
}
/*
 * php's pre-insertion validity for what conversion produced, against the REAL
 * parent this time.  For a single node argument this is where every check
 * runs -- another document before the kind-or-ancestor Hierarchy refusal, the
 * same order the conversion pass uses -- and for a converted list the only
 * question left is whether the receiver is now INSIDE the set (its ancestor
 * chain passes through a detached argument).  Note what php never checks on
 * this path: a document receiver takes a second root element and bare text
 * without complaint, so the document it writes may not be well-formed XML --
 * measured, and matched.
 */
static int DomInsertValidity(xmlNodePtr pParent,xmlNodePtr pSingle,SySet *pList)
{
	xmlNodePtr p;
	if( pSingle ){
		if( pSingle->doc != pParent->doc ){
			return DOM_ERR_WRONG_DOC;
		}
		if( pSingle->type == XML_DOCUMENT_NODE || pSingle->type == XML_HTML_DOCUMENT_NODE
		 || pSingle->type == XML_ATTRIBUTE_NODE ){
			return DOM_ERR_HIERARCHY;
		}
		for( p = pParent ; p ; p = p->parent ){
			if( p == pSingle ){
				return DOM_ERR_HIERARCHY;
			}
		}
		return 0;
	}
	for( p = pParent ; p ; p = p->parent ){
		if( DomListHas(pList,p) ){
			return DOM_ERR_HIERARCHY;
		}
	}
	return 0;
}
/*
 * The insertion itself (php's dom_insert_node_list_unchecked): everything in
 * pList goes before pRef -- at the end when NULL -- in order.  A list node
 * came through the conversion fragment, so its namespace reconcile is the
 * DEEP one (dom_reconcile_ns_list); a single node is php's dom_reconcile_ns,
 * the shallow appendChild rule.  A single node inserted before ITSELF slides
 * the reference to its next sibling first (the spec's step 3), which is what
 * makes `$r->prepend($r->firstChild)` a no-op instead of a cycle.
 */
static void DomNodesPlace(phl_xmldoc *pShell,xmlNodePtr pParent,xmlNodePtr pRef,SySet *pList)
{
	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(pList) ; ++n ){
		xmlNodePtr pNode = apNode[n];
		DomDetach(pShell,pNode);
		if( pRef ){
			DomLinkBefore(pParent,pNode,pRef);
		}else{
			DomLinkLast(pParent,pNode);
		}
		DomNsOnInsertEx(pNode,1);
	}
}
/*
 * DOMParentNode::append / prepend / replaceChildren -- one body, three
 * insertion points.  php declares all three `void`, so a refusal in the
 * non-strict mode is a warning and NOTHING is answered (DomThrowVoid).
 */
#define DOM_PN_APPEND   0
#define DOM_PN_PREPEND  1
#define DOM_PN_REPLACE  2
static int DomParentNodeInsert(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)
{
	phl_domnode *pPar = DomThisNode(pCtx);
	phl_domnode *pOne = 0;
	xmlNodePtr pParent,pSingle = 0,pRef = 0;
	SySet sList;
	int iErr,rc = PH7_OK;
	if( DomNodesScreen(pCtx,nArg,apArg) || pPar == 0 ){
		return PH7_OK;
	}
	pParent = (xmlNodePtr)pPar->pNode;
	SySetInit(&sList,&pCtx->pVm->sAllocator,sizeof(xmlNodePtr));
	if( nArg == 1 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){
		/* The one-argument shortcut: the node itself, unconverted.  A shell
		 * with no node behind it is php's SILENT no-op here (the pre-insert
		 * NULL guard), where the multi-argument conversion raises Invalid
		 * State -- one more face of the shortcut. */
		pOne = DomObjArg(apArg[0]);
		if( pOne == 0 ){
			return PH7_OK;
		}
		pSingle = (xmlNodePtr)pOne->pNode;
	}else if( DomNodesConvert(pCtx,pPar->pShell,pParent,nArg,apArg,&sList,&rc) ){
		SySetRelease(&sList);
		return rc;
	}
	iErr = DomInsertValidity(pParent,pSingle,&sList);
	if( iErr ){
		SySetRelease(&sList);
		return DomThrowVoid(pCtx,iErr);
	}
	if( iMode == DOM_PN_REPLACE ){
		/* Every remaining child goes -- through the wrapper-preserving drop a
		 * content write uses, so a PHP variable holding one keeps a live
		 * detached node rather than a dangling pointer.  After the validity
		 * check, as php orders it. */
		DomDropChildren(pCtx,pPar->pShell,pParent);
	}else if( iMode == DOM_PN_PREPEND ){
		/* The first child AFTER conversion has emptied the set out of the
		 * tree -- and never a member of the set. */
		pRef = pParent->children;
	}
	if( pSingle ){
		if( DomIsFragment(pSingle) ){
			/* A single fragment splices -- silently even when EMPTY, unlike
			 * appendChild's warning. */
			DomFragMove(pOne->pShell,pParent,pSingle,pRef);
		}else{
			if( pRef == pSingle ){
				pRef = pSingle->next;
			}
			DomDetach(pOne->pShell,pSingle);
			if( pRef ){
				DomLinkBefore(pParent,pSingle,pRef);
			}else{
				DomLinkLast(pParent,pSingle);
			}
			DomNsOnInsertEx(pSingle,0);
		}
	}else{
		DomNodesPlace(pPar->pShell,pParent,pRef,&sList);
	}
	SySetRelease(&sList);
	return PH7_OK;
}
DOM_METHOD(vm_builtin_Dom_append)
{
	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_APPEND);
}
DOM_METHOD(vm_builtin_Dom_prepend)
{
	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_PREPEND);
}
DOM_METHOD(vm_builtin_Dom_replaceChildren)
{
	return DomParentNodeInsert(pCtx,nArg,apArg,DOM_PN_REPLACE);
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
 * element contains its own attributes. A namespace DECLARATION is asked about
 * through the element that MAKES it: its own pointer is an xmlNs, which is in
 * no tree at all. */
DOM_METHOD(vm_builtin_DOMNode_contains)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	ph7_value *pArg = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? apArg[0] : 0;
	phl_domnode *pOtherNd = pArg ? DomObjArg(pArg) : 0;
	xmlNodePtr pThisNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlNodePtr pOther = pOtherNd ? (xmlNodePtr)pOtherNd->pNode : 0;
	phl_domnode *pOwnerNd = pArg ? DomNsNodeOwner(pArg) : 0;
	if( pOwnerNd ){
		pOther = (xmlNodePtr)pOwnerNd->pNode;
	}
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
 * The parser directives ride along: php's copy answers the receiver's whole
 * flag block, not the class defaults.
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
		sxu32 i;
		for( i = 0 ; i < SX_ARRAYSIZE(azDomDocFlag) ; ++i ){
			PH7_NativeSetAttrBool(pVm,pObj,azDomDocFlag[i],
				PH7_NativeAttrTruthy(pThis,azDomDocFlag[i]));
		}
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
/*
 * ===== Qualified names and the namespaces they need =====
 *
 * The grammar php screens a created name against, and the rule by which it
 * finds or declares the namespace behind it.  Both were missing here, and what
 * stood in for them wrote documents that are not XML: `setAttributeNS('urn:b',
 * '1:x', 'v')` emitted `xmlns:1="urn:b" 1:x="v"`, `('urn:b','a:b:c','v')`
 * emitted an attribute with two colons in its name, and a prefixed name with a
 * NULL namespace emitted `xmlns:q=""`.  php refuses all three with a Namespace
 * Error before the element is touched.
 */
#define DOM_XML_NS_URI   "http://www.w3.org/XML/1998/namespace"
#define DOM_XMLNS_NS_URI "http://www.w3.org/2000/xmlns/"

typedef struct dom_qname dom_qname;
struct dom_qname {
	xmlChar *zPrefix;   /* NULL when the name carries none */
	xmlChar *zLocal;    /* always allocated */
};
static void DomQNameRelease(dom_qname *pQ)
{
	if( pQ->zPrefix ){
		xmlFree(pQ->zPrefix);
	}
	if( pQ->zLocal ){
		xmlFree(pQ->zLocal);
	}
	pQ->zPrefix = pQ->zLocal = 0;
}
static int DomUriIs(const char *zUri,const char *zWant)
{
	return zUri != 0 && DomNameIs(zUri,zWant);
}
/*
 * php's `dom_check_qname`: the name has to be a QName, and a prefix demands a
 * namespace.  Three callers ask three different questions of the same name, so
 * iMode says which:
 *
 *   DOM_QN_SET   setAttributeNS -- the loosest. A prefixed name is judged as two
 *                NCNames and every failure is the Namespace Error; an unprefixed
 *                one is a plain Name, where a character libxml will not take is
 *                the Invalid Character Error. The unprefixed `xmlns` is how a
 *                program writes a namespace DECLARATION, so nothing about the
 *                xmlns namespace is checked here.
 *   DOM_QN_ATTR  createAttributeNS -- a QName and nothing else, plus the DOM
 *                spec's pairing (the xmlns namespace may only be spelled by an
 *                xmlns name and an xmlns name may name nothing else) and the
 *                `xml` prefix's own URI.
 *   DOM_QN_ELEM  createElementNS -- a QName when a namespace came with it, and
 *                the SET side's split when none did (so `createElementNS(null,
 *                'x y')` is the Invalid Character Error where the attribute
 *                factory says Namespace Error, and `:x` is an element named
 *                `:x` there and a refusal here). No reserved rule at all: php
 *                checks those where it RESOLVES the namespace, which is after
 *                any binding the document already has, so
 *                `createElementNS($XML_NS, 'xmlns:x')` is an `xml:x` element
 *                rather than a refusal.
 *
 * Answers 0, or the DOM error code to raise.
 */
#define DOM_QN_SET  0
#define DOM_QN_ATTR 1
#define DOM_QN_ELEM 2
static int DomQNameParse(const char *zQname,const char *zUri,int iMode,dom_qname *pOut)
{
	int bHasUri = zUri != 0 && zUri[0] != 0;
	int bXmlnsName;
	pOut->zPrefix = pOut->zLocal = 0;
	if( zQname == 0 || zQname[0] == 0 ){
		return DOM_ERR_NAMESPACE;
	}
	if( iMode == DOM_QN_ATTR || (iMode == DOM_QN_ELEM && bHasUri) ){
		/* A created name that names a namespace has to be a QName, and every
		 * failure there is the Namespace Error. */
		if( xmlValidateQName((const xmlChar *)zQname,0) != 0 ){
			return DOM_ERR_NAMESPACE;
		}
	}
	pOut->zLocal = xmlSplitQName2((const xmlChar *)zQname,&pOut->zPrefix);
	if( pOut->zLocal == 0 ){
		/* No prefix -- or a name that BEGINS with the colon, which libxml hands
		 * back whole and php then writes literally (`:x`) as long as no
		 * namespace came with it. */
		pOut->zLocal = xmlStrdup((const xmlChar *)zQname);
		if( pOut->zLocal == 0 ){
			return DOM_ERR_NAMESPACE;
		}
	}
	if( iMode == DOM_QN_SET || (iMode == DOM_QN_ELEM && !bHasUri) ){
		/* The SET side separates the two failures php separates. A name with a
		 * PREFIX is judged as two NCNames and every failure there is the
		 * Namespace Error; an unprefixed one is judged as a plain Name, and a
		 * name libxml will not take at all is the Invalid Character Error. A
		 * namespace then demands that the local part be an NCName too, which is
		 * what refuses `:x` once a URI comes with it. */
		if( pOut->zPrefix ){
			if( xmlValidateNCName(pOut->zPrefix,0) != 0
			 || xmlValidateNCName(pOut->zLocal,0) != 0 ){
				DomQNameRelease(pOut);
				return DOM_ERR_NAMESPACE;
			}
		}else if( xmlValidateName((const xmlChar *)zQname,0) != 0 ){
			DomQNameRelease(pOut);
			return DOM_ERR_INVALID_CHAR;
		}
		if( bHasUri && xmlValidateNCName(pOut->zLocal,0) != 0 ){
			DomQNameRelease(pOut);
			return DOM_ERR_NAMESPACE;
		}
	}
	bXmlnsName = pOut->zPrefix == 0 && xmlStrEqual(pOut->zLocal,(const xmlChar *)"xmlns");
	if( pOut->zPrefix && !bHasUri ){
		/* A prefix names a namespace, so there has to be one. (Whether the
		 * prefix may be USED is the resolution's question, not the grammar's:
		 * php reuses a binding the document already has whatever prefix was
		 * asked for, and only refuses when it would have to declare one.) */
		DomQNameRelease(pOut);
		return DOM_ERR_NAMESPACE;
	}
	if( iMode == DOM_QN_ATTR && pOut->zPrefix
	 && xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xml")
	 && !DomUriIs(zUri,DOM_XML_NS_URI) ){
		DomQNameRelease(pOut);
		return DOM_ERR_NAMESPACE;
	}
	if( iMode == DOM_QN_ATTR ){
		/* The DOM spec's pairing, which php applies to a created ATTRIBUTE: the
		 * xmlns namespace may only be spelled by an xmlns name, and an xmlns
		 * name may name nothing else. */
		int bXmlnsPrefix = pOut->zPrefix != 0
			&& xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xmlns");
		if( (bXmlnsName || bXmlnsPrefix) != DomUriIs(zUri,DOM_XMLNS_NS_URI) ){
			DomQNameRelease(pOut);
			return DOM_ERR_NAMESPACE;
		}
	}
	return 0;
}
/*
 * The namespace a node in zUri should carry, declared on pAnchor when the
 * document has none.  php REUSES a binding it can find by URI as long as that
 * binding has a prefix, takes the caller's prefix when it has to declare and
 * the prefix is free, and otherwise generates `default`, `default1`, ... --
 * which is why asking for a prefix another URI already owns quietly answers
 * `default:x` rather than refusing.
 *
 * bNeedPrefix is the CREATE side (`createAttributeNS`), where an unprefixed
 * name still gets a generated prefix; the SET side may declare the DEFAULT
 * namespace instead.
 */
static xmlNsPtr DomFindPrefixedNs(xmlNodePtr pNode,const char *zUri)
{
	xmlNodePtr p;
	for( p = pNode ; p ; p = p->parent ){
		xmlNsPtr pNs;
		if( p->type != XML_ELEMENT_NODE ){
			continue;
		}
		for( pNs = p->nsDef ; pNs ; pNs = pNs->next ){
			if( pNs->prefix == 0 || pNs->href == 0
			 || !xmlStrEqual(pNs->href,(const xmlChar *)zUri) ){
				continue;
			}
			/* ...and only if a nearer declaration has not taken the prefix. */
			if( xmlSearchNs(pNode->doc,pNode,pNs->prefix) == pNs ){
				return pNs;
			}
		}
	}
	return 0;
}
/*
 * Declare a binding of zUri on pAnchor under a prefix nothing there has taken:
 * zBase, then zBase1, zBase2...  php starts from `default` for a namespace
 * with no prefix of its own and from the prefix ITSELF when it is re-spelling
 * one an inner declaration has shadowed (which is where `p1` comes from).
 *
 * bScope is what "nothing there has taken" means. xmlNewNs only refuses a second
 * declaration on the SAME element, which is the whole test for a re-spelling
 * (the shadowing declaration is the one being written). An attribute ARRIVING
 * needs the stronger one -- a prefix bound anywhere in scope is taken, or the
 * declaration written here would shadow it and re-point every node under it.
 */
static xmlNsPtr DomNsGenerateEx(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase,
	int bScope)
{
	xmlNsPtr pNs = 0;
	int i;
	for( i = 0 ; i < 1000 ; i++ ){
		char zGen[256];
		const char *zB = zBase ? (const char *)zBase : "default";
		if( SyStrlen(zB) > sizeof(zGen)-16 ){
			zB = "default";
		}
		if( i == 0 ){
			SyBufferFormat(zGen,sizeof(zGen),"%s",zB);
		}else{
			SyBufferFormat(zGen,sizeof(zGen),"%s%d",zB,i);
		}
		if( bScope && xmlSearchNs(pAnchor->doc,pAnchor,(const xmlChar *)zGen) != 0 ){
			/* Taken -- by a declaration IN SCOPE, which xmlNewNs does not see:
			 * it only refuses a second one on the same element. */
			continue;
		}
		pNs = xmlNewNs(pAnchor,(const xmlChar *)zUri,(const xmlChar *)zGen);
		if( pNs ){
			return pNs;
		}
	}
	return 0;
}
static xmlNsPtr DomNsGenerate(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase)
{
	return DomNsGenerateEx(pAnchor,zUri,zBase,0);
}
/* A binding of this URI an ATTRIBUTE can use: one that carries a prefix. */
static xmlNsPtr DomNsReuse(xmlNodePtr pAnchor,const char *zUri)
{
	xmlNsPtr pNs = xmlSearchNsByHref(pAnchor->doc,pAnchor,(const xmlChar *)zUri);
	if( pNs && pNs->prefix ){
		return pNs;   /* including libxml's implicit `xml` binding */
	}
	/* Bound, but only WITHOUT a prefix, which does not serve an attribute: a
	 * prefixed binding of the same URI further out still does. */
	return pNs ? DomFindPrefixedNs(pAnchor,zUri) : 0;
}
static xmlNsPtr DomNsResolve(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zPrefix,int bNeedPrefix)
{
	xmlNsPtr pNs;
	if( !bNeedPrefix ){
		pNs = DomNsReuse(pAnchor,zUri);
		if( pNs ){
			return pNs;
		}
	}
	/* The CREATE side asks libxml's own question and no more: a document that
	 * binds this URI to the default namespace AND to a prefix answers the
	 * default one there, and php then declares its own rather than looking for
	 * the prefixed binding the SET side would have found. */
	pNs = xmlSearchNsByHref(pAnchor->doc,pAnchor,(const xmlChar *)zUri);
	if( bNeedPrefix ){
		if( pNs && pNs->prefix ){
			return pNs;
		}
		pNs = 0;   /* a prefix-less binding is no use to an attribute */
	}
	/* A prefix-less binding stops php from declaring another one under the
	 * caller's prefix -- what happens then is a generated one. */
	if( pNs == 0 && (zPrefix != 0 || !bNeedPrefix) ){
		if( !bNeedPrefix && zPrefix
		 && (xmlStrEqual(zPrefix,(const xmlChar *)"xml")
		  || xmlStrEqual(zPrefix,(const xmlChar *)"xmlns")) ){
			/* A RESERVED prefix cannot be declared, and php does not paper over
			 * that with a generated one: it refuses. (Nothing is refused when
			 * the URI already had a binding -- the prefix is never consulted
			 * then, which is why `setAttributeNS($uri,'xml:id',..)` succeeds on
			 * a document that binds $uri and fails on one that does not.) */
			return 0;
		}
		pNs = xmlNewNs(pAnchor,(const xmlChar *)zUri,zPrefix);
		if( pNs ){
			return pNs;
		}
	}
	return DomNsGenerate(pAnchor,zUri,0);
}
/*
 * The namespace a node CREATED in zUri carries, which is a different rule from
 * either side above and php's smallest one: a binding already in scope is used
 * whatever prefix was asked for -- for a fresh node that means only libxml's own
 * `xml` declaration, which is why every `createElementNS($XML_NS, ...)` comes
 * back spelled `xml:` -- and otherwise the node declares zUri on ITSELF under
 * the caller's prefix, with no generated prefix and no fallback: the three
 * reserved-name rules php checks here (`dom_get_ns`) are a refusal, not a
 * rename. NULL means Namespace Error.
 */
static xmlNsPtr DomNsForCreate(xmlNodePtr pNode,const char *zUri,const xmlChar *zPrefix)
{
	xmlNsPtr pNs = xmlSearchNsByHref(pNode->doc,pNode,(const xmlChar *)zUri);
	if( pNs ){
		return pNs;
	}
	if( zPrefix != 0
	 && ((xmlStrEqual(zPrefix,(const xmlChar *)"xml") && !DomUriIs(zUri,DOM_XML_NS_URI))
	  || (xmlStrEqual(zPrefix,(const xmlChar *)"xmlns") && !DomUriIs(zUri,DOM_XMLNS_NS_URI))
	  || (DomUriIs(zUri,DOM_XMLNS_NS_URI)
	   && !xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){
		return 0;
	}
	return xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);
}
/*
 * A declaration that lands on pElem takes the SPELLING away from every node
 * under it that reached its namespace through a declaration this one now
 * shadows -- the node still points at a binding nothing can name from there, so
 * a re-parse of the serialized document reads it in the wrong namespace (or in
 * none).  php re-points those nodes at a binding of their OWN URI: one still in
 * scope when there is one, and otherwise a fresh declaration on pElem under
 * their own prefix numbered up (`p` -> `p1`), or `default` when they had none.
 *
 * Only called when a write actually declared something, which is what keeps it
 * off the ordinary path.
 */
static void DomNsRespell(xmlNodePtr pNode,int bAttr)
{
	xmlNsPtr pNs = pNode->ns,pAlt;
	/* php declares what it needs on the node that NEEDS it -- the element
	 * itself, or the element an attribute belongs to. */
	xmlNodePtr pSite = bAttr ? pNode->parent : pNode;
	if( pNs == 0 || pNs->href == 0 || pSite == 0 ){
		return;
	}
	if( xmlSearchNs(pNode->doc,pNode,pNs->prefix) == pNs ){
		return;   /* the prefix still names this very binding */
	}
	/* An attribute needs a PREFIXED binding; an element is happy with the
	 * default one. */
	pAlt = bAttr ? DomNsReuse(pNode,(const char *)pNs->href)
	             : xmlSearchNsByHref(pNode->doc,pNode,pNs->href);
	if( pAlt == 0 ){
		/* Its own prefix first -- a declaration that was REMOVED leaves that
		 * prefix free again, and php re-declares it unchanged there. */
		pAlt = xmlNewNs(pSite,pNs->href,pNs->prefix);
	}
	if( pAlt == 0 ){
		pAlt = DomNsGenerate(pSite,(const char *)pNs->href,pNs->prefix);
	}
	if( pAlt ){
		pNode->ns = pAlt;
	}
}
static int DomNsDefCount(xmlNodePtr pElem)
{
	xmlNsPtr pNs;
	int n = 0;
	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){
		n++;
	}
	return n;
}
static void DomNsReconcile(xmlNodePtr pElem)
{
	xmlNodePtr pCur = pElem;
	while( pCur ){
		xmlAttrPtr pAttr;
		if( pCur->type == XML_ELEMENT_NODE ){
			DomNsRespell(pCur,0);
			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){
				if( pAttr->type == XML_ATTRIBUTE_NODE ){
					DomNsRespell((xmlNodePtr)pAttr,1);
				}
			}
		}
		pCur = DomWalkNext(pCur,pElem);
	}
}
/*
 * A declaration is freed with the element that carries it, so one REMOVED from
 * an element cannot simply be dropped: a node further down may still point at
 * it. It goes where libxml's own document teardown will free it and nothing
 * resolves through it -- `doc->oldNs`, which is what php's `dom_set_old_ns`
 * writes to.
 */
static void DomNsPark(xmlNodePtr pOwner,xmlNsPtr pNs)
{
	xmlDocPtr pDoc = pOwner->doc;
	xmlNsPtr pTail;
	pNs->next = 0;
	/* The list's HEAD must stay libxml's own `xml` declaration, because
	 * xmlSearchNs answers doc->oldNs DIRECTLY for the `xml` prefix. Asking for
	 * it is what builds it. */
	xmlSearchNs(pDoc,pOwner,(const xmlChar *)"xml");
	if( pDoc->oldNs == 0 ){
		pDoc->oldNs = pNs;
		return;
	}
	for( pTail = pDoc->oldNs ; pTail->next ; pTail = pTail->next ){}
	pTail->next = pNs;
}
/*
 * php's `dom_reconcile_ns`, which every mutator runs on the node it LINKED.
 * Without it a move wrote documents that are not XML in both directions:
 * appending a node whose namespace was declared on the ancestor it just left
 * emitted `<p:b k="1"/>` with the prefix bound nowhere, and appending one that
 * carries its own declaration (`createElementNS`, or a chunk `appendXML` built)
 * emitted a second copy of a declaration the new parent already makes.
 *
 * The strip is php's own test: same URI, and either the node's declaration
 * carries NO prefix -- then any binding of that URI in scope replaces it, even
 * a prefixed one, which is how an appended `createElementNS($uri,'y')` comes
 * out spelled `p:y` -- or the in-scope binding spells it the same way.
 *
 * The re-pointing after it is libxml's own `xmlReconciliateNs`, called here
 * rather than paraphrased: it re-points EVERY node of the subtree at the first
 * in-scope binding of its URI found from the inserted node -- so a URI two
 * prefixes bind is respelled to the first of them, which the setAttributeNS
 * respeller (DomNsReconcile, which only touches a node whose spelling BROKE)
 * does not do -- and declares one on the inserted node for a URI nothing in
 * scope binds any more (including one a DESCENDANT declares, since the search
 * only ever looks up).
 */
static void DomNsStrip(xmlNodePtr pNode,xmlNodePtr pScopeAt)
{
	xmlNsPtr pCur = pNode->nsDef,pPrev = 0;
	if( pNode->doc == 0 ){
		/* Nothing would own a removed declaration, and a node under it may
		 * still point at one: leave the element's list alone. */
		return;
	}
	while( pCur ){
		xmlNsPtr pNext = pCur->next;
		xmlNsPtr pScope = pCur->href
			? xmlSearchNsByHref(pNode->doc,pScopeAt,pCur->href) : 0;
		if( pScope != 0
		 && (pCur->prefix == 0
		  || (pScope->prefix != 0 && xmlStrEqual(pScope->prefix,pCur->prefix))) ){
			if( pPrev ){
				pPrev->next = pNext;
			}else{
				pNode->nsDef = pNext;
			}
			DomNsPark(pNode,pCur);
		}else{
			pPrev = pCur;
		}
		pCur = pNext;
	}
}
/*
 * php runs the strip on the node it linked and NO deeper -- a redundant
 * declaration one level down survives an `appendChild` -- but a FRAGMENT is
 * spliced by a second function that walks each moved child WHOLE, so the same
 * subtree arriving that way comes out stripped at every depth. bDeep is that
 * difference, and both halves are measurable.
 *
 * The deep walk judges every node against the same scope -- the INSERTION
 * POINT, not each node's own parent -- so a declaration duplicated inside the
 * moved subtree survives when the new parent does not make it too.
 */
static void DomNsOnInsertEx(xmlNodePtr pNode,int bDeep)
{
	if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
		return;
	}
	if( bDeep ){
		xmlNodePtr pCur = pNode,pAt = pNode->parent;
		while( pCur ){
			if( pCur->type == XML_ELEMENT_NODE ){
				DomNsStrip(pCur,pAt);
			}
			pCur = DomWalkNext(pCur,pNode);
		}
	}else{
		DomNsStrip(pNode,pNode->parent);
	}
	xmlReconciliateNs(pNode->doc,pNode);
}
/*
 * The namespace an attribute NODE carries once it is linked onto pElem. Its own
 * ns struct is a declaration of wherever it came FROM, and moving the node does
 * not move that: written unchanged it emitted `p:k="1"` with the prefix bound
 * NOWHERE, and -- when the target's scope binds that prefix to something else --
 * bound to the WRONG URI, which is the worse half, because those bytes parse
 * back cleanly as an attribute in a namespace the program never wrote.
 *
 * php keeps the spelling when the attribute's own declaration is still in scope
 * here, even if a nearer one binds the same URI under another prefix. Otherwise
 * it takes any binding of the URI in scope -- INCLUDING a prefix-less one, where
 * the attribute then serializes with no prefix at all and still answers the URI,
 * which is NOT how the by-NAME writes resolve (there an attribute always wants a
 * prefixed binding, DomNsReuse) -- and otherwise declares one here under the
 * attribute's own prefix, numbered up when that prefix is taken (`p` -> `p1`).
 */
static void DomNsAttrArrive(xmlNodePtr pElem,xmlAttrPtr pAttr)
{
	xmlNsPtr pNs = pAttr->ns;
	if( pNs == 0 || pNs->href == 0 ){
		return;
	}
	if( DomUriIs((const char *)pNs->href,DOM_XMLNS_NS_URI) ){
		/* An attribute in the xmlns namespace IS a declaration, and php resolves
		 * nothing for it: `xmlns="urn:z"` stays spelled that way wherever it is
		 * written, and never acquires a declaration of the xmlns namespace. */
		return;
	}
	if( xmlSearchNs(pElem->doc,pElem,pNs->prefix) == pNs ){
		return;
	}
	pNs = xmlSearchNsByHref(pElem->doc,pElem,pAttr->ns->href);
	if( pNs ){
		pAttr->ns = pNs;
		return;
	}
	pNs = DomNsGenerateEx(pElem,(const char *)pAttr->ns->href,pAttr->ns->prefix,1);
	if( pNs == 0 ){
		return;
	}
	pAttr->ns = pNs;
	/*
	 * A declaration LANDED on this element, and php then judges its whole
	 * subtree from HERE: a descendant whose namespace is declared further down
	 * is re-pointed at a fresh declaration on this element -- even though the
	 * one it had is still in scope where it stands. That is libxml's
	 * xmlReconciliateNs, and it runs ONLY on this path: an arriving attribute
	 * that needed no declaration leaves the subtree exactly as it was, which is
	 * measurable both ways.
	 */
	xmlReconciliateNs(pElem->doc,pElem);
}
/* A namespace DECLARATION on this element: php's setAttributeNS writes one
 * when the name is `xmlns` or its prefix is, and REBINDS the one already
 * there rather than adding a second. */
static void DomNsDeclare(xmlNodePtr pElem,const xmlChar *zPrefix,const char *zHref)
{
	xmlNsPtr pNs;
	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){
		int bSame = zPrefix == 0 ? pNs->prefix == 0
			: (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix));
		if( bSame ){
			xmlChar *zNew = xmlStrdup((const xmlChar *)zHref);
			if( zNew == 0 ){
				return;
			}
			if( pNs->href ){
				xmlFree((xmlChar *)pNs->href);
			}
			pNs->href = zNew;
			return;
		}
	}
	xmlNewNs(pElem,(const xmlChar *)zHref,zPrefix);
}

/*
 * ===== Namespace DECLARATIONS, which php answers from the attribute surface =====
 *
 * `xmlns:x="urn:x"` is not an attribute in libxml -- it is an xmlNs on the
 * element's nsDef chain -- but php answers it from `getAttributeNode('xmlns:x')`
 * (as a DOMNameSpaceNode), from `getAttribute`, `hasAttribute`,
 * `removeAttribute`, `toggleAttribute` and `getAttributeNames`, which is how a
 * program reads or drops one.  Here every one of those said the declaration was
 * not there: `hasAttribute('xmlns:x')` was false and `getAttribute('xmlns:x')`
 * was "" on a document whose root declares it.
 *
 * The lookup is the element's OWN declarations, not the ones in scope: a child
 * answers false for a prefix its parent declared.
 */
#define DOM_XMLNS_NAME "xmlns"

/* The declaration this element makes for zPrefix (NULL for the DEFAULT one). */
static xmlNsPtr DomNsDeclOf(xmlNodePtr pElem,const xmlChar *zPrefix)
{
	xmlNsPtr pNs;
	if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
		return 0;
	}
	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){
		if( zPrefix == 0 ? pNs->prefix == 0
		                 : (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix)) ){
			return pNs;
		}
	}
	return 0;
}
/* ...under the NAME php spells it with: `xmlns` or `xmlns:<prefix>`. */
static xmlNsPtr DomNsDeclByName(xmlNodePtr pElem,const char *zName)
{
	sxu32 n = (sxu32)SyStrlen(DOM_XMLNS_NAME);
	if( SyStrlen(zName) < n || SyStrncmp(zName,DOM_XMLNS_NAME,n) != 0 ){
		return 0;
	}
	if( zName[n] == 0 ){
		return DomNsDeclOf(pElem,0);
	}
	if( zName[n] != ':' ){
		return 0;
	}
	return DomNsDeclOf(pElem,(const xmlChar *)(zName+n+1));
}
/*
 * Dropping one: the declaration leaves the element's chain, but the xmlNs
 * itself must NOT be freed -- nodes below can still point at it, and php's own
 * answer for that case is a document that keeps saying what it said. The
 * document's oldNs chain owns it from here, so it dies with the document.
 */
static void DomNsDeclRemove(xmlNodePtr pElem,xmlNsPtr pNs)
{
	xmlNsPtr pPrev = 0,pCur;
	xmlDocPtr pDoc = pElem->doc;
	for( pCur = pElem->nsDef ; pCur ; pPrev = pCur,pCur = pCur->next ){
		if( pCur != pNs ){
			continue;
		}
		if( pPrev ){
			pPrev->next = pCur->next;
		}else{
			pElem->nsDef = pCur->next;
		}
		pCur->next = 0;
		if( pDoc == 0 ){
			return;
		}
		if( pDoc->oldNs == 0 ){
			pDoc->oldNs = pCur;
		}else{
			xmlNsPtr pTail = pDoc->oldNs;
			while( pTail->next ){
				pTail = pTail->next;
			}
			pTail->next = pCur;
		}
		return;
	}
}

/*
 * php's wrapper for one: DOMNameSpaceNode, a class of its own that does NOT
 * extend DOMNode and answers ten properties.  A FRESH object every time (php's
 * two calls are never `===`), so it needs none of the identity cache.
 */
/*
 * The handle for one declaration, kept in the document's identity cache under
 * the xmlNs pointer.  php's two lookups answer two OBJECTS (`===` is false) but
 * the same declaration, and a shared handle is what makes them `==` -- and what
 * stops a loop over `getAttributeNode('xmlns:x')` from allocating one per call.
 */
static phl_domnode * DomNsRes(ph7_vm *pVm,ph7_class_instance *pDoc,phl_xmldoc *pShell,xmlNsPtr pNs)
{
	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);
	ph7_hashmap_node *pEntry = 0;
	phl_domnode *pRes;
	ph7_value sKey,sVal;
	if( pCache == 0 ){
		return DomNewRes(&(*pVm),pShell,pNs);
	}
	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNs);
	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){
		ph7_value *pHit = HashmapExtractNodeValue(pEntry);
		if( pHit && (pHit->iFlags & MEMOBJ_RES) ){
			PH7_MemObjRelease(&sKey);
			return (phl_domnode *)pHit->x.pOther;
		}
	}
	pRes = DomNewRes(&(*pVm),pShell,pNs);
	if( pRes ){
		PH7_MemObjInit(&(*pVm),&sVal);
		sVal.x.pOther = pRes;
		sVal.iFlags = MEMOBJ_RES;
		PH7_HashmapInsert(pCache,&sKey,&sVal);
	}
	PH7_MemObjRelease(&sKey);
	return pRes;
}
static ph7_class_instance * DomNewNsNode(ph7_vm *pVm,ph7_class_instance *pDoc,
	phl_xmldoc *pShell,xmlNsPtr pNs,xmlNodePtr pElem)
{
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DOMNameSpaceNode",
		(sxu32)SyStrlen("DOMNameSpaceNode"),FALSE,0);
	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;
	phl_domnode *pRes = pObj ? DomNsRes(&(*pVm),pDoc,pShell,pNs) : 0;
	if( pRes == 0 ){
		if( pObj ){
			PH7_ClassInstanceUnref(pObj);
		}
		return 0;
	}
	DomSetRes(&(*pVm),pObj,pRes);
	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDoc);
	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_NS_OWNER,DomWrap(&(*pVm),pDoc,pShell,pElem));
	return pObj;   /* the CALLER owns this reference */
}
/* Answer one from a native method (php hands back a fresh object every time). */
static int DomResultNsNode(ph7_context *pCtx,phl_domnode *pNd,xmlNsPtr pNs,xmlNodePtr pElem)
{
	ph7_class_instance *pObj = DomNewNsNode(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNs,pElem);
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}

/*
 * An attribute of pElem by QUALIFIED name, php's own two-step lookup.
 *
 * libxml's xmlHasProp() matches the stored name and ignores namespaces
 * entirely, which is wrong in both directions and was the answer the whole
 * by-name surface gave: `getAttribute('x:b')` found NOTHING (the stored name is
 * `b`, the prefix lives in the node's ns) while `getAttribute('b')` found the
 * NAMESPACED one and `removeAttribute('b')` deleted it.  php looks for an
 * attribute in NO namespace under the whole name first -- which is what finds
 * one written under an unresolvable prefix, stored with the colon in its name --
 * and only then splits the prefix, resolves it in the element's scope and asks
 * again by (local name, URI).  An unresolvable prefix finds nothing.
 *
 * A DTD-declared DEFAULT is not an attribute here: xmlHasNsProp answers the
 * DECLARATION node for those, which is a different node kind entirely.
 */
static xmlAttrPtr DomAttrNoNs(xmlNodePtr pElem,const char *zName)
{
	xmlAttrPtr pAttr;
	if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
		return 0;
	}
	pAttr = xmlHasNsProp(pElem,(const xmlChar *)zName,0);
	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;
}
static xmlAttrPtr DomAttrByName(xmlNodePtr pElem,const char *zName)
{
	xmlAttrPtr pAttr = DomAttrNoNs(pElem,zName);
	if( pAttr == 0 && pElem ){
		xmlChar *zPrefix = 0;
		xmlChar *zLocal = xmlSplitQName2((const xmlChar *)zName,&zPrefix);
		if( zLocal ){
			if( zPrefix ){
				xmlNsPtr pNs = xmlSearchNs(pElem->doc,pElem,zPrefix);
				if( pNs ){
					pAttr = xmlHasNsProp(pElem,zLocal,pNs->href);
				}
				xmlFree(zPrefix);
			}
			xmlFree(zLocal);
		}
	}
	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;
}
static xmlAttrPtr DomAttrByNs(xmlNodePtr pElem,const xmlChar *zUri,const char *zLocal)
{
	xmlAttrPtr pAttr = pElem ? xmlHasNsProp(pElem,(const xmlChar *)zLocal,zUri) : 0;
	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;
}

/* DOMElement::getAttribute(string $qualifiedName): string -- "" when absent */
DOM_METHOD(vm_builtin_DOMElement_getAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlNsPtr pDecl = DomNsDeclByName(pElem,zName);
	xmlAttrPtr pAttr = pDecl ? 0 : DomAttrByName(pElem,zName);
	xmlChar *zVal = pAttr ? xmlNodeListGetString(pAttr->doc,pAttr->children,1) : 0;
	if( pDecl ){
		/* A declaration's "value" is the URI it binds. */
		ph7_result_string(pCtx,pDecl->href ? (const char *)pDecl->href : "",-1);
		return PH7_OK;
	}
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
	ph7_result_bool(pCtx,pNd != 0
		&& (DomAttrByName((xmlNodePtr)pNd->pNode,zName) != 0
		 || DomNsDeclByName((xmlNodePtr)pNd->pNode,zName) != 0));
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
		return DomThrowAlways(pCtx,DOM_ERR_INVALID_CHAR);
	}
	if( DomNsDeclByName((xmlNodePtr)pNd->pNode,zName) ){
		/* php will not write THROUGH a declaration this element already makes:
		 * the write is dropped and the answer is false. (A name that is not one
		 * yet becomes an ordinary attribute, colon and all.) */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);
	pAttr = DomAttrByName((xmlNodePtr)pNd->pNode,zName);
	if( pAttr == 0 ){
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
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlAttrPtr pAttr = DomAttrByName(pElem,zName);
	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);
	if( pDecl ){
		/* The declaration goes, and whatever still needs it gets it back: php
		 * answers TRUE either way, and a binding nothing uses simply vanishes. */
		DomNsDeclRemove(pElem,pDecl);
		DomNsReconcile(pElem);
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( pAttr == 0 ){
		/* Absent (or a DTD default): php returns false */
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	xmlRemoveProp(pAttr);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * A `?string $namespace` argument as libxml wants it: NULL for php's null,
 * which is how a caller asks for the attribute in NO namespace, and the bytes
 * otherwise.  Handing libxml "" for a null instead is not the same question --
 * "" matches a namespace whose URI is the empty string, which no document has,
 * so `getAttributeNS(null, 'href')` answered "" for every plain attribute.
 */
static const xmlChar * DomArgUri(int nArg,ph7_value **apArg,int iArg)
{
	const char *z = DomArgStrOrNull(nArg,apArg,iArg);
	return (const xmlChar *)z;
}
/* DOMElement::getAttributeNS(?string $namespace, string $localName): string */
DOM_METHOD(vm_builtin_DOMElement_getAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const xmlChar *zUri = DomArgUri(nArg,apArg,0);
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,zUri) : 0;
	if( zVal == 0 && pNd && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){
		/* The other door, the one hasAttributeNS already knew about: a
		 * DECLARATION answers its URI here. `getAttributeNS($XMLNS, 'p')` was ""
		 * on an element declaring `xmlns:p`, where php answers the namespace. */
		xmlNsPtr pDecl = DomNsDeclOf((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal);
		if( pDecl && pDecl->href ){
			ph7_result_string(pCtx,(const char *)pDecl->href,-1);
			return PH7_OK;
		}
	}
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
	const char *zUri = DomArgStrOrNull(nArg,apArg,0);
	const char *zQname = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";
	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";
	int bHasUri = zUri != 0 && zUri[0] != 0;
	xmlNodePtr pNode;
	xmlNsPtr pNs = 0;
	dom_qname sQ;
	int rc,bDecl,nOldDefs;
	if( pNd == 0 ){
		return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);
	}
	if( zQname[0] == 0 ){
		/* php screens the EMPTY name at the parameter, before the DOM sees it. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMElement::setAttributeNS(): Argument #2 ($qualifiedName) must not be empty");
	}
	rc = DomQNameParse(zQname,zUri,DOM_QN_SET,&sQ);
	if( rc ){
		return DomThrowVoid(pCtx,rc);
	}
	pNode = (xmlNodePtr)pNd->pNode;
	nOldDefs = DomNsDefCount(pNode);
	/* The DECLARATION spelling is the xmlns NAMESPACE plus an xmlns name --
	 * `xmlns:z` binds z, plain `xmlns` binds the default one. Everything else
	 * is an ordinary attribute, including a bare `xmlns` under some other URI:
	 * php writes it as an ATTRIBUTE whose name happens to be `xmlns`, which
	 * serializes beside the declaration already there. */
	bDecl = DomUriIs(zUri,DOM_XMLNS_NS_URI)
		&& (sQ.zPrefix ? xmlStrEqual(sQ.zPrefix,(const xmlChar *)"xmlns")
		               : xmlStrEqual(sQ.zLocal,(const xmlChar *)"xmlns"));
	if( bHasUri && !bDecl ){
		/* An ordinary attribute: find or declare the namespace it names. The
		 * xmlns URI is not special here -- php declares it like any other,
		 * EXCEPT under a prefix, which is the one binding it will not write.
		 * The same goes for the prefix `xmlns` itself: php will REUSE a binding
		 * for it (which is how `setAttributeNS(XML_NS,'xmlns:z')` ends up as
		 * `xml:z`) and refuses to declare one. */
		if( sQ.zPrefix && DomUriIs(zUri,DOM_XMLNS_NS_URI) ){
			DomQNameRelease(&sQ);
			return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);
		}
		pNs = DomNsResolve(pNode,zUri,sQ.zPrefix,0);
		if( pNs == 0 ){
			DomQNameRelease(&sQ);
			return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);
		}
	}
	if( bDecl ){
		/* The declaration spelling: `xmlns:z` binds z, plain `xmlns` binds the
		 * default namespace, and the VALUE is the URI being bound. */
		DomNsDeclare(pNode,sQ.zPrefix ? sQ.zLocal : 0,zVal);
	}else{
		xmlSetNsProp(pNode,pNs,sQ.zLocal,(const xmlChar *)zVal);
	}
	/* Either path may have declared something here -- and a declaration, new or
	 * rebound, can take the spelling away from what is already below it. */
	if( bDecl || DomNsDefCount(pNode) != nOldDefs ){
		DomNsReconcile(pNode);
	}
	DomQNameRelease(&sQ);
	return PH7_OK;
}

/* ===== Attribute NODES ===== */

/*
 * The half of the attribute surface that hands out (and takes) the attribute
 * NODE rather than its string.  It is how a program moves an attribute between
 * elements, reads one it has held across an edit, or asks which attributes an
 * element carries at all -- and none of it existed here, so `getAttributeNode`
 * was a `Call to undefined method` and every idiom built on it stopped at the
 * first line.
 */

/* The xmlAttr a DOMAttr-typed argument stands for (already screened by ZPP:
 * anything that is not a DOMAttr never reaches the body). */
static xmlAttrPtr DomAttrArg(ph7_value *pVal)
{
	phl_domnode *pNd = DomObjArg(pVal);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	return (pNode && pNode->type == XML_ATTRIBUTE_NODE) ? (xmlAttrPtr)pNode : 0;
}
/* The plain setAttributeNode spelling asks libxml's own name-only question --
 * php's does too, so an attribute named `b` displaces a namespaced `x:b`
 * there where the NS spelling would not. */
static xmlAttrPtr DomAttrByLocal(xmlNodePtr pElem,const char *zName)
{
	xmlAttrPtr pAttr = pElem ? xmlHasProp(pElem,(const xmlChar *)zName) : 0;
	return (pAttr && pAttr->type == XML_ATTRIBUTE_NODE) ? pAttr : 0;
}
/* Append an attribute to an element's property list. The list is its own chain
 * (pElem->properties), not the child chain, which is why DomLinkLast will not
 * do: an attribute spliced among the CHILDREN serializes inside the tag body. */
static void DomAttrLinkLast(xmlNodePtr pElem,xmlAttrPtr pAttr)
{
	xmlAttrPtr pLast = pElem->properties;
	pAttr->parent = pElem;
	pAttr->doc = pElem->doc;
	pAttr->next = 0;
	if( pLast == 0 ){
		pElem->properties = pAttr;
		pAttr->prev = 0;
		return;
	}
	while( pLast->next ){
		pLast = pLast->next;
	}
	pLast->next = pAttr;
	pAttr->prev = pLast;
}
/* Detach an attribute from its element (or the orphan set) without freeing it:
 * php hands the caller back the node it displaced, alive. */
static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr)
{
	if( pAttr->parent ){
		xmlUnlinkNode((xmlNodePtr)pAttr);
	}else{
		DomOrphanRemove(pShell,(xmlNodePtr)pAttr);
	}
}
/* DOMElement::getAttributeNode(string $qualifiedName): DOMAttr|false -- FALSE
 * for an absent one, where the NS spelling below answers null. */
DOM_METHOD(vm_builtin_DOMElement_getAttributeNode)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlAttrPtr pAttr = DomAttrByName(pElem,zName);
	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);
	if( pDecl ){
		return DomResultNsNode(pCtx,pNd,pDecl,pElem);
	}
	if( pAttr == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);
}
/* DOMElement::getAttributeNodeNS(?string $namespace, string $localName): ?DOMAttr */
DOM_METHOD(vm_builtin_DOMElement_getAttributeNodeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	const xmlChar *zUri = DomArgUri(nArg,apArg,0);
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){
		/* Here the LOCAL name is the prefix being declared -- and the DEFAULT
		 * declaration, whose local name would be `xmlns`, is not reachable this
		 * way at all. */
		xmlNsPtr pDecl = DomNsDeclOf(pElem,(const xmlChar *)zLocal);
		if( pDecl == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		return DomResultNsNode(pCtx,pNd,pDecl,pElem);
	}
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)DomAttrByNs(pElem,zUri,zLocal));
}
/*
 * DOMElement::setAttributeNode(DOMAttr $attr): ?DOMAttr and its NS spelling.
 *
 * php answers the attribute it DISPLACED (alive and ownerless) or null, and
 * the two spellings differ only in how they decide what "the same attribute"
 * is: the plain one matches on the local name alone, the NS one on the name
 * and the namespace URI together.  An attribute that already belongs to
 * another element of the same document is MOVED, not copied.
 */
static int DomSetAttrNode(ph7_context *pCtx,int nArg,ph7_value **apArg,int bNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlAttrPtr pAttr = nArg > 0 ? DomAttrArg(apArg[0]) : 0;
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlAttrPtr pOld;
	if( pElem == 0 || pAttr == 0 ){
		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);
	}
	if( pAttr->doc != pElem->doc ){
		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);
	}
	pOld = bNS ? DomAttrByNs(pElem,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name)
	           : DomAttrByLocal(pElem,(const char *)pAttr->name);
	if( pOld == pAttr ){
		/* Already this element's, under this spelling: php does nothing at all
		 * and answers null rather than handing the node back to itself. */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pOld ){
		xmlUnlinkNode((xmlNodePtr)pOld);
		DomOrphanAdd(pNd->pShell,(xmlNodePtr)pOld);
	}
	DomAttrDetach(pNd->pShell,pAttr);
	DomAttrLinkLast(pElem,pAttr);
	DomNsAttrArrive(pElem,pAttr);
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pOld);
}
DOM_METHOD(vm_builtin_DOMElement_setAttributeNode)
{
	return DomSetAttrNode(pCtx,nArg,apArg,0);
}
DOM_METHOD(vm_builtin_DOMElement_setAttributeNodeNS)
{
	return DomSetAttrNode(pCtx,nArg,apArg,1);
}
/* DOMElement::removeAttributeNode(DOMAttr $attr): DOMAttr -- an attribute that
 * is not THIS element's is php's Not Found, whichever element owns it. */
DOM_METHOD(vm_builtin_DOMElement_removeAttributeNode)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlAttrPtr pAttr = nArg > 0 ? DomAttrArg(apArg[0]) : 0;
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pElem == 0 || pAttr == 0 || pAttr->parent != pElem ){
		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);
	}
	xmlUnlinkNode((xmlNodePtr)pAttr);
	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);
}
/* The qualified name a serializer would write for an attribute. */
static int DomAttrQName(xmlAttrPtr pAttr,SyBlob *pOut)
{
	if( pAttr->ns && pAttr->ns->prefix ){
		SyBlobAppend(pOut,(const void *)pAttr->ns->prefix,SyStrlen((const char *)pAttr->ns->prefix));
		SyBlobAppend(pOut,(const void *)":",sizeof(char));
	}
	SyBlobAppend(pOut,(const void *)pAttr->name,SyStrlen((const char *)pAttr->name));
	return PH7_OK;
}
/* DOMElement::getAttributeNames(): array -- the qualified names, in document
 * order, as a LIST (php re-keys from zero). */
DOM_METHOD(vm_builtin_DOMElement_getAttributeNames)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	ph7_value *pArray = ph7_context_new_array(pCtx);
	ph7_value *pVal = ph7_context_new_scalar(pCtx);
	xmlAttrPtr pAttr;
	xmlNsPtr pNs;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pArray == 0 || pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* php lists the element's own DECLARATIONS first, in the order it makes
	 * them, and the attributes after. */
	for( pNs = pNd && ((xmlNodePtr)pNd->pNode)->type == XML_ELEMENT_NODE
			? ((xmlNodePtr)pNd->pNode)->nsDef : 0 ; pNs ; pNs = pNs->next ){
		SyBlob sName;
		SyBlobInit(&sName,&pCtx->pVm->sAllocator);
		SyBlobAppend(&sName,(const void *)DOM_XMLNS_NAME,SyStrlen(DOM_XMLNS_NAME));
		if( pNs->prefix ){
			SyBlobAppend(&sName,(const void *)":",sizeof(char));
			SyBlobAppend(&sName,(const void *)pNs->prefix,SyStrlen((const char *)pNs->prefix));
		}
		ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));
		ph7_array_add_elem(pArray,0,pVal);
		ph7_value_reset_string_cursor(pVal);
		SyBlobRelease(&sName);
	}
	for( pAttr = DomAttrList(pNd ? (xmlNodePtr)pNd->pNode : 0) ; pAttr ; pAttr = pAttr->next ){
		SyBlob sName;
		if( pAttr->type != XML_ATTRIBUTE_NODE ){
			continue;
		}
		SyBlobInit(&sName,&pCtx->pVm->sAllocator);
		DomAttrQName(pAttr,&sName);
		ph7_value_string_format(pVal,"%.*s",(int)SyBlobLength(&sName),(const char *)SyBlobData(&sName));
		ph7_array_add_elem(pArray,0,pVal);
		ph7_value_reset_string_cursor(pVal);
		SyBlobRelease(&sName);
	}
	ph7_result_value(pCtx,pArray);
	return PH7_OK;
}
/* DOMElement::hasAttributeNS(?string $namespace, string $localName): bool */
DOM_METHOD(vm_builtin_DOMElement_hasAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	const xmlChar *zUri = DomArgUri(nArg,apArg,0);
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){
		/*
		 * Two doors onto that namespace, and php answers about EITHER: a
		 * DECLARATION, which is not an attribute in libxml at all, and a real
		 * attribute in it -- which is what `createAttributeNS($XMLNS, ...)`
		 * makes, and which this only asked the first door about. (A DEFAULT
		 * declaration is not one of them: php answers false for the local name
		 * `xmlns`, and the prefix comparison below never matches it.)
		 */
		ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0
			|| DomNsDeclOf(pElem,(const xmlChar *)zLocal) != 0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0);
	return PH7_OK;
}
/* DOMElement::removeAttributeNS(?string $namespace, string $localName): void --
 * an absent one is silence, as php's is. */
DOM_METHOD(vm_builtin_DOMElement_removeAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlAttrPtr pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,DomArgUri(nArg,apArg,0),zLocal) : 0;
	if( pAttr ){
		xmlRemoveProp(pAttr);
	}
	return PH7_OK;
}
/*
 * DOMElement::toggleAttribute(string $qualifiedName, ?bool $force = null): bool
 *
 * php's 8.3 verb: with no $force it flips (removing answers false, adding a
 * value-less attribute answers true), and with one it only ADDS or only
 * REMOVES -- an add that finds the attribute already there leaves its value
 * alone.  The name is validated first, so a bad one is refused before the
 * element is touched.
 */
DOM_METHOD(vm_builtin_DOMElement_toggleAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int bForceGiven = nArg > 1 && !ph7_value_is_null(apArg[1]);
	int bForce = bForceGiven && ph7_value_to_bool(apArg[1]);
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlAttrPtr pAttr;
	xmlNsPtr pDecl;
	if( pElem == 0 || xmlValidateName((const xmlChar *)zName,0) != 0 ){
		return DomThrowAlways(pCtx,DOM_ERR_INVALID_CHAR);
	}
	pAttr = DomAttrByName(pElem,zName);
	pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);
	if( bForceGiven ? bForce : (pAttr == 0 && pDecl == 0) ){
		if( pAttr == 0 && pDecl == 0 ){
			sxu32 nXmlns = (sxu32)SyStrlen(DOM_XMLNS_NAME);
			int bXmlnsName = SyStrlen(zName) >= nXmlns
				&& SyStrncmp(zName,DOM_XMLNS_NAME,nXmlns) == 0
				&& (zName[nXmlns] == 0 || zName[nXmlns] == ':');
			if( bXmlnsName ){
				/* An xmlns name toggled ON becomes a DECLARATION bound to the
				 * empty URI, not an attribute -- which is why it comes out
				 * before the attributes rather than after them. */
				DomNsDeclare(pElem,zName[nXmlns] == ':' ? (const xmlChar *)(zName+nXmlns+1) : 0,"");
			}else{
				xmlSetProp(pElem,(const xmlChar *)zName,(const xmlChar *)"");
			}
		}
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( pAttr ){
		xmlRemoveProp(pAttr);
	}else if( pDecl ){
		DomNsDeclRemove(pElem,pDecl);
		DomNsReconcile(pElem);
	}
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
/*
 * The ID three.  php's `setIdAttribute*` is what makes an attribute the one
 * `getElementById()` answers by, in a document with no DTD to say so, and
 * `DOMAttr::isId()` is how a caller reads the flag back.  All three refuse a
 * name the element does not carry with Not Found -- including an attribute that
 * belongs to a DIFFERENT element, which is why the node spelling checks the
 * owner rather than trusting the argument.
 */
static int DomMarkId(xmlAttrPtr pAttr,int bIsId)
{
	if( bIsId ){
		if( pAttr->atype != XML_ATTRIBUTE_ID ){
			xmlChar *zVal = xmlNodeListGetString(pAttr->doc,pAttr->children,1);
			if( zVal ){
				xmlAddID(0,pAttr->doc,zVal,pAttr);
				xmlFree(zVal);
			}
		}
		pAttr->atype = XML_ATTRIBUTE_ID;
	}else{
		if( pAttr->atype == XML_ATTRIBUTE_ID ){
			xmlRemoveID(pAttr->doc,pAttr);
		}
		pAttr->atype = (xmlAttributeType)0;
	}
	return PH7_OK;
}
DOM_METHOD(vm_builtin_DOMElement_setIdAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[0],0) : "";
	/* php's ID lookup is the STRICT one -- the whole name in NO namespace, with
	 * no prefix resolution, so `setIdAttribute('p:k')` is Not Found even on an
	 * element that carries `p:k`. */
	xmlAttrPtr pAttr = pNd ? DomAttrNoNs((xmlNodePtr)pNd->pNode,zName) : 0;
	if( pAttr == 0 ){
		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);
	}
	return DomMarkId(pAttr,nArg > 1 && ph7_value_to_bool(apArg[1]));
}
/* php's second parameter is spelled $qualifiedName and matched as a LOCAL one:
 * the namespace decides the rest. */
DOM_METHOD(vm_builtin_DOMElement_setIdAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zLocal = nArg > 2 ? ph7_value_to_string(apArg[1],0) : "";
	xmlAttrPtr pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,DomArgUri(nArg,apArg,0),zLocal) : 0;
	if( pAttr == 0 ){
		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);
	}
	return DomMarkId(pAttr,nArg > 2 && ph7_value_to_bool(apArg[2]));
}
DOM_METHOD(vm_builtin_DOMElement_setIdAttributeNode)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlAttrPtr pAttr = nArg > 1 ? DomAttrArg(apArg[0]) : 0;
	if( pNd == 0 || pAttr == 0 || pAttr->parent != (xmlNodePtr)pNd->pNode ){
		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);
	}
	return DomMarkId(pAttr,nArg > 1 && ph7_value_to_bool(apArg[1]));
}
/* DOMAttr::isId(): bool */
DOM_METHOD(vm_builtin_DOMAttr_isId)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,pNode != 0 && pNode->type == XML_ATTRIBUTE_NODE
		&& ((xmlAttrPtr)pNode)->atype == XML_ATTRIBUTE_ID);
	return PH7_OK;
}
/*
 * DOMDocument::getElementById(string $elementId): ?DOMElement
 *
 * The other half of the ID three: what `setIdAttribute()` is FOR. libxml keeps
 * the table (a DTD `ATTLIST ... ID` fills it at parse time, `xmlAddID` fills it
 * when a program marks one), and php answers the attribute's element -- but
 * only while that element is still IN the document, so an element removed from
 * the tree stops being findable even though its attribute still carries the
 * flag. A DTD-declared DEFAULT has no attribute node behind it and libxml says
 * so with its own sentinel.
 */
DOM_METHOD(vm_builtin_DOMDocument_getElementById)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zId = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlAttrPtr pAttr = pNd ? xmlGetID((xmlDocPtr)pNd->pNode,(const xmlChar *)zId) : 0;
	if( pAttr == 0 || pAttr == (xmlAttrPtr)-1 || pAttr->type != XML_ATTRIBUTE_NODE
	 || pAttr->parent == 0 || !DomIsConnected(pAttr->parent) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultNodeOf(pCtx,pNd,pAttr->parent);
}
/* DOMDocument::createAttribute(string $localName): DOMAttr -- ownerless: php
 * gives it this document but NO element until it is set on one. */
DOM_METHOD(vm_builtin_DOMDocument_createAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlAttrPtr pAttr;
	if( pNd == 0 || xmlValidateName((const xmlChar *)zName,0) != 0 ){
		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);
	}
	pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,(const xmlChar *)zName,0);
	if( pAttr == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);
}
/*
 * DOMDocument::createAttributeNS(?string $namespace, string $qualifiedName): DOMAttr
 *
 * The namespace is declared on the document's ROOT ELEMENT, not on the
 * attribute -- which is why a document that has no root element yet cannot
 * answer at all, and says so with php's warning and a false.
 */
DOM_METHOD(vm_builtin_DOMDocument_createAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zUri = DomArgStrOrNull(nArg,apArg,0);
	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlNodePtr pRoot;
	xmlAttrPtr pAttr;
	dom_qname sQ;
	int rc;
	if( pNd == 0 ){
		return DomThrow(pCtx,DOM_ERR_NAMESPACE);
	}
	rc = DomQNameParse(zQname,zUri,DOM_QN_ATTR,&sQ);
	if( rc ){
		return DomThrow(pCtx,rc);
	}
	pRoot = xmlDocGetRootElement((xmlDocPtr)pNd->pNode);
	if( pRoot == 0 ){
		DomQNameRelease(&sQ);
		/* The context prints php's `DOMDocument::createAttributeNS(): ` itself. */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Document Missing Root Element");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,sQ.zLocal,0);
	if( pAttr == 0 ){
		DomQNameRelease(&sQ);
		return PH7_ContextMemoryError(pCtx);
	}
	if( zUri && zUri[0] && sQ.zPrefix ){
		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,sQ.zPrefix,1));
	}else if( zUri && zUri[0]
	 && xmlStrEqual(sQ.zLocal,(const xmlChar *)DOM_XMLNS_NAME) ){
		/*
		 * The unprefixed `xmlns` -- the only name the grammar lets through with
		 * the xmlns namespace, and the name a DEFAULT declaration is written
		 * with. It IS in that namespace and php answers so, but nothing is
		 * DECLARED for it: the binding is the attribute's alone, so it is parked
		 * on the document, which is what frees it. Without this the attribute
		 * answered namespaceURI null, `hasAttributeNS($XMLNS, 'xmlns')` was
		 * false for it once written, and `getAttribute('xmlns')` -- a by-NAME
		 * lookup, which skips a namespaced attribute -- answered its value where
		 * php answers "".
		 */
		xmlNsPtr pNs = DomNsReuse(pRoot,zUri);
		if( pNs == 0 ){
			pNs = xmlNewNs(0,(const xmlChar *)zUri,0);
			if( pNs ){
				DomNsPark((xmlNodePtr)pAttr,pNs);
			}
		}
		if( pNs ){
			xmlSetNs((xmlNodePtr)pAttr,pNs);
		}
	}else if( zUri && zUri[0] ){
		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,0,1));
	}
	DomQNameRelease(&sQ);
	DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);
}

/* ===== getElementsByTagName (live) ===== */

/* Length-carrying: the name comes from a declared string SLOT, whose bytes are
 * NOT NUL-terminated (PH7_NativeAttrStr borrows the blob as-is). */
static int DomLenEq(const xmlChar *zHave,const char *zWant,int nWant)
{
	return zHave != 0 && (int)SyStrlen((const char *)zHave) == nWant
		&& SyMemcmp((const void *)zHave,(const void *)zWant,(sxu32)nWant) == 0;
}
/*
 * One element against a (namespace, local name) pair. nUri < 0 is the name-only
 * query, `getElementsByTagName` -- the sentinel is the LENGTH and not the
 * pointer because an empty declared string slot reads back as a NULL one, which
 * is exactly the namespace-aware "in NO namespace" case. Under the
 * namespace-aware query php's three cases are NOT symmetric, and that asymmetry
 * is the whole content of the rule:
 *
 *   `*`          every element, in a namespace or in none
 *   null or ""   only the elements in NO namespace (php maps its null argument
 *                and the empty string to the same question)
 *   a URI        only the elements in it
 *
 * The local name is `*` for every name, and an exact match otherwise -- against
 * libxml's `name`, which is the LOCAL name, so a prefix never enters into it.
 */
static int DomGebtnMatch(xmlNodePtr pNode,const char *zUri,int nUri,
	const char *zName,int nName)
{
	if( pNode->type != XML_ELEMENT_NODE ){
		return 0;
	}
	if( !(nName == 1 && zName[0] == '*') && !DomLenEq(pNode->name,zName,nName) ){
		return 0;
	}
	if( nUri < 0 || (nUri == 1 && zUri[0] == '*') ){
		return 1;
	}
	if( nUri == 0 ){
		return pNode->ns == 0;
	}
	return pNode->ns != 0 && DomLenEq(pNode->ns->href,zUri,nUri);
}
/* The list is LIVE: nothing is snapshotted, both queries re-walk the subtree
 * every time DOMNodeList asks. Passing iWant < 0 counts instead of indexing. */
static xmlNodePtr DomGebtnWalk(xmlNodePtr pRoot,const char *zUri,int nUri,
	const char *zName,int nName,int iWant,int *pnCount)
{
	xmlNodePtr pCur = pRoot ? pRoot->children : 0;
	int iCount = 0;
	while( pCur ){
		if( DomGebtnMatch(pCur,zUri,nUri,zName,nName) ){
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
	/* The version goes through as WRITTEN -- `new DOMDocument('')` is a document
	 * whose `version` reads "" and whose declaration says `version=""`, which is
	 * php's answer (only an omitted argument takes the "1.0" default, and that
	 * one is the signature's). */
	pDoc = xmlNewDoc((const xmlChar *)zVersion);
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
/*
 * The URI php stamps on a document parsed from MEMORY.
 *
 * A file parse takes its URI from the file; a memory parse has none, and php
 * gives it the process's CURRENT DIRECTORY with a trailing separator so that a
 * relative `xml:base` (and every `baseURI` under it) resolves against the same
 * place a relative include would. The path is the bytes getcwd() answers, not a
 * URI: a space stays a space. Asks the VFS rather than the C library so the
 * win32 backend answers its own spelling.
 *
 * The answer travels through the context's RESULT slot, which is where the VFS
 * writes it -- every caller sets its own return value afterwards.
 */
static void DomCwdUri(ph7_context *pCtx,SyBlob *pOut)
{
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	SyBlobInit(pOut,&pCtx->pVm->sAllocator);
	PH7_MemObjRelease(pCtx->pRet);
	if( pVfs && pVfs->xGetcwd && pVfs->xGetcwd(pCtx) == PH7_OK
	 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0 ){
		const char *zDir = (const char *)SyBlobData(&pCtx->pRet->sBlob);
		sxu32 nDir = SyBlobLength(&pCtx->pRet->sBlob);
		SyBlobAppend(pOut,zDir,nDir);
		if( nDir < 1 || (zDir[nDir - 1] != '/' && zDir[nDir - 1] != '\\') ){
			SyBlobAppend(pOut,"/",sizeof(char));
		}
	}
	PH7_MemObjRelease(pCtx->pRet);
	SyBlobNullAppend(pOut);
}
/* Stamp it, unless the parse already gave the document one. */
static void DomStampCwd(ph7_context *pCtx,xmlDocPtr pDoc)
{
	SyBlob sDir;
	if( pDoc == 0 || pDoc->URL ){
		return;
	}
	DomCwdUri(pCtx,&sDir);
	if( SyBlobLength(&sDir) > 0 ){
		pDoc->URL = xmlStrdup((const xmlChar *)SyBlobData(&sDir));
	}
	SyBlobRelease(&sDir);
}
/*
 * The parse options one of php's load methods actually runs with: the caller's
 * `$options`, OR'd with what the document's own directives ask for.
 *
 *   preserveWhiteSpace = false  ->  NOBLANKS   (drop ignorable whitespace)
 *   substituteEntities = true   ->  NOENT      (expand entity references)
 *   validateOnParse    = true   ->  DTDVALID   (validate against the DTD)
 *   resolveExternals   = true   ->  DTDATTR    (apply the DTD's default
 *                                               attributes -- which is also
 *                                               what makes libxml LOAD an
 *                                               external subset)
 *   recover            = true   ->  RECOVER    (keep what parsed)
 *
 * The two directions never cancel: a directive can only ADD to the argument,
 * which is why `loadXML($s, LIBXML_NOENT)` expands entities on a document whose
 * `substituteEntities` is false.
 */
static int DomParseOptions(ph7_class_instance *pThis,int iOpts)
{
	if( pThis == 0 ){
		return iOpts;
	}
	if( !PH7_NativeAttrTruthy(pThis,"preserveWhiteSpace") ){
		iOpts |= XML_PARSE_NOBLANKS;
	}
	if( PH7_NativeAttrTruthy(pThis,"substituteEntities") ){
		iOpts |= XML_PARSE_NOENT;
	}
	if( PH7_NativeAttrTruthy(pThis,"validateOnParse") ){
		iOpts |= XML_PARSE_DTDVALID;
	}
	if( PH7_NativeAttrTruthy(pThis,"resolveExternals") ){
		iOpts |= XML_PARSE_DTDATTR;
	}
	if( PH7_NativeAttrTruthy(pThis,"recover") ){
		iOpts |= XML_PARSE_RECOVER;
	}
	return iOpts;
}
/*
 * A RECOVERING parse reports its diagnostics whatever `error_reporting()` says.
 *
 * php forces E_WARNING back into the mask for the duration of a parse it is
 * recovering from -- the point being that a document which came back DAMAGED
 * must not do so in silence, however the script has configured reporting. It
 * forces that one level only: a libxml WARNING (an E_NOTICE) stays suppressed.
 * Answers the previous state, which the caller restores.
 */
typedef struct { sxi32 iMask; int bOn; } phl_dom_errsave;
static phl_dom_errsave DomForceWarnings(ph7_vm *pVm,int bRecover)
{
	phl_dom_errsave sSave;
	sSave.iMask = pVm->iErrMask;
	sSave.bOn = pVm->bErrReport;
	if( bRecover ){
		pVm->iErrMask |= E_WARNING;
		pVm->bErrReport = 1;
	}
	return sSave;
}
static void DomRestoreWarnings(ph7_vm *pVm,phl_dom_errsave sSave)
{
	pVm->iErrMask = sSave.iMask;
	pVm->bErrReport = sSave.bOn;
}
/*
 * The path php names a loaded file by: the VFS's canonical absolute name, or
 * the working directory joined to it when the file does not exist and there is
 * nothing to canonicalize. Answers it NUL-terminated in *pOut.
 */
static void DomAbsPath(ph7_context *pCtx,const char *zFile,SyBlob *pOut)
{
	const ph7_vfs *pVfs = pCtx->pVm->pEngine->pVfs;
	int bAbs = zFile[0] == '/' || zFile[0] == '\\'
		|| (zFile[0] && zFile[1] == ':');   /* the win32 spelling */
	SyBlobInit(pOut,&pCtx->pVm->sAllocator);
	PH7_MemObjRelease(pCtx->pRet);
	if( pVfs && pVfs->xRealpath && pVfs->xRealpath(zFile,pCtx) == PH7_OK
	 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0
	 && SyBlobLength(&pCtx->pRet->sBlob) > 0 ){
		SyBlobAppend(pOut,SyBlobData(&pCtx->pRet->sBlob),SyBlobLength(&pCtx->pRet->sBlob));
	}else{
		/* Not there to canonicalize -- but php still resolves the part that IS
		 * there (expand_filepath walks each existing component through its
		 * links), so a missing file under a linked directory is named by the
		 * directory's real path; on macOS every temp path is one (/var ->
		 * /private/var). The longest existing prefix is resolved and the missing
		 * tail kept as written. A bare drive ("C:") is never a prefix: it would
		 * resolve to that drive's working directory. */
		SyBlob sFull,sPre;
		const char *zFull;
		sxu32 nFull,i,nTail = 0;
		SyBlobInit(&sFull,&pCtx->pVm->sAllocator);
		SyBlobInit(&sPre,&pCtx->pVm->sAllocator);
		if( !bAbs ){
			SyBlob sDir;
			DomCwdUri(pCtx,&sDir);
			SyBlobAppend(&sFull,SyBlobData(&sDir),SyBlobLength(&sDir));
			SyBlobRelease(&sDir);
		}
		SyBlobAppend(&sFull,zFile,(sxu32)SyStrlen(zFile));
		SyBlobNullAppend(&sFull);
		zFull = (const char *)SyBlobData(&sFull);
		nFull = (sxu32)SyStrlen(zFull);
		for( i = nFull ; i > 1 && pVfs && pVfs->xRealpath ; --i ){
			if( (zFull[i-1] != '/' && zFull[i-1] != '\\') || zFull[i-2] == ':' ){
				continue;
			}
			SyBlobReset(&sPre);
			SyBlobAppend(&sPre,zFull,i-1);
			SyBlobNullAppend(&sPre);
			PH7_MemObjRelease(pCtx->pRet);
			if( pVfs->xRealpath((const char *)SyBlobData(&sPre),pCtx) == PH7_OK
			 && (pCtx->pRet->iFlags & MEMOBJ_STRING) != 0
			 && SyBlobLength(&pCtx->pRet->sBlob) > 0 ){
				SyBlobAppend(pOut,SyBlobData(&pCtx->pRet->sBlob),SyBlobLength(&pCtx->pRet->sBlob));
				nTail = nFull - (i-1);
				break;
			}
		}
		if( nTail > 0 ){
			SyBlobAppend(pOut,zFull + (nFull - nTail),nTail);
		}else{
			SyBlobAppend(pOut,zFull,nFull);
		}
		SyBlobRelease(&sPre);
		SyBlobRelease(&sFull);
	}
	PH7_MemObjRelease(pCtx->pRet);
	SyBlobNullAppend(pOut);
}
/*
 * Point the receiver at a freshly parsed tree, as both load methods do: the
 * document object keeps its identity and everything under the OLD tree becomes
 * stale, so the per-document wrapper cache is dropped with it. Answers 0 when
 * there is no tree to install, which is each method's `false`; the previous
 * tree is left alone in that case, as php leaves it.
 */
static int DomInstallParsed(ph7_context *pCtx,ph7_class_instance *pThis,xmlDocPtr pDoc)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_xmldoc *pShell = pDoc ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;
	phl_domnode *pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;
	ph7_value *pNodes;
	if( pRes == 0 ){
		if( pDoc && pShell == 0 ){
			xmlFreeDoc(pDoc);
		}
		return 0;
	}
	DomSetRes(pVm,pThis,pRes);
	pNodes = PH7_NativeAttr(pThis,DOM_NODES);
	if( pNodes ){
		PH7_MemObjRelease(pNodes);
		PH7_MemObjToHashmap(pNodes);
	}
	return 1;
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
	phl_dom_errsave sErr;
	xmlDocPtr pDoc;
	sxu32 nMark;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( nLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMDocument::loadXML(): Argument #1 ($source) must not be empty");
	}
	iOpts = DomParseOptions(pThis,iOpts);
	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pDoc = xmlReadMemory(zSrc,nLen,0,0,iOpts);
	PH7_LibxmlCaptureEndOpts(pVm,nMark,"DOMDocument::loadXML",iOpts);
	DomRestoreWarnings(pVm,sErr);
	DomStampCwd(pCtx,pDoc);
	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));
	return PH7_OK;
}
/*
 * DOMDocument::load(string $filename, int $options = 0): bool
 *
 * The same parse as loadXML from a FILE, and php reads that file through its
 * own stream layer (which is what makes a wrapper and a userland stream valid
 * destinations there, and what this does too) while letting libxml word the
 * failure. What only a differential decides:
 *
 *   * a file that is not THERE is libxml's own `I/O warning : failed to load
 *     external entity "<path>"` and nothing else, while one that exists and
 *     cannot be opened ALSO gets php's stream warning in front of it;
 *   * the document's URI is the RESOLVED absolute path -- so `load('a/../b.xml')`
 *     answers the canonical name -- and it is a URI, not a path: a space in it
 *     comes back as `%20`;
 *   * a failed load leaves the receiver's previous tree exactly where it was.
 */
/*
 * Read a file for one of the two file-loading methods: the bytes into *pBody,
 * the name libxml is to know it by into *pPath. Answers 0 when the file could
 * not be read at all, with php's diagnostics already raised -- the receiver is
 * left alone then, and the method answers false.
 */
static int DomReadFileAs(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,
	SyBlob *pBody,SyBlob *pPath,int bVerbatim);
static int DomReadFile(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,
	SyBlob *pBody,SyBlob *pPath)
{
	return DomReadFileAs(pCtx,zFile,nFile,zFn,pBody,pPath,0);
}
/*
 * bVerbatim: libxml is to know the file by the path AS WRITTEN rather than by
 * its canonical absolute name -- loadHTMLFile's rule (see its call).
 */
static int DomReadFileAs(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,
	SyBlob *pBody,SyBlob *pPath,int bVerbatim)
{
	ph7_vm *pVm = pCtx->pVm;
	const ph7_io_stream *pStream;
	void *pHandle;
	if( bVerbatim ){
		SyBlobInit(pPath,&pVm->sAllocator);
		SyBlobAppend(pPath,zFile,(sxu32)nFile);
		SyBlobNullAppend(pPath);
	}else{
		DomAbsPath(pCtx,zFile,pPath);
	}
	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);
	pHandle = (pStream && pStream->xRead) ? PH7_StreamOpenHandle(pVm,pStream,zFile,
		PH7_IO_OPEN_RDONLY,FALSE,0,FALSE,0,ph7_function_name(pCtx)) : 0;
	if( pHandle == 0 ){
		/* php's stream layer says nothing about a file that is simply absent --
		 * only libxml does, in its own words and with no source location. A file
		 * that IS there and would not open (a mode, a lock) gets both. */
		const ph7_vfs *pVfs = pVm->pEngine->pVfs;
		SyBlob sMsg;
		sxu32 nMark;
		if( pVfs && pVfs->xFileExists && pVfs->xFileExists(zFile) == PH7_OK ){
			VfsThrowOpenWarning(pCtx,zFile);
		}
		SyBlobInit(&sMsg,&pVm->sAllocator);
		SyBlobFormat(&sMsg,"failed to load external entity \"%s\"\n",
			(const char *)SyBlobData(pPath));
		SyBlobNullAppend(&sMsg);
		/* Both of libxml's channels, as php feeds them: the structured copy is
		 * what `libxml_get_errors()`/`libxml_get_last_error()` answer (level
		 * WARNING, no file, no line), and the generic one is the text that gets
		 * PRINTED -- with the severity spelled into it and at E_WARNING. */
		nMark = PH7_LibxmlCaptureBegin(pVm);
		PH7_LibxmlQueueError(pVm,XML_ERR_WARNING,XML_IO_LOAD_ERROR,0,0,
			(const char *)SyBlobData(&sMsg),0);
		if( pVm->bLibxmlInternalErr ){
			xmlSetStructuredErrorFunc(0,0);   /* nothing to drain: it stays queued */
		}else{
			SyBlob sGen;
			PH7_LibxmlDropErrors(pVm,nMark);
			SyBlobInit(&sGen,&pVm->sAllocator);
			SyBlobFormat(&sGen,"I/O warning : %s",(const char *)SyBlobData(&sMsg));
			SyBlobNullAppend(&sGen);
			PH7_LibxmlRaiseGeneric(pVm,zFn,(const char *)SyBlobData(&sGen));
			SyBlobRelease(&sGen);
		}
		SyBlobRelease(&sMsg);
		SyBlobRelease(pPath);
		return 0;
	}
	SyBlobInit(pBody,&pVm->sAllocator);
	PH7_StreamReadWholeFile(pHandle,pStream,pBody);
	PH7_StreamCloseHandle(pStream,pHandle);
	return 1;
}
DOM_METHOD(vm_builtin_DOMDocument_load)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zFile;
	int nFile = 0;
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	phl_dom_errsave sErr;
	SyBlob sBody,sPath;
	xmlDocPtr pDoc;
	sxu32 nMark;
	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( nFile != (int)SyStrlen(zFile) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMDocument::load(): Argument #1 ($filename) must not contain any null bytes");
	}
	if( nFile < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMDocument::load(): Argument #1 ($filename) must not be empty");
	}
	if( !DomReadFile(pCtx,zFile,nFile,"DOMDocument::load",&sBody,&sPath) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( SyBlobLength(&sBody) < 1 ){
		/* libxml's memory parser will not even start on nothing, so the error
		 * php's FILE parser raises there is queued by hand -- same level, same
		 * code, same wording, so `libxml_get_errors()` reports what php's does
		 * and the drain prints php's sentence. */
		nMark = PH7_LibxmlCaptureBegin(pVm);
		PH7_LibxmlQueueError(pVm,XML_ERR_FATAL,XML_ERR_DOCUMENT_EMPTY,1,1,
			"Document is empty\n",(const char *)SyBlobData(&sPath));
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::load");
		SyBlobRelease(&sBody);
		SyBlobRelease(&sPath);
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iOpts = DomParseOptions(pThis,iOpts);
	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);
	nMark = PH7_LibxmlCaptureBegin(pVm);
	/* The path is the parse's URL: libxml turns it into the document's URI and
	 * names it in every diagnostic the parse raises. */
	pDoc = xmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),
		(const char *)SyBlobData(&sPath),0,iOpts);
	PH7_LibxmlCaptureEndOpts(pVm,nMark,"DOMDocument::load",iOpts);
	DomRestoreWarnings(pVm,sErr);
	SyBlobRelease(&sBody);
	SyBlobRelease(&sPath);
	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));
	return PH7_OK;
}
/*
 * DOMDocument::loadHTML(string $source, int $options = 0): bool
 * DOMDocument::loadHTMLFile(string $filename, int $options = 0): bool
 *
 * The other parser: HTML is not XML and libxml has a second one for it, which
 * closes what the markup left open, supplies the `html`/`body` php's
 * `LIBXML_HTML_NOIMPLIED` asks it not to, and stamps the DTD
 * `LIBXML_HTML_NODEFDTD` asks it not to. What comes out is an HTML DOCUMENT --
 * node type 13, its own serializer -- and the differences from the XML side are
 * measured ones: the document's own directives reach NOTHING here (only
 * `$options` does), a document parsed from a STRING is given no URI at all
 * (where loadXML stamps the working directory), and there is no well-formedness
 * to fail on, so the answer is true for anything that is not empty.
 */
static int DomLoadHtml(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zFn = bFile ? "DOMDocument::loadHTMLFile" : "DOMDocument::loadHTML";
	const char *zArg = bFile ? "filename" : "source";
	const char *zSrc;
	int nSrc = 0;
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	SyBlob sBody,sPath;
	xmlDocPtr pDoc;
	sxu32 nMark;
	zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( bFile && nSrc != (int)SyStrlen(zSrc) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($filename) must not contain any null bytes",zFn);
	}
	if( nSrc < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($%s) must not be empty",zFn,zArg);
	}
	if( bFile ){
		/* loadHTMLFile hands libxml the path AS WRITTEN -- php's
		 * htmlCreateFileParserCtxt(source): no working directory joined and no
		 * link resolved, unlike load(), which canonicalizes first. It is the name
		 * the document's URI and every diagnostic carry. */
		if( !DomReadFileAs(pCtx,zSrc,nSrc,zFn,&sBody,&sPath,1) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
	}else{
		/* A string has no URI: php leaves the document's null. */
		SyBlobInit(&sBody,&pVm->sAllocator);
		SyBlobAppend(&sBody,zSrc,(sxu32)nSrc);
		SyBlobInit(&sPath,&pVm->sAllocator);
		SyBlobNullAppend(&sPath);
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	if( SyBlobLength(&sBody) > 0 ){
		pDoc = htmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),
			bFile ? (const char *)SyBlobData(&sPath) : 0,0,iOpts);
	}else{
		/* An empty FILE is still a document here, unlike on the XML side: php's
		 * HTML parser says `Document is empty` and hands back the DTD-only
		 * document `htmlNewDoc` builds. libxml's memory parser will not start on
		 * nothing, so both halves are made by hand. (An empty STRING never gets
		 * this far -- it is the ValueError above.) */
		PH7_LibxmlQueueError(pVm,XML_ERR_ERROR,XML_ERR_DOCUMENT_EMPTY,1,1,
			"Document is empty\n",(const char *)SyBlobData(&sPath));
		pDoc = htmlNewDoc(0,0);
		if( pDoc ){
			pDoc->URL = xmlStrdup((const xmlChar *)SyBlobData(&sPath));
		}
	}
	PH7_LibxmlCaptureEndOpts(pVm,nMark,zFn,iOpts);
	SyBlobRelease(&sBody);
	SyBlobRelease(&sPath);
	ph7_result_bool(pCtx,DomInstallParsed(pCtx,pThis,pDoc));
	return PH7_OK;
}
DOM_METHOD(vm_builtin_DOMDocument_loadHTML)
{
	return DomLoadHtml(pCtx,nArg,apArg,FALSE);
}
DOM_METHOD(vm_builtin_DOMDocument_loadHTMLFile)
{
	return DomLoadHtml(pCtx,nArg,apArg,TRUE);
}
/*
 * The two save options php reads, and what they mean to libxml.
 *
 * `LIBXML_NOEMPTYTAG` turns `<e/>` into `<e></e>` and reaches BOTH dumps -- a
 * node's as much as a document's -- while `LIBXML_NOXMLDECL` only reaches the
 * whole-document one (a node's output has no declaration to drop). Every other
 * bit of `$options` is ignored, unknown ones included. php spells the first one
 * with libxml's library-wide switch; this file asks for it per dump instead,
 * which says the same thing without touching global state (and without the
 * deprecated symbol: the MSVC gate refuses it under /WX).
 *
 * Both dumps therefore run through libxml's save API. The DOCUMENT's goes out
 * in the encoding its declaration names -- which is also how a document whose
 * encoding has no converter fails, with no context to write through -- and a
 * NODE's is always UTF-8, as php's is.
 */
#define DOM_SAVE_NOXMLDECL  2
#define DOM_SAVE_NOEMPTYTAG 4
static int DomSaveFlags(int bFormat,int iOpts,int bDoc)
{
	/* AS_XML because the receiver may be an HTML document (`loadHTML` makes
	 * one): libxml's save context would hand such a document to the HTML
	 * serializer, and php's XML savers write XML whatever the document is --
	 * declaration, `<br/>` and all. */
	int iSave = XML_SAVE_AS_XML | (bFormat ? XML_SAVE_FORMAT : 0);
	if( iOpts & DOM_SAVE_NOEMPTYTAG ){
		iSave |= XML_SAVE_NO_EMPTY;
	}
	if( bDoc && (iOpts & DOM_SAVE_NOXMLDECL) ){
		iSave |= XML_SAVE_NO_DECL;
	}
	return iSave;
}
/*
 * Serialize a whole document (pNode == 0) or one node the way php's savers do.
 * Answers the bytes in *pzOut (xmlFree'd by the caller) and their count, or -1.
 */
static int DomDumpTree(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,int iOpts,xmlChar **pzOut)
{
	xmlBufferPtr pBuf = xmlBufferCreate();
	xmlSaveCtxtPtr pSave;
	int nOut = 0;
	*pzOut = 0;
	if( pBuf == 0 ){
		return -1;
	}
	/* A NODE's dump is UTF-8 whatever the document declares, and naming that
	 * encoding is also what keeps libxml from ESCAPING every non-ASCII character
	 * (its no-encoding path writes `&#xE9;`, which is right for a document that
	 * declares nothing and wrong for a node). A DOCUMENT's goes out in its own
	 * declared encoding, or in that escaping form when it declares none -- which
	 * is what php answers there. */
	pSave = xmlSaveToBuffer(pBuf,pNode ? "UTF-8" : (const char *)pDoc->encoding,
		DomSaveFlags(bFormat,iOpts,pNode == 0));
	if( pSave == 0 ){
		xmlBufferFree(pBuf);
		return -1;
	}
	if( (pNode ? xmlSaveTree(pSave,pNode) : xmlSaveDoc(pSave,pDoc)) < 0 ){
		nOut = -1;
	}
	if( xmlSaveClose(pSave) < 0 ){
		nOut = -1;
	}
	if( nOut == 0 ){
		nOut = (int)xmlBufferLength(pBuf);
		*pzOut = xmlStrndup(xmlBufferContent(pBuf),nOut);
		if( *pzOut == 0 ){
			nOut = -1;
		}
	}
	xmlBufferFree(pBuf);
	return nOut;
}
/* DOMDocument::saveXML(?DOMNode $node = null, int $options = 0): string|false */
DOM_METHOD(vm_builtin_DOMDocument_saveXML)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pDocNd = DomThisNode(pCtx);
	phl_domnode *pTgt = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;
	int bFormat = pThis && PH7_NativeAttrTruthy(pThis,"formatOutput");
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	int bWhole;
	xmlChar *zOut = 0;
	int nOut;
	sxu32 nMark;
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	bWhole = pTgt == 0 || pTgt->pNode == pDocNd->pNode;
	nMark = PH7_LibxmlCaptureBegin(pVm);
	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,bWhole ? 0 : (xmlNodePtr)pTgt->pNode,
		bFormat,iOpts,&zOut);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::saveXML");
	if( nOut < 0 ){
		/* php says so rather than answering an empty document: the encoding the
		 * declaration names has no converter and nothing was written. */
		if( bWhole ){
			ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Could not save document");
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	ph7_result_string(pCtx,(const char *)zOut,nOut);
	xmlFree(zOut);
	return PH7_OK;
}
/*
 * DOMDocument::saveHTML(?DOMNode $node = null): string|false
 * DOMDocument::saveHTMLFile(string $filename): int|false
 *
 * The HTML serializer, which is a different one: a void element comes out
 * `<br>` rather than `<br/>`, a character with an HTML entity name comes out
 * under that name, and the whole document carries its DOCTYPE and no XML
 * declaration. Neither method takes save OPTIONS -- php declares one parameter
 * each -- but both read `formatOutput`, a NODE's dump included.
 *
 * A node from ANOTHER document is php's Wrong Document Error (in whichever mode
 * this document is in), which is the only refusal either one has.
 */
static int DomDumpHtml(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,xmlChar **pzOut)
{
	xmlBufferPtr pBuf;
	xmlOutputBufferPtr pOut;
	int nOut;
	*pzOut = 0;
	if( pNode == 0 ){
		nOut = 0;
		htmlDocDumpMemoryFormat(pDoc,pzOut,&nOut,bFormat ? 1 : 0);
		return *pzOut ? nOut : -1;
	}
	pBuf = xmlBufferCreate();
	/* The buffer is the write TARGET, not the output buffer's own storage:
	 * closing the latter leaves it to us to free. */
	pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;
	if( pOut == 0 ){
		if( pBuf ){
			xmlBufferFree(pBuf);
		}
		return -1;
	}
	htmlNodeDumpFormatOutput(pOut,pDoc,pNode,0,bFormat ? 1 : 0);
	xmlOutputBufferFlush(pOut);
	nOut = (int)xmlBufferLength(pBuf);
	*pzOut = xmlStrndup(xmlBufferContent(pBuf),nOut);
	xmlOutputBufferClose(pOut);
	xmlBufferFree(pBuf);
	return *pzOut ? nOut : -1;
}
static int DomSaveHtml(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pDocNd = DomThisNode(pCtx);
	phl_domnode *pTgt = (!bFile && nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;
	int bFormat = pThis && PH7_NativeAttrTruthy(pThis,"formatOutput");
	const ph7_io_stream *pStream = 0;
	const char *zFile = "";
	int nFile = 0,nOut;
	xmlChar *zOut = 0;
	void *pHandle;
	sxu32 nMark;
	if( bFile ){
		zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";
		if( nFile != (int)SyStrlen(zFile) ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not contain any null bytes");
		}
		if( nFile < 1 ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"DOMDocument::saveHTMLFile(): Argument #1 ($filename) must not be empty");
		}
	}
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pTgt && pTgt->pShell != pDocNd->pShell ){
		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);
	}
	if( bFile ){
		/* Writing to a FILE goes through libxml's file saver, which stamps the
		 * document with the encoding it is about to use: an `http-equiv`
		 * Content-Type meta appears in `<head>` -- in the DOCUMENT, not just in
		 * the output, so the next `saveHTML()` shows it too -- and it always
		 * says UTF-8, whatever the document's own encoding is. php inherits
		 * that; the string saver READS the same meta and adds none. */
		htmlSetMetaEncoding((xmlDocPtr)pDocNd->pNode,(const xmlChar *)"UTF-8");
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	nOut = DomDumpHtml((xmlDocPtr)pDocNd->pNode,
		(pTgt && pTgt->pNode != pDocNd->pNode) ? (xmlNodePtr)pTgt->pNode : 0,bFormat,&zOut);
	PH7_LibxmlDropErrors(pVm,nMark);
	if( nOut < 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( !bFile ){
		ph7_result_string(pCtx,(const char *)zOut,nOut);
		xmlFree(zOut);
		return PH7_OK;
	}
	pStream = PH7_VmGetStreamDevice(pVm,&zFile,nFile);
	pHandle = (pStream && pStream->xWrite) ? PH7_StreamOpenHandle(pVm,pStream,zFile,
		PH7_IO_OPEN_WRONLY|PH7_IO_OPEN_CREATE|PH7_IO_OPEN_TRUNC,FALSE,0,FALSE,0,
		ph7_function_name(pCtx)) : 0;
	if( pHandle == 0 ){
		xmlFree(zOut);
		VfsThrowOpenWarning(pCtx,zFile);
		/* php answers the bytes it WROTE, which is none of them -- not false. */
		ph7_result_int(pCtx,0);
		return PH7_OK;
	}
	if( nOut > 0 && pStream->xWrite(pHandle,(const void *)zOut,nOut) < 0 ){
		nOut = 0;
	}
	PH7_StreamCloseHandle(pStream,pHandle);
	xmlFree(zOut);
	ph7_result_int(pCtx,nOut);
	return PH7_OK;
}
DOM_METHOD(vm_builtin_DOMDocument_saveHTML)
{
	return DomSaveHtml(pCtx,nArg,apArg,FALSE);
}
DOM_METHOD(vm_builtin_DOMDocument_saveHTMLFile)
{
	return DomSaveHtml(pCtx,nArg,apArg,TRUE);
}
/*
 * DOMDocument::save(string $filename, int $options = 0): int|false
 *
 * saveXML's bytes written to a file, and the COUNT of them rather than the
 * bytes -- through the stream layer, which is where php's
 * `save(<path>): Failed to open stream: <reason>` comes from. Two rules only a
 * differential decides: `LIBXML_NOXMLDECL` does NOT reach this one (php reads
 * it in saveXML only, so a saved document always carries its declaration),
 * and a document whose declared encoding has no converter is a silent `false`
 * here where saveXML says "Could not save document".
 */
DOM_METHOD(vm_builtin_DOMDocument_save)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pDocNd = DomThisNode(pCtx);
	const ph7_io_stream *pStream;
	const char *zFile;
	int nFile = 0;
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	int bFormat = pThis && PH7_NativeAttrTruthy(pThis,"formatOutput");
	int nOut;
	xmlChar *zOut = 0;
	void *pHandle;
	sxu32 nMark;
	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";
	if( nFile != (int)SyStrlen(zFile) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMDocument::save(): Argument #1 ($filename) must not contain any null bytes");
	}
	if( nFile < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMDocument::save(): Argument #1 ($filename) must not be empty");
	}
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,0,bFormat,iOpts & ~DOM_SAVE_NOXMLDECL,&zOut);
	/* php reports this failure through the return value alone. */
	PH7_LibxmlDropErrors(pVm,nMark);
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
		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);
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
			return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);
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
/*
 * DOMDocument::createElementNS(?string $namespace, string $qualifiedName,
 *                              string $value = '')
 *
 * The only way to build a namespaced ELEMENT -- until this existed a program
 * could read a namespaced document and not write one, and `Call to undefined
 * method` was the answer to the first line of every modern DOM example.
 *
 * php's rules, measured:
 *
 *   * A NULL namespace is a plain element; an EMPTY-STRING one is not the same
 *     thing, it declares `xmlns=""` on the element and answers `''` for
 *     namespaceURI. Either with a PREFIXED name is a Namespace Error, since a
 *     prefix names a namespace.
 *   * The declaration lands on the NEW element, always: a fresh node has no
 *     parent, so nothing the document declares elsewhere is in scope yet. What
 *     the document already makes is settled later, when the element is linked
 *     in and the redundant declaration is stripped (DomNsOnInsertEx).
 *   * The $value is not text -- php hands it to libxml, which entity-parses it,
 *     so `&amp;` becomes `&`, an undefined entity is a warning and `<` is
 *     escaped. The same quirk createElement already carries.
 */
DOM_METHOD(vm_builtin_DOMDocument_createElementNS)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = DomThisNode(pCtx);
	const char *zUri = DomArgStrOrNull(nArg,apArg,0);
	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	int nVal = 0;
	const char *zVal = nArg > 2 ? ph7_value_to_string(apArg[2],&nVal) : "";
	xmlNodePtr pNode;
	dom_qname sQ;
	sxu32 nMark;
	int rc;
	if( pDocNd == 0 ){
		return DomThrow(pCtx,DOM_ERR_NAMESPACE);
	}
	rc = DomQNameParse(zQname,zUri,DOM_QN_ELEM,&sQ);
	if( rc ){
		return DomThrow(pCtx,rc);
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pNode = xmlNewDocNode((xmlDocPtr)pDocNd->pNode,0,sQ.zLocal,
		nVal ? (const xmlChar *)zVal : 0);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::createElementNS");
	if( pNode == 0 ){
		DomQNameRelease(&sQ);
		return PH7_ContextMemoryError(pCtx);
	}
	if( zUri != 0 ){
		xmlNsPtr pNs = DomNsForCreate(pNode,zUri,sQ.zPrefix);
		if( pNs == 0 ){
			DomQNameRelease(&sQ);
			xmlFreeNode(pNode);   /* never handed out, never an orphan */
			return DomThrow(pCtx,DOM_ERR_NAMESPACE);
		}
		xmlSetNs(pNode,pNs);
	}
	DomQNameRelease(&sQ);
	DomOrphanAdd(pDocNd->pShell,pNode);
	return DomResultNodeOf(pCtx,pDocNd,pNode);
}
/*
 * DOMDocument::importNode(DOMNode $node, bool $deep = false): DOMNode|false
 *
 * A node of ANOTHER document copied into this one, which is the only way to
 * carry a subtree across: every mutator refuses a node whose document is not
 * the parent's with php's Wrong Document Error, so without this a program that
 * read two files could not build a third out of them.
 *
 * php's rules, measured:
 *
 *   * A node ALREADY of this document is answered unchanged -- the same object,
 *     not a copy, and not detached from wherever it is.
 *   * A DOCUMENT is refused with a warning and `false`, not an exception.
 *   * Shallow does not mean bare: an element brings its attributes and its
 *     namespace declarations, only its children stay behind. A fragment brings
 *     nothing but itself, and an attribute brings its value whatever $deep says.
 *   * The copy is an ORPHAN of this document (no parent, and freed with it), and
 *     a second import of the same node is a second copy.
 *   * A namespaced ATTRIBUTE is the one kind libxml cannot finish: its copy
 *     arrives with no namespace at all, and php re-points it at a PREFIXED
 *     binding of the same URI on the target's ROOT -- reusing one the root
 *     already has (so the prefix can change, `p:b` arriving as `z:b`) and
 *     declaring it there otherwise.
 */
DOM_METHOD(vm_builtin_DOMDocument_importNode)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = DomThisNode(pCtx);
	phl_domnode *pSrc = nArg > 0 ? DomObjArg(apArg[0]) : 0;
	int bDeep = nArg > 1 && ph7_value_to_bool(apArg[1]);
	xmlDocPtr pDoc;
	xmlNodePtr pNode,pCopy;
	sxu32 nMark;
	if( pDocNd == 0 || pSrc == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pDoc = (xmlDocPtr)pDocNd->pNode;
	pNode = (xmlNodePtr)pSrc->pNode;
	if( pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE ){
		/* The context prints php's `DOMDocument::importNode(): ` itself. */
		ph7_context_throw_error(pCtx,PH7_CTX_WARNING,"Cannot import: Node Type Not Supported");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pNode->doc == pDoc ){
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	/* 2 is libxml's `node + namespaces + attributes, no children`, which is what
	 * makes a shallow import carry the attributes; cloneNode asks the same way. */
	pCopy = xmlDocCopyNode(pNode,pDoc,bDeep ? 1 : 2);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::importNode");
	if( pCopy == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pCopy->type == XML_ATTRIBUTE_NODE && pNode->ns != 0 && pNode->ns->href != 0 ){
		xmlNodePtr pRoot = xmlDocGetRootElement(pDoc);
		xmlNsPtr pNs = pRoot
			? DomNsResolve(pRoot,(const char *)pNode->ns->href,pNode->ns->prefix,1) : 0;
		if( pNs == 0 ){
			/* No root element to declare on. php answers an attribute that IS in
			 * the namespace anyway, through a declaration no element makes; the
			 * document owns it so that it is freed with it. */
			pNs = xmlNewNs(0,pNode->ns->href,pNode->ns->prefix);
			if( pNs ){
				DomNsPark(pCopy,pNs);
			}
		}
		xmlSetNs(pCopy,pNs);
	}
	DomOrphanAdd(pDocNd->pShell,pCopy);
	return DomResultNodeOf(pCtx,pDocNd,pCopy);
}
/*
 * Re-home one node's WRAPPER. `adoptNode` moves the node itself between
 * documents and answers the SAME object, which has to keep working: its $__doc
 * slot is what `ownerDocument` reads, its handle's shell is what will free the
 * node, and its place in a document's identity cache is what makes
 * `$doc->documentElement === $doc->documentElement` true. All three move.
 *
 * The target cache takes its reference BEFORE the source lets go, so the object
 * cannot be freed in between.
 */
static void DomAdoptWrapper(ph7_vm *pVm,ph7_hashmap *pFrom,ph7_hashmap *pTo,
	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode)
{
	ph7_hashmap_node *pEntry = 0;
	ph7_class_instance *pObj;
	ph7_value sKey,*pHit;
	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);
	if( PH7_HashmapLookup(pFrom,&sKey,&pEntry) != SXRET_OK || pEntry == 0 ){
		PH7_MemObjRelease(&sKey);
		return;   /* PHP never asked for this node: nothing to move */
	}
	pHit = HashmapExtractNodeValue(pEntry);
	pObj = (pHit && (pHit->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pHit->x.pOther : 0;
	if( pObj ){
		phl_domnode *pRes = DomResOf(pObj);
		ph7_value sVal;
		PH7_MemObjInit(&(*pVm),&sVal);
		sVal.x.pOther = pObj;
		sVal.iFlags = MEMOBJ_OBJ;
		PH7_HashmapInsert(pTo,&sKey,&sVal);
		if( pRes ){
			pRes->pShell = pDstShell;
		}
		PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pDstDoc);
	}
	PH7_HashmapUnlinkNode(pEntry,TRUE);
	PH7_MemObjRelease(&sKey);
}
/* ...for every node of the adopted subtree, attributes and their text included:
 * php's adoption reaches all of them, which `$kid->ownerDocument` shows. */
static void DomAdoptWrappers(ph7_vm *pVm,ph7_class_instance *pSrcDoc,
	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode)
{
	ph7_hashmap *pFrom = DomCache(&(*pVm),pSrcDoc);
	ph7_hashmap *pTo = DomCache(&(*pVm),pDstDoc);
	xmlNodePtr pCur = pNode;
	if( pFrom == 0 || pTo == 0 || pFrom == pTo ){
		return;
	}
	while( pCur ){
		DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,pCur);
		if( pCur->type == XML_ELEMENT_NODE ){
			xmlAttrPtr pAttr;
			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){
				xmlNodePtr pKid;
				DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,(xmlNodePtr)pAttr);
				for( pKid = pAttr->children ; pKid ; pKid = pKid->next ){
					DomAdoptWrapper(&(*pVm),pFrom,pTo,pDstDoc,pDstShell,pKid);
				}
			}
		}
		pCur = DomWalkNext(pCur,pNode);
	}
}
/*
 * DOMDocument::adoptNode(DOMNode $node): DOMNode|false
 *
 * The other half of importNode: the node is MOVED rather than copied, so the
 * source loses it and every wrapper PHP holds onto it keeps working and starts
 * answering this document.
 *
 * php's rules, measured:
 *
 *   * The answer is the SAME object, and it is always UNLINKED first -- even
 *     when it already belongs to this document, which is observable:
 *     `$d->adoptNode($d->documentElement)` leaves the document empty.
 *   * A DOCUMENT is the Not Supported refusal (raised in the mode the ARGUMENT's
 *     document is in, not the receiver's); a FRAGMENT is a plain `false` with
 *     no error at all.
 *   * An attribute is taken off its element. Every node under what moved changes
 *     document too, wrappers included.
 *   * NOTHING is re-declared: an adopted element keeps pointing at its old
 *     namespace and answers the same namespaceURI while carrying no declaration
 *     of it -- the declaration appears when it is LINKED, from the reconcile.
 */
DOM_METHOD(vm_builtin_DOMDocument_adoptNode)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = DomThisNode(pCtx);
	phl_domnode *pSrc = nArg > 0 ? DomObjArg(apArg[0]) : 0;
	ph7_class_instance *pSrcDoc = nArg > 0 ? DomObjArgDoc(apArg[0]) : 0;
	xmlNodePtr pNode;
	if( pDocNd == 0 || pSrc == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pNode = (xmlNodePtr)pSrc->pNode;
	if( pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE ){
		/* The one refusal in this file that consults the ARGUMENT's document
		 * rather than the receiver's: php reaches for the strictness of the
		 * node it was handed, so `$strict->adoptNode($lax)` warns and
		 * `$lax->adoptNode($strict)` throws. */
		return DomThrowFor(pCtx,pSrcDoc,DOM_ERR_NOT_SUPPORTED,DOM_REFUSE_FALSE);
	}
	if( pNode->type == XML_DOCUMENT_FRAG_NODE ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	DomDetach(pSrc->pShell,pNode);
	if( pNode->doc != (xmlDocPtr)pDocNd->pNode ){
		/*
		 * NOT xmlSetTreeDoc: a parsed document interns its node names in its
		 * own dictionary, so a node re-homed by hand keeps names owned by the
		 * document it LEFT -- and freeing the target document then frees
		 * strings the source's dictionary owns. ASan called it what it is, a
		 * bad free. xmlDOMWrapAdoptNode is libxml's own re-homing: it moves the
		 * strings, the attribute values and the ID table entries with the node.
		 */
		sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);
		xmlDOMWrapAdoptNode(0,pNode->doc,pNode,(xmlDocPtr)pDocNd->pNode,0,0);
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::adoptNode");
		DomAdoptWrappers(pVm,pSrcDoc,DomThisDoc(pCtx),pDocNd->pShell,pNode);
	}
	DomOrphanAdd(pDocNd->pShell,pNode);
	ph7_result_value(pCtx,apArg[0]);
	return PH7_OK;
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

/* ===== Character data: the in-place edit family ===== */

/*
 * Every offset and count on this surface is measured in UTF-8 CHARACTERS, not
 * bytes -- php runs `xmlUTF8Strlen` over the content and `xmlUTF8Strsub` to cut
 * it -- so `$t->length` on "áé漢字" is 4 and `substringData(0,1)` is one
 * character rather than one byte. PHL measured `length` with strlen(), which is
 * a silently wrong answer for every non-ASCII document: 10 where php says 4,
 * and every offset a program then computed from it landed mid-character.
 *
 * libxml's own UTF-8 helpers are used rather than PHL's, so malformed content
 * counts and cuts identically in both engines.
 */
static int DomCharLength(xmlNodePtr pNode)
{
	return (pNode && pNode->content) ? xmlUTF8Strlen(pNode->content) : 0;
}
/*
 * php's Index Size Error: a negative bound, or an offset past the end. The
 * COUNT is clamped rather than refused once the offset is in range.
 *
 * The upper bound is compared UNSIGNED on three of the five and SIGNED on the
 * other two, and only malformed content tells them apart: `xmlUTF8Strlen`
 * answers -1 for content that is not valid UTF-8 (`$t->length` reports that
 * -1), and as an UNSIGNED bound a -1 means "no limit" -- so substringData,
 * insertData and splitText all work on such a node and let libxml's own cutting
 * decide what comes back, while deleteData and replaceData refuse it outright,
 * for every offset and every count. php's own split, kept because a program
 * handed a byte string that is not UTF-8 gets a value back from three of these
 * and an exception from the other two.
 */
static int DomCharRange(ph7_context *pCtx,xmlNodePtr pNode,ph7_int64 iOffset,
	ph7_int64 iCount,int bHasCount,int bUnsignedBound,int *pnLen,int *pRc)
{
	int nLen = DomCharLength(pNode);
	int bPastEnd = bUnsignedBound ? (sxu32)iOffset > (sxu32)nLen
	                              : iOffset > (ph7_int64)nLen;
	*pnLen = nLen;
	if( iOffset < 0 || (bHasCount && iCount < 0)
	 || iOffset > (ph7_int64)SXI32_HIGH || iCount > (ph7_int64)SXI32_HIGH
	 || bPastEnd ){
		*pRc = DomThrow(pCtx,DOM_ERR_INDEX_SIZE);
		return -1;
	}
	return 0;
}
/* DOMCharacterData::substringData(int $offset, int $count): string */
DOM_METHOD(vm_builtin_DOMCharacterData_substringData)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	ph7_int64 iOffset = nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0;
	ph7_int64 iCount = nArg > 1 ? ph7_value_to_int64(apArg[1]) : 0;
	xmlChar *zSub;
	int nLen,rc = PH7_OK;
	if( pNode == 0 || pNode->content == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( DomCharRange(pCtx,pNode,iOffset,iCount,TRUE,TRUE,&nLen,&rc) != 0 ){
		return rc;
	}
	if( (sxu32)(iOffset+iCount) > (sxu32)nLen ){
		iCount = (ph7_int64)nLen - iOffset;
	}
	zSub = xmlUTF8Strsub(pNode->content,(int)iOffset,(int)iCount);
	ph7_result_string(pCtx,zSub ? (const char *)zSub : "",-1);
	if( zSub ){
		xmlFree(zSub);
	}
	return PH7_OK;
}
/* DOMCharacterData::appendData(string $data): true -- raw bytes, no entity
 * parsing, which is why `appendData('&amp;')` stores those five characters. */
DOM_METHOD(vm_builtin_DOMCharacterData_appendData)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	int nData = 0;
	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : "";
	if( pNd ){
		xmlTextConcat((xmlNodePtr)pNd->pNode,(const xmlChar *)zData,nData);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/*
 * The three writers, which php builds the same way: the head up to $offset, the
 * replacement, then whatever the count left of the tail.
 *
 * insertData is (offset, 0, data), deleteData is (offset, count, ""), and
 * replaceData is both -- php's own three bodies say the same thing three times.
 */
static int DomCharSplice(ph7_context *pCtx,ph7_int64 iOffset,ph7_int64 iCount,
	int bHasCount,const char *zData,int nData)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlChar *zHead,*zTail = 0;
	int nLen,rc = PH7_OK;
	if( pNode == 0 || pNode->content == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	/* insertData has no count and takes the unsigned bound; the two that DO
	 * take one take the signed bound. */
	if( DomCharRange(pCtx,pNode,iOffset,iCount,bHasCount,!bHasCount,&nLen,&rc) != 0 ){
		return rc;
	}
	if( (sxu32)(iOffset+iCount) > (sxu32)nLen ){
		iCount = (ph7_int64)nLen - iOffset;
	}
	zHead = iOffset > 0 ? xmlUTF8Strndup(pNode->content,(int)iOffset)
	                    : xmlStrdup((const xmlChar *)"");
	if( iOffset + iCount < (ph7_int64)nLen ){
		zTail = xmlUTF8Strsub(pNode->content,(int)(iOffset+iCount),
			(int)((ph7_int64)nLen - iOffset - iCount));
	}
	xmlNodeSetContent(pNode,zHead ? zHead : (const xmlChar *)"");
	if( nData > 0 ){
		xmlNodeAddContentLen(pNode,(const xmlChar *)zData,nData);
	}
	if( zTail ){
		xmlNodeAddContent(pNode,zTail);
	}
	if( zHead ){
		xmlFree(zHead);
	}
	if( zTail ){
		xmlFree(zTail);
	}
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}
/* DOMCharacterData::insertData(int $offset, string $data): true */
DOM_METHOD(vm_builtin_DOMCharacterData_insertData)
{
	int nData = 0;
	const char *zData = nArg > 1 ? ph7_value_to_string(apArg[1],&nData) : "";
	return DomCharSplice(pCtx,nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0,0,FALSE,zData,nData);
}
/* DOMCharacterData::deleteData(int $offset, int $count): true */
DOM_METHOD(vm_builtin_DOMCharacterData_deleteData)
{
	return DomCharSplice(pCtx,nArg > 1 ? ph7_value_to_int64(apArg[0]) : 0,
		nArg > 1 ? ph7_value_to_int64(apArg[1]) : 0,TRUE,"",0);
}
/* DOMCharacterData::replaceData(int $offset, int $count, string $data): true */
DOM_METHOD(vm_builtin_DOMCharacterData_replaceData)
{
	int nData = 0;
	const char *zData = nArg > 2 ? ph7_value_to_string(apArg[2],&nData) : "";
	return DomCharSplice(pCtx,nArg > 2 ? ph7_value_to_int64(apArg[0]) : 0,
		nArg > 2 ? ph7_value_to_int64(apArg[1]) : 0,TRUE,zData,nData);
}
/*
 * DOMText::splitText(int $offset): DOMText|false
 *
 * The receiver keeps the head and a SECOND node takes the tail, spliced in
 * right after it. Two details only the oracle states: an offset past the end is
 * plain `false` where a negative one is a ValueError, and splitting a CDATA
 * section produces a TEXT node -- so `<![CDATA[abcdef]]>` split at 2 serializes
 * as `<![CDATA[ab]]>cdef`.
 */
DOM_METHOD(vm_builtin_DOMText_splitText)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	xmlChar *zHead,*zTail;
	xmlNodePtr pNew;
	int nLen;
	if( iOffset < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMText::splitText(): Argument #1 ($offset) must be greater than or equal to 0");
	}
	if( pNode == 0
	 || (pNode->type != XML_TEXT_NODE && pNode->type != XML_CDATA_SECTION_NODE)
	 || pNode->content == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nLen = DomCharLength(pNode);
	if( iOffset > (ph7_int64)nLen ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	zHead = xmlUTF8Strndup(pNode->content,(int)iOffset);
	zTail = xmlUTF8Strsub(pNode->content,(int)iOffset,(int)((ph7_int64)nLen - iOffset));
	xmlNodeSetContent(pNode,zHead ? zHead : (const xmlChar *)"");
	pNew = xmlNewDocText(pNode->doc,zTail ? zTail : (const xmlChar *)"");
	if( zHead ){
		xmlFree(zHead);
	}
	if( zTail ){
		xmlFree(zTail);
	}
	if( pNew == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pNode->parent ){
		/* Spliced by hand, as everything in this file is: xmlAddNextSibling
		 * MERGES two adjacent text nodes and frees one of them. */
		if( pNode->next ){
			DomLinkBefore(pNode->parent,pNew,pNode->next);
		}else{
			DomLinkLast(pNode->parent,pNew);
		}
	}else{
		DomOrphanAdd(pNd->pShell,pNew);
	}
	return DomResultNodeOf(pCtx,pNd,pNew);
}
/* DOMText::isWhitespaceInElementContent() and its 8.x rename
 * isElementContentWhitespace(): one body, libxml's blank-node test. */
DOM_METHOD(vm_builtin_DOMText_isWhitespace)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,pNd && xmlIsBlankNode((xmlNodePtr)pNd->pNode));
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
#define DNL_GEBTNNS 3 /* getElementsByTagNameNS($uri, $localName) */
#define DNL_KIND  "__kind"
#define DNL_OWNER "__owner"
#define DNL_NAME  "__name"
#define DNL_URI   "__uri"
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
	const char *zName,*zUri = 0;
	int nName,nUri = -1,iCount = 0;
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
	if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_GEBTNNS ){
		PH7_NativeAttrStr(pList,DNL_URI,&zUri,&nUri);
	}
	DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,-1,&iCount);
	return iCount;
}
/* The wrapper at one index, or NULL past the end. BORROWED, like every wrap. */
static ph7_class_instance * DomListItem(ph7_vm *pVm,ph7_class_instance *pList,int iIndex)
{
	ph7_class_instance *pDoc;
	phl_domnode *pOwner;
	xmlNodePtr pNode = 0;
	const char *zName,*zUri = 0;
	int nName,nUri = -1;
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
		if( PH7_NativeAttrInt(pList,DNL_KIND) == DNL_GEBTNNS ){
			PH7_NativeAttrStr(pList,DNL_URI,&zUri,&nUri);
		}
		pNode = DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,iIndex,0);
	}
	return DomWrap(&(*pVm),pDoc,pOwner->pShell,pNode);
}
/*
 * Build one. pOwnerObj is the node the live view is of (NULL for a snapshot),
 * pSnap the frozen list (NULL otherwise). The caller owns the reference.
 */
static ph7_class_instance * DomNewCollection(ph7_vm *pVm,const char *zClass,
	ph7_class_instance *pDoc,int iKind,ph7_class_instance *pOwnerObj,
	const char *zName,const char *zUri,ph7_value *pSnap)
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
	if( zUri ){
		PH7_NativeSetAttrStr(&(*pVm),pObj,DNL_URI,zUri,(int)SyStrlen(zUri));
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
	/* The MAP asks libxml's name-only question, where DOMElement's own
	 * getAttributeNode resolves the prefix: `getNamedItem('k')` finds the
	 * namespaced `p:k` that `getAttribute('k')` does not. */
	xmlAttrPtr pAttr = pOwner ? DomAttrByLocal((xmlNodePtr)pOwner->pNode,zName) : 0;
	if( pAttr == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultWrap(pCtx,DomWrap(pCtx->pVm,PH7_NativeAttrObj(pThis,DOM_DOC),
		pOwner->pShell,(xmlNodePtr)pAttr));
}
/* DOMNamedNodeMap::getNamedItemNS(?string $namespace, string $localName): ?DOMNode */
DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItemNS)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pOwner = pThis ? DomListOwner(pThis) : 0;
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	const xmlChar *zUri = DomArgUri(nArg,apArg,0);
	/* A NULL namespace is the map's ANY here, not the element's "in no
	 * namespace": `$el->attributes->getNamedItemNS(null,'k')` answers a
	 * namespaced `p:k` where `$el->getAttributeNodeNS(null,'k')` answers null.
	 * An EMPTY namespace is neither -- it matches a URI no document has. */
	xmlAttrPtr pAttr = pOwner == 0 ? 0
		: zUri ? DomAttrByNs((xmlNodePtr)pOwner->pNode,zUri,zLocal)
		       : DomAttrByLocal((xmlNodePtr)pOwner->pNode,zLocal);
	if( pAttr == 0 ){
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
	pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_SNAP,0,0,0,pSnap);
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
	}else if( DomNameIs(zName,"baseURI") ){
		/* php's is libxml's own xmlNodeGetBase(): the nearest `xml:base` on the
		 * way up, resolved against the DOCUMENT's URI, and that URI itself when
		 * no ancestor declares one. So it answers null exactly when the document
		 * was never given a URI -- a `new DOMDocument()` that was not loaded --
		 * and a node created and never appended still answers its document's. */
		xmlChar *zBase = pNode ? xmlNodeGetBase(pNode->doc,pNode) : 0;
		if( zBase ){
			ph7_result_string(pCtx,(const char *)zBase,-1);
			xmlFree(zBase);
		}else{
			ph7_result_null(pCtx);
		}
	}else if( DomNameIs(zName,"childNodes") ){
		ph7_class_instance *pList = DomNewCollection(pVm,"DOMNodeList",pDoc,DNL_CHILD,pThis,0,0,0);
		if( pList == 0 ){
			return -1;
		}
		PH7_NativeResultObject(pCtx,pList);
	}else if( DomNameIs(zName,"attributes") ){
		/* php: NULL for anything that is not an element. */
		if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
			ph7_result_null(pCtx);
		}else{
			ph7_class_instance *pMap = DomNewCollection(pVm,"DOMNamedNodeMap",pDoc,DNL_CHILD,pThis,0,0,0);
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
/* A libxml string slot answered as php answers it: the bytes, or null when the
 * document never carried one (`encoding` on a declaration-less document). */
static void DomResultXmlStr(ph7_context *pCtx,const xmlChar *zVal)
{
	if( zVal ){
		ph7_result_string(pCtx,(const char *)zVal,-1);
	}else{
		ph7_result_null(pCtx);
	}
}
/*
 * The DOCUMENT's own state block.
 *
 * Nine of php's twenty-two DOMDocument properties are the XML DECLARATION and
 * the document's URI, read straight off libxml's xmlDoc -- and php spells most
 * of them twice, once under the DOM level-3 name and once under the level-1 one
 * it kept for compatibility (`version`/`xmlVersion`, `encoding`/`xmlEncoding`,
 * `standalone`/`xmlStandalone`).  The pairs are not synonyms in every
 * direction: `xmlEncoding` and `actualEncoding` READ the same slot `encoding`
 * writes and are themselves read-only, which is what makes `$d->xmlEncoding =
 * 'UTF-8'` php's readonly Error and `$d->encoding = 'UTF-8'` the write that
 * changes the bytes `saveXML()` emits.
 *
 * `actualEncoding` and `config` carry php 8.4's #[\Deprecated]: the notice
 * fires on a READ and on an `isset()` alike (both go through php's property
 * handler), which is why it is raised HERE rather than in __get -- and NOT on a
 * write, where the readonly refusal comes first and is raised by the writer
 * below without consulting this reader.
 */
static int DomDocStateProp(ph7_context *pCtx,const char *zName,xmlDocPtr pDoc)
{
	int bDeprAe = DomNameIs(zName,"actualEncoding");
	if( bDeprAe || DomNameIs(zName,"config") ){
		PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,
			bDeprAe ? "Property DOMDocument::$actualEncoding is deprecated"
			        : "Property DOMDocument::$config is deprecated");
		/* `config` is php's DOM level-3 configuration slot and has never been
		 * filled in there: the handler answers null and nothing else. */
		if( bDeprAe ){
			DomResultXmlStr(pCtx,pDoc ? pDoc->encoding : 0);
		}else{
			ph7_result_null(pCtx);
		}
		return 1;
	}
	if( DomNameIs(zName,"encoding") || DomNameIs(zName,"xmlEncoding") ){
		DomResultXmlStr(pCtx,pDoc ? pDoc->encoding : 0);
		return 1;
	}
	if( DomNameIs(zName,"version") || DomNameIs(zName,"xmlVersion") ){
		DomResultXmlStr(pCtx,pDoc ? pDoc->version : 0);
		return 1;
	}
	if( DomNameIs(zName,"documentURI") ){
		DomResultXmlStr(pCtx,pDoc ? pDoc->URL : 0);
		return 1;
	}
	if( DomNameIs(zName,"standalone") || DomNameIs(zName,"xmlStandalone") ){
		/* libxml records four states in one int -- no declaration (-1), a
		 * declaration without the attribute (-2), `no` (0) and `yes` (1) -- and
		 * php's bool is true for the last one only. */
		ph7_result_bool(pCtx,pDoc != 0 && pDoc->standalone == 1);
		return 1;
	}
	return 0;
}
/*
 * The three DOMParentNode properties.  php declares them on the three
 * implementers ONLY -- DOMDocument, DOMElement and DOMDocumentFragment -- so
 * `$text->childElementCount` is the Undefined property warning there, which
 * is why childElementCount cannot live in DomNodeProp (it did, and every node
 * kind answered 0 in silence where php warns and answers null).
 */
static int DomParentNodeProp(ph7_context *pCtx,const char *zName)
{
	int bLast = DomNameIs(zName,"lastElementChild");
	if( bLast || DomNameIs(zName,"firstElementChild") ){
		phl_domnode *pNd = DomThisNode(pCtx);
		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
		xmlNodePtr pChild = pNode ? (bLast ? pNode->last : pNode->children) : 0;
		while( pChild && pChild->type != XML_ELEMENT_NODE ){
			pChild = bLast ? pChild->prev : pChild->next;
		}
		DomResultNodeOf(pCtx,pNd,pChild);
		return 1;
	}
	if( DomNameIs(zName,"childElementCount") ){
		phl_domnode *pNd = DomThisNode(pCtx);
		ph7_result_int(pCtx,DomChildCount(pNd ? (xmlNodePtr)pNd->pNode : 0,1));
		return 1;
	}
	return 0;
}
/* DOMDocument adds documentElement and the state block above. */
static int DomDocProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	if( DomNameIs(zName,"documentElement") ){
		DomResultNodeOf(pCtx,pNd,pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0);
		return 1;
	}
	if( DomDocStateProp(pCtx,zName,pNd ? (xmlDocPtr)pNd->pNode : 0) ){
		return 1;
	}
	if( DomParentNodeProp(pCtx,zName) ){
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
/*
 * php declares `schemaTypeInfo` on both DOMElement and DOMAttr and has never
 * filled it in: ext/dom answers NULL from a handler that does nothing else.
 * It is a declared property all the same, so a read is NOT the undefined-property
 * warning -- which is the whole difference this row buys.
 */
static int DomSchemaTypeInfo(ph7_context *pCtx)
{
	ph7_result_null(pCtx);
	return 1;
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
	if( DomNameIs(zName,"schemaTypeInfo") ){
		return DomSchemaTypeInfo(pCtx);
	}
	if( DomParentNodeProp(pCtx,zName) ){
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
/* DOMDocumentFragment: the three DOMParentNode properties over DOMNode's. */
static int DomFragProp(ph7_context *pCtx,const char *zName)
{
	if( DomParentNodeProp(pCtx,zName) ){
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
/* DOMAttr adds name/value/ownerElement/specified/schemaTypeInfo. */
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
	/* php's `specified` is a DOM level-1 remnant: ext/dom answers TRUE for every
	 * attribute a program can reach, including one it just created. */
	if( DomNameIs(zName,"specified") ){
		ph7_result_bool(pCtx,1);
		return 1;
	}
	if( DomNameIs(zName,"schemaTypeInfo") ){
		return DomSchemaTypeInfo(pCtx);
	}
	return DomNodeProp(pCtx,zName);
}
/* DOMCharacterData adds data/length; DOMText adds wholeText on top of those. */
static int DomCharDataProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( DomNameIs(zName,"data") ){
		DomNodeValue(pCtx,pNode);
		return 1;
	}
	if( DomNameIs(zName,"length") ){
		/* php counts UTF-8 CHARACTERS here (xmlUTF8Strlen over the node's own
		 * content), which is the same unit every offset on this class uses. */
		ph7_result_int(pCtx,DomCharLength(pNode));
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
/*
 * DOMText::wholeText is the whole RUN, not the node: php walks back to the
 * first adjacent text-or-CDATA sibling and forward to the last, concatenating
 * all of them, which is what makes it the answer to "what does this element
 * actually say" after an edit has left the text in pieces. Answering the node's
 * own data (what PHL did) is the same string only when the run is one node
 * long, and silently short otherwise.
 */
static int DomIsTextRun(xmlNodePtr pNode)
{
	return pNode != 0
		&& (pNode->type == XML_TEXT_NODE || pNode->type == XML_CDATA_SECTION_NODE);
}
static int DomTextProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd;
	xmlNodePtr pNode,pCur;
	SyBlob sOut;
	if( DomNameIs(zName,"wholeText") ){
		pNd = DomThisNode(pCtx);
		pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
		if( !DomIsTextRun(pNode) ){
			DomNodeValue(pCtx,pNode);
			return 1;
		}
		while( DomIsTextRun(pNode->prev) ){
			pNode = pNode->prev;
		}
		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
		for( pCur = pNode ; DomIsTextRun(pCur) ; pCur = pCur->next ){
			xmlChar *zPart = xmlNodeGetContent(pCur);
			if( zPart ){
				SyBlobAppend(&sOut,(const void *)zPart,(sxu32)SyStrlen((const char *)zPart));
				xmlFree(zPart);
			}
		}
		ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
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
/*
 * The same screen for a `bool` property. php's weak mode takes an int, a float
 * or a string and answers its truthiness (`"0"` and `""` are false), and refuses
 * null, an array and an object -- the one difference from the `?string` block
 * above being that a bool property is NOT nullable, so `= null` is the TypeError
 * rather than the empty write. Answers 1 when the caller may go on and use
 * ph7_value_to_bool(), 0 when the refusal has been raised into *pRc.
 */
static int DomWriteBool(ph7_context *pCtx,const char *zOwner,const char *zProp,
	ph7_value *pVal,int *pRc)
{
	if( pVal == 0 || (pVal->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_NULL)) != 0 ){
		char zBuf[128];
		const char *zGiven = "null";
		if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){
			ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;
			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sName);
			zGiven = zBuf;
		}else if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 ){
			zGiven = ph7_type_name(pVal);
		}
		*pRc = PH7_VmThrowException(pCtx,"TypeError",
			"Cannot assign %s to property %s::$%s of type bool",zGiven,zOwner,zProp);
		return 0;
	}
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
/*
 * The same refusal, raised from a property WRITE.
 *
 * A write is not a call, so php has no accessor name to print in front of the
 * warning and attributes it to the CALLER's scope instead (`f(): Namespace
 * Error`, `Unknown: ...` at file scope) -- the shape DomSetContent already uses
 * for the libxml diagnostic a content write can produce. Nothing is answered
 * either way: a property write has no return value.
 */
static int DomThrowWrite(ph7_context *pCtx,int iCode)
{
	ph7_class_instance *pDoc = DomThisDoc(pCtx);
	SyBlob sFn;
	SyString sName;
	int rc;
	if( pDoc == 0 || PH7_NativeAttrTruthy(pDoc,"strictErrorChecking") ){
		return DomThrowAlways(pCtx,iCode);
	}
	SyBlobInit(&sFn,&pCtx->pVm->sAllocator);
	PH7_VmActiveFuncName(pCtx->pVm,&sFn);
	SyStringInitFromBuf(&sName,SyBlobData(&sFn),SyBlobLength(&sFn));
	rc = PH7_VmThrowError(pCtx->pVm,&sName,PH7_CTX_WARNING,DomErrText(iCode));
	SyBlobRelease(&sFn);
	return rc;
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
				*pRc = DomThrowWrite(pCtx,DOM_ERR_NAMESPACE);
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
				*pRc = DomThrowWrite(pCtx,DOM_ERR_NAMESPACE);
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
/*
 * The DOCUMENT's writable state: the three declaration slots php lets a program
 * change, plus `documentURI`.
 *
 * php declares them `?string`/`bool`, so a null goes through the string three as
 * the EMPTY string (`$d->version = null` writes `<?xml version=""?>`) and is a
 * TypeError on the bool pair; an array or an object is a TypeError on all of
 * them.  The read-only four are refused HERE rather than through the reader,
 * because two of them are deprecated and php's readonly Error comes without the
 * deprecation notice a read would have raised.
 */
static int DomSetDocProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlDocPtr pDoc = pNd ? (xmlDocPtr)pNd->pNode : 0;
	int bVersion = DomNameIs(zName,"version") || DomNameIs(zName,"xmlVersion");
	int bUri = DomNameIs(zName,"documentURI");
	SyBlob sVal;
	if( DomNameIs(zName,"actualEncoding") || DomNameIs(zName,"config")
	 || DomNameIs(zName,"xmlEncoding") ){
		*pRc = DomRefuseWrite(pCtx,zName,1);
		return DOM_SET_DONE;
	}
	if( bVersion || bUri || DomNameIs(zName,"encoding") ){
		const char *zNew;
		if( DomWriteText(pCtx,"DOMDocument",zName,"?string",pVal,&sVal,pRc) == 0 ){
			return DOM_SET_DONE;
		}
		zNew = (const char *)SyBlobData(&sVal);
		if( pDoc == 0 ){
			SyBlobRelease(&sVal);
			return DOM_SET_DONE;
		}
		if( bVersion ){
			if( pDoc->version ){
				xmlFree((xmlChar *)pDoc->version);
			}
			pDoc->version = xmlStrdup((const xmlChar *)zNew);
		}else if( bUri ){
			if( pDoc->URL ){
				xmlFree((xmlChar *)pDoc->URL);
			}
			pDoc->URL = xmlStrdup((const xmlChar *)zNew);
		}else{
			/* php asks libxml for a converter and refuses the name outright when
			 * there is none -- so `$d->encoding = 'x'` (and the empty string a
			 * null coerces to) is a ValueError BEFORE anything is written,
			 * rather than a document that cannot be serialized later. */
			/* A null is refused before libxml is asked anything, as php does: the
			 * empty string it coerces to is a name a current libxml (2.15) answers
			 * WITH a converter, so asking would let the null through. */
			xmlCharEncodingHandlerPtr pEnc = ph7_value_is_null(pVal) ? 0
				: xmlFindCharEncodingHandler(zNew);
			if( pEnc == 0 ){
				SyBlobRelease(&sVal);
				*pRc = PH7_VmThrowException(pCtx,"ValueError","Invalid document encoding");
				return DOM_SET_DONE;
			}
			xmlCharEncCloseFunc(pEnc);
			if( pDoc->encoding ){
				xmlFree((xmlChar *)pDoc->encoding);
			}
			pDoc->encoding = xmlStrdup((const xmlChar *)zNew);
		}
		SyBlobRelease(&sVal);
		return DOM_SET_DONE;
	}
	if( DomNameIs(zName,"standalone") || DomNameIs(zName,"xmlStandalone") ){
		if( DomWriteBool(pCtx,"DOMDocument",zName,pVal,pRc) == 0 ){
			return DOM_SET_DONE;
		}
		if( pDoc ){
			/* Either way it becomes a DECLARED answer: writing false is
			 * `standalone="no"` in the output, not the absent attribute. */
			pDoc->standalone = ph7_value_to_bool(pVal) ? 1 : 0;
		}
		return DOM_SET_DONE;
	}
	return DomSetNodeProp(pCtx,zName,pVal,pRc);
}
/* The two collections have nothing writable of their own; `length` is read-only. */
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
/*
 * DOMNameSpaceNode's ten properties. It is not a DOMNode -- php gives it its
 * own class with no parent -- so it shares none of the readers above: what it
 * carries is the DECLARATION (an xmlNs) and the element that makes it.
 */
static int DomNsNodeProp(ph7_context *pCtx,const char *zName)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNsPtr pNs = pNd ? (xmlNsPtr)pNd->pNode : 0;
	ph7_class_instance *pOwner = pThis ? PH7_NativeAttrObj(pThis,DOM_NS_OWNER) : 0;
	phl_domnode *pOwnerNd = DomResOf(pOwner);
	const char *zPrefix = (pNs && pNs->prefix) ? (const char *)pNs->prefix : 0;
	if( pNs == 0 ){
		return 0;
	}
	if( DomNameIs(zName,"nodeName") ){
		if( zPrefix ){
			ph7_result_string_format(pCtx,"%s:%s",DOM_XMLNS_NAME,zPrefix);
		}else{
			ph7_result_string(pCtx,DOM_XMLNS_NAME,-1);
		}
		return 1;
	}
	if( DomNameIs(zName,"nodeValue") || DomNameIs(zName,"namespaceURI") ){
		ph7_result_string(pCtx,pNs->href ? (const char *)pNs->href : "",-1);
		return 1;
	}
	if( DomNameIs(zName,"nodeType") ){
		ph7_result_int(pCtx,XML_NAMESPACE_DECL);
		return 1;
	}
	/* php answers the EMPTY prefix for the default declaration, and `xmlns` as
	 * its local name -- the two halves of the name it is spelled with. */
	if( DomNameIs(zName,"prefix") ){
		ph7_result_string(pCtx,zPrefix ? zPrefix : "",-1);
		return 1;
	}
	if( DomNameIs(zName,"localName") ){
		ph7_result_string(pCtx,zPrefix ? zPrefix : DOM_XMLNS_NAME,-1);
		return 1;
	}
	if( DomNameIs(zName,"isConnected") ){
		ph7_result_bool(pCtx,pOwnerNd != 0
			&& DomIsConnected((xmlNodePtr)pOwnerNd->pNode));
		return 1;
	}
	if( DomNameIs(zName,"ownerDocument") ){
		DomResultWrap(pCtx,DomThisDoc(pCtx));
		return 1;
	}
	if( DomNameIs(zName,"parentNode") || DomNameIs(zName,"parentElement") ){
		DomResultWrap(pCtx,pOwner);
		return 1;
	}
	return 0;
}
DOM_PROP_ACCESSORS(DOMNameSpaceNode,DomNsNodeProp,DomSetNothing)
DOM_PROP_ACCESSORS(DOMNodeList,DomListProp,DomSetNothing)
DOM_PROP_ACCESSORS(DOMNamedNodeMap,DomMapProp,DomSetNothing)
DOM_PROP_ACCESSORS(DOMNode,DomNodeProp,DomSetNodeProp)
DOM_PROP_ACCESSORS(DOMDocument,DomDocProp,DomSetDocProp)
DOM_PROP_ACCESSORS(DOMElement,DomElemProp,DomSetElemProp)
DOM_PROP_ACCESSORS(DOMAttr,DomAttrProp,DomSetAttrProp)
DOM_PROP_ACCESSORS(DOMCharacterData,DomCharProp,DomSetCharProp)
DOM_PROP_ACCESSORS(DOMText,DomTextProp,DomSetCharProp)
DOM_PROP_ACCESSORS(DOMProcessingInstruction,DomPiProp,DomSetPiProp)
/* The fragment writes what DOMNode writes; only its READ set is wider. */
DOM_PROP_ACCESSORS(DOMDocumentFragment,DomFragProp,DomSetNodeProp)
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
	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTN,pThis,zName,0,0);
	if( pList == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pList);
	return PH7_OK;
}
/*
 * DOMDocument::getElementsByTagNameNS / DOMElement::getElementsByTagNameNS
 * (?string $namespace, string $localName): DOMNodeList
 *
 * The namespace-aware half of the only two lookups the DOM has, and the one
 * every namespaced format is read with -- an XSLT stylesheet, a SOAP envelope, a
 * sitemap. Undefined here, so the URI could be answered for a node already found
 * and never searched FOR.
 *
 * The list is live and the receiver is never in it, exactly as the name-only
 * one; DomGebtnMatch carries php's asymmetric wildcard rules.
 */
DOM_METHOD(vm_builtin_Dom_getElementsByTagNameNS)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zUri = DomArgStrOrNull(nArg,apArg,0);
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	ph7_class_instance *pList;
	if( pThis == 0 ){
		return PH7_OK;
	}
	pList = DomNewCollection(pCtx->pVm,"DOMNodeList",DomThisDoc(pCtx),DNL_GEBTNNS,pThis,
		zName,zUri ? zUri : "",0);
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
		/* The four the parse reads (DomParseOptions). php's defaults are all
		 * false: nothing is validated, expanded, defaulted or recovered unless
		 * the program asks. */
		{ "validateOnParse",    PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },
		{ "resolveExternals",   PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },
		{ "substituteEntities", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },
		{ "recover",            PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 0, 0, 0.0 }, "bool" },
		/* The only one php defaults to TRUE: refusals are exceptions until a
		 * program asks for warnings (DomThrowAs). */
		{ "strictErrorChecking", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, "bool" },
		/* The identity cache DomWrap keys by node pointer. */
		{ DOM_NODES,            PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aDocMethod[] = {
		{ "__construct",          PH7_MOD_PUBLIC, "string $version = '1.0', string $encoding = ''", "",
		  vm_builtin_DOMDocument_construct },
		{ "loadXML",              PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",
		  vm_builtin_DOMDocument_loadXML },
		{ "load",                 PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@bool",
		  vm_builtin_DOMDocument_load },
		{ "save",                 PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@int|false",
		  vm_builtin_DOMDocument_save },
		{ "loadHTML",             PH7_MOD_PUBLIC, "string $source, int $options = 0", "@bool",
		  vm_builtin_DOMDocument_loadHTML },
		{ "loadHTMLFile",         PH7_MOD_PUBLIC, "string $filename, int $options = 0", "@bool",
		  vm_builtin_DOMDocument_loadHTMLFile },
		{ "saveHTML",             PH7_MOD_PUBLIC, "?DOMNode $node = null", "@string|false",
		  vm_builtin_DOMDocument_saveHTML },
		{ "saveHTMLFile",         PH7_MOD_PUBLIC, "string $filename", "@int|false",
		  vm_builtin_DOMDocument_saveHTMLFile },
		{ "saveXML",              PH7_MOD_PUBLIC, "?DOMNode $node = null, int $options = 0", "@string|false",
		  vm_builtin_DOMDocument_saveXML },
		{ "createElement",        PH7_MOD_PUBLIC, "string $localName, string $value = ''", "",
		  vm_builtin_DOMDocument_createElement },
		/* php declares no return type at all on this one either -- its answer is
		 * `DOMElement|false` and it never wrote that down. */
		{ "createElementNS",      PH7_MOD_PUBLIC,
		  "?string $namespace, string $qualifiedName, string $value = ''", "",
		  vm_builtin_DOMDocument_createElementNS },
		/* php declares no return type on this one either: `DOMNode|false`. */
		{ "importNode",           PH7_MOD_PUBLIC, "DOMNode $node, bool $deep = false", "",
		  vm_builtin_DOMDocument_importNode },
		{ "adoptNode",            PH7_MOD_PUBLIC, "DOMNode $node", "@DOMNode|false",
		  vm_builtin_DOMDocument_adoptNode },
		{ "getElementById",       PH7_MOD_PUBLIC, "string $elementId", "@?DOMElement",
		  vm_builtin_DOMDocument_getElementById },
		{ "createAttribute",      PH7_MOD_PUBLIC, "string $localName", "",
		  vm_builtin_DOMDocument_createAttribute },
		{ "createAttributeNS",    PH7_MOD_PUBLIC, "?string $namespace, string $qualifiedName", "",
		  vm_builtin_DOMDocument_createAttributeNS },
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
		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },
		/* The DOMParentNode three: real (non-tentative) void, one untyped
		 * variadic -- php's own rows, screened inside the body. */
		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },
		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },
		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },
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
		/* php declares no return type at all on the five that hand out a NODE --
		 * not even a tentative one -- because their answer is a union it never
		 * wrote down. The rows state none either. */
		{ "getAttributeNode",     PH7_MOD_PUBLIC, "string $qualifiedName", "",
		  vm_builtin_DOMElement_getAttributeNode },
		{ "getAttributeNodeNS",   PH7_MOD_PUBLIC, "?string $namespace, string $localName", "",
		  vm_builtin_DOMElement_getAttributeNodeNS },
		{ "setAttributeNode",     PH7_MOD_PUBLIC, "DOMAttr $attr", "",
		  vm_builtin_DOMElement_setAttributeNode },
		{ "setAttributeNodeNS",   PH7_MOD_PUBLIC, "DOMAttr $attr", "",
		  vm_builtin_DOMElement_setAttributeNodeNS },
		{ "removeAttributeNode",  PH7_MOD_PUBLIC, "DOMAttr $attr", "",
		  vm_builtin_DOMElement_removeAttributeNode },
		{ "getAttributeNames",    PH7_MOD_PUBLIC, "", "array",
		  vm_builtin_DOMElement_getAttributeNames },
		{ "hasAttributeNS",       PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@bool",
		  vm_builtin_DOMElement_hasAttributeNS },
		{ "removeAttributeNS",    PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@void",
		  vm_builtin_DOMElement_removeAttributeNS },
		{ "toggleAttribute",      PH7_MOD_PUBLIC, "string $qualifiedName, ?bool $force = null", "bool",
		  vm_builtin_DOMElement_toggleAttribute },
		{ "setIdAttribute",       PH7_MOD_PUBLIC, "string $qualifiedName, bool $isId", "@void",
		  vm_builtin_DOMElement_setIdAttribute },
		{ "setIdAttributeNS",     PH7_MOD_PUBLIC,
		  "string $namespace, string $qualifiedName, bool $isId", "@void",
		  vm_builtin_DOMElement_setIdAttributeNS },
		{ "setIdAttributeNode",   PH7_MOD_PUBLIC, "DOMAttr $attr, bool $isId", "@void",
		  vm_builtin_DOMElement_setIdAttributeNode },
		{ "setAttributeNS",       PH7_MOD_PUBLIC,
		  "?string $namespace, string $qualifiedName, string $value", "@void",
		  vm_builtin_DOMElement_setAttributeNS },
		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",
		  vm_builtin_Dom_getElementsByTagName },
		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },
		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },
		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },
		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },
		{ "__get",                PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMElement_get },
		{ "__isset",              PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMElement_isset },
		{ "__set",                PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMElement_set },
	};
	static const PH7_NativeMethodDef aAttrMethod[] = {
		{ "isId",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMAttr_isId },
		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMAttr_get },
		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMAttr_isset },
		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMAttr_set },
	};
	static const PH7_NativeMethodDef aCharMethod[] = {
		/* Every offset and count here is in UTF-8 CHARACTERS, php's unit. */
		{ "appendData",    PH7_MOD_PUBLIC, "string $data", "@true",
		  vm_builtin_DOMCharacterData_appendData },
		{ "substringData", PH7_MOD_PUBLIC, "int $offset, int $count", "",
		  vm_builtin_DOMCharacterData_substringData },
		{ "insertData",    PH7_MOD_PUBLIC, "int $offset, string $data", "@bool",
		  vm_builtin_DOMCharacterData_insertData },
		{ "deleteData",    PH7_MOD_PUBLIC, "int $offset, int $count", "@bool",
		  vm_builtin_DOMCharacterData_deleteData },
		{ "replaceData",   PH7_MOD_PUBLIC, "int $offset, int $count, string $data", "@bool",
		  vm_builtin_DOMCharacterData_replaceData },
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
		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },
		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },
		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },
		{ "__get",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMDocumentFragment_get },
		{ "__isset", PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMDocumentFragment_isset },
		{ "__set",   PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",
		  vm_builtin_DOMDocumentFragment_set },
	};
	static const PH7_NativeMethodDef aTextMethod[] = {
		{ "splitText", PH7_MOD_PUBLIC, "int $offset", "", vm_builtin_DOMText_splitText },
		/* php's 8.x rename and the name it renamed, one body. */
		{ "isWhitespaceInElementContent", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_DOMText_isWhitespace },
		{ "isElementContentWhitespace",   PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_DOMText_isWhitespace },
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
		/* ...and, for the namespace-aware lookup, the URI beside the name. */
		{ DNL_URI,       PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
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
		{ "getNamedItemNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@?DOMNode",
		  vm_builtin_DOMNamedNodeMap_getNamedItemNS },
		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
		{ "__get",        PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNamedNodeMap_get },
		{ "__isset",      PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNamedNodeMap_isset },
		{ "__set",        PH7_MOD_PUBLIC, "string $name, mixed $value", "@void", vm_builtin_DOMNamedNodeMap_set },
	};
	/* The declaration itself, the document it belongs to, and the element that
	 * MAKES it -- php's parentNode/parentElement. */
	static const PH7_NativePropDef aNsNodeProp[] = {
		{ DOM_RES,      PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_DOC,      PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_NS_OWNER, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aNsNodeMethod[] = {
		{ "__sleep",  PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },
		{ "__wakeup", PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },
		{ "__get",    PH7_MOD_PUBLIC, "string $name", "", vm_builtin_DOMNameSpaceNode_get },
		{ "__isset",  PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_DOMNameSpaceNode_isset },
		{ "__set",    PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",
		  vm_builtin_DOMNameSpaceNode_set },
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
	/* php's 8.0 insertion interfaces: three untyped-variadic void methods on
	 * the parent side (the child side is DOMChildNode below).  A class row
	 * declares its methods before the implement phase runs, so nothing is
	 * stubbed abstract. */
	static const PH7_NativeMethodDef aParentNodeIf[] = {
		{ "append",          PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },
		{ "prepend",         PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },
		{ "replaceChildren", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "DOMException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMParentNode", 0, 0, PH7_CLASS_INTERFACE,
		  aParentNodeIf, SX_ARRAYSIZE(aParentNodeIf), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aNodeMethod, SX_ARRAYSIZE(aNodeMethod), aNodeConst, SX_ARRAYSIZE(aNodeConst),
		  aNodeProp, SX_ARRAYSIZE(aNodeProp), 0, 0, 0 },
		{ "DOMDocument", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,
		  aDocMethod, SX_ARRAYSIZE(aDocMethod), 0, 0, aDocProp, SX_ARRAYSIZE(aDocProp), 0, 0, 0 },
		{ "DOMElement", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,
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
		{ "DOMDocumentFragment", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,
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
		/* php's own: a class of its OWN, with no parent at all -- a namespace
		 * declaration is not a DOMNode there, and `$ns instanceof DOMNode` is
		 * false. Its refusal to serialize is the same soft kind the node
		 * classes carry. */
		{ "DOMNameSpaceNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aNsNodeMethod, SX_ARRAYSIZE(aNsNodeMethod), 0, 0,
		  aNsNodeProp, SX_ARRAYSIZE(aNsNodeProp), 0, 0, 0 },
		{ "DOMXPath", 0, 0, PH7_CLASS_NOSERIALIZE,
		  aXPathMethod, SX_ARRAYSIZE(aXPathMethod), 0, 0, aXPathProp, SX_ARRAYSIZE(aXPathProp), 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_dom_unused;
#endif /* PH7_ENABLE_LIBXML */
