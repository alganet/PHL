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
#include <libxml/relaxng.h>
#include <libxml/valid.h>
#include <libxml/xinclude.h>
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
 * underlying node always yields the same object.  The entry is a BORROWED
 * pointer -- php's own shape, and the reason a wrapper can die at all: a
 * cache that took a reference kept every node object a program ever touched
 * until the document went, and with it the libxml node behind it.  So
 * DomWrap hands back an instance the CALLER OWNS, and DomNodeRelease drops
 * the entry.  (The old shape allocated a fresh phl_domnode on every
 * navigation step even when the cache then threw the result away; only a
 * genuine cache MISS allocates one now.)
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
/* The base-class => user-class table registerNodeClass writes (defined here
 * because the document CLONE, far above it, carries the table across). */
#define DOM_NCLS  "__ncls"

/*
 * The DOCUMENT's own directives: php's seven boolean properties.  Their value is
 * the extension's own state and not a question about the tree, and php keeps no
 * property SLOT for any of them -- each is a read_property/write_property handler,
 * which is why php's `(array)` cast and `get_object_vars()` show a DOMDocument as
 * empty.  So the object carries ONE hidden integer here and the class declares the
 * seven as VIRTUAL names (PH7_MOD_VIRTUAL) that DomDocProp/DomSetDocProp answer.
 * Four of them are read by every parse and one by every refusal, and a clone of a
 * document carries the whole word across with the slot itself.
 */
#define DOM_DFLAGS "__dflags"
#define DOM_F_PRESERVE_WS    0x01
#define DOM_F_FORMAT_OUTPUT  0x02
#define DOM_F_VALIDATE       0x04
#define DOM_F_RESOLVE_EXT    0x08
#define DOM_F_SUBST_ENT      0x10
#define DOM_F_RECOVER        0x20
#define DOM_F_STRICT_ERR     0x40
/* Not a directive: the document's FAMILY. php 8.4 put a second class tree over
 * the same libxml nodes, and the two never meet -- `Dom\\Element` is not a
 * DOMElement in either direction, `Dom\\ProcessingInstruction` sits under
 * `Dom\\CharacterData` where the 2004 one sits under DOMNode, and
 * `Dom\\Document` is abstract with two final documents under it. Which tree a
 * node is wrapped in is a property of the DOCUMENT it belongs to, and of nothing
 * about the node, so the answer rides in the document's flag word beside the
 * parser directives -- carried by a clone with the slot, exactly as they are. */
#define DOM_F_MODERN         0x80
/* php's defaults: nothing is validated, expanded, defaulted or recovered unless
 * the program asks, whitespace is kept, and a refusal is an exception. */
#define DOM_F_DEFAULT (DOM_F_PRESERVE_WS|DOM_F_STRICT_ERR)
static const struct { const char *zName; int iBit; } aDomDocFlag[] = {
	{ "preserveWhiteSpace",  DOM_F_PRESERVE_WS   },
	{ "formatOutput",        DOM_F_FORMAT_OUTPUT },
	{ "validateOnParse",     DOM_F_VALIDATE      },
	{ "resolveExternals",    DOM_F_RESOLVE_EXT   },
	{ "substituteEntities",  DOM_F_SUBST_ENT     },
	{ "recover",             DOM_F_RECOVER       },
	{ "strictErrorChecking", DOM_F_STRICT_ERR    }
};
/* Is this directive on for this document object? Answers false for anything that
 * is not one (a node's $__doc points at its own holder when it has no document). */
static int DomDocFlag(ph7_class_instance *pDoc,int iBit)
{
	return pDoc != 0 && (PH7_NativeAttrInt(pDoc,DOM_DFLAGS) & iBit) != 0;
}
/* The class a live view of THIS document's nodes is handed out as. One view, two
 * families: `$el->childNodes` is a DOMNodeList under a DOMDocument and a
 * `Dom\NodeList` under a `Dom\XMLDocument`, and the two are unrelated classes
 * over the same walk. */
static const char * DomCollClass(ph7_class_instance *pDoc,const char *zLegacy,
	const char *zModern)
{
	return DomDocFlag(pDoc,DOM_F_MODERN) ? zModern : zLegacy;
}
/* Is the RECEIVER a node of php 8.4's tree? Defined with the document property
 * readers below; declared here because the attribute API is one set of C bodies
 * serving both trees, and several of its answers differ only by family. */
static int DomThisModern(ph7_context *pCtx);
/* Which of the two XPath classes a diagnostic names; defined with the property
 * readers, declared here because the evaluator names it in one. */
static const char * DomXPathClassName(ph7_class_instance *pThis);

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
#define DOM_ERR_NO_MOD         7
#define DOM_ERR_NOT_FOUND      8
#define DOM_ERR_NOT_SUPPORTED  9
#define DOM_ERR_INVALID_STATE 11
#define DOM_ERR_SYNTAX        12
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
	case DOM_ERR_NO_MOD:       return "No Modification Allowed Error";
	case DOM_ERR_NOT_SUPPORTED: return "Not Supported Error";
	case DOM_ERR_INVALID_STATE: return "Invalid State Error";
	case DOM_ERR_SYNTAX:       return "Syntax Error";
	case DOM_ERR_NAMESPACE:    return "Namespace Error";
	default:                   return "Not Found Error";
	}
}
/* Forward: the refusal has to ask the receiver's document for its mode --
 * and check that what the slot holds IS a document. */
static ph7_class_instance * DomThisDoc(ph7_context *pCtx);
static phl_domnode * DomResOf(ph7_class_instance *pObj);
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
/*
 * A refusal made while the PROPERTY hook is running.
 *
 * The readers and writers below are shared: the same body answers `$el->tagName`
 * and the debug walk, and under the hook it runs on a scratch context inside the
 * member opcode. A throw raised there would run the enclosing catch mid-access,
 * before the opcode has settled its stack -- which is why PH7_NativePropCtx has
 * a refusal channel of its own. Answers 1 when the refusal was RECORDED (the
 * opcode raises it where the access lands) and 0 when the caller must throw the
 * ordinary way, which is every call made from a method body.
 */
static int DomPropRefuse(ph7_context *pCtx,const char *zClass,sxi32 iCode,const char *zMsg)
{
	PH7_NativePropCtx *pProp = pCtx ? pCtx->pPropCtx : 0;
	if( pProp == 0 ){
		return 0;
	}
	pProp->bAnswered = 1;
	pProp->zThrowClass = zClass;
	pProp->iThrowCode = iCode;
	SyBufferFormat(pProp->zThrowMsg,sizeof(pProp->zThrowMsg),"%s",zMsg);
	return 1;
}
/* The same, for a refusal whose message is formatted. */
static sxi32 DomPropThrow(ph7_context *pCtx,const char *zClass,sxi32 iCode,
	const char *zFormat,...)
{
	SyBlob sMsg;
	va_list ap;
	sxi32 rc;
	SyBlobInit(&sMsg,&pCtx->pVm->sAllocator);
	va_start(ap,zFormat);
	SyBlobFormatAp(&sMsg,zFormat,ap);
	va_end(ap);
	SyBlobNullAppend(&sMsg);
	if( DomPropRefuse(pCtx,zClass,iCode,(const char *)SyBlobData(&sMsg)) ){
		rc = PH7_OK;
	}else{
		rc = PH7_VmThrowExceptionCode(pCtx,zClass,iCode,"%s",(const char *)SyBlobData(&sMsg));
	}
	SyBlobRelease(&sMsg);
	return rc;
}
static int DomThrowAlways(ph7_context *pCtx,int iCode)
{
	if( DomPropRefuse(pCtx,"DOMException",(sxi32)iCode,DomErrText(iCode)) ){
		return PH7_OK;
	}
	return PH7_VmThrowExceptionCode(pCtx,"DOMException",(sxi32)iCode,"%s",DomErrText(iCode));
}
/* The same for a refusal that carries its OWN sentence rather than the level-2
 * table's -- php 8.4's document-child rules, whose four messages are prose and
 * not one of the eleven codes' names. Like DomThrowAlways it ignores
 * strictErrorChecking, which the namespaced documents do not have. */
static int DomThrowSentence(ph7_context *pCtx,int iCode,const char *zMsg)
{
	if( DomPropRefuse(pCtx,"DOMException",(sxi32)iCode,zMsg) ){
		return PH7_OK;
	}
	return PH7_VmThrowExceptionCode(pCtx,"DOMException",(sxi32)iCode,"%s",zMsg);
}
static int DomThrowFor(ph7_context *pCtx,ph7_class_instance *pDoc,int iCode,int iAnswer)
{
	/* Only a DOCUMENT carries the flag: a constructed ownerless node's $__doc
	 * slot points at its own holder object, and php is always strict there --
	 * there is no document to have said otherwise. */
	phl_domnode *pDocNd = pDoc ? DomResOf(pDoc) : 0;
	xmlNodePtr pDocNode = pDocNd ? (xmlNodePtr)pDocNd->pNode : 0;
	if( pDocNode
	 && (pDocNode->type != XML_DOCUMENT_NODE && pDocNode->type != XML_HTML_DOCUMENT_NODE) ){
		pDoc = 0;
	}
	if( pDoc && !DomDocFlag(pDoc,DOM_F_STRICT_ERR) ){
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
/* ...and the ONE name the DOM matches case-insensitively: insertAdjacent*'s
 * `$where` word ("BeforeBegin" works), php's zend_string_equals_literal_ci. */
static int DomNameIsCi(const char *zName,const char *zWant)
{
	sxu32 n = (sxu32)SyStrlen(zWant);
	return SyStrlen(zName) == n && SyStrnicmp(zName,zWant,n) == 0;
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
	/* A handle that IS the document names this object as the tree's document
	 * wrapper, so anything holding only the SHELL -- ext/simplexml's
	 * dom_import_simplexml() -- can reach the cache the identity rule lives in.
	 * Borrowed: DomDocRelease clears it when the object goes. */
	if( pRes && pRes->pShell && pRes->pNode
	 && (((xmlNodePtr)pRes->pNode)->type == XML_DOCUMENT_NODE
	  || ((xmlNodePtr)pRes->pNode)->type == XML_HTML_DOCUMENT_NODE) ){
		pRes->pShell->pDocObj = (void *)pObj;
	}
}
/* ph7_class::xRelease for DOMDocument: forget a document object its tree still
 * points at. Not a __destruct -- php declares none. */
static void DomDocRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pVal = PH7_NativeAttr(pThis,DOM_RES);
	phl_domnode *pNd = pVal && (pVal->iFlags & MEMOBJ_RES)
		? (phl_domnode *)pVal->x.pOther : 0;
	(void)pVm;
	if( pNd && pNd->pShell && pNd->pShell->pDocObj == (void *)pThis ){
		pNd->pShell->pDocObj = 0;
	}
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
static const char * DomClassOfKind(int bModern,int iKind)
{
	if( bModern ){
		/* php's namespaced tree. Its CDATA section is spelled with both letters
		 * capitalised where the 2004 name is not, and an ELEMENT declaration is
		 * an entity here for the same reason it is there. */
		switch( iKind ){
		case XML_ELEMENT_NODE:       return "Dom\\Element";
		case XML_ATTRIBUTE_NODE:     return "Dom\\Attr";
		case XML_TEXT_NODE:          return "Dom\\Text";
		case XML_CDATA_SECTION_NODE: return "Dom\\CDATASection";
		case XML_COMMENT_NODE:       return "Dom\\Comment";
		case XML_PI_NODE:            return "Dom\\ProcessingInstruction";
		case XML_DOCUMENT_FRAG_NODE: return "Dom\\DocumentFragment";
		case XML_ENTITY_REF_NODE:    return "Dom\\EntityReference";
		case XML_DTD_NODE:
		case XML_DOCUMENT_TYPE_NODE: return "Dom\\DocumentType";
		case XML_ENTITY_DECL:
		case XML_ELEMENT_DECL:       return "Dom\\Entity";
		case XML_NOTATION_NODE:      return "Dom\\Notation";
		default:                     return "Dom\\Node";
		}
	}
	switch( iKind ){
	case XML_ELEMENT_NODE:       return "DOMElement";
	case XML_ATTRIBUTE_NODE:     return "DOMAttr";
	case XML_TEXT_NODE:          return "DOMText";
	case XML_CDATA_SECTION_NODE: return "DOMCdataSection";
	case XML_COMMENT_NODE:       return "DOMComment";
	case XML_PI_NODE:            return "DOMProcessingInstruction";
	case XML_DOCUMENT_FRAG_NODE: return "DOMDocumentFragment";
	case XML_ENTITY_REF_NODE:    return "DOMEntityReference";
	case XML_DTD_NODE:
	case XML_DOCUMENT_TYPE_NODE: return "DOMDocumentType";
	/* php has one class for the whole declaration half of a DTD and hands an
	 * ELEMENT declaration the entity's, which is the class a `$doctype->
	 * childNodes` walk meets. DOMEntity's own readers ask the node's real
	 * type before touching a field, so an element declaration answers null
	 * from each of them rather than reading an xmlElement as an xmlEntity. */
	case XML_ENTITY_DECL:
	case XML_ELEMENT_DECL:       return "DOMEntity";
	case XML_NOTATION_NODE:      return "DOMNotation";
	default:                     return "DOMNode";
	}
}
/*
 * The nodeType php reports, which is not always libxml's own.
 *
 * The two numberings were built to agree -- a text node is 3 in both -- but
 * libxml parses a DOCTYPE into an XML_DTD_NODE (14) where the DOM's number for
 * one is DOCUMENT_TYPE_NODE (10), and php reports the DOM's.  So
 * `$n->nodeType === XML_DOCUMENT_TYPE_NODE` -- the way a walk tells the doctype
 * from an element without a `get_class` -- was FALSE here for every document
 * carrying one.
 */
static int DomNodeTypeOf(xmlNodePtr pNode)
{
	if( pNode == 0 ){
		return 0;
	}
	return pNode->type == XML_DTD_NODE ? (int)XML_DOCUMENT_TYPE_NODE : (int)pNode->type;
}
/* Defined with registerNodeClass below, which is the only thing that makes the
 * answer anything other than DomClassOfKind's. */
static const char * DomWrapClassName(ph7_vm *pVm,ph7_class_instance *pDoc,int iKind,
	SyBlob *pOut);
/* Defined with the namespaced document's readers below: is the receiver's
 * document one of php 8.4's tree? Two of the shared readers answer differently
 * there. */
static int DomThisModern(ph7_context *pCtx);
/* Defined with the teardown machinery below: every wrap marks its node HELD. */
static void DomNodeMarkHeld(xmlNodePtr pNode,ph7_class_instance *pObj);
/*
 * The wrapper object for one node of pDoc's tree -- the same one every time,
 * which is what makes `$doc->documentElement === $doc->documentElement` true.
 *
 * OWNED: the caller holds a reference and gives it back. A caller that hands
 * the node to PHP goes through DomResultOwned; one that stores it takes its own
 * reference (PH7_NativeSetAttrObj, ph7_array_add_elem) and then unrefs.
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
	SyBlob sName;
	if( pNode == 0 || pDoc == 0 ){
		return 0;
	}
	if( pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE ){
		/* The document is its own wrapper: php answers the SAME DOMDocument. */
		pDoc->iRef++;
		return pDoc;
	}
	pCache = DomCache(&(*pVm),pDoc);
	if( pCache == 0 ){
		return 0;
	}
	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);
	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){
		ph7_value *pHit = HashmapExtractNodeValue(pEntry);
		if( pHit && (pHit->iFlags & MEMOBJ_INT) ){
			ph7_class_instance *pLive = (ph7_class_instance *)(sxuptr)pHit->x.iVal;
			PH7_MemObjRelease(&sKey);
			pLive->iRef++;
			return pLive;
		}
	}
	/* php's class for the kind, unless this document has REGISTERED another
	 * one for it (registerNodeClass). The name may live in sName's buffer, so
	 * the blob outlives the lookup. */
	SyBlobInit(&sName,&pVm->sAllocator);
	zClass = DomWrapClassName(&(*pVm),pDoc,(int)pNode->type,&sName);
	pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	SyBlobRelease(&sName);
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
	DomNodeMarkHeld(pNode,pObj);
	/* The entry is the POINTER and no reference (an xmlNs handle, which shares
	 * this table, is a MEMOBJ_RES: the two never key the same address). The
	 * object's own release takes the entry back out. */
	PH7_MemObjInitFromInt(&(*pVm),&sVal,(sxi64)(sxuptr)pObj);
	PH7_HashmapInsert(pCache,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	return pObj;                             /* the constructor's reference: the caller's */
}
/*
 * The wrapper for one node of a tree whose DOCUMENT OBJECT the caller does not
 * have -- ext/simplexml's `dom_import_simplexml()`, which holds a shell and a
 * node and nothing else.
 *
 * The identity rule (`$doc->documentElement === $doc->documentElement`) lives
 * in a cache keyed on the document object, so a tree that has none yet gets one
 * built here and remembered on the shell; a tree that came from a DOMDocument
 * already names it, which is what makes an import back out of a SimpleXML made
 * from that document answer the document's own nodes. OWNED, like DomWrap's.
 */
PH7_PRIVATE ph7_class_instance * PH7_DomWrapForeign(ph7_vm *pVm,phl_xmldoc *pShell,void *pNode)
{
	ph7_class_instance *pDoc;
	if( pShell == 0 || pShell->pDoc == 0 ){
		return 0;
	}
	pDoc = (ph7_class_instance *)pShell->pDocObj;
	if( pDoc == 0 ){
		ph7_class *pClass = PH7_VmExtractClass(&(*pVm),"DOMDocument",
			sizeof("DOMDocument")-1,FALSE,0);
		phl_domnode *pRes;
		pDoc = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;
		pRes = pDoc ? DomNewRes(&(*pVm),pShell,pShell->pDoc) : 0;
		if( pRes == 0 ){
			if( pDoc ){
				PH7_ClassInstanceUnref(pDoc);
			}
			return 0;
		}
		PH7_NativeSetAttrInt(&(*pVm),pDoc,DOM_DFLAGS,DOM_F_DEFAULT);
		DomSetRes(&(*pVm),pDoc,pRes);          /* ...which records pShell->pDocObj */
		PH7_NativeSetAttrObj(&(*pVm),pDoc,DOM_DOC,pDoc);
	}
	return DomWrap(&(*pVm),pDoc,pShell,(xmlNodePtr)pNode);
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
/* The same, for an instance the caller OWNS: the result takes its own
 * reference and this one goes back. */
static int DomResultOwned(ph7_context *pCtx,ph7_class_instance *pObj)
{
	int rc = DomResultWrap(pCtx,pObj);
	if( pObj ){
		PH7_ClassInstanceUnref(pObj);
	}
	return rc;
}
/* The common tail: wrap a node of the RECEIVER's document and answer it. */
static int DomResultNodeOf(ph7_context *pCtx,phl_domnode *pNd,xmlNodePtr pNode)
{
	if( pNd == 0 || pNode == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultOwned(pCtx,DomWrap(pCtx->pVm,DomThisDoc(pCtx),pNd->pShell,pNode));
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
/*
 * Mark a node as HELD by one wrapper, in libxml's own per-node binding slot --
 * where php keeps its object too.
 *
 * That slot, and not the identity cache, is what a free walk asks: the cycle
 * collector may reach a document before its nodes, and the cache is gone by
 * then. A synthesized NOTATION stand-in is the exception -- its `_private`
 * already carries the declaration it stands for, which is how a second lookup
 * finds the same stand-in (DomNotationNode) -- and nothing frees one of those
 * anyway.
 */
static void DomNodeMarkHeld(xmlNodePtr pNode,ph7_class_instance *pObj)
{
	if( pNode->type != XML_NOTATION_NODE ){
		pNode->_private = (void *)pObj;
	}
}
/* ...and is this node still that wrapper's? A notation stand-in always is. */
static int DomNodeHeldBy(xmlNodePtr pNode,ph7_class_instance *pThis)
{
	return pNode->type == XML_NOTATION_NODE || pNode->_private == (void *)pThis;
}
/* The cache entry under one pointer -- a node's wrapper (MEMOBJ_INT) or an
 * xmlNs handle (MEMOBJ_RES) -- or NULL when nothing holds it. */
static ph7_value * DomCacheHit(ph7_vm *pVm,ph7_hashmap *pCache,const void *pKey)
{
	ph7_hashmap_node *pEntry = 0;
	ph7_value sKey,*pHit = 0;
	if( pCache == 0 || pKey == 0 ){
		return 0;
	}
	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pKey);
	if( PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){
		pHit = HashmapExtractNodeValue(pEntry);
	}
	PH7_MemObjRelease(&sKey);
	return pHit;
}
/*
 * The node kinds a detached-node free may take. Everything else -- a DTD, the
 * declaration nodes inside one, the synthesized notation stand-ins vm_libxml.c
 * frees its own way -- stays with the document: xmlFreeNode would read an
 * xmlEntity's fields as a node's, and none of them is ever what a program
 * drops in a loop.
 */
static int DomNodeIsFreeable(xmlNodePtr pNode)
{
	switch( pNode->type ){
	case XML_ELEMENT_NODE:
	case XML_ATTRIBUTE_NODE:
	case XML_TEXT_NODE:
	case XML_CDATA_SECTION_NODE:
	case XML_COMMENT_NODE:
	case XML_PI_NODE:
	case XML_ENTITY_REF_NODE:
	case XML_DOCUMENT_FRAG_NODE:
		return 1;
	default:
		return 0;
	}
}
/* A namespace DECLARATION on this element that PHP still holds a handle onto.
 * xmlFreeNode frees the xmlNs under it, and the handle is shared by every
 * DOMNameSpaceNode built from it, so such a node is kept instead. */
static xmlAttrPtr DomNsAttrFind(phl_xmldoc *pShell,xmlNsPtr pNs);
static int DomNodeNsHeld(ph7_vm *pVm,ph7_hashmap *pCache,phl_xmldoc *pShell,
	xmlNodePtr pNode)
{
	xmlNsPtr pNs;
	if( pNode->type != XML_ELEMENT_NODE ){
		return 0;
	}
	for( pNs = pNode->nsDef ; pNs ; pNs = pNs->next ){
		xmlAttrPtr pStand;
		if( DomCacheHit(&(*pVm),pCache,pNs) ){
			return 1;
		}
		/* php 8.4's tree hands a declaration out as a stand-in ATTRIBUTE
		 * instead, whose held marker is the attribute's own: freeing the
		 * element would free the xmlNs the stand-in points at. */
		pStand = DomNsAttrFind(pShell,pNs);
		if( pStand && pStand->_private ){
			return 1;
		}
	}
	return 0;
}
static void DomFreeDetached(ph7_vm *pVm,ph7_hashmap *pCache,phl_xmldoc *pShell,xmlNodePtr pNode);
/*
 * Is this handle's document still REGISTERED?
 *
 * At VM reset and release the whole registry goes first (vm_libxml.c frees every
 * tree, its orphans and the shell itself) and the objects are torn down after.
 * A wrapper released then must not free a node the shell already freed -- and
 * must not read the shell to find out, which is why this compares pointers.
 */
static int DomShellLive(ph7_vm *pVm,const phl_xmldoc *pShell)
{
	const phl_xmldoc *pCur;
	for( pCur = (const phl_xmldoc *)pVm->pXmlDocs ; pCur ; pCur = pCur->pNext ){
		if( pCur == pShell ){
			return 1;
		}
	}
	return 0;
}
/*
 * One child or attribute list of a node that is going. php's own rule
 * (php_libxml_node_free_list): a descendant PHP still holds a wrapper for is
 * UNLINKED and kept -- which is what makes `$child->parentNode` null once the
 * parent object goes -- and freed later by its own release.
 */
static void DomFreeChildList(ph7_vm *pVm,ph7_hashmap *pCache,phl_xmldoc *pShell,
	xmlNodePtr pList)
{
	xmlNodePtr pCur = pList,pNext;
	while( pCur ){
		pNext = pCur->next;
		if( !DomNodeIsFreeable(pCur) ){
			/* A declaration or a DTD: libxml's own free owns it, so it stays
			 * linked and goes with the parent. */
			pCur = pNext;
			continue;
		}
		xmlUnlinkNode(pCur);
		if( pCur->_private ){
			DomOrphanAdd(pShell,pCur);   /* held: the document owns it until then */
		}else{
			DomFreeDetached(&(*pVm),pCache,pShell,pCur);
		}
		pCur = pNext;
	}
}
/* Free a node with no parent, and everything under it nothing else holds. */
static void DomFreeDetached(ph7_vm *pVm,ph7_hashmap *pCache,phl_xmldoc *pShell,
	xmlNodePtr pNode)
{
	if( pNode == 0 ){
		return;
	}
	if( !DomNodeIsFreeable(pNode) ){
		/* A DTD, a declaration inside one, or one of the synthesized notation
		 * stand-ins: the document owns each of those by another route, and
		 * parking one on the orphan set would free it twice. */
		return;
	}
	if( DomNodeNsHeld(&(*pVm),pCache,pShell,pNode) ){
		DomOrphanAdd(pShell,pNode);
		return;
	}
	/* An entity reference's children are the ENTITY's content and not its own;
	 * libxml's own free walks past them for the same reason. */
	if( pNode->type != XML_ENTITY_REF_NODE ){
		DomFreeChildList(&(*pVm),pCache,pShell,pNode->children);
	}
	/* `properties` and `nsDef` live past an xmlAttr's end: elements only. */
	if( pNode->type == XML_ELEMENT_NODE ){
		DomFreeChildList(&(*pVm),pCache,pShell,(xmlNodePtr)pNode->properties);
	}
	DomOrphanRemove(pShell,pNode);
	xmlFreeNode(pNode);
}
/*
 * ph7_class::xRelease for every node class -- php's dom_object free.
 *
 * Two things happen when the last PHP reference to a wrapper goes. The
 * document's identity cache holds a BORROWED pointer to it, so this is the only
 * place that entry can be dropped, and it must be: the next node libxml puts at
 * the same address would otherwise answer a dead object. And a node with no
 * PARENT is this object's to free, as it is php's -- an attached one belongs to
 * the tree and is never touched.
 */
static void DomNodeRelease(ph7_vm *pVm,ph7_class_instance *pThis)
{
	ph7_value *pVal = PH7_NativeAttr(pThis,DOM_RES);
	phl_domnode *pNd = pVal && (pVal->iFlags & MEMOBJ_RES)
		? (phl_domnode *)pVal->x.pOther : 0;
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	ph7_hashmap *pCache;
	if( pNd == 0 ){
		return;
	}
	pCache = DomCache(&(*pVm),PH7_NativeAttrObj(pThis,DOM_DOC));
	if( pNode && DomShellLive(&(*pVm),pNd->pShell) && DomNodeHeldBy(pNode,pThis) ){
		ph7_hashmap_node *pEntry = 0;
		ph7_value sKey;
		if( pNode->type != XML_NOTATION_NODE ){
			pNode->_private = 0;
		}
		PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);
		if( pCache && PH7_HashmapLookup(pCache,&sKey,&pEntry) == SXRET_OK && pEntry ){
			ph7_value *pHit = HashmapExtractNodeValue(pEntry);
			/* ...only when the entry is still THIS object. A document that has
			 * been repointed (loadXML) drops its cache, and a node of the new
			 * tree can land on the address a surviving old wrapper names. */
			if( pHit && (pHit->iFlags & MEMOBJ_INT)
			 && (ph7_class_instance *)(sxuptr)pHit->x.iVal == pThis ){
				PH7_HashmapUnlinkNode(pEntry,TRUE);
			}
		}
		PH7_MemObjRelease(&sKey);
		/* A namespace handle is the document's and shared, so the nsDef screen
		 * inside the walk needs a cache to read: with none, nothing is freed. */
		if( pCache && pNode != (xmlNodePtr)pNd->pShell->pDoc && pNode->parent == 0 ){
			DomFreeDetached(&(*pVm),pCache,pNd->pShell,pNode);
		}
	}
	SyMemBackendFree(&pVm->sAllocator,pNd);
	pVal->x.pOther = 0;
	MemObjSetType(pVal,MEMOBJ_NULL);
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
	case XML_TEXT_NODE:
	case XML_COMMENT_NODE:
	case XML_CDATA_SECTION_NODE:
	case XML_PI_NODE:
		/* The CONTENT POINTER itself, not xmlNodeGetContent's copy: a null
		 * pointer -- the omitted-argument constructor's state -- reads NULL
		 * where an empty string reads "", and newer libxml's
		 * xmlNodeGetContent papers over exactly that difference (2.13 answers
		 * "" for both, 2.9 answers NULL for the pointer). */
		if( pNode->content == 0 ){
			ph7_result_null(pCtx);
		}else{
			ph7_result_string(pCtx,(const char *)pNode->content,-1);
		}
		return;
	case XML_ATTRIBUTE_NODE:
	case XML_ELEMENT_NODE:
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
/* The `data` property's reading of the same content: php COERCES there, so a
 * NULL content pointer -- the omitted-argument constructors' state -- reads ""
 * from `$node->data` and null from `$node->nodeValue`, one node, two answers. */
static void DomDataValue(ph7_context *pCtx,xmlNodePtr pNode)
{
	DomNodeValue(pCtx,pNode);
	if( pCtx->pRet->iFlags & MEMOBJ_NULL ){
		ph7_result_string(pCtx,"",0);
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
/* Said before an ATTRIBUTE is unlinked from its element, wherever that is done:
 * a declaration written after it has to be told, while the attribute before it
 * can still be read. Anything else is a no-op. */
static void DomAttrGoing(xmlNodePtr pNode);
/* The two child counts childNodes->length and childElementCount read. */
static xmlNodePtr DomRefChildren(xmlNodePtr pNode);
static int DomChildCount(xmlNodePtr pNode,int bElementsOnly)
{
	xmlNodePtr pChild = DomRefChildren(pNode);
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
	xmlNodePtr pChild = DomRefChildren(pNode);
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
/* The attribute machinery, defined with the attribute surface below: the four
 * mutators reach it because php's appendChild/insertBefore ATTACH an attribute
 * argument as a property rather than splicing it among the children. */
static xmlAttrPtr DomAttrByLocal(xmlNodePtr pElem,const char *zName);
static xmlAttrPtr DomAttrByNs(xmlNodePtr pElem,const xmlChar *zUri,const char *zLocal);
static void DomAttrLinkLast(xmlNodePtr pElem,xmlAttrPtr pAttr);
static void DomAttrLinkBefore(xmlNodePtr pElem,xmlAttrPtr pAttr,xmlAttrPtr pRef);
static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr);
static void DomNsAttrArrive(xmlNodePtr pElem,xmlAttrPtr pAttr);
/* The adoptNode wrapper machinery, defined with it below: the insertion doors
 * run it too, because php ADOPTS a constructed, ownerless argument -- doc,
 * identity-cache home and handle shell all move on the first insertion. */
static void DomAdoptWrappers(ph7_vm *pVm,ph7_class_instance *pSrcDoc,
	ph7_class_instance *pDstDoc,phl_xmldoc *pDstShell,xmlNodePtr pNode);
static xmlNodePtr DomWalkNext(xmlNodePtr pCur,xmlNodePtr pRoot);
/*
 * An entity REFERENCE's children as php answers them. libxml's re-homing
 * CLEARS the raw link when a constructed reference is adopted -- and php's
 * raw state stays cleared, which replaceChild's childless-false cell measures
 * -- but php's READERS still resolve: `$ref->firstChild` answers the NEW
 * document's declaration for the name, or the predefined five, or nothing.
 */
static xmlNodePtr DomRefChildren(xmlNodePtr pNode)
{
	if( pNode && pNode->type == XML_ENTITY_REF_NODE
	 && pNode->children == 0 && pNode->doc ){
		return (xmlNodePtr)xmlGetDocEntity(pNode->doc,pNode->name);
	}
	return pNode ? pNode->children : 0;
}
/*
 * Take an OWNERLESS subtree into the receiver's world, php's constructed-node
 * adoption: the libxml nodes get the receiver's document (none of their
 * strings are dict-interned -- a constructed node's are plain allocations, so
 * xmlSetTreeDoc is the whole move), the orphan entry crosses from the limbo
 * shell to the receiver's, and every wrapper PHP holds re-homes into the
 * receiver's identity cache.  Also the OWNERLESS-to-OWNERLESS merge, where no
 * document changes hands but the wrappers still need ONE holder for
 * `$a->firstChild === $b` to hold.  The caller has already screened documents:
 * a mismatch here means the argument's is NULL.
 */
static void DomAdoptIntoRecv(ph7_context *pCtx,ph7_value *pArgVal,phl_domnode *pArgNd)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pSrcHolder = DomObjArgDoc(pArgVal);
	ph7_class_instance *pDstHolder = DomThisDoc(pCtx);
	phl_domnode *pRecv = DomThisNode(pCtx);
	xmlNodePtr pNode = (xmlNodePtr)pArgNd->pNode;
	xmlNodePtr pRecvNode = pRecv ? (xmlNodePtr)pRecv->pNode : 0;
	if( pSrcHolder == pDstHolder || pRecvNode == 0 ){
		return;
	}
	if( pNode->doc == 0 && pRecvNode->doc ){
		xmlSetTreeDoc(pNode,pRecvNode->doc);
	}
	if( pArgNd->pShell != pRecv->pShell ){
		DomOrphanRemove(pArgNd->pShell,pNode);
		DomOrphanAdd(pRecv->pShell,pNode);
		pArgNd->pShell = pRecv->pShell;
	}
	DomAdoptWrappers(pVm,pSrcHolder,pDstHolder,pRecv->pShell,pNode);
}

/*
 * php's dom_node_children_valid: the node kinds that can never have children.
 * A level-2 mutator on such a receiver answers FALSE with nothing said at all
 * -- no warning, no exception -- and answers it BEFORE any other screen, so
 * `$text->appendChild($nodeFromAnotherDocument)` is false, not Wrong Document.
 */
static int DomChildrenValid(xmlNodePtr pNode)
{
	switch( pNode->type ){
	case XML_TEXT_NODE:
	case XML_CDATA_SECTION_NODE:
	case XML_PI_NODE:
	case XML_COMMENT_NODE:
	case XML_DOCUMENT_TYPE_NODE:
	case XML_DTD_NODE:
	case XML_NOTATION_NODE:
		return 0;
	default:
		return 1;
	}
}
/*
 * The two ends of the child list php's `firstChild`/`lastChild` answer, and
 * what `hasChildNodes()` asks -- all three through the same screen, so the
 * DOCTYPE (whose declarations libxml really does link as children) answers
 * null, null and false the way php's do.
 */
static xmlNodePtr DomNodeChildFirst(xmlNodePtr pNode)
{
	if( pNode == 0 || !DomChildrenValid(pNode) ){
		return 0;
	}
	return DomRefChildren(pNode);
}
static xmlNodePtr DomNodeChildLast(xmlNodePtr pNode)
{
	if( pNode == 0 || !DomChildrenValid(pNode) ){
		return 0;
	}
	return pNode->last ? pNode->last : DomRefChildren(pNode);
}
/*
 * php's dom_node_is_read_only: the DTD-owned kinds -- an entity reference's
 * subtree is the entity's, shared by every reference to it -- and, one clause
 * later, a node with NO document: a constructed `new DOMText('t')` that was
 * never adopted refuses the level-2 child-list doors with No Modification
 * Allowed where the modern variadic family compares documents instead.
 */
static int DomNodeReadOnly(xmlNodePtr pNode)
{
	switch( pNode->type ){
	case XML_ENTITY_REF_NODE:
	case XML_ENTITY_NODE:
	case XML_DOCUMENT_TYPE_NODE:
	case XML_NOTATION_NODE:
	case XML_DTD_NODE:
	case XML_ELEMENT_DECL:
	case XML_ATTRIBUTE_DECL:
	case XML_ENTITY_DECL:
		return 1;
	default:
		return pNode->doc == 0;
	}
}
/* The two screens every level-2 mutator opens with, in php's order: an
 * invalid-children receiver answers false in silence, then the read-only
 * refusal -- the receiver's own, or that of the parent the CHILD would be
 * taken from. Returns non-zero when the caller must stop (result already
 * set). */
static int DomMutatorScreen(ph7_context *pCtx,xmlNodePtr pParent,xmlNodePtr pChild,int *pRc)
{
	if( !DomChildrenValid(pParent) ){
		ph7_result_bool(pCtx,0);
		*pRc = PH7_OK;
		return 1;
	}
	if( DomNodeReadOnly(pParent)
	 || (pChild->parent && DomNodeReadOnly(pChild->parent)) ){
		*pRc = DomThrow(pCtx,DOM_ERR_NO_MOD);
		return 1;
	}
	return 0;
}
/*
 * The attribute HALF of appendChild/insertBefore: php hands an attribute
 * argument to xmlAddChild, which attaches it as a PROPERTY -- so
 * `$el->appendChild($attr)` is a spelling of setAttributeNode, not a child
 * splice (the chunk spliced it among the children and serialized `<r> k=""`,
 * bytes that are not XML). The receiver must be an ELEMENT: a document, a
 * fragment or an attribute answers the Hierarchy refusal. An existing
 * attribute of the same name AND namespace (a plain `k` leaves a namespaced
 * `p:k` alone -- php 8.5.10's rule, both spellings; 8.5.9 matched the name
 * alone) is displaced UNLESS it is the argument itself, and the argument always (re)enters at the
 * tail of the property list, which is observable: appending an element's own
 * first attribute moves it last.
 *
 * One deliberate divergence, recorded: php FREES the displaced
 * attribute, so a wrapper held across the call answers Invalid State from
 * every later read ("Couldn't fetch DOMAttr" from a method). PHL parks it
 * detached and alive -- the same after-state setAttributeNode leaves.
 */
static int DomMutatorAttrAttach(ph7_context *pCtx,phl_domnode *pPar,phl_domnode *pChd,
	ph7_value *pArg)
{
	xmlNodePtr pElem = (xmlNodePtr)pPar->pNode;
	xmlAttrPtr pAttr = (xmlAttrPtr)pChd->pNode;
	xmlAttrPtr pOld;
	if( pElem->type != XML_ELEMENT_NODE ){
		return DomThrow(pCtx,DOM_ERR_HIERARCHY);
	}
	pOld = DomAttrByNs(pElem,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name);
	if( pOld && pOld != pAttr ){
		DomAttrGoing((xmlNodePtr)pOld);
		xmlUnlinkNode((xmlNodePtr)pOld);
		DomOrphanAdd(pPar->pShell,(xmlNodePtr)pOld);
	}
	DomAttrDetach(pChd->pShell,pAttr);
	DomAttrLinkLast(pElem,pAttr);
	DomNsAttrArrive(pElem,pAttr);
	ph7_result_value(pCtx,pArg);
	return PH7_OK;
}

/*
 * php's refusal taxonomy for linking pChild under pParent, or NULL when the
 * link is allowed. The chunk collapsed all of it into one message per method,
 * which cost more than a wording: nothing rejected making a node its own
 * DESCENDANT, so `$a->firstChild->appendChild($a)` spliced a CYCLE into the
 * tree and every later walk of it ran away.
 */
/*
 * The refusal is in TWO halves because php's empty-fragment answer sits
 * between them: a foreign empty fragment is Wrong Document, an empty fragment
 * on an ATTRIBUTE receiver is the "Document Fragment is empty" warning plus
 * false -- so the document screen runs before the fragment check and the
 * receiver-kind screen after it.
 */
static int DomLinkRefusalPre(xmlNodePtr pParent,xmlNodePtr pChild)
{
	/* A child with NO document is exempt: it is a constructed node, and the
	 * level-2 doors ADOPT it -- where the modern variadic family refuses it
	 * with this same code. */
	if( pParent->doc != pChild->doc && pChild->doc != 0 ){
		return DOM_ERR_WRONG_DOC;
	}
	/* A DOCUMENT is never a child, stated outright: the ancestor walk below
	 * only sees it from an ATTACHED receiver, and a detached one --
	 * `$d->createElement('x')->appendChild($d)` -- spliced the document node
	 * into its own orphan's child list, which teardown then freed twice. */
	if( pChild->type == XML_DOCUMENT_NODE || pChild->type == XML_HTML_DOCUMENT_NODE ){
		return DOM_ERR_HIERARCHY;
	}
	return 0;
}
/* The ancestor-cycle walk. Walking UP from the parent also catches
 * pChild == pParent, so `$frag->appendChild($frag)` is Hierarchy even
 * for an EMPTY fragment -- the cycle answers before the empty warning. */
static int DomLinkCycle(xmlNodePtr pParent,xmlNodePtr pChild)
{
	xmlNodePtr p;
	for( p = pParent ; p ; p = p->parent ){
		if( p == pChild ){
			return DOM_ERR_HIERARCHY;
		}
	}
	return 0;
}
/* An ATTRIBUTE takes text and entity references, nothing else -- not even a
 * fragment whose every child is text (though the EMPTY fragment's warning
 * answers before this). php's Hierarchy refusal. */
static int DomAttrRecvKind(xmlNodePtr pParent,xmlNodePtr pChild)
{
	if( pParent->type == XML_ATTRIBUTE_NODE
	 && pChild->type != XML_TEXT_NODE && pChild->type != XML_ENTITY_REF_NODE ){
		return DOM_ERR_HIERARCHY;
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
	int iErr,rc;
	if( pPar == 0 || pChd == 0 ){
		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);
	}
	if( DomMutatorScreen(pCtx,(xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode,&rc) ){
		return rc;
	}
	iErr = DomLinkRefusalPre((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);
	if( iErr == 0 ){
		iErr = DomLinkCycle((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);
	}
	/* The empty-fragment answer sits between the screens: a foreign empty
	 * fragment is Wrong Document, appending a fragment to ITSELF is the cycle's
	 * Hierarchy, and only an empty one on an attribute receiver reaches the
	 * warning plus false. */
	if( iErr == 0 && DomIsFragment((xmlNodePtr)pChd->pNode)
	 && ((xmlNodePtr)pChd->pNode)->children == 0 ){
		return DomFragEmpty(pCtx);
	}
	if( iErr == 0 ){
		iErr = DomAttrRecvKind((xmlNodePtr)pPar->pNode,(xmlNodePtr)pChd->pNode);
	}
	/* An attribute lands on an ELEMENT or nowhere -- and BEFORE the adoption,
	 * which is measurable: `$doc->appendChild(new DOMAttr('k'))` refuses with
	 * the argument still ownerless. */
	if( iErr == 0 && ((xmlNodePtr)pChd->pNode)->type == XML_ATTRIBUTE_NODE
	 && ((xmlNodePtr)pPar->pNode)->type != XML_ELEMENT_NODE ){
		iErr = DOM_ERR_HIERARCHY;
	}
	if( iErr ){
		return DomThrow(pCtx,iErr);
	}
	/* Every screen passed: a document-less argument is ADOPTED here, php's
	 * constructed-node door -- wrappers, orphan entry and (for an owned
	 * receiver) the document itself all move before the link. */
	DomAdoptIntoRecv(pCtx,apArg[0],pChd);
	if( ((xmlNodePtr)pChd->pNode)->type == XML_ATTRIBUTE_NODE ){
		return DomMutatorAttrAttach(pCtx,pPar,pChd,apArg[0]);
	}
	if( DomIsFragment((xmlNodePtr)pChd->pNode) ){
		xmlNodePtr pFirst;
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
	int iErr,rc;
	if( pPar == 0 || pNew == 0 ){
		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);
	}
	pParent = (xmlNodePtr)pPar->pNode;
	pChild = (xmlNodePtr)pNew->pNode;
	pAnchor = pRef ? (xmlNodePtr)pRef->pNode : 0;
	if( DomMutatorScreen(pCtx,pParent,pChild,&rc) ){
		return rc;
	}
	iErr = DomLinkRefusalPre(pParent,pChild);
	if( iErr == 0 ){
		iErr = DomLinkCycle(pParent,pChild);
	}
	/* Between the screens, and BEFORE the reference-membership refusal: an
	 * empty fragment answers its warning even against a reference node that
	 * is no child of the receiver. */
	if( iErr == 0 && DomIsFragment(pChild) && pChild->children == 0 ){
		return DomFragEmpty(pCtx);
	}
	if( iErr == 0 ){
		iErr = DomAttrRecvKind(pParent,pChild);
	}
	/* An attribute lands on an ELEMENT or nowhere, and php answers that
	 * Hierarchy refusal BEFORE the reference-membership one -- a fragment
	 * receiver with an attribute argument and a foreign reference is
	 * Hierarchy, not Not Found. */
	if( iErr == 0 && pChild->type == XML_ATTRIBUTE_NODE
	 && pParent->type != XML_ELEMENT_NODE ){
		iErr = DOM_ERR_HIERARCHY;
	}
	if( iErr == 0 && pAnchor && pAnchor->parent != pParent ){
		iErr = DOM_ERR_NOT_FOUND;
	}
	if( iErr ){
		return DomThrow(pCtx,iErr);
	}
	/* The constructed-node adoption, before ANY of the insertion tails --
	 * php's order, so even an argument the sibling Error is about to strand
	 * detached comes out of the call owned by this document. */
	DomAdoptIntoRecv(pCtx,apArg[0],pNew);
	if( pChild->type == XML_ATTRIBUTE_NODE ){
		/*
		 * The attribute half, with insertBefore's own tails. A NULL reference
		 * is the append spelling and attaches (the same-name displacement
		 * included). A reference that is itself an ATTRIBUTE of the receiver
		 * really does mean "before": the argument enters the property list at
		 * the reference's position. Any other reference runs the DISPLACEMENT
		 * and then fails the sibling link, php's own order, so
		 * `$el->insertBefore($attr, $child)` on an element carrying `k="old"`
		 * LOSES the old attribute, attaches nothing, and raises the plain
		 * Error the self-sibling splice raises.
		 */
		xmlAttrPtr pAttr = (xmlAttrPtr)pChild;
		xmlAttrPtr pOld;
		if( pAnchor == 0 ){
			return DomMutatorAttrAttach(pCtx,pPar,pNew,apArg[0]);
		}
		pOld = DomAttrByNs(pParent,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name);
		if( pOld && pOld != pAttr ){
			DomAttrGoing((xmlNodePtr)pOld);
			xmlUnlinkNode((xmlNodePtr)pOld);
			DomOrphanAdd(pPar->pShell,(xmlNodePtr)pOld);
		}
		if( pAnchor->type != XML_ATTRIBUTE_NODE
		 || pAnchor == (xmlNodePtr)pAttr || pAnchor == (xmlNodePtr)pOld ){
			/* The argument is UNLINKED before the sibling link fails -- php's
			 * own order, so `$r->insertBefore($cAttr, $child)` costs the other
			 * element its attribute and attaches nothing here. The link fails
			 * for a non-attribute reference, for the argument AS its own
			 * reference, and for a reference the displacement just took --
			 * php frees it and the sibling link then refuses. */
			DomAttrDetach(pNew->pShell,pAttr);
			DomOrphanAdd(pNew->pShell,(xmlNodePtr)pAttr);
			return PH7_VmThrowException(pCtx,"Error",
				"Cannot add newnode as the previous sibling of refnode");
		}
		DomAttrDetach(pNew->pShell,pAttr);
		DomAttrLinkBefore(pParent,pAttr,(xmlAttrPtr)pAnchor);
		DomNsAttrArrive(pParent,pAttr);
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
	}
	if( pAnchor && pAnchor->type == XML_ATTRIBUTE_NODE ){
		/*
		 * A non-attribute argument against an ATTRIBUTE reference: php hands
		 * the pair to xmlAddPrevSibling, which UNLINKS the argument and then
		 * splices it into the PROPERTY chain -- state no serializer or
		 * childNodes walk ever shows, whose exact shape is libxml's version's.
		 * The bytes agree when PHL simply DETACHES the argument and answers
		 * it; the one detail php answers differently afterwards is recorded in
		 * Recorded (the argument's `parentNode` reads the receiver there).
		 */
		DomDetach(pNew->pShell,pChild);
		DomOrphanAdd(pNew->pShell,pChild);
		ph7_result_value(pCtx,apArg[0]);
		return PH7_OK;
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
		/* An EMPTY fragment answered its warning above, before the reference
		 * screen -- php's order. */
		xmlNodePtr pFirst;
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
	/* php's membership test is `no children at all, or the parent pointer
	 * disagrees` -- which lets an ATTACHED ATTRIBUTE through (its libxml
	 * parent IS the element), so removeChild really does remove an attribute
	 * -- but only from an element that has at least one real child; on a
	 * childless one the same attribute is Not Found. (An entity reference's
	 * child fails the parent test: its parent is the DTD.) */
	if( ((xmlNodePtr)pPar->pNode)->children == 0
	 || pChild->parent != (xmlNodePtr)pPar->pNode ){
		return DomThrow(pCtx,DOM_ERR_NOT_FOUND);
	}
	/* ...and only THEN the read-only refusal, php's order: an entity
	 * reference's child is Not Found territory never reached, while
	 * `$ownerless->removeChild($its->child)` is the No Modification
	 * refusal. */
	if( DomNodeReadOnly((xmlNodePtr)pPar->pNode) ){
		return DomThrow(pCtx,DOM_ERR_NO_MOD);
	}
	DomAttrGoing(pChild);
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
	/*
	 * php's replaceChild, in its own order (dom_node_replace_child) -- and it
	 * disagrees with appendChild's twice. The document screen answers FIRST (a
	 * text receiver or a read-only receiver with a foreign argument is Wrong
	 * Document here, where appendChild answers false and No Modification).
	 * Then the two silent-false answers: the invalid-children receiver and the
	 * CHILDLESS one -- nothing to replace, and php says nothing at all, even
	 * for an attribute or a document argument. Then the shared insertion
	 * validity: read-only, the ancestor cycle, the attribute receiver's
	 * child-kind rule, an attribute argument's element-only rule, the
	 * document-as-child rule. Then a rule of replaceChild's OWN: old and new
	 * must be attributes TOGETHER or not at all -- so an attribute argument
	 * against a foreign ATTRIBUTE victim reads Not Found from the membership
	 * check (both are attributes, the pair passes) while an element argument
	 * against the same victim is Hierarchy. The victim's membership answers
	 * last.
	 */
	if( pChild->doc != pParent->doc && pChild->doc != 0 ){
		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);
	}
	if( !DomChildrenValid(pParent) || pParent->children == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( DomNodeReadOnly(pParent)
	 || (pChild->parent && DomNodeReadOnly(pChild->parent)) ){
		return DomThrow(pCtx,DOM_ERR_NO_MOD);
	}
	iErr = DomLinkCycle(pParent,pChild);
	if( iErr == 0 ){
		iErr = DomAttrRecvKind(pParent,pChild);
	}
	if( iErr == 0 && pChild->type == XML_ATTRIBUTE_NODE
	 && pParent->type != XML_ELEMENT_NODE ){
		iErr = DOM_ERR_HIERARCHY;
	}
	if( iErr == 0
	 && (pChild->type == XML_DOCUMENT_NODE || pChild->type == XML_HTML_DOCUMENT_NODE) ){
		iErr = DOM_ERR_HIERARCHY;
	}
	if( iErr == 0
	 && (pChild->type == XML_ATTRIBUTE_NODE) != (pVictim->type == XML_ATTRIBUTE_NODE) ){
		iErr = DOM_ERR_HIERARCHY;
	}
	if( iErr == 0 && pVictim->parent != pParent ){
		iErr = DOM_ERR_NOT_FOUND;
	}
	if( iErr ){
		return DomThrow(pCtx,iErr);
	}
	/* The constructed-node adoption, php's "document assignment" step. */
	DomAdoptIntoRecv(pCtx,apArg[0],pNew);
	if( DomIsFragment(pChild) ){
		/* No empty-fragment refusal here, unlike the other two: php REMOVES the
		 * old child and inserts nothing, and answers it as any replaceChild
		 * does. */
		DomFragMove(pNew->pShell,pParent,pChild,pVictim);
		DomAttrGoing(pVictim);
		xmlUnlinkNode(pVictim);
		DomOrphanAdd(pOld->pShell,pVictim);
		ph7_result_value(pCtx,apArg[1]);
		return PH7_OK;
	}
	if( pChild != pVictim && pChild->type == XML_ATTRIBUTE_NODE ){
		/* Both sides are attributes (the XOR above let them through): the swap
		 * happens in the PROPERTY list, at the victim's position, with NO
		 * same-name displacement -- php hands the pair to xmlReplaceNode
		 * as-is, so a duplicate name is the caller's to answer for. */
		DomAttrDetach(pNew->pShell,(xmlAttrPtr)pChild);
		DomAttrLinkBefore(pParent,(xmlAttrPtr)pChild,(xmlAttrPtr)pVictim);
		DomAttrGoing(pVictim);
		xmlUnlinkNode(pVictim);
		DomOrphanAdd(pOld->pShell,pVictim);
		DomNsAttrArrive(pParent,(xmlAttrPtr)pChild);
	}else if( pChild != pVictim ){
		DomDetach(pNew->pShell,pChild);
		DomLinkBefore(pParent,pChild,pVictim);
		DomAttrGoing(pVictim);
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
		SyBufferFormat(zBuf,nBuf,"%z",&pObj->pClass->sDisp);
		return zBuf;
	}
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) ){
		return "null";
	}
	return ph7_type_name(pVal);
}
/*
 * Which of the two node trees the receiver belongs to, as the base class every
 * argument must be under.  php declares these methods TWICE -- once on the 2004
 * classes taking `DOMNode|string`, once on the 8.4 ones taking `Dom\Node|string`
 * -- and the two never mix: a DOMElement handed to `Dom\Element::before()` is
 * refused by type, and a `Dom\Element` handed to `DOMElement::before()` is too.
 * The receiver's own tree is the whole answer, so the family is read off $this
 * and not off the document, which a fragment's receiver may not have.
 */
static ph7_class * DomModernNodeClass(ph7_context *pCtx)
{
	return PH7_VmExtractClass(pCtx->pVm,"Dom\\Node",sizeof("Dom\\Node")-1,FALSE,0);
}
static int DomRecvIsModern(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pModern = DomModernNodeClass(pCtx);
	return pThis != 0 && pModern != 0 && PH7_VmInstanceOf(pThis->pClass,pModern);
}
static ph7_class * DomArgNodeBase(ph7_context *pCtx,const char **pzName)
{
	if( DomRecvIsModern(pCtx) ){
		*pzName = "Dom\\Node";
		return DomModernNodeClass(pCtx);
	}
	*pzName = "DOMNode";
	return PH7_VmExtractClass(pCtx->pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);
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
	const char *zBase = "DOMNode";
	ph7_class *pNodeCls = DomArgNodeBase(pCtx,&zBase);
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
			"%z(): Argument #%d must be of type %s|string, %s given",
			&pCtx->pFunc->sName,i+1,zBase,DomGivenName(pVal,zBuf,sizeof(zBuf)));
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
	ph7_class_instance *pDstHolder = DomThisDoc(pCtx);
	int i;
	for( i = 0 ; i < nArg ; i++ ){
		phl_domnode *pNd;
		xmlNodePtr pNode;
		ph7_class_instance *pSrcHolder;
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
			/* No adoption in the modern family: a constructed node's NULL
			 * document is a mismatch like any other and refuses -- only the
			 * OWNERLESS-to-OWNERLESS pair (both NULL) passes. */
			*pRc = DomThrowVoid(pCtx,DOM_ERR_WRONG_DOC);
			return -1;
		}
		/* Same document, possibly different HOLDER: two constructed trees
		 * merging. The wrappers move to the receiver's cache so identity
		 * keeps answering. */
		pSrcHolder = DomObjArgDoc(apArg[i]);
		if( pSrcHolder != pDstHolder ){
			DomAdoptWrappers(pCtx->pVm,pSrcHolder,pDstHolder,pShell,pNode);
			pNd->pShell = pShell;
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
 * php 8.4's tree also enforces the WHATWG pre-insertion validity a DOCUMENT
 * parent carries, which the 2004 one never has: `$doc->append($el)` on a
 * document that already has a root is a Hierarchy refusal under `Dom\Document`
 * and a silent second root under DOMDocument, and the two trees are measured
 * side by side. Four prose sentences, none of them a level-2 code's name:
 *
 *   text (a CDATA section and a plain string argument included) -- never;
 *   an element -- not when the document already has one, and not anywhere a
 *     document type would end up FOLLOWING it;
 *   a document type -- not when the document already has one, and not anywhere
 *     an element already precedes.
 *
 * Every count is taken over the document's children AS THEY STAND, before
 * replaceChildren drops them and before replaceWith unlinks its receiver, so
 * replacing a document's only element with that same element is refused rather
 * than being the no-op the tree shape would allow.
 */
static const char * DomDocChildRefusal(xmlNodePtr pDoc,xmlNodePtr pRef,
	xmlNodePtr pSingle,SySet *pList)
{
	xmlNodePtr *apNode = (xmlNodePtr *)SySetBasePtr(pList);
	sxu32 nNode = pSingle ? 1 : SySetUsed(pList);
	int nElem = 0,nType = 0;
	xmlNodePtr p;
	sxu32 n;
	if( pDoc->type != XML_DOCUMENT_NODE && pDoc->type != XML_HTML_DOCUMENT_NODE ){
		return 0;
	}
	for( p = pDoc->children ; p ; p = p->next ){
		if( p->type == XML_ELEMENT_NODE ){
			nElem++;
		}else if( p->type == XML_DTD_NODE ){
			nType++;
		}
	}
	for( n = 0 ; n < nNode ; ++n ){
		xmlNodePtr pNode = pSingle ? pSingle : apNode[n];
		if( pNode->type == XML_TEXT_NODE || pNode->type == XML_CDATA_SECTION_NODE ){
			return "Cannot insert text as a child of a document";
		}
		if( pNode->type == XML_ELEMENT_NODE ){
			if( nElem > 0 ){
				return "Cannot have more than one element child in a document";
			}
			for( p = pRef ; p ; p = p->next ){
				if( p->type == XML_DTD_NODE ){
					return "Document types must be the first child in a document";
				}
			}
			nElem++;
		}else if( pNode->type == XML_DTD_NODE ){
			if( nType > 0 ){
				return "Cannot have more than one document type";
			}
			for( p = pDoc->children ; p && p != pRef ; p = p->next ){
				if( p->type == XML_ELEMENT_NODE ){
					return "Document types must be the first child in a document";
				}
			}
			nType++;
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
	if( DomRecvIsModern(pCtx) ){
		/* prepend lands before the first child; append and replaceChildren both
		 * land at the end, the latter over an emptied document -- and the count
		 * the rule asks for is still the one taken before the drop. */
		const char *zMsg = DomDocChildRefusal(pParent,
			iMode == DOM_PN_PREPEND ? pParent->children : 0,pSingle,&sList);
		if( zMsg ){
			SySetRelease(&sList);
			return DomThrowSentence(pCtx,DOM_ERR_HIERARCHY,zMsg);
		}
	}
	if( pOne ){
		/* The single-node shortcut skipped the conversion, so it re-homes its
		 * wrappers here: an ownerless argument merging into an ownerless
		 * receiver (the only mismatch the validity lets through). */
		DomAdoptIntoRecv(pCtx,apArg[0],pOne);
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
/*
 * Is this xmlNodePtr one of the ARGUMENT nodes?  The viable-sibling walks ask
 * it about tree nodes, so only object arguments can match -- php's
 * dom_is_node_in_list does the same walk over the zval list.
 */
static int DomArgListHasNode(int nArg,ph7_value **apArg,xmlNodePtr pNode)
{
	int i;
	for( i = 0 ; i < nArg ; i++ ){
		phl_domnode *pNd = DomObjArg(apArg[i]);
		if( pNd && (xmlNodePtr)pNd->pNode == pNode ){
			return 1;
		}
	}
	return 0;
}
/*
 * DOMChildNode::before / after / replaceWith -- php's WHATWG transcription
 * (dom_parent_node_before/after, dom_child_replace_with), sharing the parent
 * side's conversion machinery.  The order is the measurable part: the TYPE
 * screen runs first even for a node with no parent; a parentless receiver
 * then returns in SILENCE -- around an argument that could never be inserted
 * -- and only then does conversion run, with the same mid-list detachment the
 * parent side has.  The reference sibling ("viable") is the nearest sibling
 * NOT in the argument set, read before anything moves; the insertion point is
 * derived from it after conversion, so a set member that was also the first
 * child no longer counts.
 */
#define DOM_CN_BEFORE   0
#define DOM_CN_AFTER    1
#define DOM_CN_REPLACE  2
static int DomChildNodeOp(ph7_context *pCtx,int nArg,ph7_value **apArg,int iMode)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	phl_domnode *pOne = 0;
	xmlNodePtr pThis,pParent,pViable,pRef,pSingle = 0;
	SySet sList;
	int iErr,rc = PH7_OK;
	if( DomNodesScreen(pCtx,nArg,apArg) || pNd == 0 ){
		return PH7_OK;
	}
	pThis = (xmlNodePtr)pNd->pNode;
	pParent = pThis->parent;
	if( pParent == 0 ){
		return PH7_OK;
	}
	if( iMode == DOM_CN_REPLACE
	 && (DomNodeReadOnly(pThis) || DomNodeReadOnly(pParent)) ){
		/* replaceWith carries the read-only refusal (it REMOVES the receiver)
		 * where before/after do not: a text child of a constructed ownerless
		 * element takes before() and refuses replaceWith(). The parentless
		 * silence above still answers first -- a constructed ROOT is a silent
		 * no-op, not this refusal. */
		return DomThrowVoid(pCtx,DOM_ERR_NO_MOD);
	}
	if( iMode == DOM_CN_BEFORE ){
		pViable = pThis->prev;
		while( pViable && DomArgListHasNode(nArg,apArg,pViable) ){
			pViable = pViable->prev;
		}
	}else{
		pViable = pThis->next;
		while( pViable && DomArgListHasNode(nArg,apArg,pViable) ){
			pViable = pViable->next;
		}
	}
	SySetInit(&sList,&pCtx->pVm->sAllocator,sizeof(xmlNodePtr));
	if( nArg == 1 && (apArg[0]->iFlags & MEMOBJ_OBJ) != 0 ){
		pOne = DomObjArg(apArg[0]);
		if( pOne == 0 ){
			return PH7_OK;
		}
		pSingle = (xmlNodePtr)pOne->pNode;
	}else if( DomNodesConvert(pCtx,pNd->pShell,pParent,nArg,apArg,&sList,&rc) ){
		SySetRelease(&sList);
		return rc;
	}
	iErr = DomInsertValidity(pParent,pSingle,&sList);
	if( iErr ){
		SySetRelease(&sList);
		return DomThrowVoid(pCtx,iErr);
	}
	if( DomRecvIsModern(pCtx) ){
		/* before() lands where the viable previous sibling ends; after() and
		 * replaceWith() both land at the viable next one -- replaceWith over a
		 * receiver still linked, which is what refuses a root replaced by an
		 * element. */
		const char *zMsg = DomDocChildRefusal(pParent,
			iMode == DOM_CN_BEFORE ? (pViable ? pViable->next : pParent->children)
			                       : pViable,pSingle,&sList);
		if( zMsg ){
			SySetRelease(&sList);
			return DomThrowSentence(pCtx,DOM_ERR_HIERARCHY,zMsg);
		}
	}
	if( pOne ){
		/* The single-node shortcut skipped the conversion's wrapper re-home:
		 * an ownerless argument merging into an ownerless receiver's tree, the
		 * only mismatch the validity lets through. */
		DomAdoptIntoRecv(pCtx,apArg[0],pOne);
	}
	if( iMode == DOM_CN_BEFORE ){
		/* Step 5: the viable previous sibling's NEXT -- the parent's first
		 * child when there is none -- both read after conversion. */
		pRef = pViable ? pViable->next : pParent->children;
	}else{
		pRef = pViable;
	}
	if( iMode == DOM_CN_REPLACE ){
		/* php unlinks the receiver unless conversion already took it. */
		if( pThis != pSingle && !DomListHas(&sList,pThis) ){
			xmlUnlinkNode(pThis);
			DomOrphanAdd(pNd->pShell,pThis);
		}
	}
	if( pSingle ){
		if( DomIsFragment(pSingle) ){
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
		DomNodesPlace(pNd->pShell,pParent,pRef,&sList);
	}
	SySetRelease(&sList);
	return PH7_OK;
}
DOM_METHOD(vm_builtin_Dom_before)
{
	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_BEFORE);
}
DOM_METHOD(vm_builtin_Dom_after)
{
	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_AFTER);
}
DOM_METHOD(vm_builtin_Dom_replaceWith)
{
	return DomChildNodeOp(pCtx,nArg,apArg,DOM_CN_REPLACE);
}
/*
 * DOMChildNode::remove(): void -- and php's asymmetry: where before() on a
 * parentless node is a silent no-op, remove() is the Not Found refusal, in
 * whichever mode the document is in.
 */
DOM_METHOD(vm_builtin_Dom_removeSelf)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pNd == 0 ){
		return PH7_OK;
	}
	pNode = (xmlNodePtr)pNd->pNode;
	/* The read-only refusal answers BEFORE the parentless one: a constructed
	 * ownerless node -- necessarily parentless -- is No Modification Allowed
	 * here, where an owned parentless node is Not Found. */
	if( DomNodeReadOnly(pNode) || (pNode->parent && DomNodeReadOnly(pNode->parent)) ){
		return DomThrowVoid(pCtx,DOM_ERR_NO_MOD);
	}
	if( pNode->parent == 0 ){
		return DomThrowVoid(pCtx,DOM_ERR_NOT_FOUND);
	}
	DomAttrGoing(pNode);
	xmlUnlinkNode(pNode);
	DomOrphanAdd(pNd->pShell,pNode);
	return PH7_OK;
}

/* DOMNode::hasChildNodes(): bool / hasAttributes(): bool / getLineNo(): int */
DOM_METHOD(vm_builtin_DOMNode_hasChildNodes)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx,pNd && DomNodeChildFirst((xmlNodePtr)pNd->pNode) != 0);
	return PH7_OK;
}
/* Defined with the namespace-parking machinery below: which declarations the
 * engine MINTED to bind a name, rather than the document spelling one. */
static void DomNsMarkMinted(xmlNsPtr pNs);
static int DomNsIsSpelt(xmlNsPtr pNs);
/* ...and, with them, where on the attribute map each spelt one sits: a copy
 * carries both over from the node it was made from. */
static void DomNsCopyMarks(xmlNodePtr pSrc,xmlNodePtr pDst);
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
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	/* A namespace DECLARATION answers this question too, exactly as it already
	 * answers getAttributeNames(): `<r xmlns:x="urn:x"/>` has attributes in php
	 * and had none here. Only an element carries one. */
	ph7_result_bool(pCtx,pNode != 0
		&& (DomAttrCount(pNode) > 0
		 || (pNode->type == XML_ELEMENT_NODE && pNode->nsDef != 0)));
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
 * One node copied the way php copies it.
 *
 * libxml's generic copier has no case for a DTD node and answers NULL there, so
 * `$doc->doctype->cloneNode()` was `false` -- php reaches for xmlCopyDtd
 * instead, which carries the whole internal subset (its declarations, entities
 * and notations) across. The copy keeps the SOURCE's document in its `doc`
 * slot without being linked into it, which is what makes php's cloned doctype
 * still answer an `internalSubset` while its `parentNode` is null.
 */
static xmlNodePtr DomCopyNode(xmlNodePtr pNode,xmlDocPtr pDoc,int iExtended)
{
	xmlNodePtr pCopy;
	if( pNode->type == XML_DTD_NODE || pNode->type == XML_DOCUMENT_TYPE_NODE ){
		pCopy = (xmlNodePtr)xmlCopyDtd((xmlDtdPtr)pNode);
		if( pCopy ){
			pCopy->doc = pNode->doc;
		}
		return pCopy;
	}
	pCopy = xmlDocCopyNode(pNode,pDoc,iExtended);
	DomNsCopyMarks(pNode,pCopy);
	return pCopy;
}

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
	DomNsCopyMarks((xmlNodePtr)pNd->pNode,(xmlNodePtr)pCopy);
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
		ph7_value *pFrom,*pTo;
		/* The seven directives travel as the one word they are stored in. */
		PH7_NativeSetAttrInt(pVm,pObj,DOM_DFLAGS,PH7_NativeAttrInt(pThis,DOM_DFLAGS));
		/* ...and the registerNodeClass table, which php's copy answers too --
		 * shared copy-on-write, which the map's own writer separates. */
		pFrom = PH7_NativeAttr(pThis,DOM_NCLS);
		pTo = PH7_NativeAttr(pObj,DOM_NCLS);
		if( pFrom && pTo && (pFrom->iFlags & MEMOBJ_HASHMAP) ){
			PH7_MemObjStore(pFrom,pTo);
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
	pCopy = DomCopyNode(pNode,pNode->doc,bDeep ? 1 : 2);
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
/* Enter one wrapper into a holder's identity cache, keyed by the node pointer.
 * BORROWED, like every entry: the object's own release takes it back out. */
static void DomCacheStore(ph7_vm *pVm,ph7_class_instance *pDoc,xmlNodePtr pNode,
	ph7_class_instance *pObj)
{
	ph7_hashmap *pCache = DomCache(&(*pVm),pDoc);
	ph7_value sKey,sVal;
	if( pCache == 0 ){
		return;
	}
	PH7_MemObjInitFromInt(&(*pVm),&sKey,(sxi64)(sxuptr)pNode);
	PH7_MemObjInitFromInt(&(*pVm),&sVal,(sxi64)(sxuptr)pObj);
	PH7_HashmapInsert(pCache,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	DomNodeMarkHeld(pNode,pObj);
}
/* Empty a slot the instance copied from its clone source: the null value. */
static void DomSetSlotNull(ph7_vm *pVm,ph7_class_instance *pObj,const char *zName,sxu32 nName)
{
	ph7_value sNull;
	PH7_MemObjInit(&(*pVm),&sNull);
	PH7_NativeSetProp(&(*pVm),pObj,zName,nName,&sNull);
}
/*
 * `clone $node` / `clone $doc` -- ph7_class::xClone for the DOM classes.
 *
 * php's clone_obj handler copies the NODE, so the clone is a second SUBTREE and
 * not a second object over the same one.  The slot-by-slot copy that runs
 * before this hook duplicated $__res, and stopping there is the XMLWriter clone
 * bug one family later: a write through either object shows through both.
 *
 * php's rules, measured: the copy is always DEEP (`clone $el` carries the whole
 * subtree where cloneNode() defaults shallow), always DETACHED, and stays in
 * the SAME document -- `$c->ownerDocument === $d` -- while a DOCUMENT is copied
 * whole into a second document, directives, declaration and URI included, so
 * mutating the copy's tree leaves the original's bytes alone.  A user subclass
 * clones through the inherited hook and keeps its class and its own properties,
 * php's handler inheritance (the ENGINE's chain walk serves that).
 */
static void DomInstanceClone(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)
{
	phl_domnode *pNd = DomResOf(pSrc);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	ph7_class_instance *pDoc = PH7_NativeAttrObj(pClone,DOM_DOC);
	xmlNodePtr pCopy;
	phl_domnode *pRes;
	if( pNode == 0 ){
		return;   /* no node behind the source: the copy has none either */
	}
	pCopy = DomCopyNode(pNode,pNode->doc,1);
	pRes = pCopy ? DomNewRes(&(*pVm),pNd->pShell,pCopy) : 0;
	if( pRes == 0 ){
		/* Never leave the slot-copied handle in place: two objects over one
		 * node is the exact aliasing this hook exists to prevent. */
		if( pCopy ){
			xmlFreeNode(pCopy);
		}
		DomSetSlotNull(&(*pVm),pClone,DOM_RES,sizeof(DOM_RES)-1);
		return;
	}
	/* The same namespace borrow cloneNode() does: an attribute copied with no
	 * element to resolve against comes back in NO namespace. */
	if( pCopy->type == XML_ATTRIBUTE_NODE && pCopy->ns == 0 ){
		pCopy->ns = pNode->ns;
	}
	DomOrphanAdd(pNd->pShell,pCopy);
	DomSetRes(&(*pVm),pClone,pRes);
	/* The clone IS the copy's wrapper: enter it into the identity cache so
	 * `$c->firstChild->parentNode === $c` holds. ($__doc rode the slot copy.) */
	DomCacheStore(&(*pVm),pDoc,pCopy,pClone);
}
static void DomInstanceCloneDoc(ph7_vm *pVm,ph7_class_instance *pClone,ph7_class_instance *pSrc)
{
	phl_domnode *pNd = DomResOf(pSrc);
	xmlDocPtr pCopy;
	phl_xmldoc *pShell;
	phl_domnode *pRes;
	if( pNd == 0 || pNd->pNode == 0 ){
		return;
	}
	pCopy = xmlCopyDoc((xmlDocPtr)pNd->pNode,1);
	pShell = pCopy ? PH7_LibxmlNewDoc(&(*pVm),pCopy) : 0;
	pRes = pShell ? DomNewRes(&(*pVm),pShell,pCopy) : 0;
	if( pRes == 0 ){
		if( pCopy && pShell == 0 ){
			xmlFreeDoc(pCopy);   /* not registered: nothing else will free it */
		}
		DomSetSlotNull(&(*pVm),pClone,DOM_RES,sizeof(DOM_RES)-1);
		return;
	}
	DomSetRes(&(*pVm),pClone,pRes);
	/* Its own document, its own identity cache: the slot copy pointed both at
	 * the SOURCE's, so the copy's documentElement would have answered the
	 * original document as its owner. (The directive slots the copy carried
	 * across are php's answer and stay.) */
	PH7_NativeSetAttrObj(&(*pVm),pClone,DOM_DOC,pClone);
	DomSetSlotNull(&(*pVm),pClone,DOM_NODES,sizeof(DOM_NODES)-1);
}

/* ===== The node CONSTRUCTORS: php's ownerless nodes ===== */

/*
 * php gives a constructed node NO document at all -- `(new DOMText('t'))->
 * ownerDocument` is null and the libxml node's doc is NULL -- and adopts it on
 * the first insertion.  Until then the node has to be OWNED by something that
 * frees it: the limbo shell, one per VM, a phl_xmldoc with no xmlDoc whose
 * orphan set carries every constructed-and-never-adopted node to teardown.
 */
static phl_xmldoc * DomLimboShell(ph7_vm *pVm)
{
	if( pVm->pXmlLimbo == 0 ){
		pVm->pXmlLimbo = PH7_LibxmlNewDoc(&(*pVm),0);
	}
	return (phl_xmldoc *)pVm->pXmlLimbo;
}
/*
 * The shared constructor tail: park the fresh node on the limbo shell, wire
 * the instance's two slots, and make the instance its OWN holder -- $__doc
 * points at itself and the identity cache lives on it, exactly the document's
 * own arrangement, so `$e->firstChild->parentNode === $e` holds for a tree
 * that belongs to no document.  (ownerDocument still answers null: the getter
 * reads the NODE's document, not the slot.)  Takes ownership of pNode either
 * way; a re-run constructor -- `$t->__construct('b')`, which php allows --
 * simply re-points the slots and leaves the old node parked.
 */
static int DomCtorInstall(ph7_context *pCtx,xmlNodePtr pNode)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_xmldoc *pShell;
	phl_domnode *pRes;
	if( pNode == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pShell = pThis ? DomLimboShell(pVm) : 0;
	pRes = pShell ? DomNewRes(pVm,pShell,pNode) : 0;
	if( pRes == 0 ){
		xmlFreeNode(pNode);
		return pThis ? PH7_ContextMemoryError(pCtx) : PH7_OK;
	}
	DomOrphanAdd(pShell,pNode);
	DomSetRes(pVm,pThis,pRes);
	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);
	DomCacheStore(pVm,pThis,pNode,pThis);
	return PH7_OK;
}
/* The content of the character-data three: php passes NULL for an OMITTED
 * argument and the string -- even the empty one -- for a given one, which is
 * why `new DOMText()` has a NULL nodeValue where `new DOMText('')` reads "". */
DOM_METHOD(vm_builtin_DOMText_construct)
{
	int nData = 0;
	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : 0;
	xmlNodePtr pNode = zData
		? xmlNewDocTextLen(0,(const xmlChar *)zData,nData)
		: xmlNewDocText(0,0);
	return DomCtorInstall(pCtx,pNode);
}
DOM_METHOD(vm_builtin_DOMComment_construct)
{
	int nData = 0;
	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : 0;
	xmlNodePtr pNode;
	if( zData ){
		/* libxml has no length-taking comment constructor and
		 * xmlNewDocComment measures with strlen, so a NUL-carrying PHP string
		 * goes through a bounded copy. */
		xmlChar *zCopy = xmlStrndup((const xmlChar *)zData,nData);
		pNode = zCopy ? xmlNewDocComment(0,zCopy) : 0;
		if( zCopy ){
			xmlFree(zCopy);
		}
	}else{
		pNode = xmlNewDocComment(0,0);
	}
	return DomCtorInstall(pCtx,pNode);
}
DOM_METHOD(vm_builtin_DOMCdataSection_construct)
{
	int nData = 0;
	const char *zData = nArg > 0 ? ph7_value_to_string(apArg[0],&nData) : "";
	return DomCtorInstall(pCtx,
		xmlNewCDataBlock(0,(const xmlChar *)zData,nData));
}
/*
 * DOMElement::__construct(string $qualifiedName, ?string $value = null,
 *                         string $namespace = '')
 *
 * The constructor's name grammar is its OWN, not createElementNS's, each cell
 * measured: the whole name must be an XML Name first (so `1:a` is Invalid
 * Character where createElementNS answers Namespace), a prefix without a
 * namespace is the Namespace refusal, and WITH one the name must be a QName
 * whose prefix is neither `xml` nor `xmlns` -- php refuses `xml:a` here even
 * against the xml namespace's own URI, where createElementNS allows it. A
 * plain `xmlns` passes as an ordinary name and binds the DEFAULT namespace.
 *
 * The $value rides libxml's entity parser, createElement's own quirk: `&amp;`
 * becomes `&`, and an unterminated reference warns (under this constructor's
 * name) and drops the whole value. An attribute's value -- the constructor
 * below -- is LITERAL instead: `&amp;` stays five characters.
 */
DOM_METHOD(vm_builtin_DOMElement_construct)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int nVal = 0;
	const char *zVal = (nArg > 1 && !ph7_value_is_null(apArg[1]))
		? ph7_value_to_string(apArg[1],&nVal) : 0;
	const char *zUri = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";
	int bHasUri = zUri[0] != 0;
	xmlChar *zPrefix = 0;
	xmlChar *zLocal;
	xmlNodePtr pNode;
	sxu32 nMark;
	if( zName[0] == 0 || xmlValidateName((const xmlChar *)zName,0) != 0 ){
		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);
	}
	/* The split is BY HAND, at the first colon, with a leading colon meaning
	 * no prefix at all: libxml's xmlSplitQName2 changed its answer for a name
	 * that ENDS in the colon between 2.9 and 2.13 (the Windows gate caught
	 * `new DOMElement('a:')` constructing there), and the grammar must answer
	 * the same on every platform. */
	{
		const xmlChar *zColon = xmlStrchr((const xmlChar *)zName,':');
		if( zColon && zColon != (const xmlChar *)zName ){
			zPrefix = xmlStrndup((const xmlChar *)zName,
				(int)(zColon - (const xmlChar *)zName));
			zLocal = xmlStrdup(zColon + 1);
		}else{
			zLocal = 0;
		}
	}
	if( !bHasUri ){
		if( zPrefix ){
			/* A prefix names a namespace, and none came. */
			xmlFree(zPrefix);
			xmlFree(zLocal);
			return DomThrow(pCtx,DOM_ERR_NAMESPACE);
		}
		if( zLocal ){
			xmlFree(zLocal);
		}
		zLocal = 0;   /* the whole name, `:a` included */
	}else{
		if( xmlValidateQName((const xmlChar *)zName,0) != 0
		 || (zPrefix && (xmlStrEqual(zPrefix,(const xmlChar *)"xml")
		              || xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){
			if( zPrefix ){
				xmlFree(zPrefix);
			}
			if( zLocal ){
				xmlFree(zLocal);
			}
			return DomThrow(pCtx,DOM_ERR_NAMESPACE);
		}
	}
	pNode = xmlNewNode(0,zLocal ? zLocal : (const xmlChar *)zName);
	if( zLocal ){
		xmlFree(zLocal);
	}
	if( pNode == 0 ){
		if( zPrefix ){
			xmlFree(zPrefix);
		}
		return PH7_ContextMemoryError(pCtx);
	}
	if( bHasUri ){
		/* On the node's OWN nsDef, so the declaration serializes here once an
		 * insertion adopts the element and lookupNamespaceURI answers it
		 * meanwhile; the reconcile strips it wherever an ancestor already
		 * declares the binding. */
		xmlNsPtr pNs = xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);
		if( pNs ){
			DomNsMarkMinted(pNs);
			xmlSetNs(pNode,pNs);
		}
	}
	if( zPrefix ){
		xmlFree(zPrefix);
	}
	if( zVal && nVal > 0 ){
		/* The EMPTY value is skipped whole -- php's `new DOMElement('a','')`
		 * has no text child at all, where libxml's setter would leave one. */
		nMark = PH7_LibxmlCaptureBegin(pVm);
		xmlNodeSetContentLen(pNode,(const xmlChar *)zVal,nVal);
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMElement::__construct");
	}
	return DomCtorInstall(pCtx,pNode);
}
/*
 * The last three: a FRAGMENT takes nothing at all; a PROCESSING INSTRUCTION
 * validates its target as a plain XML Name (`xml`, `XML` and `p:a` all pass --
 * php never asks whether the target is reserved) and stores its data
 * literally, NULL when omitted like the character-data three; an ENTITY
 * REFERENCE validates its name and takes libxml's answer for the content: a
 * PREDEFINED name (`amp`) arrives with the shared entity declaration as its
 * child -- a STATIC libxml global, wrapped but never owned, which is why the
 * constructor parks only the reference node itself on the limbo shell.
 */
DOM_METHOD(vm_builtin_DOMDocumentFragment_construct)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return DomCtorInstall(pCtx,xmlNewDocFragment(0));
}
DOM_METHOD(vm_builtin_DOMProcessingInstruction_construct)
{
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	const char *zData = nArg > 1 ? ph7_value_to_string(apArg[1],0) : 0;
	if( zName[0] == 0 || xmlValidateName((const xmlChar *)zName,0) != 0 ){
		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);
	}
	return DomCtorInstall(pCtx,
		xmlNewPI((const xmlChar *)zName,(const xmlChar *)zData));
}
DOM_METHOD(vm_builtin_DOMEntityReference_construct)
{
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	if( zName[0] == 0 || xmlValidateName((const xmlChar *)zName,0) != 0 ){
		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);
	}
	return DomCtorInstall(pCtx,xmlNewReference(0,(const xmlChar *)zName));
}
/*
 * DOMAttr::__construct(string $name, string $value = '')
 *
 * The name is a plain XML Name -- NO QName split at all, so `p:a` and even
 * `xmlns:x` pass whole and carry no namespace (`prefix` reads "" and
 * `localName` the full spelling). The value is LITERAL: php builds the text
 * child directly rather than through the entity parser, which is what keeps
 * `&amp;` five characters where the element constructor's value collapses it.
 */
DOM_METHOD(vm_builtin_DOMAttr_construct)
{
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	const char *zVal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	if( zName[0] == 0 || xmlValidateName((const xmlChar *)zName,0) != 0 ){
		return DomThrow(pCtx,DOM_ERR_INVALID_CHAR);
	}
	return DomCtorInstall(pCtx,
		(xmlNodePtr)xmlNewProp(0,(const xmlChar *)zName,(const xmlChar *)zVal));
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
	int bModern = DomThisModern(pCtx);
	xmlNsPtr pNs;
	if( zPrefix != 0 && zPrefix[0] == 0 && bModern ){
		/* The same normalization on the PREFIX: php 8.4's tree reads `''` as
		 * the null prefix and answers the DEFAULT namespace, where the 2004
		 * tree looks for a prefix spelled with no characters and finds none. */
		zPrefix = 0;
	}
	pNs = pNode ? xmlSearchNs(pNode->doc,pNode,(const xmlChar *)zPrefix) : 0;
	if( pNs && pNs->href && (pNs->href[0] || !bModern) ){
		/* `xmlns=""` is a declaration whose URI is empty, and it exists to put
		 * its subtree back in NO namespace.  php 8.4's tree reads it that way
		 * and answers null; the 2004 tree hands back the empty URI it found. */
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
	xmlNsPtr pNs;
	if( zUri[0] == 0 ){
		/* "Is NOTHING the default here?" -- a question only php 8.4's tree
		 * takes, and it takes it spelled either way: `''` and null are the
		 * same argument there.  It is the one this asks of the scope rather
		 * than of a URI, and it is true wherever `lookupNamespaceURI(null)`
		 * answers null: an undeclared document, and any subtree an
		 * `xmlns=""` has put back in no namespace.  The 2004 tree reads the
		 * empty string as a URI, which nothing carries, and answers false. */
		if( !DomThisModern(pCtx) ){
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pNs = pNode ? xmlSearchNs(pNode->doc,pNode,0) : 0;
		ph7_result_bool(pCtx,pNs == 0 || pNs->href == 0 || pNs->href[0] == 0);
		return PH7_OK;
	}
	pNs = pNode ? xmlSearchNs(pNode->doc,pNode,0) : 0;
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
 *   DOM_QN_MATTR php 8.4's `Dom\\Document::createAttributeNS` -- DOM_QN_ATTR with
 *                the grammar split out the same way the namespaced element
 *                factory splits it: `createAttributeNS('urn:u', '1:x')` refuses
 *                with 5 where the 2004 factory answers 14. Every rule ABOUT a
 *                namespace -- a prefix with none, the xmlns pairing, the `xml`
 *                prefix's own URI -- is still the Namespace Error, because
 *                those are rules and not spellings.
 *   DOM_QN_MELEM php 8.4's `Dom\\Document::createElementNS` -- DOM_QN_ELEM with
 *                the same split: every grammar failure is the Invalid Character
 *                Error, so `createElementNS('urn:u', '1:x')` refuses with 5
 *                where the 2004 factory answers 14. A prefix with no namespace
 *                is still the Namespace Error, because that is a rule and not
 *                a spelling.
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
#define DOM_QN_SET   0
#define DOM_QN_ATTR  1
#define DOM_QN_ELEM  2
#define DOM_QN_MELEM 3
#define DOM_QN_MATTR 4
static int DomQNameParse(const char *zQname,const char *zUri,int iMode,dom_qname *pOut)
{
	int bHasUri = zUri != 0 && zUri[0] != 0;
	int bAnyElem = iMode == DOM_QN_ELEM || iMode == DOM_QN_MELEM;
	int bAnyAttr = iMode == DOM_QN_ATTR || iMode == DOM_QN_MATTR;
	/* Which code a GRAMMAR failure takes -- the only thing each 2004 mode and
	 * its namespaced twin disagree about. */
	int iBadName = (iMode == DOM_QN_MELEM || iMode == DOM_QN_MATTR)
		? DOM_ERR_INVALID_CHAR : DOM_ERR_NAMESPACE;
	int bXmlnsName;
	pOut->zPrefix = pOut->zLocal = 0;
	if( zQname == 0 || zQname[0] == 0 ){
		return iBadName;
	}
	if( bAnyAttr || (bAnyElem && bHasUri) ){
		/* A created name that names a namespace has to be a QName. */
		if( xmlValidateQName((const xmlChar *)zQname,0) != 0 ){
			return iBadName;
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
	if( iMode == DOM_QN_SET || (bAnyElem && !bHasUri) ){
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
				return iBadName;
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
	if( bAnyAttr && pOut->zPrefix
	 && xmlStrEqual(pOut->zPrefix,(const xmlChar *)"xml")
	 && !DomUriIs(zUri,DOM_XML_NS_URI) ){
		DomQNameRelease(pOut);
		return DOM_ERR_NAMESPACE;
	}
	if( bAnyAttr ){
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
	int bScope,int iFirst)
{
	xmlNsPtr pNs = 0;
	int i;
	for( i = iFirst ; i < 1000 + iFirst ; i++ ){
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
			DomNsMarkMinted(pNs);
			return pNs;
		}
	}
	return 0;
}
static xmlNsPtr DomNsGenerate(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zBase)
{
	return DomNsGenerateEx(pAnchor,zUri,zBase,0,0);
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
static xmlNsPtr DomNsResolve(xmlNodePtr pAnchor,const char *zUri,const xmlChar *zPrefix,
	int bNeedPrefix,int bModern)
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
			DomNsMarkMinted(pNs);
			return pNs;
		}
	}
	/* The invented prefix is spelt differently by the two trees: the 2004 one
	 * writes `default`, `default1`, ... and php 8.4's writes `ns1`, `ns2`, ...
	 * -- numbered from ONE there, and only against the declarations this very
	 * element makes, so an `ns1` an ANCESTOR binds is shadowed rather than
	 * stepped over. */
	return bModern
		? DomNsGenerateEx(pAnchor,zUri,(const xmlChar *)"ns",0,1)
		: DomNsGenerate(pAnchor,zUri,0);
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
	pNs = xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);
	DomNsMarkMinted(pNs);
	return pNs;
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
static int DomNsIsParked(xmlDocPtr pDoc,xmlNsPtr pNs);
static int DomNsIsFactory(xmlNsPtr pNs);
static void DomNsRespell(xmlNodePtr pNode,int bAttr,int bModern,int bWrite)
{
	xmlNsPtr pNs = pNode->ns,pAlt;
	/* php declares what it needs on the node that NEEDS it -- the element
	 * itself, or the element an attribute belongs to. */
	xmlNodePtr pSite = bAttr ? pNode->parent : pNode;
	if( pNs == 0 || pNs->href == 0 || pSite == 0 ){
		return;
	}
	if( bModern && pNs->prefix == 0 && DomNsIsParked(pNode->doc,pNs) ){
		/* php's namespaced tree re-declares no DEFAULT: a binding it left
		 * declared nowhere -- one the factory minted (DomNsForCreateModern), or
		 * one a write removed -- has no spelling to lose, and its serializer
		 * writes nothing for it.  The 2004 tree gives every removed binding
		 * back, which is the whole difference between the two here.  A PREFIXED
		 * one is respelled on both: there the serializer must name the prefix,
		 * so php's own output carries the declaration too. */
		return;
	}
	pAlt = xmlSearchNs(pNode->doc,pNode,pNs->prefix);
	if( pAlt == pNs ){
		return;   /* the prefix still names this very binding */
	}
	/* The two doors settle a broken spelling differently, and php's answers
	 * only line up when they are asked apart.
	 *
	 * A WRITE that rebinds a prefix takes any binding of the URI still in
	 * scope, whatever its prefix -- an attribute a PREFIXED one (DomNsReuse),
	 * an element the default one as happily.
	 *
	 * A REMOVAL re-declares the node's OWN prefix instead, and reuses only a
	 * binding that is that same prefix bound to that same URI -- which is how
	 * a declaration an ancestor already makes absorbs the node, and how one
	 * that merely binds the URI under ANOTHER prefix does not. Reusing by URI
	 * there re-prefixed nodes php leaves spelled as they were written:
	 * `<r xmlns="urn:d" xmlns:q="urn:d">` losing its default read back as
	 * `<q:r>`, and its whole subtree with it. */
	if( bWrite ){
		pAlt = bAttr ? DomNsReuse(pNode,(const char *)pNs->href)
		             : xmlSearchNsByHref(pNode->doc,pNode,pNs->href);
	}else if( pAlt != 0 && !xmlStrEqual(pAlt->href,pNs->href) ){
		pAlt = 0;
	}
	if( pAlt == 0 ){
		/* A declaration that was REMOVED leaves that prefix free again, and php
		 * re-declares it unchanged on the node that still needs it. */
		pAlt = xmlNewNs(pSite,pNs->href,pNs->prefix);
		if( pAlt && !DomNsIsSpelt(pNs) ){
			DomNsMarkMinted(pAlt);   /* a re-declaration of an invented binding */
		}
	}
	if( pAlt == 0 ){
		pAlt = DomNsGenerate(pSite,(const char *)pNs->href,pNs->prefix);
	}
	if( pAlt ){
		pNode->ns = pAlt;
	}
}
/* How many declarations this element makes. Only an element makes one, and
 * php 8.4's attribute map counts them ahead of the attributes. */
static int DomNsDefCount(xmlNodePtr pElem)
{
	xmlNsPtr pNs;
	int n = 0;
	if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
		return 0;
	}
	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){
		n++;
	}
	return n;
}
static void DomNsReconcile(xmlNodePtr pElem,int bModern,int bWrite)
{
	xmlNodePtr pCur = pElem;
	while( pCur ){
		xmlAttrPtr pAttr;
		if( pCur->type == XML_ELEMENT_NODE ){
			DomNsRespell(pCur,0,bModern,bWrite);
			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){
				if( pAttr->type == XML_ATTRIBUTE_NODE ){
					DomNsRespell((xmlNodePtr)pAttr,1,bModern,bWrite);
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
/* Is pNs one of the document's PARKED bindings -- declared on no element, and
 * so resolvable from nowhere? The list is short: libxml's own `xml`
 * declaration, whatever a write removed, and the unprefixed bindings the
 * namespaced factory hands out. */
static int DomNsIsParked(xmlDocPtr pDoc,xmlNsPtr pNs)
{
	xmlNsPtr pCur;
	if( pDoc == 0 || pNs == 0 ){
		return 0;
	}
	for( pCur = pDoc->oldNs ; pCur ; pCur = pCur->next ){
		if( pCur == pNs ){
			return 1;
		}
	}
	return 0;
}
/*
 * A parked binding is the one thing libxml's reconciliation cannot cope with:
 * finding a namespace it can resolve from nowhere, it mints a declaration for
 * it on the spot under a generated prefix (`default`, then `default1`) -- and
 * that declaration is exactly what the binding must never acquire. php never
 * meets this because its namespaced tree does not reconcile at all; here the
 * walk steps over those nodes instead, by taking the namespace off them for the
 * duration and handing it back after. Nothing else about them changes: they are
 * already spelled the only way they can be spelled.
 */
typedef struct DomNsHold DomNsHold;
struct DomNsHold {
	xmlNodePtr *apNode;    /* the nodes the namespace was taken from */
	xmlNsPtr *apNs;        /* and what each one carried */
	int nUsed;
	int nAlloc;
};
static void DomNsHoldInit(DomNsHold *pHold)
{
	pHold->apNode = 0;
	pHold->apNs = 0;
	pHold->nUsed = 0;
	pHold->nAlloc = 0;
}
/* Remember what pNode carries, so it can be handed back. 0 means the memory was
 * not there: the caller stops collecting and leaves the rest of the walk alone,
 * which costs a spelling rather than the tree. */
static int DomNsHoldAdd(DomNsHold *pHold,xmlNodePtr pNode)
{
	if( pHold->nUsed == pHold->nAlloc ){
		int nNew = pHold->nAlloc ? pHold->nAlloc * 2 : 8;
		xmlNodePtr *apNode;
		xmlNsPtr *apNs;
		apNode = (xmlNodePtr *)xmlRealloc(pHold->apNode,
			(size_t)nNew * sizeof(xmlNodePtr));
		if( apNode == 0 ){
			return 0;
		}
		pHold->apNode = apNode;
		apNs = (xmlNsPtr *)xmlRealloc(pHold->apNs,(size_t)nNew * sizeof(xmlNsPtr));
		if( apNs == 0 ){
			return 0;
		}
		pHold->apNs = apNs;
		pHold->nAlloc = nNew;
	}
	pHold->apNode[pHold->nUsed] = pNode;
	pHold->apNs[pHold->nUsed] = pNode->ns;
	pHold->nUsed++;
	return 1;
}
static void DomNsHoldParked(DomNsHold *pHold,xmlNodePtr pRoot)
{
	xmlNodePtr pCur = pRoot;
	DomNsHoldInit(pHold);
	while( pCur ){
		if( pCur->type == XML_ELEMENT_NODE ){
			xmlAttrPtr pAttr;
			if( pCur->ns != 0 && pCur->ns->prefix == 0
			 && DomNsIsParked(pCur->doc,pCur->ns) ){
				if( !DomNsHoldAdd(pHold,pCur) ){
					return;
				}
				pCur->ns = 0;
			}
			/* An attribute's binding needs holding for the same reason and
			 * under any prefix -- the factory that minted it cannot declare it
			 * anywhere, so libxml would mint the declaration on every insert
			 * and php mints none, ever. Only a MINTED one: a binding parked
			 * because a write removed it is still owed a declaration. */
			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){
				if( DomNsIsFactory(pAttr->ns) ){
					if( !DomNsHoldAdd(pHold,(xmlNodePtr)pAttr) ){
						return;
					}
					pAttr->ns = 0;
				}
			}
		}
		pCur = DomWalkNext(pCur,pRoot);
	}
}
static void DomNsReleaseParked(DomNsHold *pHold)
{
	int i;
	for( i = 0 ; i < pHold->nUsed ; ++i ){
		pHold->apNode[i]->ns = pHold->apNs[i];
	}
	if( pHold->apNode ){
		xmlFree(pHold->apNode);
	}
	if( pHold->apNs ){
		xmlFree(pHold->apNs);
	}
}
/* libxml's own reconciliation, with the parked bindings held out of its way. */
static void DomReconciliateNs(xmlNodePtr pNode)
{
	DomNsHold sHold;
	DomNsHoldParked(&sHold,pNode);
	xmlReconciliateNs(pNode->doc,pNode);
	DomNsReleaseParked(&sHold);
}
/*
 * php's `dom_relink_ns_decls`, the DEFAULT-namespace half, which its namespaced
 * tree runs over the whole document before canonicalizing it and undoes after.
 *
 * An element's default namespace is not what the element POINTS at, it is what
 * the nearest `xmlns=` in scope says -- so php hands the canonicalizer the
 * declaration it would find rather than the binding the node carries, and a
 * binding declared nowhere becomes no namespace at all. Without it libxml
 * canonicalizes from the pointer, which puts an `xmlns=` on the descendants of
 * a node whose namespace the document never declares (and, for a descendant in
 * NO namespace, one that says it is).
 *
 * A declaration that IS in scope answers itself here, so the pass is a no-op on
 * every document that was parsed rather than built.
 */
static void DomC14NHoldDefaults(DomNsHold *pHold,xmlNodePtr pRoot)
{
	xmlNodePtr pCur = pRoot;
	DomNsHoldInit(pHold);
	while( pCur ){
		if( pCur->type == XML_ELEMENT_NODE && pCur->ns != 0 && pCur->ns->prefix == 0 ){
			xmlNsPtr pInScope = xmlSearchNs(pCur->doc,pCur,0);
			if( pInScope != pCur->ns ){
				if( !DomNsHoldAdd(pHold,pCur) ){
					return;
				}
				pCur->ns = pInScope;
			}
		}
		pCur = DomWalkNext(pCur,pRoot);
	}
}
/*
 * The namespace a node created through the NAMESPACED factory carries, which is
 * a smaller rule than the 2004 one: php 8.4's tree binds exactly the prefix it
 * was asked for and reuses nothing -- not a declaration of the same URI already
 * in scope, and not libxml's own `xml` binding -- so on a document that binds
 * $uri to `p`, `createElementNS($uri,'x')` comes back spelled `x` where the
 * 2004 door spells it `p:x`, and `createElementNS($XML_NS,'x')` comes back
 * spelled `x` where the 2004 door spells it `xml:x`.
 *
 * And an UNPREFIXED binding is declared NOWHERE. php's namespaced tree keeps
 * its bindings off the elements entirely -- a per-document table hands them out
 * and the canonicalizer mints the declarations it needs at output time -- so a
 * default namespace out of this factory leaves no `xmlns=` behind: the element
 * canonicalizes as `<s></s>` at the node, at every ancestor and after an insert,
 * and whatever default declaration IS in scope is the one that names it. Here
 * the binding rides on `doc->oldNs`, which is where a REMOVED declaration
 * already goes: freed with the document, and resolvable from nothing.
 */
static xmlNsPtr DomNsForCreateModern(xmlNodePtr pNode,const char *zUri,
	const xmlChar *zPrefix)
{
	xmlNsPtr pNs;
	if( zPrefix != 0
	 && ((xmlStrEqual(zPrefix,(const xmlChar *)"xml") && !DomUriIs(zUri,DOM_XML_NS_URI))
	  || (xmlStrEqual(zPrefix,(const xmlChar *)"xmlns") && !DomUriIs(zUri,DOM_XMLNS_NS_URI))
	  || (DomUriIs(zUri,DOM_XMLNS_NS_URI)
	   && !xmlStrEqual(zPrefix,(const xmlChar *)"xmlns"))) ){
		return 0;
	}
	if( zPrefix != 0 ){
		pNs = xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);
		DomNsMarkMinted(pNs);
		return pNs;
	}
	/* Unowned: xmlNewNs() with no element allocates without linking, and the
	 * park list owns it from there. */
	pNs = xmlNewNs(0,(const xmlChar *)zUri,0);
	if( pNs ){
		DomNsPark(pNode,pNs);
	}
	return pNs;
}
/*
 * Which parked bindings came out of the namespaced factory.
 *
 * `doc->oldNs` holds two unrelated things: a declaration a write REMOVED from
 * an element, which every reconciliation must still be free to re-declare
 * somewhere, and a binding the namespaced factory minted, which must never
 * acquire a declaration at all. Being parked does not tell them apart -- an
 * adopted node's removed `xmlns:p` is parked exactly like a minted one -- and
 * telling them apart by prefix stops working the moment an ATTRIBUTE is the
 * one being minted, because that factory parks every prefix it is given. So
 * the minted ones are marked, in the one field an xmlNs has spare.
 */
static const int DomNsFactoryTag = 0;
/*
 * ...and, beside it, which declarations the engine MINTED to bind a name
 * rather than being asked for by a program.
 *
 * php 8.4 answers a declaration from the attribute map, but only one a
 * document actually SPELLS: a parsed `xmlns:p`, or one written through the
 * `xmlns` attribute door.  The binding `createElementNS('urn:q','q:c')` or
 * `setAttributeNS('urn:p','p:z',..)` has to invent to name what it made is
 * not in the map, though it serializes exactly like one.  Being on an
 * element's nsDef does not tell the two apart, so the invented ones are
 * marked -- with their own tag, since a parked factory binding is a narrower
 * thing the reconciliation asks about by itself.
 */
static const int DomNsMintedTag = 0;
static void DomNsMarkMinted(xmlNsPtr pNs)
{
	if( pNs && pNs->_private == 0 ){
		pNs->_private = (void *)&DomNsMintedTag;
	}
}
/* Is this a declaration the document spells, and so one php's map lists? */
static int DomNsIsSpelt(xmlNsPtr pNs)
{
	return pNs != 0
		&& pNs->_private != (void *)&DomNsFactoryTag
		&& pNs->_private != (void *)&DomNsMintedTag;
}
/*
 * ...and WHERE on the map a spelt one sits.
 *
 * php's namespaced tree keeps a declaration in the element's PROPERTY chain,
 * beside the attributes, so a parsed `xmlns:p` comes before every attribute --
 * the parser hands an element's declarations over first whatever the source
 * order, which is why `<r a="1" xmlns:p="urn:p"/>` still reads `xmlns:p a` --
 * while one WRITTEN through setAttributeNS lands where it was written: after
 * the attributes the element already had, and before any written next.
 * libxml has no such chain -- a declaration is an xmlNs off nsDef and carries
 * no position at all -- so a written one remembers the attribute it was
 * written AFTER, in the same spare field the marks above use. A value that is
 * neither tag is that anchor, and no anchor is the HEAD of the map, which is
 * both where the parser's declarations sit and where one written onto an
 * element with no attributes yet sits.
 */
static void DomNsMapSetAfter(xmlNsPtr pNs,xmlAttrPtr pAttr)
{
	if( pNs && DomNsIsSpelt(pNs) ){
		pNs->_private = (void *)pAttr;
	}
}
/* The attribute a spelt declaration sits after, or NULL for the head of the
 * map. An anchor whose attribute has left the element reads as the LAST one
 * rather than as a wild pointer: every reader here asks this one question, so
 * they cannot disagree, and the map stays walkable whatever happened to the
 * attribute. */
static xmlAttrPtr DomNsMapAfter(xmlNodePtr pElem,xmlNsPtr pNs)
{
	xmlAttrPtr pAnchor,pCur,pLast = 0;
	if( pNs == 0 || pNs->_private == 0 || !DomNsIsSpelt(pNs) ){
		return 0;
	}
	pAnchor = (xmlAttrPtr)pNs->_private;
	for( pCur = DomAttrList(pElem) ; pCur ; pCur = pCur->next ){
		if( pCur == pAnchor ){
			return pAnchor;
		}
		pLast = pCur;
	}
	return pLast;
}
/* An attribute leaving its element takes no declaration with it: php's chain
 * closes over the gap, so one written after this attribute is now written
 * after the attribute before it. Every door that unlinks an attribute says so
 * before the unlink, while the anchor's predecessor can still be read. */
static void DomAttrGoing(xmlNodePtr pNode)
{
	xmlNodePtr pElem;
	xmlNsPtr pNs;
	if( pNode == 0 || pNode->type != XML_ATTRIBUTE_NODE ){
		return;
	}
	pElem = pNode->parent;
	if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
		return;
	}
	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){
		if( pNs->_private == (void *)pNode && DomNsIsSpelt(pNs) ){
			DomNsMapSetAfter(pNs,((xmlAttrPtr)pNode)->prev);
		}
	}
}
/*
 * A COPY carries the marks, and the positions, over.
 *
 * libxml builds every declaration on a copy with xmlNewNs, whose spare field is
 * zero, so a copied element came back claiming the engine's minted bindings
 * were spelt -- `$el->cloneNode()` listed an `xmlns:k` on the attribute map
 * that the original never listed -- and stood a written declaration back at the
 * head of the map. The copy's nsDef list is the source's, in order, with
 * anything libxml had to invent for the copy appended after it: a copy of a
 * marked declaration takes its mark, an anchor is re-read as the attribute at
 * the same position in the COPY's property list, and an invented one is a
 * binding no document spells, so it is minted.
 *
 * The children are walked in step for the same reason, and no deeper than
 * libxml's own copier already went to build them.
 */
static void DomNsCopyMarks(xmlNodePtr pSrc,xmlNodePtr pDst)
{
	xmlNodePtr pS,pD;
	if( pSrc == 0 || pDst == 0 ){
		return;
	}
	if( pSrc->type == XML_ELEMENT_NODE && pDst->type == XML_ELEMENT_NODE ){
		xmlNsPtr pSNs = pSrc->nsDef,pDNs = pDst->nsDef;
		for( ; pSNs && pDNs ; pSNs = pSNs->next,pDNs = pDNs->next ){
			if( !DomNsIsSpelt(pSNs) ){
				pDNs->_private = pSNs->_private;
				continue;
			}
			pDNs->_private = 0;
			if( DomNsMapAfter(pSrc,pSNs) ){
				xmlAttrPtr pAnchor = DomNsMapAfter(pSrc,pSNs),pCur;
				int iAt = 0;
				for( pCur = DomAttrList(pSrc) ; pCur && pCur != pAnchor ; pCur = pCur->next ){
					iAt++;
				}
				DomNsMapSetAfter(pDNs,DomAttrAt(pDst,iAt));
			}
		}
		for( ; pDNs ; pDNs = pDNs->next ){
			DomNsMarkMinted(pDNs);
		}
	}
	for( pS = pSrc->children,pD = pDst->children ; pS && pD ; pS = pS->next,pD = pD->next ){
		DomNsCopyMarks(pS,pD);
	}
}
static void DomNsMarkFactory(xmlNsPtr pNs)
{
	if( pNs ){
		pNs->_private = (void *)&DomNsFactoryTag;
	}
}
static int DomNsIsFactory(xmlNsPtr pNs)
{
	return pNs != 0 && pNs->_private == (void *)&DomNsFactoryTag;
}
/*
 * The same rule for an ATTRIBUTE out of the namespaced factory, where it is not
 * even a choice: an `xmlAttr` has no `nsDef`, so there is nowhere on the node to
 * declare anything. The binding is minted free-standing and parked, whatever
 * prefix it carries -- which is exactly what php's namespaced tree does, and
 * what makes the three answers it gives fall out: a document with no ROOT
 * ELEMENT still answers (the 2004 door declares on the root, so it cannot), the
 * prefix asked for is the prefix reported, and nothing is reused until the
 * attribute is written somewhere and a serializer resolves it.
 *
 * The `xml` prefix is the one binding that cannot be minted: libxml refuses to
 * build a second declaration of its own reserved namespace and answers NULL, so
 * ask the document for the implicit one instead.
 */
static xmlNsPtr DomNsForCreateModernAttr(xmlNodePtr pAttr,const char *zUri,
	const xmlChar *zPrefix)
{
	xmlNsPtr pNs;
	if( zPrefix != 0 && xmlStrEqual(zPrefix,(const xmlChar *)"xml")
	 && DomUriIs(zUri,DOM_XML_NS_URI) ){
		return xmlSearchNs(pAttr->doc,(xmlNodePtr)pAttr->doc,(const xmlChar *)"xml");
	}
	pNs = xmlNewNs(0,(const xmlChar *)zUri,zPrefix);
	if( pNs ){
		DomNsMarkFactory(pNs);
		DomNsPark(pAttr,pNs);
	}
	return pNs;
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
	DomNsHold sHold;
	if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
		return;
	}
	/* Held BEFORE the strip, so that only bindings ALREADY unreachable are kept
	 * out of the reconciliation. One the strip is about to park is a redundant
	 * declaration, and the nodes under it must still be re-pointed at whatever
	 * replaced it -- which is the reconciliation's whole job. */
	DomNsHoldParked(&sHold,pNode);
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
	DomNsReleaseParked(&sHold);
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
	if( DomNsIsFactory(pNs) ){
		/* Out of the namespaced factory, and declared nowhere on purpose: php
		 * resolves nothing for it on arrival either, so the attribute keeps
		 * answering the prefix it was ASKED for however the document spells
		 * that URI, and each serializer settles the spelling its own way. */
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
	pNs = DomNsGenerateEx(pElem,(const char *)pAttr->ns->href,pAttr->ns->prefix,1,0);
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
	DomReconciliateNs(pElem);
}
/* A namespace DECLARATION on this element: php's setAttributeNS writes one
 * when the name is `xmlns` or its prefix is, and REBINDS the one already
 * there rather than adding a second. */
static void DomNsDeclare(xmlNodePtr pElem,const xmlChar *zPrefix,const char *zHref)
{
	xmlNsPtr pNs;
	xmlAttrPtr pLast;
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
			return;   /* a REBIND keeps the position the declaration already had */
		}
	}
	/* A new one is written where the program writes it: after the attributes
	 * the element carries now, and before whatever is written next. */
	for( pLast = DomAttrList(pElem) ; pLast && pLast->next ; pLast = pLast->next ){
		;
	}
	DomNsMapSetAfter(xmlNewNs(pElem,(const xmlChar *)zHref,zPrefix),pLast);
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
 * php 8.4's tree answers a namespace DECLARATION through the whole attribute
 * map: `attributes` lists `xmlns:p` as a `Dom\Attr` of its own, `length` counts
 * it, and every by-name door on the map finds it.  The 2004 tree lists none --
 * it answers a declaration through DOMNameSpaceNode and nowhere else -- so this
 * is the modern branch alone, and the two shapes never meet on one document.
 *
 * What php hands back is ATTRIBUTE-shaped and not DOMNameSpaceNode's: nodeType
 * 2, `namespaceURI` the xmlns URI rather than the href being bound, `prefix`
 * `xmlns` (null for the default declaration), `localName` the prefix being
 * declared, and a `Dom\Text` child holding the URI.  libxml keeps a declaration
 * in an xmlNs, which is not a node at all -- its `_private` sits where a node's
 * `children` does, so it can never be cast to one -- so a STAND-IN attribute is
 * built per declaration and parked on the shell, the way the notation stand-ins
 * are.  It names the element as its parent but is NOT spliced into the property
 * chain, so nothing that walks or serializes an element ever meets one, while
 * every reader a wrapper asks -- name, prefix, namespaceURI, value, firstChild,
 * ownerElement, parentNode -- reads a real attribute and needs no branch of its
 * own.  One per declaration, so `$m->item(0) === $m->item(0)` holds here too.
 *
 * The declaration stays the live one, so the stand-in tracks it: an href that
 * changed underneath is copied onto the text child IN PLACE rather than by
 * rebuilding the child, which would drop a wrapper someone still holds.
 */
static xmlAttrPtr DomNsAttrFind(phl_xmldoc *pShell,xmlNsPtr pNs)
{
	xmlAttrPtr *apHave = (xmlAttrPtr *)SySetBasePtr(&pShell->aNsAttrs);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pShell->aNsAttrs) ; ++n ){
		if( apHave[n]->psvi == (void *)pNs ){
			return apHave[n];
		}
	}
	return 0;
}
static xmlAttrPtr DomNsAttr(phl_xmldoc *pShell,xmlNodePtr pElem,xmlNsPtr pNs)
{
	xmlAttrPtr pAttr;
	xmlNodePtr pTxt;
	xmlNsPtr pOwn;
	const xmlChar *zHref;
	if( pShell == 0 || pElem == 0 || pNs == 0 ){
		return 0;
	}
	zHref = pNs->href ? pNs->href : (const xmlChar *)"";
	pAttr = DomNsAttrFind(pShell,pNs);
	if( pAttr ){
		/* Live: the URI may have been rewritten under it, and the element may
		 * be a different one after an adopt. */
		if( pAttr->children && !xmlStrEqual(pAttr->children->content,zHref) ){
			xmlNodeSetContent(pAttr->children,zHref);
		}
		pAttr->parent = pElem;
		pAttr->doc = pElem->doc;
		return pAttr;
	}
	pAttr = (xmlAttrPtr)xmlMalloc(sizeof(xmlAttr));
	pOwn = (xmlNsPtr)xmlMalloc(sizeof(xmlNs));
	if( pAttr == 0 || pOwn == 0 ){
		if( pAttr ){
			xmlFree(pAttr);
		}
		if( pOwn ){
			xmlFree(pOwn);
		}
		return 0;
	}
	SyZero(pAttr,sizeof(xmlAttr));
	SyZero(pOwn,sizeof(xmlNs));
	pOwn->type = XML_NAMESPACE_DECL;
	pOwn->href = xmlStrdup((const xmlChar *)DOM_XMLNS_NS_URI);
	/* php spells the DEFAULT declaration `xmlns` with NO prefix and a local
	 * name of `xmlns`; a prefixed one is `xmlns:p`, prefix `xmlns`, local `p`. */
	pOwn->prefix = pNs->prefix ? xmlStrdup((const xmlChar *)DOM_XMLNS_NAME) : 0;
	pAttr->type = XML_ATTRIBUTE_NODE;
	pAttr->name = xmlStrdup(pNs->prefix ? pNs->prefix : (const xmlChar *)DOM_XMLNS_NAME);
	pAttr->ns = pOwn;
	pAttr->doc = pElem->doc;
	pAttr->parent = pElem;
	pAttr->psvi = (void *)pNs;          /* the declaration this stands for */
	pTxt = xmlNewDocText(pElem->doc,zHref);
	if( pTxt ){
		pTxt->parent = (xmlNodePtr)pAttr;
		pAttr->children = pAttr->last = pTxt;
	}
	if( SySetPut(&pShell->aNsAttrs,(const void *)&pAttr) != SXRET_OK ){
		if( pTxt ){
			xmlFreeNode(pTxt);
		}
		xmlFreeNs(pOwn);
		if( pAttr->name ){
			xmlFree((xmlChar *)pAttr->name);
		}
		xmlFree(pAttr);
		return 0;
	}
	return pAttr;
}
/* Whether pNode IS one of those stand-ins: an attribute whose psvi names a
 * declaration the shell parked it for. A real attribute's psvi is libxml's
 * own and never answers this. */
static xmlNsPtr DomNsAttrStandIn(phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNsPtr pNs;
	if( pShell == 0 || pNode == 0 || pNode->type != XML_ATTRIBUTE_NODE
	 || ((xmlAttrPtr)pNode)->psvi == 0 ){
		return 0;
	}
	pNs = (xmlNsPtr)((xmlAttrPtr)pNode)->psvi;
	return DomNsAttrFind(pShell,pNs) == (xmlAttrPtr)pNode ? pNs : 0;
}
/*
 * ===== The one sibling chain the two halves of the map share =====
 *
 * php walks an element's declarations and its attributes as a SINGLE list, so
 * a declaration's `nextSibling` can be an attribute and an attribute's
 * `previousSibling` a declaration. The order is the one DomNsMapAfter records:
 * the declarations the map lists at its head, then every attribute, each
 * followed by the declarations written after it -- `xmlns:p` -> `a` -> `xmlns:w`
 * for a document that spells the first and a program that writes the second.
 *
 * libxml keeps declarations off `pElem->properties` entirely, and the stand-ins
 * are deliberately parked on the shell so that nothing which walks or
 * serializes an element ever meets one; splicing them in would break every one
 * of those walks. So the two READERS branch instead, and the chain exists only
 * where it is asked for -- both of them stepping through the one walk below, so
 * that the map's order and the sibling links can never disagree.
 *
 * Only the modern tree lists declarations on the attribute map at all: the
 * 2004 tree answers one through DOMNameSpaceNode, which declares no sibling
 * property and warns on the read, so its chain is libxml's untouched.
 */

/* The declarations written after pAfter (NULL for the map's head), in the order
 * the element makes them: the one after pFrom, or the first when pFrom is NULL. */
static xmlNsPtr DomNsAnchoredNext(xmlNodePtr pElem,xmlAttrPtr pAfter,xmlNsPtr pFrom)
{
	xmlNsPtr pNs;
	int bSeen = pFrom == 0;
	if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
		return 0;
	}
	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){
		if( pNs == pFrom ){
			bSeen = 1;
			continue;
		}
		if( !bSeen || !DomNsIsSpelt(pNs) ){
			continue;
		}
		if( DomNsMapAfter(pElem,pNs) == pAfter ){
			return pNs;
		}
	}
	return 0;
}
/* ...the one before pFrom in that same group, and the LAST of it -- what an
 * attribute reads backwards, since the group sits between it and the attribute
 * it follows. */
static xmlNsPtr DomNsAnchoredPrev(xmlNodePtr pElem,xmlAttrPtr pAfter,xmlNsPtr pFrom)
{
	xmlNsPtr pNs,pPrev = 0;
	if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
		return 0;
	}
	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){
		if( pNs == pFrom ){
			break;
		}
		if( DomNsIsSpelt(pNs) && DomNsMapAfter(pElem,pNs) == pAfter ){
			pPrev = pNs;
		}
	}
	return pPrev;
}
static xmlNsPtr DomNsAnchoredLast(xmlNodePtr pElem,xmlAttrPtr pAfter)
{
	return DomNsAnchoredPrev(pElem,pAfter,0);
}
/* One step along the merged chain. The cursor is an attribute or a declaration,
 * never both; both NULL is the end. */
static void DomMapWalkFirst(xmlNodePtr pElem,xmlAttrPtr *ppAttr,xmlNsPtr *ppNs)
{
	*ppNs = DomNsAnchoredNext(pElem,0,0);
	*ppAttr = *ppNs ? 0 : DomAttrList(pElem);
}
static void DomMapWalkNext(xmlNodePtr pElem,xmlAttrPtr *ppAttr,xmlNsPtr *ppNs)
{
	if( *ppNs ){
		xmlAttrPtr pAfter = DomNsMapAfter(pElem,*ppNs);
		xmlNsPtr pNext = DomNsAnchoredNext(pElem,pAfter,*ppNs);
		*ppNs = pNext;
		if( pNext == 0 ){
			/* The group is spent: the attribute it follows hands over to the
			 * next attribute, and the HEAD group to the first one. */
			*ppAttr = pAfter ? pAfter->next : DomAttrList(pElem);
		}
		return;
	}
	if( *ppAttr ){
		xmlNsPtr pNext = DomNsAnchoredNext(pElem,*ppAttr,0);
		if( pNext ){
			*ppNs = pNext;
			*ppAttr = 0;
			return;
		}
		*ppAttr = (*ppAttr)->next;
	}
}
static xmlNodePtr DomAttrSiblingNext(int bModern,phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNsPtr pDecl,pNext;
	xmlNodePtr pElem;
	xmlAttrPtr pAnchor;
	if( pNode == 0 ){
		return 0;
	}
	if( !bModern || pNode->type != XML_ATTRIBUTE_NODE ){
		return pNode->next;
	}
	pElem = pNode->parent;
	pDecl = DomNsAttrStandIn(pShell,pNode);
	if( pDecl == 0 ){
		/* A real attribute: whatever was written after it comes first. */
		pNext = DomNsAnchoredNext(pElem,(xmlAttrPtr)pNode,0);
		return pNext ? (xmlNodePtr)DomNsAttr(pShell,pElem,pNext) : pNode->next;
	}
	pAnchor = DomNsMapAfter(pElem,pDecl);
	pNext = DomNsAnchoredNext(pElem,pAnchor,pDecl);
	if( pNext ){
		return (xmlNodePtr)DomNsAttr(pShell,pElem,pNext);
	}
	return (xmlNodePtr)(pAnchor ? pAnchor->next : DomAttrList(pElem));
}
static xmlNodePtr DomAttrSiblingPrev(int bModern,phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNsPtr pDecl,pPrev;
	xmlNodePtr pElem;
	xmlAttrPtr pAnchor;
	if( pNode == 0 ){
		return 0;
	}
	if( !bModern || pNode->type != XML_ATTRIBUTE_NODE ){
		return pNode->prev;
	}
	pElem = pNode->parent;
	pDecl = DomNsAttrStandIn(pShell,pNode);
	if( pDecl == 0 ){
		/* A real attribute, whose predecessor is the last declaration written
		 * after the attribute BEFORE it -- and, for the element's first, the
		 * last of the head group. A detached attribute has neither. */
		pAnchor = ((xmlAttrPtr)pNode)->prev;
		if( pAnchor == 0 && DomAttrList(pElem) != (xmlAttrPtr)pNode ){
			return 0;
		}
		pPrev = DomNsAnchoredLast(pElem,pAnchor);
		return pPrev ? (xmlNodePtr)DomNsAttr(pShell,pElem,pPrev) : (xmlNodePtr)pAnchor;
	}
	pAnchor = DomNsMapAfter(pElem,pDecl);
	pPrev = DomNsAnchoredPrev(pElem,pAnchor,pDecl);
	if( pPrev ){
		return (xmlNodePtr)DomNsAttr(pShell,pElem,pPrev);
	}
	return (xmlNodePtr)pAnchor;
}
/* How many of them the attribute map lists, and the node at that position in
 * the merged order: only the declarations the document SPELLS, never a binding
 * the engine minted to name something it was asked to make. */
static int DomNsMapCount(xmlNodePtr pElem)
{
	xmlNsPtr pNs;
	int iCount = 0;
	if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
		return 0;
	}
	for( pNs = pElem->nsDef ; pNs ; pNs = pNs->next ){
		if( DomNsIsSpelt(pNs) ){
			iCount++;
		}
	}
	return iCount;
}
static xmlNodePtr DomMapNodeAt(phl_xmldoc *pShell,xmlNodePtr pElem,int iWant)
{
	xmlAttrPtr pAttr;
	xmlNsPtr pNs;
	DomMapWalkFirst(pElem,&pAttr,&pNs);
	while( iWant > 0 && (pAttr || pNs) ){
		DomMapWalkNext(pElem,&pAttr,&pNs);
		iWant--;
	}
	if( pNs ){
		return (xmlNodePtr)DomNsAttr(pShell,pElem,pNs);
	}
	return (xmlNodePtr)pAttr;
}
/* ...and the same screen on the by-name doors, which read the whole chain. */
static xmlNsPtr DomNsMapSpelt(xmlNsPtr pNs)
{
	return DomNsIsSpelt(pNs) ? pNs : 0;
}
/* The declaration a modern by-(namespace, local name) lookup names: the local
 * name IS the prefix being declared, and `xmlns` names the default one. */
static xmlNsPtr DomNsDeclByLocal(xmlNodePtr pElem,const char *zLocal)
{
	return SyStrncmp(zLocal,DOM_XMLNS_NAME,sizeof(DOM_XMLNS_NAME)) == 0
		? DomNsDeclOf(pElem,0)
		: DomNsDeclOf(pElem,(const xmlChar *)zLocal);
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
 * The 2004 tree's harsher way to drop one.  Removing a declaration by NAME
 * leaves whatever still resolves through it a declaration closer to where it is
 * used; removing it through `removeAttributeNS(<the URI it binds>, <prefix>)`
 * does not -- the binding's own strings are freed, and every element and
 * attribute below that resolved through it is left in NO namespace at all.  So
 * `<a:c a:x="1"/>` under an eliminated `xmlns:a` reads back as `<c x="1"/>`,
 * and a declaration handle still held answers nothing.  Elimination is by
 * POINTER: a second prefix bound to the same URI is untouched.
 */
static void DomNsEliminate(xmlNodePtr pElem,xmlNsPtr pNs)
{
	xmlNodePtr pCur = pElem;
	DomNsDeclRemove(pElem,pNs);
	if( pNs->href ){
		xmlFree((xmlChar *)pNs->href);
		pNs->href = 0;
	}
	if( pNs->prefix ){
		xmlFree((xmlChar *)pNs->prefix);
		pNs->prefix = 0;
	}
	while( pCur ){
		if( pCur->type == XML_ELEMENT_NODE ){
			xmlAttrPtr pAttr;
			if( pCur->ns == pNs ){
				pCur->ns = 0;
			}
			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){
				if( pAttr->ns == pNs ){
					pAttr->ns = 0;
				}
			}
		}
		pCur = DomWalkNext(pCur,pElem);
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
	{
		ph7_class_instance *pOwn = DomWrap(&(*pVm),pDoc,pShell,pElem);
		PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_NS_OWNER,pOwn);
		if( pOwn ){
			PH7_ClassInstanceUnref(pOwn);   /* the slot took its own */
		}
	}
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
static xmlAttrPtr DomAttr2004(xmlNodePtr pElem,const char *zName)
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
/*
 * ...and php 8.4's rule, which resolves NOTHING.
 *
 * The namespaced tree matches the qualified name as WRITTEN: an attribute
 * carrying a prefix answers only its own prefix and its own local name, spelled
 * exactly, and one carrying none answers only its whole stored name. So two
 * prefixes bound to the same URI are two different names here where the 2004
 * rule -- which resolves the sought prefix and then matches by (local, URI) --
 * makes them one, and that is the whole difference between the trees: on
 * `<r xmlns:p="urn:x" xmlns:q="urn:x"><e p:b="1"/>`, `hasAttribute('q:b')` is
 * true on the old tree and false on the new one.
 *
 * The walk is the property list in order, not a hash, because the question is
 * about the prefix the attribute WEARS and libxml keys its table by the local
 * name alone. php lowercases the sought name first when the element is HTML in
 * an HTML document; that branch is unreachable until the HTML parser lands, so
 * it belongs with the parser and not here.
 */
static int DomQNameIsSpec(const char *zName,xmlNodePtr pNode)
{
	const xmlChar *zLocal = pNode->name;
	if( pNode->ns && pNode->ns->prefix ){
		const char *zPrefix = (const char *)pNode->ns->prefix;
		sxu32 nPrefix = SyStrlen(zPrefix);
		/* The prefix compare stops at the sought name's own NUL when the two
		 * lengths differ, so it never reads past it. */
		if( SyMemcmp(zName,zPrefix,nPrefix) != 0 || zName[nPrefix] != ':' ){
			return 0;
		}
		return xmlStrEqual((const xmlChar *)(zName + nPrefix + 1),zLocal);
	}
	return xmlStrEqual(zLocal,(const xmlChar *)zName);
}
static xmlAttrPtr DomAttrBySpec(xmlNodePtr pElem,const char *zName)
{
	xmlAttrPtr pAttr;
	if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
		return 0;
	}
	for( pAttr = pElem->properties ; pAttr ; pAttr = pAttr->next ){
		if( pAttr->type == XML_ATTRIBUTE_NODE && DomQNameIsSpec(zName,(xmlNodePtr)pAttr) ){
			return pAttr;
		}
	}
	return 0;
}
/* The door every by-name reader comes through: the receiver's tree picks the
 * rule, exactly as php's own one helper branches on it. */
static xmlAttrPtr DomAttrByName(xmlNodePtr pElem,const char *zName,int bModern)
{
	return bModern ? DomAttrBySpec(pElem,zName) : DomAttr2004(pElem,zName);
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
	xmlAttrPtr pAttr = pDecl ? 0 : DomAttrByName(pElem,zName,DomThisModern(pCtx));
	xmlChar *zVal = pAttr ? xmlNodeListGetString(pAttr->doc,pAttr->children,1) : 0;
	if( pDecl ){
		/* A declaration's "value" is the URI it binds. */
		ph7_result_string(pCtx,pDecl->href ? (const char *)pDecl->href : "",-1);
		return PH7_OK;
	}
	if( pAttr == 0 && DomThisModern(pCtx) ){
		/* php 8.4's element answers null for an attribute it does not carry,
		 * where the 2004 one answers "" -- the whole difference between a
		 * `string` return and a `?string` one, and the reason
		 * `getAttribute('x') === null` is the modern absence test. */
		ph7_result_null(pCtx);
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
		&& (DomAttrByName((xmlNodePtr)pNd->pNode,zName,DomThisModern(pCtx)) != 0
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
		if( DomThisModern(pCtx) ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	xmlSetProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zName,(const xmlChar *)zVal);
	if( DomThisModern(pCtx) ){
		/* php 8.4 declares this one `void`: the write happens and the attribute
		 * node it wrote is not handed back. */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pAttr = DomAttrByName((xmlNodePtr)pNd->pNode,zName,DomThisModern(pCtx));
	if( pAttr == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);
}
/* The 2004 removal answers a bool; the modern one is `void`. */
static void DomResultRemoved(ph7_context *pCtx,int bModern,int bRemoved)
{
	if( bModern ){
		ph7_result_null(pCtx);
	}else{
		ph7_result_bool(pCtx,bRemoved);
	}
}
/* DOMElement::removeAttribute(string $qualifiedName): bool */
DOM_METHOD(vm_builtin_DOMElement_removeAttribute)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlAttrPtr pAttr = DomAttrByName(pElem,zName,DomThisModern(pCtx));
	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);
	/* php 8.4 declares this one `void`, so every answer below is null there --
	 * including the absent one, which is NOT a refusal in either tree. */
	int bModern = DomThisModern(pCtx);
	if( pDecl ){
		/* The declaration goes, and on the 2004 tree whatever still needs it
		 * gets it back -- php answers TRUE either way, and a binding nothing
		 * uses simply vanishes. php 8.4's tree gives back a PREFIXED one only:
		 * a node left pointing at an unprefixed binding declared nowhere still
		 * ANSWERS the namespace there, and the serializer writes nothing. */
		DomNsDeclRemove(pElem,pDecl);
		DomNsReconcile(pElem,bModern,0);
		DomResultRemoved(pCtx,bModern,1);
		return PH7_OK;
	}
	if( pAttr == 0 ){
		/* Absent (or a DTD default): php returns false */
		DomResultRemoved(pCtx,bModern,0);
		return PH7_OK;
	}
	DomAttrGoing((xmlNodePtr)pAttr);
	xmlRemoveProp(pAttr);
	DomResultRemoved(pCtx,bModern,1);
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
/*
 * The same argument read on php 8.4's tree, where "no namespace" is spelled
 * two ways.  That tree's FACTORIES already normalize the empty string to null
 * -- `setAttributeNS('', 'a', 'V')` puts `a` in no namespace, not in one whose
 * URI is empty -- and its read side asks the question the same way, so
 * `getAttributeNS('', 'a')` answers what `getAttributeNS(null, 'a')` answers.
 * The 2004 tree does not normalize: there the empty string is a URI like any
 * other and matches nothing a document carries, which is why this is asked of
 * the document rather than applied to every namespace argument.  It is asked
 * only of the doors that LOOK a namespace up; `setIdAttributeNS('', ...)` is
 * Not Found on both trees and keeps the literal argument.
 */
static const xmlChar * DomArgUriLookup(ph7_context *pCtx,int nArg,ph7_value **apArg,int iArg)
{
	const xmlChar *zUri = DomArgUri(nArg,apArg,iArg);
	if( zUri != 0 && zUri[0] == 0 && DomThisModern(pCtx) ){
		return 0;
	}
	return zUri;
}
/* DOMElement::getAttributeNS(?string $namespace, string $localName): string */
DOM_METHOD(vm_builtin_DOMElement_getAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const xmlChar *zUri = DomArgUriLookup(pCtx,nArg,apArg,0);
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlChar *zVal = pNd ? xmlGetNsProp((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal,zUri) : 0;
	if( zVal == 0 && pNd && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){
		/* The other door, the one hasAttributeNS already knew about: a
		 * DECLARATION answers its URI here. `getAttributeNS($XMLNS, 'p')` was ""
		 * on an element declaring `xmlns:p`, where php answers the namespace.
		 * The local name `xmlns` names the DEFAULT declaration, but only in php
		 * 8.4's tree -- the 2004 one reads it as a prefix like any other and so
		 * answers about no element that merely carries `xmlns="..."`. */
		xmlNsPtr pDecl = DomThisModern(pCtx)
			? DomNsDeclByLocal((xmlNodePtr)pNd->pNode,zLocal)
			: DomNsDeclOf((xmlNodePtr)pNd->pNode,(const xmlChar *)zLocal);
		if( pDecl && pDecl->href ){
			ph7_result_string(pCtx,(const char *)pDecl->href,-1);
			return PH7_OK;
		}
	}
	if( zVal == 0 && DomThisModern(pCtx) ){
		ph7_result_null(pCtx);
		return PH7_OK;
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
	int bModern = DomThisModern(pCtx);
	xmlNodePtr pNode;
	xmlNsPtr pNs = 0;
	dom_qname sQ;
	int rc,bDecl,nOldDefs;
	if( pNd == 0 ){
		return DomThrowVoid(pCtx,DOM_ERR_NAMESPACE);
	}
	if( zQname[0] == 0 && !bModern ){
		/* php screens the EMPTY name at the parameter, before the DOM sees it --
		 * on the 2004 tree only, where the namespaced one lets the grammar
		 * refuse it like any other name it cannot read. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMElement::setAttributeNS(): Argument #2 ($qualifiedName) must not be empty");
	}
	/*
	 * The name is judged by the tree's own rules, and php 8.4's are the ones it
	 * judges a CREATED attribute by rather than the 2004 SET side's: the name
	 * must be a QName whatever namespace came with it (`:x` and the empty name
	 * are refusals here and writable names there), every grammar failure is the
	 * Invalid Character Error rather than the Namespace Error, and the DOM
	 * spec's two reserved rules -- the xmlns pairing, and the `xml` prefix's own
	 * URI -- are checked on the name instead of at the resolution, so
	 * `setAttributeNS('urn:u','xml:id',..)` is refused here even on a document
	 * that already binds `urn:u`, which the 2004 tree writes as `p:id`.
	 */
	rc = DomQNameParse(zQname,zUri,bModern ? DOM_QN_MATTR : DOM_QN_SET,&sQ);
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
		pNs = DomNsResolve(pNode,zUri,sQ.zPrefix,bModern,bModern);
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
		DomNsReconcile(pNode,DomThisModern(pCtx),1);
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
/* libxml's own name-only question, which the getAttributeNode family asks.
 * php 8.5.9's setAttributeNode asked it too; 8.5.10 matches the namespace as
 * well, so the displacement doors no longer come here. */
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
/* ...and at a POSITION: before pRef, which must be one of pElem's own --
 * insertBefore against an attribute reference, and replaceChild's
 * attribute-for-attribute swap, the two places php lets a caller state the
 * property list's order. */
static void DomAttrLinkBefore(xmlNodePtr pElem,xmlAttrPtr pAttr,xmlAttrPtr pRef)
{
	pAttr->parent = pElem;
	pAttr->doc = pElem->doc;
	pAttr->next = pRef;
	pAttr->prev = pRef->prev;
	if( pRef->prev ){
		pRef->prev->next = pAttr;
	}else{
		pElem->properties = pAttr;
	}
	pRef->prev = pAttr;
}
/* Detach an attribute from its element (or the orphan set) without freeing it:
 * php hands the caller back the node it displaced, alive. */
static void DomAttrDetach(phl_xmldoc *pShell,xmlAttrPtr pAttr)
{
	if( pAttr->parent ){
		DomAttrGoing((xmlNodePtr)pAttr);
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
	xmlAttrPtr pAttr = DomAttrByName(pElem,zName,DomThisModern(pCtx));
	xmlNsPtr pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);
	if( pDecl ){
		/* php 8.4 answers a declaration as an ATTRIBUTE, the same object its
		 * attribute map lists; the 2004 tree answers a DOMNameSpaceNode. */
		return DomThisModern(pCtx)
			? DomResultNodeOf(pCtx,pNd,(xmlNodePtr)DomNsAttr(pNd->pShell,pElem,pDecl))
			: DomResultNsNode(pCtx,pNd,pDecl,pElem);
	}
	if( pAttr == 0 ){
		/* `DOMAttr|false` in 2004, `?Dom\Attr` in php 8.4 -- the NS spelling
		 * below already answered null in both. */
		if( DomThisModern(pCtx) ){
			ph7_result_null(pCtx);
		}else{
			ph7_result_bool(pCtx,0);
		}
		return PH7_OK;
	}
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);
}
/* DOMElement::getAttributeNodeNS(?string $namespace, string $localName): ?DOMAttr */
DOM_METHOD(vm_builtin_DOMElement_getAttributeNodeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	const xmlChar *zUri = DomArgUriLookup(pCtx,nArg,apArg,0);
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){
		/* Here the LOCAL name is the prefix being declared -- and the DEFAULT
		 * declaration, whose local name would be `xmlns`, is not reachable this
		 * way at all. */
		int bModern = DomThisModern(pCtx);
		/* ...except in php 8.4's tree, where `xmlns` names the default
		 * declaration and the answer is the map's attribute. */
		xmlNsPtr pDecl = bModern ? DomNsDeclByLocal(pElem,zLocal)
		                         : DomNsDeclOf(pElem,(const xmlChar *)zLocal);
		if( pDecl == 0 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		return bModern
			? DomResultNodeOf(pCtx,pNd,(xmlNodePtr)DomNsAttr(pNd->pShell,pElem,pDecl))
			: DomResultNsNode(pCtx,pNd,pDecl,pElem);
	}
	return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)DomAttrByNs(pElem,zUri,zLocal));
}
/*
 * DOMElement::setAttributeNode(DOMAttr $attr): ?DOMAttr and its NS spelling.
 *
 * php answers the attribute it DISPLACED (alive and ownerless) or null. Since
 * 8.5.10 both spellings decide "the same attribute" the same way, on the local
 * name and the namespace URI together (8.5.9's plain spelling matched the name
 * alone, so a plain `k` displaced a namespaced `p:k`).  An attribute that already belongs to
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
	if( pAttr->doc != pElem->doc && pAttr->doc != 0 ){
		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);
	}
	/* A constructed, document-less attribute is ADOPTED, like every insertion
	 * door -- `$el->setAttributeNode(new DOMAttr('k','v'))` is how a built
	 * attribute reaches a real document. */
	DomAdoptIntoRecv(pCtx,apArg[0],DomObjArg(apArg[0]));
	pOld = DomAttrByNs(pElem,pAttr->ns ? pAttr->ns->href : 0,(const char *)pAttr->name);
	SXUNUSED(bNS);
	if( pOld == pAttr ){
		/* Already this element's, under this spelling: php does nothing at all
		 * and answers null rather than handing the node back to itself. */
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pOld ){
		DomAttrGoing((xmlNodePtr)pOld);
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
	DomAttrGoing((xmlNodePtr)pAttr);
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
	const xmlChar *zUri = DomArgUriLookup(pCtx,nArg,apArg,0);
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pElem && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){
		/*
		 * Two doors onto that namespace, and php answers about EITHER: a
		 * DECLARATION, which is not an attribute in libxml at all, and a real
		 * attribute in it -- which is what `createAttributeNS($XMLNS, ...)`
		 * makes, and which this only asked the first door about. (A DEFAULT
		 * declaration is the third, and only in php 8.4's tree, where the local
		 * name `xmlns` names it; the 2004 tree reads that name as a prefix and
		 * answers false.)
		 */
		ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0
			|| (DomThisModern(pCtx) ? DomNsDeclByLocal(pElem,zLocal)
			                        : DomNsDeclOf(pElem,(const xmlChar *)zLocal)) != 0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx,DomAttrByNs(pElem,zUri,zLocal) != 0);
	return PH7_OK;
}
/*
 * DOMElement::removeAttributeNS(?string $namespace, string $localName): void --
 * an absent one is silence, as php's is.
 *
 * The two trees address a DECLARATION here by opposite halves of the pair.  php
 * 8.4's element carries its declarations as attributes in the xmlns namespace,
 * so it is named `($XMLNS, <prefix>)` -- and `($XMLNS, 'xmlns')` names the
 * default one.  The 2004 element carries none, so the pair it answers to is
 * `(<the URI the prefix binds>, <prefix>)`, with the EMPTY local name naming
 * the default one; asking that tree for the xmlns namespace names nothing.
 * There the lookup also SCREENS: a local name this element declares and a URI
 * that is not what it binds stops the call dead, so the attribute the same pair
 * names is left in place too.
 */
DOM_METHOD(vm_builtin_DOMElement_removeAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zLocal = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	const xmlChar *zUri = DomArgUriLookup(pCtx,nArg,apArg,0);
	xmlNodePtr pElem = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlAttrPtr pAttr = DomAttrByNs(pElem,zUri,zLocal);
	if( pElem == 0 ){
		return PH7_OK;
	}
	if( DomThisModern(pCtx) ){
		xmlNsPtr pDecl = DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI)
			? DomNsDeclByLocal(pElem,zLocal) : 0;
		if( pDecl ){
			/* The declaration goes and whatever still needs it gets it back,
			 * exactly as removing it under its written name does. */
			DomNsDeclRemove(pElem,pDecl);
			DomNsReconcile(pElem,1,0);
			return PH7_OK;
		}
	}else{
		xmlNsPtr pDecl = zLocal[0] == 0 ? DomNsDeclOf(pElem,0)
			: DomNsDeclOf(pElem,(const xmlChar *)zLocal);
		if( pDecl ){
			if( !xmlStrEqual(zUri,pDecl->href) ){
				return PH7_OK;
			}
			DomNsEliminate(pElem,pDecl);
		}
	}
	if( pAttr ){
		DomAttrGoing((xmlNodePtr)pAttr);
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
	pAttr = DomAttrByName(pElem,zName,DomThisModern(pCtx));
	pDecl = pAttr ? 0 : DomNsDeclByName(pElem,zName);
	if( bForceGiven ? bForce : (pAttr == 0 && pDecl == 0) ){
		if( pAttr == 0 && pDecl == 0 ){
			sxu32 nXmlns = (sxu32)SyStrlen(DOM_XMLNS_NAME);
			int bXmlnsName = SyStrlen(zName) >= nXmlns
				&& SyStrncmp(zName,DOM_XMLNS_NAME,nXmlns) == 0
				&& (zName[nXmlns] == 0 || zName[nXmlns] == ':');
			if( bXmlnsName ){
				/* An xmlns name toggled ON becomes a DECLARATION bound to the
				 * empty URI, not an attribute -- and it is written like one, so
				 * it comes out where a written declaration comes out. */
				DomNsDeclare(pElem,zName[nXmlns] == ':' ? (const xmlChar *)(zName+nXmlns+1) : 0,"");
			}else{
				xmlSetProp(pElem,(const xmlChar *)zName,(const xmlChar *)"");
			}
		}
		ph7_result_bool(pCtx,1);
		return PH7_OK;
	}
	if( pAttr ){
		DomAttrGoing((xmlNodePtr)pAttr);
		xmlRemoveProp(pAttr);
	}else if( pDecl ){
		DomNsDeclRemove(pElem,pDecl);
		DomNsReconcile(pElem,DomThisModern(pCtx),0);
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
	const xmlChar *zUri = DomArgUri(nArg,apArg,0);
	xmlAttrPtr pAttr;
	/* Unlike getAttributeNS(), where a null namespace asks for the attribute in
	 * NO namespace, php matches this one as a URI only: a null is the empty
	 * URI, which nothing carries, so `setIdAttributeNS(null,'a',true)` is Not
	 * Found even on an element that has `a`. php 8.4's row is the only one that
	 * can ask -- the 2004 row still declares a plain `string`. */
	if( zUri == 0 ){
		zUri = (const xmlChar *)"";
	}
	pAttr = pNd ? DomAttrByNs((xmlNodePtr)pNd->pNode,zUri,zLocal) : 0;
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
 * The namespace a RENAMED element carries.  Neither create rule fits: the
 * create side keys on the URI and takes any binding already in scope for it,
 * which would answer `xml:` for a rename into the XML namespace and `p:` for a
 * rename that asked for `q:`.  php honours the prefix asked for exactly, so
 * this one keys on the PREFIX -- a declaration in scope that already spells
 * this URI that way is reused, and anything else is declared on the element
 * itself, shadowing an outer binding of the same prefix when there is one.
 */
static xmlNsPtr DomNsForRename(xmlNodePtr pNode,const char *zUri,const xmlChar *zPrefix)
{
	xmlNsPtr pNs = xmlSearchNs(pNode->doc,pNode,zPrefix);
	if( pNs && pNs->href && xmlStrEqual(pNs->href,(const xmlChar *)zUri) ){
		return pNs;
	}
	pNs = xmlNewNs(pNode,(const xmlChar *)zUri,zPrefix);
	if( pNs == 0 ){
		/* The element ALREADY declares this prefix, for another URI -- the one
		 * case xmlNewNs refuses. The declaration is the element's own, and the
		 * rename is what re-points it. */
		for( pNs = pNode->nsDef ; pNs ; pNs = pNs->next ){
			if( (pNs->prefix == 0) == (zPrefix == 0)
			 && (zPrefix == 0 || xmlStrEqual(pNs->prefix,zPrefix)) ){
				break;
			}
		}
		return pNs;
	}
	DomNsMarkMinted(pNs);
	return pNs;
}
/*
 * Dom\Element::rename(?string $namespaceURI, string $qualifiedName): void
 * Dom\Attr::rename(?string $namespaceURI, string $qualifiedName): void
 *
 * php 8.4's one way to change a node's name AND its namespace at once -- the
 * 2004 tree has no door for it at all, which is why a program that wants one
 * there rebuilds the node and moves every child by hand.
 *
 * The name is judged exactly as the namespaced attribute FACTORY judges one
 * (DOM_QN_MATTR), on both receivers: a QName always, so `:z`, `z:` and `a:b:c`
 * are the Invalid Character Error even on an element, where the namespaced
 * element factory would have written `:z` literally; and every rule ABOUT a
 * namespace -- a prefix with none, the `xml` prefix off its own URI, the xmlns
 * pairing -- is still the Namespace Error.
 *
 * What the two receivers do NOT share is where the binding goes. An element
 * DECLARES it (DomNsForRename), so a child renamed into a namespace re-points
 * what its own descendants resolve. An attribute declares nothing: like the
 * namespaced factory it carries a free-standing binding parked on the document
 * (DomNsForCreateModernAttr), and the prefix a serializer writes is that
 * serializer's decision -- `saveXml` takes an in-scope prefix for the URI or
 * invents `ns1`, which is why renaming an attribute into a URI the document
 * already binds comes out under the document's prefix and not the one asked
 * for, while `prefix` still reads back the one asked for.
 *
 * An attribute is the only receiver that can COLLIDE, and php's refusal there
 * is prose rather than one of the level-2 names: code 13 under a sentence. It
 * is asked of the owner element, so a detached attribute never collides, and
 * renaming an attribute to the name it already has is not a collision with
 * itself.
 */
DOM_METHOD(vm_builtin_Dom_rename)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	const char *zUri = DomArgStrOrNull(nArg,apArg,0);
	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	xmlNsPtr pNs = 0;
	dom_qname sQ;
	int rc;
	if( pNode == 0 ){
		return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);
	}
	if( zUri != 0 && zUri[0] == 0 ){
		zUri = 0;   /* the empty namespace is no namespace, as everywhere here */
	}
	rc = DomQNameParse(zQname,zUri,DOM_QN_MATTR,&sQ);
	if( rc ){
		return DomThrowAlways(pCtx,rc);
	}
	if( pNode->type == XML_ATTRIBUTE_NODE ){
		if( pNode->parent ){
			xmlAttrPtr pHave = zUri
				? DomAttrByNs(pNode->parent,(const xmlChar *)zUri,(const char *)sQ.zLocal)
				: DomAttrNoNs(pNode->parent,(const char *)sQ.zLocal);
			if( pHave != 0 && pHave != (xmlAttrPtr)pNode ){
				DomQNameRelease(&sQ);
				return DomThrowSentence(pCtx,13,
					"An attribute with the given name in the given namespace already exists");
			}
		}
		if( zUri != 0 ){
			pNs = DomNsForCreateModernAttr(pNode,zUri,sQ.zPrefix);
			if( pNs == 0 ){
				DomQNameRelease(&sQ);
				return PH7_ContextMemoryError(pCtx);
			}
		}
	}else if( zUri != 0 ){
		pNs = DomNsForRename(pNode,zUri,sQ.zPrefix);
		if( pNs == 0 ){
			DomQNameRelease(&sQ);
			return PH7_ContextMemoryError(pCtx);
		}
	}
	xmlNodeSetName(pNode,sQ.zLocal);
	xmlSetNs(pNode,pNs);
	DomQNameRelease(&sQ);
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
 * Dom\Document::createAttributeNS(?string $namespace, string $qualifiedName): Dom\Attr
 *
 * Two factories, not one retyped: php's namespaced tree keeps its bindings off
 * the tree entirely, and that changes every answer this door gives.
 *
 * On the 2004 door the namespace is declared on the document's ROOT ELEMENT,
 * not on the attribute -- which is why a document that has no root element yet
 * cannot answer at all, and says so with php's warning and a false, and why the
 * prefix asked for is replaced by any the root already binds to that URI.
 *
 * The namespaced one declares nothing (DomNsForCreateModernAttr): the binding
 * rides on the attribute as a free-standing declaration parked on the document,
 * so a rootless document answers, the prefix asked for is the prefix reported,
 * and no reuse happens at all -- not on create and not on insert. What a
 * serializer then writes is its own decision, and the two disagree: `saveXml`
 * takes an in-scope prefix for that URI or invents `ns1`, while `C14N` puts the
 * default declaration on it. Its grammar failures are the Invalid Character
 * Error rather than the Namespace Error (DOM_QN_MATTR), and an EMPTY-STRING
 * namespace is simply no namespace, both exactly as the element factory splits
 * them.
 */
DOM_METHOD(vm_builtin_DOMDocument_createAttributeNS)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zUri = DomArgStrOrNull(nArg,apArg,0);
	const char *zQname = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	int bModern = DomThisModern(pCtx);
	xmlNodePtr pRoot;
	xmlAttrPtr pAttr;
	dom_qname sQ;
	int rc;
	if( pNd == 0 ){
		return DomThrow(pCtx,DOM_ERR_NAMESPACE);
	}
	if( bModern && zUri != 0 && zUri[0] == 0 ){
		zUri = 0;
	}
	rc = DomQNameParse(zQname,zUri,bModern ? DOM_QN_MATTR : DOM_QN_ATTR,&sQ);
	if( rc ){
		return DomThrow(pCtx,rc);
	}
	if( bModern ){
		pAttr = xmlNewDocProp((xmlDocPtr)pNd->pNode,sQ.zLocal,0);
		if( pAttr == 0 ){
			DomQNameRelease(&sQ);
			return PH7_ContextMemoryError(pCtx);
		}
		if( zUri != 0 ){
			xmlNsPtr pNs = DomNsForCreateModernAttr((xmlNodePtr)pAttr,zUri,sQ.zPrefix);
			if( pNs == 0 ){
				DomQNameRelease(&sQ);
				xmlFreeProp(pAttr);   /* never handed out, never an orphan */
				return PH7_ContextMemoryError(pCtx);
			}
			xmlSetNs((xmlNodePtr)pAttr,pNs);
		}
		DomQNameRelease(&sQ);
		DomOrphanAdd(pNd->pShell,(xmlNodePtr)pAttr);
		return DomResultNodeOf(pCtx,pNd,(xmlNodePtr)pAttr);
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
		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,sQ.zPrefix,1,0));
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
		xmlSetNs((xmlNodePtr)pAttr,DomNsResolve(pRoot,zUri,0,1,0));
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
/*
 * ...on the 2004 tree. php 8.4's `getElementsByTagName` asks the QUALIFIED name
 * instead -- `a` finds only the unprefixed `a` there and `p:a` finds the
 * prefixed one, where the 2004 query answers all three for `a` and nothing at
 * all for `p:a`. Only the name-only query differs: `getElementsByTagNameNS`
 * takes a LOCAL name on both trees, and `*` is the wildcard on both (a `p:*`
 * is a name like any other and matches nothing).
 */
static int DomGebtnNameEq(xmlNodePtr pNode,const char *zName,int nName,int bQName)
{
	const xmlChar *zPrefix = (bQName && pNode->ns) ? pNode->ns->prefix : 0;
	int nPrefix;
	if( zPrefix == 0 ){
		return DomLenEq(pNode->name,zName,nName);
	}
	nPrefix = (int)SyStrlen((const char *)zPrefix);
	return nName > nPrefix + 1 && zName[nPrefix] == ':'
		&& SyMemcmp((const void *)zPrefix,(const void *)zName,(sxu32)nPrefix) == 0
		&& DomLenEq(pNode->name,zName+nPrefix+1,nName-nPrefix-1);
}
static int DomGebtnMatch(xmlNodePtr pNode,const char *zUri,int nUri,
	const char *zName,int nName,int bQName)
{
	if( pNode->type != XML_ELEMENT_NODE ){
		return 0;
	}
	if( !(nName == 1 && zName[0] == '*') && !DomGebtnNameEq(pNode,zName,nName,bQName) ){
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
	const char *zName,int nName,int bQName,int iWant,int *pnCount)
{
	xmlNodePtr pCur = pRoot ? pRoot->children : 0;
	int iCount = 0;
	while( pCur ){
		if( DomGebtnMatch(pCur,zUri,nUri,zName,nName,bQName) ){
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
	if( !DomDocFlag(pThis,DOM_F_PRESERVE_WS) ){
		iOpts |= XML_PARSE_NOBLANKS;
	}
	if( DomDocFlag(pThis,DOM_F_SUBST_ENT) ){
		iOpts |= XML_PARSE_NOENT;
	}
	if( DomDocFlag(pThis,DOM_F_VALIDATE) ){
		iOpts |= XML_PARSE_DTDVALID;
	}
	if( DomDocFlag(pThis,DOM_F_RESOLVE_EXT) ){
		iOpts |= XML_PARSE_DTDATTR;
	}
	if( DomDocFlag(pThis,DOM_F_RECOVER) ){
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
PH7_PRIVATE int PH7_DomReadFile(ph7_context *pCtx,const char *zFile,int nFile,const char *zFn,
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
	/* php hands its document loaders the context libxml_set_streams_context()
	 * left, which is what lets a `load('http://…')` carry a script's own headers
	 * and user agent. The slot was stored and answered and READ BY NOTHING until
	 * there was an http:// wrapper to read it; a file:// open ignores it exactly
	 * as php's does. */
	PH7_StreamCtxArm(pVm,PH7_StreamCtxFromValue(&pVm->sXmlStreamsCtx));
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
	if( !PH7_DomReadFile(pCtx,zFile,nFile,"DOMDocument::load",&sBody,&sPath) ){
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
/*
 * php 8.4's documents do not go through libxml's document saver: they run php's
 * own serializer, which writes the declaration and then each child back to
 * back. libxml's saver separates the document's top-level children with a
 * newline and ends the document with one, so the two disagree on every document
 * that has a prolog PI, a trailing comment, or nothing after its root at all.
 *
 * The declaration is still libxml's -- php gets it by emptying the document's
 * child list and asking the document saver for what is left, which is the
 * declaration and the newline behind it -- and so is every child's own dump.
 * Only the joins between them are php's, and a DTD is the one child it does
 * write a newline after.
 */
static int DomSaveModernDoc(xmlSaveCtxtPtr pSave,xmlBufferPtr pBuf,xmlDocPtr pDoc)
{
	xmlNodePtr pChild = pDoc->children;
	int rc;
	pDoc->children = 0;
	rc = xmlSaveDoc(pSave,pDoc);
	pDoc->children = pChild;
	if( rc < 0 ){
		return -1;
	}
	while( pChild ){
		if( xmlSaveTree(pSave,pChild) < 0 ){
			return -1;
		}
		if( pChild->type == XML_DTD_NODE ){
			/* Written past the save context, so it is flushed first: the
			 * newline is the join, not part of the DTD's own dump. */
			if( xmlSaveFlush(pSave) < 0
			 || xmlBufferAdd(pBuf,(const xmlChar *)"\n",1) != 0 ){
				return -1;
			}
		}
		pChild = pChild->next;
	}
	return 0;
}
/*
 * php 8.4's tree parks an attribute's binding on the DOCUMENT rather than
 * declaring it (DomNsForCreateModernAttr), so an attribute made by
 * `createAttributeNS`, or renamed into a URI, carries a prefix nothing on its
 * element declares. libxml's saver writes that prefix literally and declares
 * nothing, which is not namespace-well-formed -- our own parser refuses to read
 * the bytes back with `Namespace prefix p for foo on r is not defined`. php
 * mints the declaration as it writes, and the prefix it writes is the
 * serializer's choice rather than the attribute's:
 *
 *   an in-scope PREFIXED binding of the URI      -- write under that prefix
 *   else the attribute's own prefix, when this
 *     element does not itself declare it         -- declare it here
 *   else                                         -- declare `ns1`, `ns2`, ...
 *
 * A DEFAULT declaration is never reused and never satisfies one, on either
 * side of the test: an unprefixed attribute name is in no namespace whatever is
 * in scope, so an attribute in a URI the default binds still gets `ns1`. An
 * ancestor's binding of the prefix is SHADOWED rather than stepped over -- the
 * collision that forces `ns1` is one on this very element, which is exactly the
 * only collision `xmlNewNs` itself refuses.
 *
 * "In scope" is counted from the node being DUMPED, not from the root: a
 * `saveXml($node)` whose subtree binds nothing mints its own declaration even
 * when an ancestor of $node binds that URI, and the same document saved whole
 * writes the ancestor's prefix instead. That is why the search is bounded here
 * rather than left to xmlSearchNs.
 *
 * The tree itself does not change -- `$attr->prefix` reads back the prefix it
 * was made with whatever the bytes say -- so every retarget and every minted
 * declaration is undone once the bytes are out.
 */
typedef struct dom_ns_fix dom_ns_fix;
struct dom_ns_fix {
	xmlAttrPtr pAttr;    /* the attribute whose binding was retargeted */
	xmlNsPtr pOld;       /* what it was bound to before */
	xmlNodePtr pElem;    /* element a declaration was minted on, or 0 */
	xmlNsPtr pMint;      /* that declaration */
	dom_ns_fix *pNext;
};
/* A declaration of zPrefix in scope at pElem, looking no further out than
 * pStop (0 = as far as the tree goes). A prefix of 0 asks for the default. */
static xmlNsPtr DomNsScopePrefix(xmlNodePtr pElem,xmlNodePtr pStop,const xmlChar *zPrefix)
{
	xmlNodePtr pCur;
	for( pCur = pElem ; pCur && pCur->type == XML_ELEMENT_NODE ; pCur = pCur->parent ){
		xmlNsPtr pNs;
		for( pNs = pCur->nsDef ; pNs ; pNs = pNs->next ){
			if( zPrefix == 0 ? pNs->prefix == 0
			  : (pNs->prefix != 0 && xmlStrEqual(pNs->prefix,zPrefix)) ){
				return pNs;
			}
		}
		if( pCur == pStop ){
			break;
		}
	}
	return 0;
}
/* The nearest PREFIXED binding of zHref in that same scope -- the only kind an
 * attribute can be written under. */
static xmlNsPtr DomNsScopeHref(xmlNodePtr pElem,xmlNodePtr pStop,const xmlChar *zHref)
{
	xmlNodePtr pCur;
	for( pCur = pElem ; pCur && pCur->type == XML_ELEMENT_NODE ; pCur = pCur->parent ){
		xmlNsPtr pNs;
		for( pNs = pCur->nsDef ; pNs ; pNs = pNs->next ){
			if( pNs->prefix != 0 && pNs->href != 0 && xmlStrEqual(pNs->href,zHref) ){
				return pNs;
			}
		}
		if( pCur == pStop ){
			break;
		}
	}
	return 0;
}
static int DomNsFixRecord(dom_ns_fix **ppFix,xmlAttrPtr pAttr,xmlNsPtr pOld,
	xmlNodePtr pElem,xmlNsPtr pMint)
{
	dom_ns_fix *pRec = (dom_ns_fix *)xmlMalloc(sizeof(dom_ns_fix));
	if( pRec == 0 ){
		return -1;
	}
	pRec->pAttr = pAttr;
	pRec->pOld = pOld;
	pRec->pElem = pMint ? pElem : 0;
	pRec->pMint = pMint;
	pRec->pNext = *ppFix;
	*ppFix = pRec;
	return 0;
}
/* Give every attribute under pRoot a prefix the bytes will actually declare. */
static int DomNsSaveReconcile(xmlNodePtr pRoot,xmlNodePtr pStop,dom_ns_fix **ppFix)
{
	xmlNodePtr pCur = pRoot;
	while( pCur ){
		if( pCur->type == XML_ELEMENT_NODE ){
			xmlAttrPtr pAttr;
			for( pAttr = pCur->properties ; pAttr ; pAttr = pAttr->next ){
				xmlNsPtr pNs = pAttr->ns, pUse, pMint = 0;
				if( pNs == 0 || pNs->href == 0 ){
					continue;
				}
				if( pNs->prefix != 0 ){
					/* libxml writes pNs->prefix; when that prefix already binds
					 * this URI here the bytes are right as they stand. The `xml`
					 * prefix is bound everywhere and declared nowhere. */
					xmlNsPtr pHave = DomNsScopePrefix(pCur,pStop,pNs->prefix);
					if( pHave != 0 && xmlStrEqual(pHave->href,pNs->href) ){
						continue;
					}
					if( xmlStrEqual(pNs->prefix,(const xmlChar *)"xml") ){
						continue;
					}
				}
				pUse = DomNsScopeHref(pCur,pStop,pNs->href);
				if( pUse == 0 ){
					if( pNs->prefix != 0 ){
						/* Refused only by a declaration on THIS element, which
						 * is the collision php spells `ns1` for. */
						pMint = xmlNewNs(pCur,pNs->href,pNs->prefix);
						if( pMint ){
							DomNsMarkMinted(pMint);
						}
					}
					if( pMint == 0 ){
						pMint = DomNsGenerateEx(pCur,(const char *)pNs->href,
							(const xmlChar *)"ns",0,1);
					}
					if( pMint == 0 ){
						return -1;
					}
					pUse = pMint;
				}
				if( DomNsFixRecord(ppFix,pAttr,pNs,pCur,pMint) ){
					return -1;
				}
				pAttr->ns = pUse;
			}
		}
		/* Iterative: a document deep enough to matter must not cost stack. */
		if( pCur->children ){
			pCur = pCur->children;
			continue;
		}
		while( pCur != pRoot && pCur->next == 0 ){
			pCur = pCur->parent;
		}
		if( pCur == pRoot ){
			break;
		}
		pCur = pCur->next;
	}
	return 0;
}
/* Put every attribute back on the binding it was made with and drop the
 * declarations minted for the bytes -- retargets first, so nothing points at a
 * declaration by the time it is freed. */
static void DomNsSaveRestore(dom_ns_fix *pFix)
{
	dom_ns_fix *pRec;
	for( pRec = pFix ; pRec ; pRec = pRec->pNext ){
		pRec->pAttr->ns = pRec->pOld;
	}
	while( pFix ){
		dom_ns_fix *pNext = pFix->pNext;
		if( pFix->pMint ){
			xmlNsPtr *ppNs = &pFix->pElem->nsDef;
			while( *ppNs ){
				if( *ppNs == pFix->pMint ){
					*ppNs = pFix->pMint->next;
					xmlFreeNs(pFix->pMint);
					break;
				}
				ppNs = &(*ppNs)->next;
			}
		}
		xmlFree(pFix);
		pFix = pNext;
	}
}
static int DomDumpTree(xmlDocPtr pDoc,xmlNodePtr pNode,int bFormat,int iOpts,int bModern,
	xmlChar **pzOut)
{
	xmlBufferPtr pBuf = xmlBufferCreate();
	xmlSaveCtxtPtr pSave;
	dom_ns_fix *pFix = 0;
	int nOut = 0;
	*pzOut = 0;
	if( pBuf == 0 ){
		return -1;
	}
	if( bModern ){
		/* A document's children are each their own scope root; a node dump is
		 * scoped to the node, which is what makes those two disagree. */
		xmlNodePtr pTop = pNode ? pNode : (xmlNodePtr)pDoc->children;
		for( ; pTop ; pTop = pNode ? 0 : pTop->next ){
			if( DomNsSaveReconcile(pTop,pNode,&pFix) ){
				DomNsSaveRestore(pFix);
				xmlBufferFree(pBuf);
				return -1;
			}
		}
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
	if( pNode ){
		if( xmlSaveTree(pSave,pNode) < 0 ){
			nOut = -1;
		}
	}else if( bModern ){
		if( DomSaveModernDoc(pSave,pBuf,pDoc) < 0 ){
			nOut = -1;
		}
	}else if( xmlSaveDoc(pSave,pDoc) < 0 ){
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
	DomNsSaveRestore(pFix);
	xmlBufferFree(pBuf);
	return nOut;
}
/*
 * DOMDocument::saveXML(?DOMNode $node = null, int $options = 0): string|false
 *
 * php 8.4's namespaced document declares this one AGAIN under its own name
 * rather than inheriting it, so the bytes are shared and only the name a
 * libxml diagnostic carries is per-class; `zWho` is that name.
 */
static int DomSaveXml(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zWho)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pDocNd = DomThisNode(pCtx);
	phl_domnode *pTgt = (nArg > 0 && !ph7_value_is_null(apArg[0])) ? DomObjArg(apArg[0]) : 0;
	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	int bWhole;
	xmlChar *zOut = 0;
	int nOut;
	sxu32 nMark;
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pTgt && pTgt->pNode
	 && ((xmlNodePtr)pTgt->pNode)->doc != (xmlDocPtr)pDocNd->pNode ){
		/* Another document's node -- or a constructed one that belongs to none
		 * yet -- is not this document's to serialize. */
		return DomThrow(pCtx,DOM_ERR_WRONG_DOC);
	}
	bWhole = pTgt == 0 || pTgt->pNode == pDocNd->pNode;
	nMark = PH7_LibxmlCaptureBegin(pVm);
	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,bWhole ? 0 : (xmlNodePtr)pTgt->pNode,
		bFormat,iOpts,DomDocFlag(pThis,DOM_F_MODERN),&zOut);
	PH7_LibxmlCaptureEnd(pVm,nMark,zWho);
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
	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);
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
static int DomSaveXmlFile(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zWho)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pDocNd = DomThisNode(pCtx);
	const ph7_io_stream *pStream;
	const char *zFile;
	int nFile = 0;
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	int bFormat = DomDocFlag(pThis,DOM_F_FORMAT_OUTPUT);
	int nOut;
	xmlChar *zOut = 0;
	void *pHandle;
	sxu32 nMark;
	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";
	if( nFile != (int)SyStrlen(zFile) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($filename) must not contain any null bytes",zWho);
	}
	if( nFile < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($filename) must not be empty",zWho);
	}
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	nOut = DomDumpTree((xmlDocPtr)pDocNd->pNode,0,bFormat,iOpts & ~DOM_SAVE_NOXMLDECL,
		DomDocFlag(pThis,DOM_F_MODERN),&zOut);
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
DOM_METHOD(vm_builtin_DOMDocument_saveXML)
{
	return DomSaveXml(pCtx,nArg,apArg,"DOMDocument::saveXML");
}
DOM_METHOD(vm_builtin_DOMDocument_save)
{
	return DomSaveXmlFile(pCtx,nArg,apArg,"DOMDocument::save");
}
/* And php 8.4's pair, whose file writer takes the option word the 2004 one
 * takes and drops `LIBXML_NOXMLDECL` from it for the same reason: the option
 * is read on the way to a string only, so a saved document always carries its
 * declaration. */
DOM_METHOD(vm_builtin_DomXMLDocument_saveXml)
{
	return DomSaveXml(pCtx,nArg,apArg,"Dom\\XMLDocument::saveXml");
}
DOM_METHOD(vm_builtin_DomXMLDocument_saveXmlFile)
{
	return DomSaveXmlFile(pCtx,nArg,apArg,"Dom\\XMLDocument::saveXmlFile");
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
		/* An ABSENT data argument stays a NULL content pointer, matching php's
		 * node state: `<?bare?>` serializes with no separator space,
		 * `nodeValue` reads null -- and `data` reads "", because THAT getter
		 * coerces. An argument that was PASSED and is empty is a different
		 * node: php gives it an empty content string, `nodeValue` reads "" and
		 * the serializer writes the separator space (`<?bare ?>`). nVal is
		 * negative for the absent one; a length alone cannot tell them apart,
		 * and conflating them is what php's namespaced factory would have
		 * inherited on every call, its `$data` being required there. */
		pNode = xmlNewDocPI(pDoc,(const xmlChar *)zName,nVal >= 0 ? (const xmlChar *)zVal : 0);
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
 *   * On the 2004 door the declaration lands on the NEW element, always: a
 *     fresh node has no parent, so nothing the document declares elsewhere is
 *     in scope yet. What the document already makes is settled later, when the
 *     element is linked in and the redundant declaration is stripped
 *     (DomNsOnInsertEx). The namespaced one settles nothing later and declares
 *     no default at all (DomNsForCreateModern).
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
	/* php 8.4's factory differs from the 2004 one twice over: a grammar failure
	 * is the Invalid Character Error rather than the Namespace Error, and an
	 * EMPTY-STRING namespace is simply no namespace. The 2004 one keeps ''
	 * apart from null -- it declares `xmlns=""` on the element and answers ''
	 * for namespaceURI -- and the namespaced one answers null for both. */
	int bModern = DomThisModern(pCtx);
	xmlNodePtr pNode;
	dom_qname sQ;
	sxu32 nMark;
	int rc;
	if( pDocNd == 0 ){
		return DomThrow(pCtx,DOM_ERR_NAMESPACE);
	}
	if( bModern && zUri != 0 && zUri[0] == 0 ){
		zUri = 0;
	}
	rc = DomQNameParse(zQname,zUri,bModern ? DOM_QN_MELEM : DOM_QN_ELEM,&sQ);
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
		xmlNsPtr pNs = bModern ? DomNsForCreateModern(pNode,zUri,sQ.zPrefix)
		                       : DomNsForCreate(pNode,zUri,sQ.zPrefix);
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
	DomNsCopyMarks(pNode,pCopy);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::importNode");
	if( pCopy == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pCopy->type == XML_ATTRIBUTE_NODE && pNode->ns != 0 && pNode->ns->href != 0 ){
		xmlNodePtr pRoot = xmlDocGetRootElement(pDoc);
		xmlNsPtr pNs = pRoot
			? DomNsResolve(pRoot,(const char *)pNode->ns->href,pNode->ns->prefix,1,0) : 0;
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
 * Both entries are borrowed pointers, so the move is two edits and no
 * reference changes hands.
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
	pObj = (pHit && (pHit->iFlags & MEMOBJ_INT))
		? (ph7_class_instance *)(sxuptr)pHit->x.iVal : 0;
	if( pObj ){
		phl_domnode *pRes = DomResOf(pObj);
		ph7_value sVal;
		PH7_MemObjInitFromInt(&(*pVm),&sVal,(sxi64)(sxuptr)pObj);
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
		 * (A CONSTRUCTED node has no document and no dictionary at all, and
		 * xmlSetTreeDoc IS its whole move.)
		 */
		if( pNode->doc == 0 ){
			xmlSetTreeDoc(pNode,(xmlDocPtr)pDocNd->pNode);
		}else{
			sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);
			xmlDOMWrapAdoptNode(0,pNode->doc,pNode,(xmlDocPtr)pDocNd->pNode,0,0);
			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::adoptNode");
		}
		DomAdoptWrappers(pVm,pSrcDoc,DomThisDoc(pCtx),pDocNd->pShell,pNode);
	}
	DomOrphanAdd(pDocNd->pShell,pNode);
	ph7_result_value(pCtx,apArg[0]);
	return PH7_OK;
}
/*
 * DOMElement::insertAdjacentElement(string $where, DOMElement $element): ?DOMElement
 * DOMElement::insertAdjacentText(string $where, string $data): void
 *
 * php's dom_insert_adjacent, transcribed.  The WHERE word is matched
 * case-insensitively against the four positions and anything else is the
 * Syntax refusal (code 12, new to DomErrText) -- even on a receiver no
 * position could serve; beforebegin/afterend on a parentless receiver answer
 * null BEFORE anything moves; and then the argument is ADOPTED into this
 * document -- a node of another document is MOVED here, wrappers and all,
 * where every other insertion method refuses it with Wrong Document Error.
 * Only then does the pre-insertion validity run, so a refusal (the receiver
 * inside the argument) leaves the adopted argument DETACHED --
 * `$in->insertAdjacentElement('afterbegin',$host)` costs the tree the whole
 * host subtree, php's own answer -- and the insertion point is read AFTER the
 * adopt unlinked the argument, which is what makes inserting one's own next
 * sibling `afterend` a no-op rather than a swap.
 *
 * One deliberate divergence: php SEGFAULTS on
 * `$a->insertAdjacentElement('beforebegin',$a)` -- its adopt unlinks the
 * receiver and the insertion then walks a NULL parent.  PHL answers the
 * Hierarchy refusal its validity was about to reach.
 */
static int DomInsertAdjacentOp(ph7_context *pCtx,phl_domnode *pRecv,const char *zWhere,
	phl_xmldoc *pArgShell,xmlNodePtr pOther,ph7_class_instance *pArgDoc)
{
	ph7_vm *pVm = pCtx->pVm;
	xmlNodePtr pThis = (xmlNodePtr)pRecv->pNode;
	xmlNodePtr pParent,pRef;
	int iPos,iErr;
	if( DomNameIsCi(zWhere,"beforebegin") ){
		iPos = 0;
	}else if( DomNameIsCi(zWhere,"afterbegin") ){
		iPos = 1;
	}else if( DomNameIsCi(zWhere,"beforeend") ){
		iPos = 2;
	}else if( DomNameIsCi(zWhere,"afterend") ){
		iPos = 3;
	}else{
		DomThrowVoid(pCtx,DOM_ERR_SYNTAX);
		return -1;
	}
	if( (iPos == 0 || iPos == 3) && pThis->parent == 0 ){
		return 1;   /* the null answer, nothing moved */
	}
	/* The adopt: detach, re-home across documents (adoptNode's machinery),
	 * and park until linked. A document-less argument -- a constructed node --
	 * has no dict-interned strings to move, so xmlSetTreeDoc is its whole
	 * move; the wrappers cross either way. */
	DomDetach(pArgShell,pOther);
	if( pOther->doc != pThis->doc ){
		if( pOther->doc == 0 ){
			xmlSetTreeDoc(pOther,pThis->doc);
		}else{
			sxu32 nMark = PH7_LibxmlCaptureBegin(pVm);
			xmlDOMWrapAdoptNode(0,pOther->doc,pOther,pThis->doc,0,0);
			PH7_LibxmlCaptureEnd(pVm,nMark,"DOMElement::insertAdjacentElement");
		}
		DomAdoptWrappers(pVm,pArgDoc,DomThisDoc(pCtx),pRecv->pShell,pOther);
	}
	DomOrphanAdd(pRecv->pShell,pOther);
	switch( iPos ){
	case 0:  pParent = pThis->parent;  pRef = pThis;            break;
	case 1:  pParent = pThis;          pRef = pThis->children;  break;
	case 2:  pParent = pThis;          pRef = 0;                break;
	default: pParent = pThis->parent;  pRef = pThis->next;      break;
	}
	if( pParent == 0 ){
		/* The argument WAS the receiver: adopting it took the parent away. */
		DomThrowVoid(pCtx,DOM_ERR_HIERARCHY);
		return -1;
	}
	iErr = DomInsertValidity(pParent,pOther,0);
	if( iErr ){
		DomThrowVoid(pCtx,iErr);
		return -1;
	}
	if( pRef == pOther ){
		pRef = pOther->next;
	}
	DomDetach(pRecv->pShell,pOther);
	if( pRef ){
		DomLinkBefore(pParent,pOther,pRef);
	}else{
		DomLinkLast(pParent,pOther);
	}
	DomNsOnInsertEx(pOther,0);
	return 0;
}
DOM_METHOD(vm_builtin_DOMElement_insertAdjacentElement)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	phl_domnode *pOther = nArg > 1 ? DomObjArg(apArg[1]) : 0;
	const char *zWhere = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	if( pNd == 0 || pOther == 0 ){
		return PH7_OK;
	}
	if( DomInsertAdjacentOp(pCtx,pNd,zWhere,pOther->pShell,(xmlNodePtr)pOther->pNode,
		DomObjArgDoc(apArg[1])) == 0 ){
		/* The answer is the argument itself, now linked. */
		ph7_result_value(pCtx,apArg[1]);
	}
	return PH7_OK;
}
DOM_METHOD(vm_builtin_DOMElement_insertAdjacentText)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	const char *zWhere = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	const char *zData;
	int nData = 0;
	xmlNodePtr pText;
	if( pNd == 0 ){
		return PH7_OK;
	}
	zData = nArg > 1 ? ph7_value_to_string(apArg[1],&nData) : "";
	pText = xmlNewDocTextLen(((xmlNodePtr)pNd->pNode)->doc,(const xmlChar *)zData,nData);
	if( pText == 0 ){
		return PH7_OK;
	}
	/* Park it FIRST. The op's two earliest refusals -- the Syntax word and
	 * the parentless beforebegin/afterend null -- return before its own
	 * DomOrphanAdd runs, and an unparked fresh node outlives every owner
	 * (the leak checker is what noticed). Parking is idempotent, the op's
	 * detach removes exactly one entry, and the linked node ends OFF the
	 * orphan list -- so the early paths leave it parked in the shell where
	 * teardown frees it, unobservable, which is php's answer. */
	DomOrphanAdd(pNd->pShell,pText);
	DomInsertAdjacentOp(pCtx,pNd,zWhere,pNd->pShell,pText,0);
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
	int nVal = -1;   /* the sentinel for "no data argument came at all" */
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
/*
 * php 8.4's `createCDATASection` screens the one sequence a CDATA section
 * cannot contain, and says which one it was rather than raising the bare
 * sentence. The 2004 factory takes `]]>` without a word and writes a document
 * that will not parse back, so the screen is the namespaced tree's alone.
 */
DOM_METHOD(vm_builtin_DOMDocument_createCDATASection)
{
	if( DomThisModern(pCtx) && nArg > 0 ){
		int nData = 0;
		const char *zData = ph7_value_to_string(apArg[0],&nData);
		int i;
		for( i = 0 ; i + 2 < nData ; ++i ){
			if( zData[i] == ']' && zData[i+1] == ']' && zData[i+2] == '>' ){
				return DomThrowSentence(pCtx,DOM_ERR_INVALID_CHAR,
					"Invalid character sequence \"]]>\" in CDATA section");
			}
		}
	}
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
 *
 * php 8.4's namespaced tree drops the negative screen and takes the standard's
 * UNSIGNED offset and count instead: both are read as 32-bit quantities, so a
 * negative is not refused, it IS that bit pattern. `substringData(0,-1)` is the
 * whole rest of the string there and an Index Size Error here, and
 * `substringData(0,-4294967295)` is one character rather than the rest, because
 * the low 32 bits of that count are 1. The screen still runs on the SIGNED
 * value first, which is why a positive count above SXI32_HIGH is refused under
 * both names while a negative one of far greater magnitude is not. The offset
 * follows whichever bound its method already picks: the two that compare it
 * unsigned read it unsigned, and the two that compare it signed keep php's
 * refusal there too -- `deleteData(-1,1)` is an Index Size Error in both trees.
 */
static int DomCharRange(ph7_context *pCtx,xmlNodePtr pNode,ph7_int64 *piOffset,
	ph7_int64 *piCount,int bHasCount,int bUnsignedBound,int bModern,int *pnLen,int *pRc)
{
	ph7_int64 iOffset = *piOffset;
	ph7_int64 iCount = *piCount;
	int nLen = DomCharLength(pNode);
	int bPastEnd = bUnsignedBound ? (sxu32)iOffset > (sxu32)nLen
	                              : iOffset > (ph7_int64)nLen;
	*pnLen = nLen;
	if( (iOffset < 0 && !(bModern && bUnsignedBound))
	 || (bHasCount && iCount < 0 && !bModern)
	 || iOffset > (ph7_int64)SXI32_HIGH || iCount > (ph7_int64)SXI32_HIGH
	 || bPastEnd ){
		*pRc = DomThrow(pCtx,DOM_ERR_INDEX_SIZE);
		return -1;
	}
	if( bModern ){
		/* Past the screen, hand the caller back the unsigned reading so the
		 * clamp and the cut below stay plain non-negative arithmetic. */
		if( iOffset < 0 ){
			*piOffset = (ph7_int64)(sxu32)iOffset;
		}
		if( bHasCount && iCount < 0 ){
			*piCount = (ph7_int64)(sxu32)iCount;
		}
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
	if( pNode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( DomCharRange(pCtx,pNode,&iOffset,&iCount,TRUE,TRUE,DomThisModern(pCtx),
		&nLen,&rc) != 0 ){
		return rc;
	}
	if( pNode->content == 0 ){
		/* php reads a NULL content pointer -- the omitted-argument
		 * constructor's node -- as "": the range still screens (so an offset
		 * past zero is Index Size), and what is left of nothing is "". */
		ph7_result_string(pCtx,"",0);
		return PH7_OK;
	}
	/* The LIMIT keeps its unsigned reading -- a -1 length is malformed content
	 * and means "no limit", which is what lets three of these five answer on such
	 * a node at all -- but the SUM is compared in full rather than at 32 bits: an
	 * unsigned count of 4294967295 at offset 10 wraps to 9 there, and a cut that
	 * runs off the end reads as one that is already inside the string. */
	if( iOffset + iCount > (ph7_int64)(sxu32)nLen ){
		iCount = (ph7_int64)(sxu32)nLen - iOffset;
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
	if( !DomThisModern(pCtx) ){
		/* `Dom\\CharacterData::appendData()` is declared `void`; a native body
		 * that sets a result keeps it, so the true is the 2004 name's alone. */
		ph7_result_bool(pCtx,1);
	}
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
	if( pNode == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pNode->content == 0 ){
		/* A writer normalizes the omitted-argument constructor's NULL content
		 * to "" and proceeds, php's own answer: `insertData(0,'i')` on a
		 * `new DOMComment()` writes "i", and its nodeValue reads "" after. */
		xmlNodeSetContent(pNode,(const xmlChar *)"");
	}
	/* insertData has no count and takes the unsigned bound; the two that DO
	 * take one take the signed bound. */
	if( DomCharRange(pCtx,pNode,&iOffset,&iCount,bHasCount,!bHasCount,
		DomThisModern(pCtx),&nLen,&rc) != 0 ){
		return rc;
	}
	if( iOffset + iCount > (ph7_int64)(sxu32)nLen ){
		iCount = (ph7_int64)(sxu32)nLen - iOffset;
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
	if( !DomThisModern(pCtx) ){
		ph7_result_bool(pCtx,1);
	}
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
 *
 * php 8.4's `Dom\Text::splitText(int $offset): Dom\Text` returns a node or
 * nothing, so its past-end answer cannot be `false`: it is an Index Size Error
 * there. The negative one stays a ValueError in both trees, and the sentence
 * names the class that DECLARED the method rather than the receiver's -- a
 * `Dom\CDATASection` is told about `Dom\Text::splitText()` and a
 * DOMCdataSection about `DOMText::splitText()`.
 */
DOM_METHOD(vm_builtin_DOMText_splitText)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	ph7_int64 iOffset = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	int bModern = DomThisModern(pCtx);
	xmlChar *zHead,*zTail;
	xmlNodePtr pNew;
	int nLen;
	if( iOffset < 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			bModern
			? "Dom\\Text::splitText(): Argument #1 ($offset) must be greater "
			  "than or equal to 0"
			: "DOMText::splitText(): Argument #1 ($offset) must be greater "
			  "than or equal to 0");
	}
	if( pNode == 0
	 || (pNode->type != XML_TEXT_NODE && pNode->type != XML_CDATA_SECTION_NODE) ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	if( pNode->content == 0 ){
		/* The omitted-argument constructor's node splits as "": both halves
		 * empty, php's answer. The split WRITES, so normalizing is its own. */
		xmlNodeSetContent(pNode,(const xmlChar *)"");
	}
	nLen = DomCharLength(pNode);
	if( iOffset > (ph7_int64)nLen ){
		if( bModern ){
			return DomThrow(pCtx,DOM_ERR_INDEX_SIZE);
		}
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
 * Is the node reachable from its document, rather than merely made by one?
 *
 * php's namespaced tree asks this before it canonicalizes anything, and "made
 * by" is not enough: a fresh node, a node lifted back out with removeChild, and
 * a whole subtree hanging off a DocumentFragment all carry a `doc` pointer and
 * none of them is in the document. Walking the parents to the document node is
 * the same question libxml's own node set would answer with "visible from
 * nowhere", only asked before the walk instead of after it.
 */
static int DomNodeIsAttached(xmlNodePtr pNode)
{
	xmlNodePtr p;
	for( p = pNode ; p ; p = p->parent ){
		if( p->type == XML_DOCUMENT_NODE || p->type == XML_HTML_DOCUMENT_NODE ){
			return 1;
		}
	}
	return 0;
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
	char zGiven[64];
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
	if( pNode == 0 ){
		nOut = -1;
		goto done;
	}
	/* php 8.4's tree refuses a detached node outright where the 2004 door
	 * canonicalizes it to the empty string. The refusal comes FIRST, ahead of
	 * every argument: a bad `$xpath` query and an inclusive-mode `$nsPrefixes`
	 * list both reach it without their own complaint in front. */
	if( DomThisModern(pCtx) && !DomNodeIsAttached(pNode) ){
		*pRc = DomThrowSentence(pCtx,DOM_ERR_HIERARCHY,
			"Canonicalization can only happen on nodes attached to a document.");
		nOut = -1;
		goto done;
	}
	if( pNode->doc == 0 ){
		/* php's plain Error, no DOM code: canonicalization asks libxml for the
		 * document's context, and a constructed node has none. */
		*pRc = PH7_VmThrowException(pCtx,"Error","Node must be associated with a document");
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
				zFn,iXPathPos,VmValueGivenName(pQuery,zGiven,sizeof(zGiven)));
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
	/* Canonicalized into an output buffer of our own rather than through
	 * xmlC14NDocDumpMemory, which is php's shape and one diagnostic quieter:
	 * that wrapper adds an "Internal error : saving doc to output buffer" of
	 * its own on top of libxml's real complaint, and php -- which drives the
	 * save itself -- never prints it. */
	{
		xmlBufferPtr pBuf = xmlBufferCreate();
		xmlOutputBufferPtr pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;
		xmlNodePtr pRoot = xmlDocGetRootElement(pNode->doc);
		DomNsHold sDefaults;
		DomNsHoldInit(&sDefaults);
		if( pOut == 0 ){
			if( pBuf ){
				xmlBufferFree(pBuf);
			}
			nOut = -1;
			goto done;
		}
		/* Only php's namespaced tree resolves the default namespace this way;
		 * the 2004 one canonicalizes the declarations exactly as they stand. */
		if( DomThisModern(pCtx) && pRoot ){
			DomC14NHoldDefaults(&sDefaults,pRoot);
		}
		nMark = PH7_LibxmlCaptureBegin(pVm);
		nOut = xmlC14NDocSaveTo(pNode->doc,pSet,
			bExclusive ? XML_C14N_EXCLUSIVE_1_0 : XML_C14N_1_0,
			sPrefixes.apPrefix,bComments,pOut);
		PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
		DomNsReleaseParked(&sDefaults);
		xmlOutputBufferFlush(pOut);
		if( nOut >= 0 ){
			const xmlChar *zBuf = xmlBufferContent(pBuf);
			nOut = (int)xmlBufferLength(pBuf);
			/* An EMPTY canonicalization is a real answer -- a detached node
			 * is visible from nowhere in the document -- so the bytes are
			 * always allocated, even when there are none. */
			*pzOut = xmlStrndup(zBuf ? zBuf : (const xmlChar *)"",nOut);
			if( *pzOut == 0 ){
				nOut = -1;
			}
		}
		xmlOutputBufferClose(pOut);
		xmlBufferFree(pBuf);
	}
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
 * FALSE when the canonicalization fails, which is the answer a signer has to
 * be able to tell from a document that canonicalizes to nothing: a detached
 * node and a fragment are both the EMPTY STRING (nothing of either is visible
 * from the document, and that is a real answer), while an entity REFERENCE
 * anywhere in the tree -- an ordinary document parsed without
 * `substituteEntities` -- is a refusal libxml states and php reports as false.
 * Answering "" for both signed the empty string instead of failing.
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
		ph7_result_bool(pCtx,0);
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
		"are implemented in a subclass",&pThis->pClass->sDisp);
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
		"are implemented in a subclass",&pThis->pClass->sDisp);
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
/* ...and a NAMED map is one of two: an element's attribute list, or one of the
 * two DTD declaration TABLES, which are libxml hash tables rather than node
 * lists -- the reason a parameter entity, which is a child of the DTD like
 * every other declaration, is not in `entities`. */
#define DNL_ENTS  4   /* $doctype->entities */
#define DNL_NOTS  5   /* $doctype->notations */
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
	DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,
		nUri < 0 && DomDocFlag(PH7_NativeAttrObj(pList,DOM_DOC),DOM_F_MODERN),-1,&iCount);
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
		if( pHit && (pHit->iFlags & MEMOBJ_OBJ) ){
			/* A namespace:: axis entry holds the DOMNameSpaceNode ITSELF (a
			 * fresh object per query, one object per list -- php's answer);
			 * the snapshot owns it, so this one is borrowed and referenced
			 * here to answer with the same contract DomWrap's does. */
			ph7_class_instance *pNs = (ph7_class_instance *)pHit->x.pOther;
			pNs->iRef++;
			return pNs;
		}
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
		pNode = DomGebtnWalk((xmlNodePtr)pOwner->pNode,zUri,nUri,zName,nName,
			nUri < 0 && DomDocFlag(pDoc,DOM_F_MODERN),iIndex,0);
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
/*
 * The index both collections take, screened before it is narrowed.
 *
 * php's is a `int` position in a list that cannot hold more than INT_MAX
 * entries, so everything outside [0, INT_MAX] is out of range -- and the two
 * classes then disagree about what to DO with one: the list answers null and
 * the named map raises a ValueError naming the bound.  Narrowing first was a
 * silent wrong answer either way: `item(4294967296)` and `item(PHP_INT_MIN)`
 * truncate to 0 and answered the FIRST node of the collection.
 *
 * Answers 1 when the index is usable.
 */
#define DOM_INDEX_MAX 2147483647
static int DomCollectionIndex(int nArg,ph7_value **apArg,int *piIndex)
{
	ph7_int64 iWant = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	*piIndex = 0;
	if( iWant < 0 || iWant > DOM_INDEX_MAX ){
		return 0;
	}
	*piIndex = (int)iWant;
	return 1;
}
DOM_METHOD(vm_builtin_DOMNodeList_item)
{
	int iIndex;
	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultOwned(pCtx,DomListItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));
}
/* php exposes `length` on both collections as a virtual property; the
 * property HOOK below turns each recognizer into php's read and has handlers. */
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
/* Is this map one of the DTD DECLARATION tables rather than an element's
 * attribute list? The two are walked with entirely different machinery. */
/* Defined with the by-name lookup below: a collection has no receiver to read
 * the family off, so it reads the document it was made against. */
static int DomMapModern(ph7_class_instance *pMap);
static int DomMapIsTable(ph7_class_instance *pMap)
{
	sxi64 iKind = pMap ? PH7_NativeAttrInt(pMap,DNL_KIND) : (sxi64)DNL_CHILD;
	return iKind == DNL_ENTS || iKind == DNL_NOTS;
}
/*
 * The table itself, which is NULL for a doctype that declares nothing of that
 * kind: libxml allocates the hash only when the first declaration arrives, so
 * an absent table is an EMPTY map and not an error.
 */
static xmlHashTablePtr DomMapHash(ph7_class_instance *pMap,phl_domnode *pOwner)
{
	xmlDtdPtr pDtd = pOwner ? (xmlDtdPtr)pOwner->pNode : 0;
	if( !DomMapIsTable(pMap) || pDtd == 0
	 || (pDtd->type != XML_DTD_NODE && pDtd->type != XML_DOCUMENT_TYPE_NODE) ){
		return 0;
	}
	return (xmlHashTablePtr)(PH7_NativeAttrInt(pMap,DNL_KIND) == DNL_NOTS
		? pDtd->notations : pDtd->entities);
}
/*
 * A NOTATION declaration answered as a node.
 *
 * libxml's xmlNotation is `{name, PublicID, SystemID}` -- three strings and no
 * type field -- so it cannot be handed to anything that walks a node.  php
 * builds an entity-shaped stand-in around it (the two structs share their
 * header, which is why the same reader answers both) with NO document and NO
 * parent, and that absence is php-visible: a notation's `ownerDocument` is
 * null, its `isConnected` false and its `getRootNode()` itself.
 *
 * php builds a FRESH one per lookup and frees it with the object; PHL builds
 * one per declaration and keeps it on the document's shell, so the wrapper
 * identity every other node has holds here too (recorded: `$map->item(0) ===
 * $map->item(0)` is true here and false there).
 */
static xmlNodePtr DomNotationNode(phl_xmldoc *pShell,xmlNotationPtr pNot)
{
	xmlEntityPtr *apHave;
	xmlEntityPtr pNode;
	sxu32 n;
	if( pShell == 0 || pNot == 0 ){
		return 0;
	}
	apHave = (xmlEntityPtr *)SySetBasePtr(&pShell->aNotations);
	for( n = 0 ; n < SySetUsed(&pShell->aNotations) ; ++n ){
		if( apHave[n]->_private == (void *)pNot ){
			return (xmlNodePtr)apHave[n];
		}
	}
	pNode = (xmlEntityPtr)xmlMalloc(sizeof(xmlEntity));
	if( pNode == 0 ){
		return 0;
	}
	SyZero(pNode,sizeof(xmlEntity));
	pNode->type = XML_NOTATION_NODE;
	pNode->name = xmlStrdup(pNot->name);
	pNode->ExternalID = xmlStrdup(pNot->PublicID);
	pNode->SystemID = xmlStrdup(pNot->SystemID);
	/* The declaration this stands for, so a second lookup finds it again. */
	pNode->_private = (void *)pNot;
	if( SySetPut(&pShell->aNotations,(const void *)&pNode) != SXRET_OK ){
		if( pNode->name ){
			xmlFree((xmlChar *)pNode->name);
		}
		if( pNode->ExternalID ){
			xmlFree((xmlChar *)pNode->ExternalID);
		}
		if( pNode->SystemID ){
			xmlFree((xmlChar *)pNode->SystemID);
		}
		xmlFree(pNode);
		return 0;
	}
	return (xmlNodePtr)pNode;
}
/* The payload a table map hands back, as a node: an entity declaration IS one,
 * a notation declaration needs its stand-in. */
static xmlNodePtr DomTablePayload(ph7_class_instance *pMap,phl_domnode *pOwner,void *pPayload)
{
	if( pPayload == 0 ){
		return 0;
	}
	if( PH7_NativeAttrInt(pMap,DNL_KIND) == DNL_NOTS ){
		return DomNotationNode(pOwner->pShell,(xmlNotationPtr)pPayload);
	}
	return (xmlNodePtr)pPayload;
}
/*
 * php walks these tables with xmlHashScan and takes the n-th thing it is
 * handed, so the ORDER a map answers in is the hash's and not the document's.
 * The same walk, so the same order.
 */
typedef struct DomHashPick DomHashPick;
struct DomHashPick {
	int iWant;              /* index still to be stepped over */
	void *pHit;             /* the payload at index 0 of what is left */
};
static void DomHashPickOne(void *pPayload,void *pData,const xmlChar *zName)
{
	DomHashPick *pPick = (DomHashPick *)pData;
	SXUNUSED(zName);
	if( pPick->iWant > 0 ){
		pPick->iWant--;
	}else if( pPick->pHit == 0 ){
		pPick->pHit = pPayload;
	}
}
static void * DomHashAt(xmlHashTablePtr pTab,int iIndex)
{
	DomHashPick sPick;
	if( pTab == 0 || iIndex < 0 || iIndex >= xmlHashSize(pTab) ){
		return 0;
	}
	sPick.iWant = iIndex;
	sPick.pHit = 0;
	xmlHashScan(pTab,DomHashPickOne,&sPick);
	return sPick.pHit;
}
static ph7_class_instance * DomMapItem(ph7_vm *pVm,ph7_class_instance *pMap,int iIndex)
{
	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;
	xmlNodePtr pNode;
	if( pOwner == 0 || iIndex < 0 ){
		return 0;
	}
	if( DomMapIsTable(pMap) ){
		pNode = DomTablePayload(pMap,pOwner,DomHashAt(DomMapHash(pMap,pOwner),iIndex));
	}else{
		/* php 8.4's map answers the element's declarations and its attributes
		 * as one chain, each declaration where the element makes it; the 2004
		 * map lists no declaration at all, so it is libxml's list alone. */
		pNode = DomMapModern(pMap)
			? DomMapNodeAt(pOwner->pShell,(xmlNodePtr)pOwner->pNode,iIndex)
			: (xmlNodePtr)DomAttrAt((xmlNodePtr)pOwner->pNode,iIndex);
	}
	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,pNode);
}
static int DomMapCount(ph7_class_instance *pMap)
{
	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;
	if( DomMapIsTable(pMap) ){
		xmlHashTablePtr pTab = DomMapHash(pMap,pOwner);
		return pTab ? xmlHashSize(pTab) : 0;
	}
	if( pOwner == 0 ){
		return 0;
	}
	return DomAttrCount((xmlNodePtr)pOwner->pNode)
		+ (DomMapModern(pMap) ? DomNsMapCount((xmlNodePtr)pOwner->pNode) : 0);
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
	int iIndex;
	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){
		/* The map REFUSES what the list answers null for, and names the bound. */
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and %d",
			DOM_INDEX_MAX);
	}
	return DomResultOwned(pCtx,DomMapItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));
}
/*
 * The by-NAME lookup, which `getNamedItem()` and the `$map['href']` subscript
 * share.
 *
 * The MAP asks libxml's name-only question, where DOMElement's own
 * getAttributeNode resolves the prefix: `getNamedItem('k')` finds the
 * namespaced `p:k` that `getAttribute('k')` does not. A DECLARATION table is
 * keyed by that name to begin with, so it is one lookup.
 */
/* A collection is not a node, so it has no receiver to read the family off:
 * the tree comes from the document it was made against, the way every other
 * live view reads its own. */
static int DomMapModern(ph7_class_instance *pMap)
{
	return pMap != 0 && DomDocFlag(PH7_NativeAttrObj(pMap,DOM_DOC),DOM_F_MODERN);
}
static ph7_class_instance * DomMapNamed(ph7_vm *pVm,ph7_class_instance *pMap,const char *zName)
{
	phl_domnode *pOwner = pMap ? DomListOwner(pMap) : 0;
	xmlNodePtr pHit;
	if( pOwner == 0 ){
		return 0;
	}
	if( DomMapIsTable(pMap) ){
		pHit = DomTablePayload(pMap,pOwner,
			xmlHashLookup(DomMapHash(pMap,pOwner),(const xmlChar *)zName));
	}else if( DomMapModern(pMap) ){
		/* A declaration is in this map under the name php spells it with, and
		 * ahead of the attributes -- so it is asked first. */
		xmlNsPtr pDecl = DomNsMapSpelt(DomNsDeclByName((xmlNodePtr)pOwner->pNode,zName));
		pHit = pDecl
			? (xmlNodePtr)DomNsAttr(pOwner->pShell,(xmlNodePtr)pOwner->pNode,pDecl)
			: (xmlNodePtr)DomAttrBySpec((xmlNodePtr)pOwner->pNode,zName);
	}else{
		pHit = (xmlNodePtr)DomAttrByLocal((xmlNodePtr)pOwner->pNode,zName);
	}
	if( pHit == 0 ){
		return 0;
	}
	return DomWrap(&(*pVm),PH7_NativeAttrObj(pMap,DOM_DOC),pOwner->pShell,pHit);
}
/* DOMNamedNodeMap::getNamedItem(string $qualifiedName): ?DOMAttr */
DOM_METHOD(vm_builtin_DOMNamedNodeMap_getNamedItem)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	return DomResultOwned(pCtx,DomMapNamed(pCtx->pVm,pThis,zName));
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
	 * An EMPTY namespace is neither -- it matches a URI no document has.
	 * A DECLARATION table has no namespaces at all and php reads right past
	 * the argument there: the URI decides nothing, the name decides
	 * everything. */
	xmlNodePtr pHit;
	if( pOwner == 0 ){
		pHit = 0;
	}else if( DomMapIsTable(pThis) ){
		pHit = DomTablePayload(pThis,pOwner,
			xmlHashLookup(DomMapHash(pThis,pOwner),(const xmlChar *)zLocal));
	}else if( zUri && DomMapModern(pThis) && DomUriIs((const char *)zUri,DOM_XMLNS_NS_URI) ){
		/* In php 8.4's map the xmlns namespace names the DECLARATIONS, and the
		 * local name is the prefix each declares -- `xmlns` naming the default
		 * one, which the element's own getAttributeNodeNS cannot reach in the
		 * 2004 tree. */
		xmlNsPtr pDecl = DomNsMapSpelt(DomNsDeclByLocal((xmlNodePtr)pOwner->pNode,zLocal));
		pHit = pDecl
			? (xmlNodePtr)DomNsAttr(pOwner->pShell,(xmlNodePtr)pOwner->pNode,pDecl)
			: 0;
	}else if( zUri ){
		pHit = (xmlNodePtr)DomAttrByNs((xmlNodePtr)pOwner->pNode,zUri,zLocal);
	}else if( DomMapModern(pThis) ){
		/* The map's ANY, which reads the whole QUALIFIED name -- so a
		 * declaration answers here under `xmlns:p` as it does by name. */
		xmlNsPtr pDecl = DomNsMapSpelt(DomNsDeclByName((xmlNodePtr)pOwner->pNode,zLocal));
		pHit = pDecl
			? (xmlNodePtr)DomNsAttr(pOwner->pShell,(xmlNodePtr)pOwner->pNode,pDecl)
			: (xmlNodePtr)DomAttrBySpec((xmlNodePtr)pOwner->pNode,zLocal);
	}else{
		pHit = (xmlNodePtr)DomAttrByLocal((xmlNodePtr)pOwner->pNode,zLocal);
	}
	if( pHit == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return DomResultOwned(pCtx,DomWrap(pCtx->pVm,PH7_NativeAttrObj(pThis,DOM_DOC),
		pOwner->pShell,pHit));
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
 * `$list[0]`, `$map['href']` and `isset($list[0])` -- ph7_class::xDim for the
 * two collections, which are php's read_dimension / has_dimension handlers.
 *
 * php 8.3 gave both classes those handlers WITHOUT declaring ArrayAccess, so
 * `$list instanceof ArrayAccess` is FALSE there and the subscript reads anyway
 * -- which is how every modern DOM example is written, and which no interface
 * list can state. Only the READ half exists: a store, an append and an unset
 * are all `Cannot use object of type C as array`, which is what the opcode
 * answers for a class carrying no ArrayAccess.
 *
 * What an offset MEANS is one rule for both classes, and it is not the array
 * one. A STRING that STARTS with a number is an INDEX -- `"1x"` is 1 and
 * `" 2 "` is 2, silently, php's is_numeric_string with errors allowed -- and
 * one that does not is a NAME, which the list has no door for at all (so
 * `$list['x']` is null even on a document whose child element is named x).
 * Every other offset type takes the ordinary int cast, warning exactly where
 * php's `(int)` warns (`$map[new stdClass]` is index 1 and a warning).
 *
 * Then the two classes DISAGREE about an index outside [0, INT_MAX]: the list
 * answers null and the map raises `item()`'s own ValueError -- worded the way
 * php words it with no function frame active to name the argument, so the
 * `DOMNamedNodeMap::item(): Argument #1 ($index) ` head is not there. isset()
 * never refuses: php asks has_dimension, and that one answers a plain false.
 */
/*
 * Classify one offset. Answers 1 for a NAME (pScratch holds the string), 0 for
 * an INDEX in *piIndex. Works on a COPY: the conversion is destructive, the
 * caller's offset must survive it (empty() asks the same offset twice), and
 * php's own `$map[$k]` leaves $k alone. The caller releases pScratch.
 */
static int DomDimClassify(ph7_vm *pVm,ph7_value *pOffset,ph7_value *pScratch,sxi64 *piIndex)
{
	PH7_MemObjInit(&(*pVm),pScratch);
	PH7_MemObjStore(pOffset,pScratch);
	*piIndex = 0;
	if( (pScratch->iFlags & MEMOBJ_STRING) && !PH7_MemObjStringNumericPrefix(pScratch,0) ){
		return 1; /* a NAME: the caller reads the bytes out of the copy */
	}
	/* php reads an int out of every other offset type -- null is 0, a bool its
	 * value, a float truncated, an array 0 or 1 by emptiness, an object 1 behind
	 * `could not be converted to int`. The cast is what hands back the reference
	 * an object / array copy took (MemObjIntValue unrefs before it overwrites the
	 * type in place), so the release below has only a string blob left to free. */
	PH7_MemObjWarnIntCast(pScratch);
	*piIndex = ph7_value_to_int64(pScratch);
	return 0;
}
/* Hand the hook's answer back: the node, or nothing at all for a miss (which
 * the caller initialized NULL, and which isset() reads as false). Consumes the
 * reference DomWrap handed the caller. */
static void DomDimAnswer(ph7_vm *pVm,PH7_NativeDimCtx *pCtx,ph7_class_instance *pHit)
{
	ph7_value sVal;
	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){
		pCtx->pResult->x.iVal = pHit ? 1 : 0;
		MemObjSetType(pCtx->pResult,MEMOBJ_BOOL);
		if( pHit ){
			PH7_ClassInstanceUnref(pHit);
		}
		return;
	}
	if( pHit == 0 ){
		return;
	}
	PH7_MemObjInit(&(*pVm),&sVal);
	sVal.x.pOther = pHit;
	sVal.iFlags = MEMOBJ_OBJ;
	PH7_MemObjStore(&sVal,pCtx->pResult);   /* takes its own reference */
	PH7_ClassInstanceUnref(pHit);           /* ...and ours goes back */
}
/* php's refusal for the keyless `$list[]` spelling, which reaches the handler
 * with no offset at all. */
static void DomDimNoOffset(ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	SyString *pName = &pThis->pClass->sName;
	pCtx->zThrowClass = "Error";
	SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
		"Cannot access %.*s without offset",(int)pName->nByte,pName->zString);
}
/* The collection's by-NAME walk is the standard's id-then-name one and is
 * written with `Dom\HTMLCollection` below; the handler here needs it. */
static ph7_class_instance * DomCollectionNamed(ph7_vm *pVm,ph7_class_instance *pColl,
	const char *zKey);
/* ===== the namespaced collections' OWN dimension rules =====
 *
 * php 8.4 gave `Dom\NodeList`, `Dom\NamedNodeMap`, `Dom\DtdNamedNodeMap` and
 * `Dom\HTMLCollection` handlers of their own rather than the 2004 classes', and
 * the two differ in both directions -- swept over every offset type on both
 * families:
 *
 *   - The 2004 classes take php's ordinary offset cast, so `null`, `false` and
 *     `[1]` all become an integer index and answer null. The namespaced four
 *     REFUSE every one of those: `Cannot access offset of type null on
 *     Dom\NodeList`, and `... in isset or empty` for the isset face, which is
 *     the one wording that does not name the class.
 *   - `DOMNamedNodeMap[-1]` raises the ValueError its item() raises; the
 *     namespaced maps answer null to the same subscript and keep the refusal on
 *     the METHOD alone. `Dom\NamedNodeMap::item(-1)` still throws.
 *
 * What a namespaced offset may be: an int, a float (truncated, so `[1.9]` is
 * index 1 and `[-1.9]` is -1 -> null), and for the three by-NAME collections a
 * string. A LIST refuses a string outright unless it is php's canonical integer
 * spelling -- `'12'` and `'-1'` are indices, while `'007'`, `'+1'`, `' 1'`,
 * `'-0'` and `'9223372036854775808'` are refused as strings, which is
 * _zend_handle_numeric_str_ex's rule and not a numeric-prefix one.
 */
static int DomModernIntStr(ph7_value *pOffset,sxi64 *piIndex)
{
	const char *zIn,*zEnd;
	int nLen;
	sxi64 iVal = 0;
	int bNeg = 0;
	zIn = ph7_value_to_string(pOffset,&nLen);
	if( nLen < 1 ){
		return 0;
	}
	zEnd = &zIn[nLen];
	if( zIn[0] == '-' ){
		bNeg = 1;
		zIn++;
	}
	if( zIn >= zEnd ){
		return 0;
	}
	/* A leading zero disqualifies unless the WHOLE key is "0" -- so "-0", "00"
	 * and "007" stay strings, exactly as they stay string ARRAY keys. */
	if( zIn[0] == '0' && nLen > 1 ){
		return 0;
	}
	while( zIn < zEnd ){
		if( !SyisDigit(zIn[0]) ){
			return 0;
		}
		/* Overflow is a refusal, not a saturation: php's rule only says yes to
		 * what fits, which is why '9223372036854775808' is a string. */
		if( iVal > (SXI64_HIGH - 9) / 10 ){
			return 0;
		}
		iVal = iVal * 10 + (zIn[0] - '0');
		zIn++;
	}
	*piIndex = bNeg ? -iVal : iVal;
	return 1;
}
/*
 * Classify a namespaced collection's offset. Answers 1 for an INDEX (written to
 * *piIndex), 2 for a NAME the caller reads out of *pScratch, and 0 when the
 * offset's type is refused -- with the refusal already worded into pCtx.
 */
#define DOM_MODERN_INDEX 1
#define DOM_MODERN_NAME  2
static int DomModernDimClassify(ph7_vm *pVm,ph7_class_instance *pThis,
	PH7_NativeDimCtx *pCtx,ph7_value *pScratch,sxi64 *piIndex,int bByName)
{
	ph7_value *pOffset = pCtx->pOffset;
	SyString *pClass = &pThis->pClass->sName;
	const char *zType;
	*piIndex = 0;
	if( pOffset->iFlags & MEMOBJ_INT ){
		*piIndex = pOffset->x.iVal;
		return DOM_MODERN_INDEX;
	}
	if( pOffset->iFlags & MEMOBJ_REAL ){
		*piIndex = (sxi64)pOffset->rVal;
		return DOM_MODERN_INDEX;
	}
	if( pOffset->iFlags & MEMOBJ_STRING ){
		if( DomModernIntStr(pOffset,piIndex) ){
			return DOM_MODERN_INDEX;
		}
		if( bByName ){
			PH7_MemObjInit(&(*pVm),pScratch);
			PH7_MemObjStore(pOffset,pScratch);
			return DOM_MODERN_NAME;
		}
		zType = "string";
	}else if( pOffset->iFlags & MEMOBJ_OBJ ){
		/* php names the CLASS, as get_debug_type() does. */
		ph7_class_instance *pInst = (ph7_class_instance *)pOffset->x.pOther;
		SyString *pName = pInst && pInst->pClass ? &pInst->pClass->sName : 0;
		pCtx->zThrowClass = "TypeError";
		if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){
			SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
				"Cannot access offset of type %z in isset or empty",pName);
		}else{
			SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
				"Cannot access offset of type %z on %z",pName,pClass);
		}
		return 0;
	}else{
		zType = ph7_type_name(pOffset);
	}
	pCtx->zThrowClass = "TypeError";
	if( pCtx->iMode == PH7_NATIVE_DIM_ISSET ){
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Cannot access offset of type %s in isset or empty",zType);
	}else{
		SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
			"Cannot access offset of type %s on %z",zType,pClass);
	}
	return 0;
}
/* The namespaced LIST and COLLECTION: index-only and index-or-name, over the
 * same two walks their 2004 counterparts use. */
static void DomModernDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx,
	int bByName,int bMapWalk)
{
	ph7_value sKey;
	sxi64 iIndex;
	int iKind;
	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){
		return;   /* see DomListDim: the write side is php's generic sentence */
	}
	if( pCtx->pOffset == 0 ){
		DomDimNoOffset(pThis,pCtx);
		return;
	}
	iKind = DomModernDimClassify(&(*pVm),pThis,pCtx,&sKey,&iIndex,bByName);
	if( iKind == 0 ){
		return;   /* the offset's TYPE is refused, and the refusal is worded */
	}
	if( iKind == DOM_MODERN_NAME ){
		ph7_class_instance *pHit = bMapWalk
			? DomMapNamed(&(*pVm),pThis,ph7_value_to_string(&sKey,0))
			: DomCollectionNamed(&(*pVm),pThis,ph7_value_to_string(&sKey,0));
		DomDimAnswer(&(*pVm),pCtx,pHit);
		PH7_MemObjRelease(&sKey);
		return;
	}
	/* No range REFUSAL here, unlike DomMapDim: the namespaced maps answer null
	 * to an out-of-range subscript and keep the ValueError on item(). */
	if( iIndex < 0 || iIndex > DOM_INDEX_MAX ){
		return;
	}
	DomDimAnswer(&(*pVm),pCtx,bMapWalk
		? DomMapItem(&(*pVm),pThis,(int)iIndex)
		: DomListItem(&(*pVm),pThis,(int)iIndex));
}
static void DomModernListDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	DomModernDim(&(*pVm),pThis,pCtx,0,0);
}
static void DomModernCollDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	DomModernDim(&(*pVm),pThis,pCtx,1,0);
}
static void DomModernMapDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	DomModernDim(&(*pVm),pThis,pCtx,1,1);
}
static void DomListDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	ph7_value sKey;
	sxi64 iIndex;
	int bNamed;
	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){
		/* The WRITE modes only ask for a refusal WORDING, and these two classes
		 * have none of their own: php answers `Cannot use object of type C as
		 * array` for a store, an append and an unset, which is what the caller
		 * formats when the hook declines. */
		return;
	}
	if( pCtx->pOffset == 0 ){
		DomDimNoOffset(pThis,pCtx);
		return;
	}
	bNamed = DomDimClassify(&(*pVm),pCtx->pOffset,&sKey,&iIndex);
	PH7_MemObjRelease(&sKey);
	if( bNamed || iIndex < 0 || iIndex > DOM_INDEX_MAX ){
		return; /* a name the list cannot answer, or an index it answers null to */
	}
	DomDimAnswer(&(*pVm),pCtx,DomListItem(&(*pVm),pThis,(int)iIndex));
}
static void DomMapDim(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativeDimCtx *pCtx)
{
	ph7_value sKey;
	sxi64 iIndex;
	int bNamed;
	if( pCtx->iMode != PH7_NATIVE_DIM_READ && pCtx->iMode != PH7_NATIVE_DIM_ISSET ){
		return;   /* see DomListDim: the write side is php's generic sentence */
	}
	if( pCtx->pOffset == 0 ){
		DomDimNoOffset(pThis,pCtx);
		return;
	}
	bNamed = DomDimClassify(&(*pVm),pCtx->pOffset,&sKey,&iIndex);
	if( bNamed ){
		DomDimAnswer(&(*pVm),pCtx,DomMapNamed(&(*pVm),pThis,ph7_value_to_string(&sKey,0)));
		PH7_MemObjRelease(&sKey);
		return;
	}
	PH7_MemObjRelease(&sKey);
	if( iIndex < 0 || iIndex > DOM_INDEX_MAX ){
		/* The range is screened BEFORE the map is looked at, so a map with no
		 * owner at all -- `(new DOMNamedNodeMap())[-1]` -- refuses too. */
		if( pCtx->iMode == PH7_NATIVE_DIM_READ ){
			pCtx->zThrowClass = "ValueError";
			SyBufferFormat(pCtx->zThrowMsg,sizeof(pCtx->zThrowMsg),
				"must be between 0 and %d",DOM_INDEX_MAX);
		}
		return;
	}
	DomDimAnswer(&(*pVm),pCtx,DomMapItem(&(*pVm),pThis,(int)iIndex));
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
	PH7_NativeSetAttrObj(&(*pVm),pIt,PH7_NATIVE_IT_CUR,pCur);
	PH7_ClassInstanceUnref(pCur);   /* the slot took its own; pCur lives on it */
	if( bNamed ){
		phl_domnode *pNd = DomResOf(pCur);
		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
		const char *zKey = (pNode && pNode->name) ? (const char *)pNode->name : "";
		/* The key is the name the map is KEYED by, which is the tree's by-name
		 * rule and not the node's identity: `p:b` walks past as `b` on the 2004
		 * map and as `p:b` on php 8.4's. */
		if( pNode && pNode->ns && pNode->ns->prefix && DomMapModern(pSrc) ){
			SyBlob sQ;
			SyBlobInit(&sQ,&pVm->sAllocator);
			SyBlobFormat(&sQ,"%s:%s",(const char *)pNode->ns->prefix,zKey);
			PH7_NativeSetAttrStr(&(*pVm),pIt,PH7_NATIVE_IT_KEY,
				(const char *)SyBlobData(&sQ),(int)SyBlobLength(&sQ));
			SyBlobRelease(&sQ);
		}else{
			PH7_NativeSetAttrStr(&(*pVm),pIt,PH7_NATIVE_IT_KEY,zKey,(int)SyStrlen(zKey));
		}
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
static const PH7_NativeIterVtab sDomListIterVtab = { DomListRewind, DomListNext, 0, 0 };
static const PH7_NativeIterVtab sDomMapIterVtab  = { DomMapRewind,  DomMapNext, 0, 0 };
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

/* ===== php 8.4's Dom\ namespace: the collection half =====
 *
 * php 8.4 re-faces ext/dom under a `Dom\` namespace whose classes are declared,
 * typed and named the way the DOM standard writes them rather than the way the
 * 2004 binding did. The node classes are the bulk of it; the three names here
 * are the ones that stand on their own -- two pure interfaces, and a collection
 * whose bare instance is a complete object in php:
 *
 *     var_dump(new Dom\HTMLCollection());   // object(Dom\HTMLCollection)#1 (1) { ["length"]=> int(0) }
 *
 * `Dom\HTMLCollection` therefore ships whole: it wears DOMNodeList's slot layout
 * (rule: a live view is of an $__owner, wrapped against a $__doc), so the walk
 * that fills it is the one the node classes bring with them, and an instance with
 * no owner -- the only one obtainable until they land -- answers 0/null/empty,
 * which is exactly what php answers for the same object.
 *
 * The two interfaces state php's abstract signatures verbatim. Their parameter
 * and return types name `Dom\Node`, `Dom\Element` and `Dom\NodeList`, which do
 * not exist yet; that is not a forward declaration problem in either engine,
 * because a type in a signature is resolved when a call is checked against it
 * and an interface method is never called.
 */
/*
 * Dom\HTMLCollection::namedItem(): the standard's rule, which is not
 * DOMNamedNodeMap's -- a *collection* is keyed by the `id` attribute of the
 * elements in it, and falls back to `name`. It walks the collection rather than
 * a node's attribute list, so it is its own reader.
 */
static ph7_class_instance * DomCollectionNamed(ph7_vm *pVm,ph7_class_instance *pColl,
	const char *zKey)
{
	int nItem,i;
	if( pColl == 0 || zKey == 0 || zKey[0] == 0 ){
		return 0;
	}
	nItem = DomListCount(pColl);
	for( i = 0 ; i < nItem ; ++i ){
		ph7_class_instance *pItem = DomListItem(&(*pVm),pColl,i);
		phl_domnode *pNd = pItem ? DomResOf(pItem) : 0;
		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
		xmlChar *zVal;
		if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
			continue;
		}
		zVal = xmlGetNoNsProp(pNode,(const xmlChar *)"id");
		if( zVal == 0 ){
			zVal = xmlGetNoNsProp(pNode,(const xmlChar *)"name");
		}
		if( zVal ){
			sxu32 nKey = (sxu32)SyStrlen(zKey);
			int bHit = SyStrlen((const char *)zVal) == nKey
				&& SyStrncmp((const char *)zVal,zKey,nKey) == 0;
			xmlFree(zVal);
			if( bHit ){
				return pItem;
			}
		}
	}
	return 0;
}
/*
 * The namespaced maps' item(). php's ValueError names the DECLARING class --
 * a userland subclass of Dom\DtdNamedNodeMap still reads
 * `Dom\DtdNamedNodeMap::item()` -- so each row states its own name rather
 * than reading it off $this.
 */
static int DomModernMapItem(ph7_context *pCtx,int nArg,ph7_value **apArg,const char *zClass)
{
	int iIndex;
	if( !DomCollectionIndex(nArg,apArg,&iIndex) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::item(): Argument #1 ($index) must be between 0 and %d",
			zClass,DOM_INDEX_MAX);
	}
	return DomResultOwned(pCtx,DomMapItem(pCtx->pVm,PH7_ContextThis(pCtx),iIndex));
}
DOM_METHOD(vm_builtin_DomNamedNodeMap_item)
{
	return DomModernMapItem(pCtx,nArg,apArg,"Dom\\NamedNodeMap");
}
DOM_METHOD(vm_builtin_DomDtdNamedNodeMap_item)
{
	return DomModernMapItem(pCtx,nArg,apArg,"Dom\\DtdNamedNodeMap");
}
DOM_METHOD(vm_builtin_DomHTMLCollection_namedItem)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zKey = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	return DomResultOwned(pCtx,DomCollectionNamed(pCtx->pVm,pThis,zKey));
}

/* ===== DOMXPath ===== */

/* The prefix => URI table registerNamespace() feeds, replayed onto the fresh
 * evaluation context each query. Hidden slot, materialized by DomXPathSlotMap
 * below with the same three moves the document's identity cache needs. */
#define XP_NSREG "__nsreg"
/* php declares `document` and `registerNodeNamespaces` VIRTUAL -- its object holds
 * neither, and both are read_property handlers -- so the two values live in hidden
 * slots and the class declares the php-visible names with no storage at all. */
#define XP_DOC   "__xdoc"
#define XP_NSDEF "__xnsdef"
/*
 * DOMXPath::quote(string $str): string  (static, php 8.4)
 *
 * XPath 1.0 has no escape inside a string literal, so a value is quotable
 * only with the quote character it does not contain: no `'` and it goes in
 * single quotes, no `"` in double ones, and a value carrying BOTH becomes a
 * `concat()` of runs, split by the rule stated at the loop below. php's
 * algorithm exactly, and its output byte for byte.
 */
DOM_METHOD(vm_builtin_DOMXPath_quote)
{
	int nStr = 0;
	const char *zStr = nArg > 0 ? ph7_value_to_string(apArg[0],&nStr) : "";
	int bSq = 0,bDq = 0,i;
	SyBlob sOut;
	for( i = 0 ; i < nStr ; ++i ){
		if( zStr[i] == '\'' ){
			bSq = 1;
		}else if( zStr[i] == '"' ){
			bDq = 1;
		}
	}
	if( !bSq ){
		ph7_result_string_format(pCtx,"'%.*s'",nStr,zStr);
		return PH7_OK;
	}
	if( !bDq ){
		ph7_result_string_format(pCtx,"\"%.*s\"",nStr,zStr);
		return PH7_OK;
	}
	SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
	SyBlobAppend(&sOut,"concat(",sizeof("concat(")-1);
	i = 0;
	while( i < nStr ){
		/* Whichever quote kind appears FIRST in what is left decides the run:
		 * the run is wrapped in the OTHER kind and reaches to that first
		 * occurrence, so it swallows every quote of the kind it is not wrapped
		 * in. `a'b"c'd"e` is four runs that way, and `0"&'<` is two -- the
		 * first single-quoted, because its first quote character is the double
		 * one. */
		int iStart = i,j;
		char cQuote = '"';
		for( j = i ; j < nStr ; ++j ){
			if( zStr[j] == '\'' || zStr[j] == '"' ){
				cQuote = zStr[j] == '"' ? '\'' : '"';
				break;
			}
		}
		while( i < nStr && zStr[i] != cQuote ){
			i++;
		}
		if( iStart > 0 ){
			SyBlobAppend(&sOut,",",1);
		}
		SyBlobAppend(&sOut,&cQuote,1);
		SyBlobAppend(&sOut,zStr + iStart,(sxu32)(i - iStart));
		SyBlobAppend(&sOut,&cQuote,1);
	}
	SyBlobAppend(&sOut,")",1);
	ph7_result_string(pCtx,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}

/* ===== The PHP-function bridge (php:function / php:functionString / own-URI) ===== */

/* php's reserved URI: `php:function()` is reached through whatever PREFIX the
 * caller bound to it, so every lookup here is by URI. */
#define XP_PHPNS "http://php.net/xpath"
/* Which callables an evaluation may reach: php's register_phpfunctions. */
#define XP_MODE_NONE   0   /* registerPhpFunctions() never called -- nothing runs */
#define XP_MODE_ALL    1   /* called bare -- any callable name runs */
#define XP_MODE_LIST   2   /* called with a restriction -- the table below decides */
#define XP_FNMODE "__fnmode"
#define XP_FNREG  "__fnreg"    /* restricted: xpath name => the callable to run */
#define XP_NSFN   "__nsfn"     /* own-URI: "<uri>\x01<name>" => callable */
static ph7_hashmap * DomXPathSlotMap(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot)
{
	ph7_value *pSlot = pThis ? PH7_NativeAttr(pThis,zSlot) : 0;
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
/* Insert (or replace) one string-keyed entry. */
static void DomXPathMapPut(ph7_vm *pVm,ph7_hashmap *pMap,const char *zKey,int nKey,ph7_value *pVal)
{
	ph7_value sKey;
	PH7_MemObjInitFromString(&(*pVm),&sKey,0);
	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);
	PH7_HashmapInsert(pMap,&sKey,pVal);
	PH7_MemObjRelease(&sKey);
}
/* ...and the matching read, or NULL. The value BELONGS to the map. */
static ph7_value * DomXPathMapGet(ph7_vm *pVm,ph7_class_instance *pThis,const char *zSlot,
	const char *zKey,int nKey)
{
	ph7_hashmap *pMap = DomXPathSlotMap(&(*pVm),pThis,zSlot);
	ph7_hashmap_node *pEntry = 0;
	ph7_value sKey,*pHit;
	if( pMap == 0 ){
		return 0;
	}
	PH7_MemObjInitFromString(&(*pVm),&sKey,0);
	PH7_MemObjStringAppend(&sKey,zKey,(sxu32)nKey);
	if( PH7_HashmapLookup(pMap,&sKey,&pEntry) != SXRET_OK ){
		pEntry = 0;
	}
	PH7_MemObjRelease(&sKey);
	pHit = pEntry ? HashmapExtractNodeValue(pEntry) : 0;
	return pHit;
}
/*
 * What one evaluation needs to reach PHP from inside libxml, and what it
 * brings BACK.
 *
 * The bringing back is the whole design problem: a refusal raised from the
 * callback would run the enclosing catch RIGHT THERE, in the middle of
 * libxml's own recursion (the builtin-throw rail), so nothing is raised here.
 * The reason is PARKED -- a code and the name it quotes -- the evaluation is
 * stopped by setting the parser's error field (not xmlXPathErr, which would
 * queue a libxml diagnostic php does not print), and DomXPathEvalRun raises
 * once libxml has unwound. A throw from the CALLBACK ITSELF is the same
 * story one level up: its dispatch status is parked in rcUnwound and returned
 * from the method verbatim, which is what makes `php:function("boom") or
 * php:function("after")` run neither the `or` arm nor anything past it --
 * php's answer.
 */
#define XP_FN_OK        0
#define XP_FN_NOREG     1   /* registerPhpFunctions() was never called */
#define XP_FN_NOHANDLER 2   /* restricted, and this name is not in the table */
#define XP_FN_NOTSTR    3   /* the handler name argument is not a string */
#define XP_FN_NONAME    4   /* php:function() with no arguments at all */
#define XP_FN_BADCB     5   /* the name is not callable */
#define XP_FN_NOTNODE   6   /* the callback answered an object that is not a node */
typedef struct DomXPathFnCtx DomXPathFnCtx;
struct DomXPathFnCtx {
	ph7_context *pCtx;            /* the method's own call context */
	ph7_class_instance *pThis;    /* the DOMXPath */
	ph7_class_instance *pDoc;     /* its document object (where wrappers cache) */
	phl_domnode *pDocNd;
	int iErr;                     /* XP_FN_* -- raised after libxml unwinds */
	SyBlob sErrName;              /* the name that refusal quotes */
	sxi32 rcUnwound;              /* a callback that did not return */
};
/* Stop the evaluation without emitting a libxml diagnostic. */
static void DomXPathFnStop(xmlXPathParserContextPtr pPCtx,DomXPathFnCtx *pFn,int iErr,
	const char *zName,int nName)
{
	if( pFn->iErr == XP_FN_OK ){
		pFn->iErr = iErr;
		SyBlobReset(&pFn->sErrName);
		if( zName && nName > 0 ){
			SyBlobAppend(&pFn->sErrName,zName,(sxu32)nName);
		}
	}
	pPCtx->error = XPATH_EXPR_ERROR;
}
/*
 * What a callback that did not RETURN leaves behind, which php's two
 * dispatchers do differently and both visibly.
 *
 * The one that looks a callable up in a REGISTERED table -- restricted
 * php:function, and every own-URI name -- returns without pushing, and libxml,
 * finding its value stack one short, queues its own "Stack usage error" before
 * unwinding; that entry is then on the list libxml_get_errors() answers. The
 * UNRESTRICTED php:function path pushes a value first, so its queue stays
 * clean. Either way the exception is the answer, and nothing further of the
 * expression runs.
 */
static void DomXPathFnUnwound(xmlXPathParserContextPtr pPCtx,int bRegistered)
{
	if( !bRegistered ){
		valuePush(pPCtx,xmlXPathNewCString(""));
		pPCtx->error = XPATH_EXPR_ERROR;
	}
}
/* One XPath argument as php sees it: a nodeset becomes an ARRAY of wrappers
 * (php's own conversion), the three scalars their php types. bAsString is
 * php:functionString's flag, under which a nodeset arrives as its string
 * value instead. */
static void DomXPathArgToValue(DomXPathFnCtx *pFn,xmlXPathObjectPtr pArg,int bAsString,
	ph7_value *pOut)
{
	ph7_vm *pVm = pFn->pCtx->pVm;
	if( pArg == 0 ){
		PH7_MemObjInit(pVm,pOut);
		return;
	}
	if( pArg->type == XPATH_NODESET && !bAsString ){
		/* The array is built on its OWN reference rather than the method's call
		 * context: a predicate calls this once per node, and a context-owned
		 * one would live until the whole evaluation ended. */
		ph7_hashmap *pMap = PH7_NewHashmap(pVm,0,0);
		int i;
		PH7_MemObjInit(pVm,pOut);
		if( pMap == 0 ){
			return;
		}
		/* pOut CARRIES the map's only reference, and the caller's release of
		 * it after the call is what frees it. */
		pOut->x.pOther = pMap;
		pOut->iFlags = MEMOBJ_HASHMAP;
		for( i = 0 ; pArg->nodesetval && i < pArg->nodesetval->nodeNr ; ++i ){
			xmlNodePtr pNode = pArg->nodesetval->nodeTab[i];
			ph7_value sElem;
			ph7_class_instance *pObj;
			if( pNode == 0 ){
				continue;
			}
			if( pNode->type == XML_NAMESPACE_DECL ){
				xmlNsPtr pNs = (xmlNsPtr)pNode;
				xmlNodePtr pElem = (xmlNodePtr)pNs->next;
				xmlNsPtr pOrig = (pElem && pElem->type == XML_ELEMENT_NODE)
					? xmlSearchNs((xmlDocPtr)pFn->pDocNd->pNode,pElem,pNs->prefix) : 0;
				if( pOrig == 0 ){
					continue;
				}
				pObj = DomNewNsNode(pVm,pFn->pDoc,pFn->pDocNd->pShell,pOrig,pElem);
				if( pObj == 0 ){
					continue;
				}
				PH7_MemObjInit(pVm,&sElem);
				sElem.x.pOther = pObj;
				sElem.iFlags = MEMOBJ_OBJ;
				ph7_array_add_elem(pOut,0,&sElem);   /* takes its own reference */
				PH7_ClassInstanceUnref(pObj);        /* ...and ours goes back */
				continue;
			}
			pObj = DomWrap(pVm,pFn->pDoc,pFn->pDocNd->pShell,pNode);
			if( pObj == 0 ){
				continue;
			}
			PH7_MemObjInit(pVm,&sElem);
			sElem.x.pOther = pObj;
			sElem.iFlags = MEMOBJ_OBJ;
			ph7_array_add_elem(pOut,0,&sElem);   /* takes its own reference */
			PH7_ClassInstanceUnref(pObj);        /* ...and ours goes back */
		}
		return;
	}
	switch( pArg->type ){
	case XPATH_BOOLEAN:
		PH7_MemObjInitFromBool(pVm,pOut,pArg->boolval);
		break;
	case XPATH_NUMBER:
		PH7_MemObjInitFromReal(pVm,pOut,pArg->floatval);
		break;
	default: {
		xmlChar *zStr = xmlXPathCastToString(pArg);
		PH7_MemObjInitFromString(pVm,pOut,0);
		if( zStr ){
			PH7_MemObjStringAppend(pOut,(const char *)zStr,(sxu32)SyStrlen((const char *)zStr));
			xmlFree(zStr);
		}
		break;
	}
	}
}
/* The callback's ANSWER, pushed back on the XPath stack: a bool stays a
 * boolean, a DOM node becomes a one-node set, and everything else is php's
 * string conversion -- the SAME three for both spellings, since
 * functionString's flag is about the ARGUMENTS. The conversion is the
 * user-visible one (an array draws php's "Array to string conversion" notice
 * and reads "Array"); a non-node object is the TypeError parked above. */
static void DomXPathPushResult(xmlXPathParserContextPtr pPCtx,DomXPathFnCtx *pFn,
	ph7_value *pRes)
{
	if( (pRes->iFlags & MEMOBJ_BOOL) && (pRes->iFlags & MEMOBJ_STRING) == 0 ){
		valuePush(pPCtx,xmlXPathNewBoolean(pRes->x.iVal != 0));
		return;
	}
	if( pRes->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pObj = (ph7_class_instance *)pRes->x.pOther;
		phl_domnode *pNd = DomResOf(pObj);
		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
		if( pNode == 0 || pNode->type == XML_NAMESPACE_DECL ){
			DomXPathFnStop(pPCtx,pFn,XP_FN_NOTNODE,0,0);
			return;
		}
		valuePush(pPCtx,xmlXPathNewNodeSet(pNode));
		return;
	}
	{
		int nStr = 0;
		const char *zStr;
		xmlChar *zDup;
		if( (pRes->iFlags & MEMOBJ_STRING) == 0 ){
			sxi32 rcStr = PH7_MemObjToStringUV(pRes);
			if( PH7_CALLBACK_UNWOUND(rcStr) ){
				/* A __toString() that threw: the same rail as the callback's
				 * own throw, one conversion later. */
				pFn->rcUnwound = rcStr;
				return;
			}
		}
		zStr = ph7_value_to_string(pRes,&nStr);
		zDup = xmlStrndup((const xmlChar *)zStr,nStr);
		valuePush(pPCtx,xmlXPathWrapString(zDup));
	}
}
/*
 * The one C function behind every PHP-backed XPath name. libxml reaches it
 * through the lookup below, with the called name and URI on the context.
 */
static void DomXPathPhpFn(xmlXPathParserContextPtr pPCtx,int nArgs)
{
	xmlXPathContextPtr pXCtx = pPCtx ? pPCtx->context : 0;
	DomXPathFnCtx *pFn = pXCtx ? (DomXPathFnCtx *)pXCtx->funcLookupData : 0;
	const xmlChar *zFn = pXCtx ? pXCtx->function : 0;
	const xmlChar *zUri = pXCtx ? pXCtx->functionURI : 0;
	int bPhpNs = zUri && xmlStrEqual(zUri,(const xmlChar *)XP_PHPNS);
	int bAsString = bPhpNs && zFn && xmlStrEqual(zFn,(const xmlChar *)"functionString");
	int bRegistered = !bPhpNs;   /* a table lookup rather than the name itself */
	xmlXPathObjectPtr *apArg;
	ph7_value *apVal = 0,sResult,sName;
	ph7_value *pCallable = 0;
	int bNameOwned = 0;
	ph7_vm *pVm;
	int nSkip = bPhpNs ? 1 : 0;   /* php:function's first argument NAMES the callback */
	int i,nCall;
	sxi32 rc;
	if( pFn == 0 ){
		return;
	}
	pVm = pFn->pCtx->pVm;
	/* Take the arguments off the stack FIRST (valuePop answers them last-first),
	 * so every exit below leaves libxml's stack where it found it. */
	apArg = nArgs > 0
		? (xmlXPathObjectPtr *)SyMemBackendAlloc(&pVm->sAllocator,
			sizeof(xmlXPathObjectPtr) * (sxu32)nArgs)
		: 0;
	if( nArgs > 0 && apArg == 0 ){
		pPCtx->error = XPATH_MEMORY_ERROR;
		return;
	}
	for( i = nArgs - 1 ; i >= 0 ; --i ){
		apArg[i] = valuePop(pPCtx);
	}
	if( pFn->iErr != XP_FN_OK || pFn->rcUnwound != 0 ){
		goto done;   /* a previous call already stopped this evaluation */
	}
	if( bPhpNs ){
		int nName = 0;
		const char *zName;
		sxi64 iMode = PH7_NativeAttrInt(pFn->pThis,XP_FNMODE);
		if( nArgs < 1 ){
			DomXPathFnStop(pPCtx,pFn,XP_FN_NONAME,0,0);
			goto done;
		}
		if( apArg[0] == 0 || apArg[0]->type != XPATH_STRING ){
			DomXPathFnStop(pPCtx,pFn,XP_FN_NOTSTR,0,0);
			goto done;
		}
		zName = apArg[0]->stringval ? (const char *)apArg[0]->stringval : "";
		nName = (int)SyStrlen(zName);
		if( iMode == XP_MODE_NONE ){
			DomXPathFnStop(pPCtx,pFn,XP_FN_NOREG,0,0);
			goto done;
		}
		if( iMode == XP_MODE_LIST ){
			bRegistered = 1;
			pCallable = DomXPathMapGet(pVm,pFn->pThis,XP_FNREG,zName,nName);
			if( pCallable == 0 ){
				DomXPathFnStop(pPCtx,pFn,XP_FN_NOHANDLER,zName,nName);
				goto done;
			}
		}else{
			/* Unrestricted: the NAME ITSELF is the callable, screened here
			 * because no registration screened it. */
			PH7_MemObjInitFromString(pVm,&sName,0);
			PH7_MemObjStringAppend(&sName,zName,(sxu32)nName);
			bNameOwned = 1;
			if( !PH7_VmIsCallable(pVm,&sName,TRUE) ){
				DomXPathFnStop(pPCtx,pFn,XP_FN_BADCB,zName,nName);
				goto done;
			}
			pCallable = &sName;
		}
	}else{
		SyBlob sKey;
		SyBlobInit(&sKey,&pVm->sAllocator);
		SyBlobAppend(&sKey,(const char *)zUri,zUri ? (sxu32)SyStrlen((const char *)zUri) : 0);
		SyBlobAppend(&sKey,"\1",1);
		SyBlobAppend(&sKey,(const char *)zFn,zFn ? (sxu32)SyStrlen((const char *)zFn) : 0);
		pCallable = DomXPathMapGet(pVm,pFn->pThis,XP_NSFN,
			(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey));
		SyBlobRelease(&sKey);
		if( pCallable == 0 ){
			goto done;   /* not ours after all: libxml reports the unknown function */
		}
	}
	nCall = nArgs - nSkip;
	if( nCall > 0 ){
		apVal = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,
			sizeof(ph7_value) * (sxu32)nCall);
		if( apVal == 0 ){
			pPCtx->error = XPATH_MEMORY_ERROR;
			goto done;
		}
		for( i = 0 ; i < nCall ; ++i ){
			DomXPathArgToValue(pFn,apArg[i + nSkip],bAsString,&apVal[i]);
		}
	}
	PH7_MemObjInit(pVm,&sResult);
	{
		ph7_value **apPtr = nCall > 0
			? (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,
				sizeof(ph7_value *) * (sxu32)nCall)
			: 0;
		if( nCall > 0 && apPtr == 0 ){
			pPCtx->error = XPATH_MEMORY_ERROR;
			PH7_MemObjRelease(&sResult);
			goto done;
		}
		/* Dispatch off a COPY: pCallable points into a registration map this
		 * very callback can rewrite (a callback calling registerPhpFunctions
		 * on its own DOMXPath), and the map's value would go out from under
		 * the dispatch. */
		ph7_value sCall;
		PH7_MemObjInit(pVm,&sCall);
		PH7_MemObjStore(pCallable,&sCall);
		for( i = 0 ; i < nCall ; ++i ){
			apPtr[i] = &apVal[i];
		}
		rc = PH7_VmCallCallbackByValue(pVm,&sCall,nCall,apPtr,&sResult,0);
		PH7_MemObjRelease(&sCall);
		if( apPtr ){
			SyMemBackendFree(&pVm->sAllocator,apPtr);
		}
	}
	if( PH7_CALLBACK_UNWOUND(rc) ){
		pFn->rcUnwound = rc;
		DomXPathFnUnwound(pPCtx,bRegistered);
	}else{
		DomXPathPushResult(pPCtx,pFn,&sResult);
	}
	PH7_MemObjRelease(&sResult);
done:
	if( bNameOwned ){
		PH7_MemObjRelease(&sName);
	}
	if( apVal ){
		for( i = 0 ; i < nArgs - nSkip ; ++i ){
			PH7_MemObjRelease(&apVal[i]);
		}
		SyMemBackendFree(&pVm->sAllocator,apVal);
	}
	for( i = 0 ; i < nArgs ; ++i ){
		if( apArg[i] ){
			xmlXPathFreeObject(apArg[i]);
		}
	}
	if( apArg ){
		SyMemBackendFree(&pVm->sAllocator,apArg);
	}
}
/*
 * libxml's function-resolution hook: answer the bridge for php's two reserved
 * names and for any (URI, name) this object registered, and NULL for
 * everything else -- which is what makes libxml fall through to its own table
 * (so `count()` and friends still resolve).
 */
static xmlXPathFunction DomXPathFnLookup(void *pUserData,const xmlChar *zName,const xmlChar *zUri)
{
	DomXPathFnCtx *pFn = (DomXPathFnCtx *)pUserData;
	SyBlob sKey;
	ph7_value *pHit;
	if( pFn == 0 || zUri == 0 || zName == 0 ){
		return 0;
	}
	if( xmlStrEqual(zUri,(const xmlChar *)XP_PHPNS) ){
		if( xmlStrEqual(zName,(const xmlChar *)"function")
		 || xmlStrEqual(zName,(const xmlChar *)"functionString") ){
			return DomXPathPhpFn;
		}
		return 0;
	}
	SyBlobInit(&sKey,&pFn->pCtx->pVm->sAllocator);
	SyBlobAppend(&sKey,(const char *)zUri,(sxu32)SyStrlen((const char *)zUri));
	SyBlobAppend(&sKey,"\1",1);
	SyBlobAppend(&sKey,(const char *)zName,(sxu32)SyStrlen((const char *)zName));
	pHit = DomXPathMapGet(pFn->pCtx->pVm,pFn->pThis,XP_NSFN,
		(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey));
	SyBlobRelease(&sKey);
	return pHit ? DomXPathPhpFn : 0;
}
/* The parked refusal, raised once libxml has unwound. */
static int DomXPathFnRaise(ph7_context *pCtx,DomXPathFnCtx *pFn)
{
	const char *zName = (const char *)SyBlobData(&pFn->sErrName);
	int nName = (int)SyBlobLength(&pFn->sErrName);
	switch( pFn->iErr ){
	case XP_FN_NOREG:
		return PH7_VmThrowException(pCtx,"Error","No callbacks were registered");
	case XP_FN_NOHANDLER:
		return PH7_VmThrowException(pCtx,"Error",
			"No callback handler \"%.*s\" registered",nName,zName);
	case XP_FN_NOTSTR:
		return PH7_VmThrowException(pCtx,"TypeError","Handler name must be a string");
	case XP_FN_NONAME:
		return PH7_VmThrowException(pCtx,"Error",
			"Function name must be passed as the first argument");
	case XP_FN_BADCB:
		return PH7_VmThrowException(pCtx,"Error",
			"Invalid callback %.*s, function \"%.*s\" not found or invalid function name",
			nName,zName,nName,zName);
	case XP_FN_NOTNODE:
		return PH7_VmThrowException(pCtx,"TypeError",
			"Only objects that are instances of DOM nodes can be converted to an XPath expression");
	default:
		break;
	}
	return PH7_OK;
}
/*
 * Build the evaluation context for one query()/evaluate() call: a FRESH
 * xmlXPathContext (php keeps a persistent one; replaying the registration
 * table onto a fresh one answers the same), anchored at the explicit context
 * node -- or, with none, at the document ELEMENT, php's own substitution (so
 * query('file') matches a child of the root; an explicitly PASSED document
 * node is NOT substituted and carries no namespaces).
 *
 * bRegNodeNs is php's $registerNodeNS: the context NODE's in-scope
 * declarations go into pXCtx->namespaces, the array xmlXPathNsLookup consults
 * BEFORE the registered table -- which is why a document prefix beats a
 * registerNamespace() one only for that call. The caller frees the returned
 * list with xmlFree AFTER evaluating (the xmlNs entries belong to the tree;
 * only the array is owned).
 */
static xmlNsPtr * DomXPathCtxOpen(ph7_context *pCtx,phl_domnode *pDocNd,
	phl_domnode *pCtxNd,int bRegNodeNs,xmlXPathContextPtr *ppXCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	xmlXPathContextPtr pXCtx;
	ph7_value *pNsReg;
	xmlNsPtr *aNs = 0;
	*ppXCtx = 0;
	pXCtx = xmlXPathNewContext((xmlDocPtr)pDocNd->pNode);
	if( pXCtx == 0 ){
		return 0;
	}
	if( pCtxNd ){
		pXCtx->node = (xmlNodePtr)pCtxNd->pNode;
	}else{
		pXCtx->node = xmlDocGetRootElement((xmlDocPtr)pDocNd->pNode);
	}
	pNsReg = pThis ? PH7_NativeAttr(pThis,XP_NSREG) : 0;
	if( pNsReg && (pNsReg->iFlags & MEMOBJ_HASHMAP) ){
		ph7_array_walk(pNsReg,DomC14NRegisterNs,pXCtx);
	}
	if( bRegNodeNs && pXCtx->node ){
		aNs = xmlGetNsList((xmlDocPtr)pDocNd->pNode,pXCtx->node);
		if( aNs ){
			int nNs = 0;
			while( aNs[nNs] ){
				nNs++;
			}
			pXCtx->namespaces = aNs;
			pXCtx->nsNr = nNs;
		}
	}
	*ppXCtx = pXCtx;
	return aNs;
}
/*
 * Freeze a nodeset result into the document-order snapshot a DNL_SNAP
 * DOMNodeList serves (php's query() is not live), and answer the list. A
 * non-nodeset pObj answers the EMPTY list: php's query() gives that for a
 * scalar-typed expression (`count(//x)`), not false.
 */
static int DomXPathResultList(ph7_context *pCtx,ph7_class_instance *pDoc,
	phl_domnode *pDocNd,xmlXPathObjectPtr pObj)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pList;
	ph7_value *pSnap = ph7_context_new_array(pCtx);
	if( pSnap == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pObj && pObj->type == XPATH_NODESET && pObj->nodesetval ){
		int i;
		for( i = 0 ; i < pObj->nodesetval->nodeNr ; i++ ){
			xmlNodePtr pNode = pObj->nodesetval->nodeTab[i];
			phl_domnode *pWrap;
			ph7_value *pRes;
			if( pNode == 0 ){
				continue;
			}
			if( pNode->type == XML_NAMESPACE_DECL ){
				/*
				 * php 8.4 REFUSES the axis rather than wrapping it: the living
				 * DOM has no namespace node, so `Dom\XPath` throws where the
				 * 2004 class answers a list of DOMNameSpaceNode. The refusal is
				 * per RESULT and not per expression -- an axis step that selects
				 * nothing evaluates to an empty list under both trees, and only
				 * a declaration actually reaching the set raises.
				 */
				if( DomDocFlag(pDoc,DOM_F_MODERN) ){
					return DomThrowSentence(pCtx,DOM_ERR_NOT_SUPPORTED,
						"The namespace axis is not well-defined in the living DOM "
						"specification. Use Dom\\Element::getInScopeNamespaces() or "
						"Dom\\Element::getDescendantNamespaces() instead.");
				}
				/*
				 * A namespace:: axis result. libxml hands the set a COPY that
				 * dies with the XPath object (xmlXPathNodeSetDupNs, its `next`
				 * pointing at the element the axis ran ON), so the snapshot
				 * wraps the ORIGINAL in-scope declaration found back through
				 * that element -- as php answers it: a DOMNameSpaceNode whose
				 * parentNode is the axis element even for a declaration an
				 * ANCESTOR made, fresh per query, stored as the OBJECT itself
				 * (item() twice on one list is one object, php's answer too).
				 */
				xmlNsPtr pNs = (xmlNsPtr)pNode;
				xmlNodePtr pElem = (xmlNodePtr)pNs->next;
				xmlNsPtr pOrig;
				ph7_class_instance *pNsObj;
				if( pElem == 0 || pElem->type != XML_ELEMENT_NODE ){
					continue; /* not derivable: no element behind the copy */
				}
				pOrig = xmlSearchNs((xmlDocPtr)pDocNd->pNode,pElem,pNs->prefix);
				if( pOrig == 0 ){
					continue;
				}
				pNsObj = DomNewNsNode(pVm,pDoc,pDocNd->pShell,pOrig,pElem);
				pRes = ph7_context_new_scalar(pCtx);
				if( pNsObj == 0 || pRes == 0 ){
					if( pNsObj ){
						PH7_ClassInstanceUnref(pNsObj);
					}
					break;
				}
				/* pRes CARRIES the constructor's reference (no bump here): the
				 * array's insert takes its own, and the call context's release
				 * of pRes at method end consumes ours -- ending at exactly the
				 * array's one. */
				pRes->x.pOther = pNsObj;
				pRes->iFlags = MEMOBJ_OBJ;
				ph7_array_add_elem(pSnap,0,pRes);
				continue;
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
	pList = DomNewCollection(pVm,DomCollClass(pDoc,"DOMNodeList","Dom\\NodeList"),
		pDoc,DNL_SNAP,0,0,0,pSnap);
	if( pList == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pList);
	return PH7_OK;
}
/*
 * The one evaluation body under DOMXPath::query and DOMXPath::evaluate. The
 * two differ only in what they make of the RESULT: query wants a node list
 * (a scalar gets the empty one), evaluate answers the XPath TYPE as php's
 * value -- boolean as bool, number as float, string as string, nodeset as
 * the same snapshot list. An expression that does not evaluate (bad grammar,
 * unknown function, unresolved prefix) answers false from both, with the
 * libxml diagnostics on the shared queue.
 */
/* php 8.4's two evaluators DECLARE an answer -- `Dom\NodeList` for query(), a
 * union without `false` for evaluate() -- so an evaluation that does not happen
 * cannot be reported as `false` the way the 2004 pair reports it. php raises its
 * own plain Error after the libxml warning it has already printed. */
static int DomXPathNoAnswer(ph7_context *pCtx,int bModern)
{
	if( bModern ){
		return PH7_VmThrowException(pCtx,"Error",
			"Could not evaluate XPath expression");
	}
	ph7_result_bool(pCtx,0);
	return PH7_OK;
}
static int DomXPathEvalRun(ph7_context *pCtx,int nArg,ph7_value **apArg,
	const char *zName,int bTyped)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pDoc = pThis ? PH7_NativeAttrObj(pThis,XP_DOC) : 0;
	phl_domnode *pDocNd = DomResOf(pDoc);
	/* The diagnostic names the class the CALL was made on, and the answer shape
	 * follows the document's family -- the same C body serving both trees. */
	int bModern = DomDocFlag(pDoc,DOM_F_MODERN);
	char zMethod[64];
	const char *zExpr = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	phl_domnode *pCtxNd = (nArg > 1 && !ph7_value_is_null(apArg[1])) ? DomObjArg(apArg[1]) : 0;
	/* php's stub says `= true`, but the live default of the third argument is
	 * the registerNodeNamespaces PROPERTY (the constructor's second argument
	 * lands there, and a later property write moves the default with it). */
	int bRegNodeNs = nArg > 2 ? ph7_value_to_bool(apArg[2])
		: (pThis ? PH7_NativeAttrTruthy(pThis,XP_NSDEF) : 1);
	xmlXPathContextPtr pXCtx;
	xmlXPathObjectPtr pObj;
	xmlNsPtr *aNodeNs;
	DomXPathFnCtx sFn;
	sxu32 nMark;
	sxi32 rc;
	SyBufferFormat(zMethod,sizeof(zMethod),"%s::%s",DomXPathClassName(pThis),zName);
	if( pDocNd == 0 ){
		return DomXPathNoAnswer(pCtx,bModern);
	}
	if( pCtxNd && pCtxNd->pNode
	 && ((xmlNodePtr)pCtxNd->pNode)->doc != (xmlDocPtr)pDocNd->pNode ){
		/* php's plain Error, no DOM code -- a context node of another document
		 * (or of none, a constructed node) cannot anchor this evaluation. */
		return PH7_VmThrowException(pCtx,"Error","Node from wrong document");
	}
	aNodeNs = DomXPathCtxOpen(pCtx,pDocNd,pCtxNd,bRegNodeNs,&pXCtx);
	if( pXCtx == 0 ){
		return DomXPathNoAnswer(pCtx,bModern);
	}
	/* The PHP-function bridge rides this one evaluation: the record lives on
	 * THIS stack frame, and libxml carries a pointer to it as its lookup data. */
	sFn.pCtx = pCtx;
	sFn.pThis = pThis;
	sFn.pDoc = pDoc;
	sFn.pDocNd = pDocNd;
	sFn.iErr = XP_FN_OK;
	sFn.rcUnwound = 0;
	SyBlobInit(&sFn.sErrName,&pVm->sAllocator);
	xmlXPathRegisterFuncLookup(pXCtx,DomXPathFnLookup,&sFn);
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pObj = xmlXPathEvalExpression((const xmlChar *)zExpr,pXCtx);
	PH7_LibxmlCaptureEnd(pVm,nMark,zMethod);
	if( aNodeNs ){
		pXCtx->namespaces = 0;
		pXCtx->nsNr = 0;
		xmlFree(aNodeNs);
	}
	if( sFn.rcUnwound != 0 || sFn.iErr != XP_FN_OK ){
		/* A callback did not return, or the bridge parked a refusal it could
		 * not raise from inside libxml's recursion. Either way the evaluation
		 * is over and this is its answer -- raised HERE, where the enclosing
		 * catch runs with libxml already unwound. */
		sxi32 rcFn = sFn.rcUnwound;
		if( pObj ){
			xmlXPathFreeObject(pObj);
		}
		xmlXPathFreeContext(pXCtx);
		if( rcFn == 0 ){
			rcFn = DomXPathFnRaise(pCtx,&sFn);
		}else{
			pCtx->nThrowRc = rcFn;
		}
		SyBlobRelease(&sFn.sErrName);
		return rcFn;
	}
	SyBlobRelease(&sFn.sErrName);
	if( pObj == 0 ){
		xmlXPathFreeContext(pXCtx);
		return DomXPathNoAnswer(pCtx,bModern);
	}
	if( !bTyped ){
		rc = DomXPathResultList(pCtx,pDoc,pDocNd,pObj);
	}else{
		switch( pObj->type ){
		case XPATH_BOOLEAN:
			ph7_result_bool(pCtx,pObj->boolval);
			rc = PH7_OK;
			break;
		case XPATH_NUMBER:
			ph7_result_double(pCtx,pObj->floatval);
			rc = PH7_OK;
			break;
		case XPATH_STRING:
			ph7_result_string(pCtx,pObj->stringval ? (const char *)pObj->stringval : "",-1);
			rc = PH7_OK;
			break;
		case XPATH_NODESET:
			rc = DomXPathResultList(pCtx,pDoc,pDocNd,pObj);
			break;
		default:
			ph7_result_bool(pCtx,0);
			rc = PH7_OK;
			break;
		}
	}
	xmlXPathFreeObject(pObj);
	xmlXPathFreeContext(pXCtx);
	return rc;
}
/*
 * DOMXPath::query(string $expression, ?DOMNode $contextNode = null,
 *                 bool $registerNodeNS = true): DOMNodeList|false
 */
DOM_METHOD(vm_builtin_DOMXPath_query)
{
	return DomXPathEvalRun(pCtx,nArg,apArg,"query",0);
}
/*
 * DOMXPath::evaluate(string $expression, ?DOMNode $contextNode = null,
 *                    bool $registerNodeNS = true): mixed
 */
DOM_METHOD(vm_builtin_DOMXPath_evaluate)
{
	return DomXPathEvalRun(pCtx,nArg,apArg,"evaluate",1);
}
/* DOMXPath::__construct(DOMDocument $document, bool $registerNodeNS = true) */
DOM_METHOD(vm_builtin_DOMXPath_construct)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis && nArg > 0 && (apArg[0]->iFlags & MEMOBJ_OBJ) ){
		PH7_NativeSetAttrObj(pCtx->pVm,pThis,XP_DOC,
			(ph7_class_instance *)apArg[0]->x.pOther);
	}
	if( pThis && nArg > 1 ){
		PH7_NativeSetAttrBool(pCtx->pVm,pThis,XP_NSDEF,
			ph7_value_to_bool(apArg[1]));
	}
	return PH7_OK;
}
/*
 * DOMXPath::registerNamespace(string $prefix, string $namespace): bool
 *
 * php hands the pair to xmlXPathRegisterNs on its persistent context and
 * answers its status: only the EMPTY prefix refuses (an invalid NCName one is
 * taken, and an empty URI is a registration too -- the prefix then resolves,
 * to a namespace nothing is in). Here the pair goes into the per-object table
 * the next evaluation replays.
 */
DOM_METHOD(vm_builtin_DOMXPath_registerNamespace)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nPfx = 0;
	const char *zPfx = nArg > 0 ? ph7_value_to_string(apArg[0],&nPfx) : "";
	ph7_hashmap *pMap;
	ph7_value sKey,sVal;
	if( nPfx < 1 || nArg < 2 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	pMap = DomXPathSlotMap(pVm,pThis,XP_NSREG);
	if( pMap == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	PH7_MemObjInitFromString(pVm,&sKey,0);
	PH7_MemObjStringAppend(&sKey,zPfx,(sxu32)nPfx);
	PH7_MemObjInitFromString(pVm,&sVal,0);
	{
		int nUri = 0;
		const char *zUri = ph7_value_to_string(apArg[1],&nUri);
		PH7_MemObjStringAppend(&sVal,zUri,(sxu32)nUri);
	}
	PH7_HashmapInsert(pMap,&sKey,&sVal);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* One row of the $restrict ARRAY: the value must be callable, and the NAME an
 * expression calls it by is the string key when there is one -- php's alias --
 * and otherwise the value coerced to a string (an array callable therefore
 * registers under "Array", with php's own conversion notice). */
struct DomXPathRestrict {
	ph7_context *pCtx;
	ph7_hashmap *pMap;
	sxi32 rc;
};
static int DomXPathRestrictRow(ph7_value *pKey,ph7_value *pVal,void *pUserData)
{
	struct DomXPathRestrict *pWalk = (struct DomXPathRestrict *)pUserData;
	ph7_vm *pVm = pWalk->pCtx->pVm;
	char zBuf[128];
	const char *zWhy;
	if( pWalk->rc != PH7_OK ){
		return PH7_OK;
	}
	zWhy = PH7_VmCallableReason(pVm,pVal,zBuf,(int)sizeof(zBuf));
	if( zWhy ){
		pWalk->rc = PH7_VmThrowException(pWalk->pCtx,"TypeError",
			"%s::registerPhpFunctions(): Argument #1 ($restrict) must be an array "
			"with valid callbacks as values, %s",
			DomXPathClassName(PH7_ContextThis(pWalk->pCtx)),zWhy);
		return PH7_ABORT;
	}
	if( pKey && ph7_value_is_string(pKey) ){
		int nKey = 0;
		const char *zKey = ph7_value_to_string(pKey,&nKey);
		DomXPathMapPut(pVm,pWalk->pMap,zKey,nKey,pVal);
	}else{
		/* ph7_value_to_string COERCES in place, which would rewrite the map's
		 * own value; name off a copy. */
		ph7_value sName;
		int nName = 0;
		const char *zName;
		PH7_MemObjInit(pVm,&sName);
		PH7_MemObjStore(pVal,&sName);
		zName = ph7_value_to_string(&sName,&nName);
		DomXPathMapPut(pVm,pWalk->pMap,zName,nName,pVal);
		PH7_MemObjRelease(&sName);
	}
	return PH7_OK;
}
/*
 * DOMXPath::registerPhpFunctions(array|string|null $restrict = null): void
 *
 * Bare (or null) opens the door to ANY callable name; a string or an array
 * restricts it to the named ones, accumulating across calls -- a later bare
 * call re-opens without forgetting the table, and a later restriction closes
 * it again with everything registered so far still reachable. Each name is
 * screened for callability HERE, so an evaluation never has to.
 */
DOM_METHOD(vm_builtin_DOMXPath_registerPhpFunctions)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_hashmap *pMap;
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( nArg < 1 || ph7_value_is_null(apArg[0]) ){
		PH7_NativeSetAttrInt(pVm,pThis,XP_FNMODE,XP_MODE_ALL);
		return PH7_OK;
	}
	pMap = DomXPathSlotMap(pVm,pThis,XP_FNREG);
	if( pMap == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* The mode moves FIRST, and each row is taken as it is screened: php's
	 * refusal leaves the object restricted with everything registered up to
	 * the bad row -- `registerPhpFunctions(['strrev','nope','strtolower'])`
	 * throws, and afterwards strrev runs while strtolower does not. */
	PH7_NativeSetAttrInt(pVm,pThis,XP_FNMODE,XP_MODE_LIST);
	if( ph7_value_is_array(apArg[0]) ){
		struct DomXPathRestrict sWalk;
		sWalk.pCtx = pCtx;
		sWalk.pMap = pMap;
		sWalk.rc = PH7_OK;
		ph7_array_walk(apArg[0],DomXPathRestrictRow,&sWalk);
		if( sWalk.rc != PH7_OK ){
			return sWalk.rc;
		}
	}else{
		char zBuf[128];
		const char *zWhy = PH7_VmCallableReason(pVm,apArg[0],zBuf,(int)sizeof(zBuf));
		int nName = 0;
		const char *zName;
		if( zWhy ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"%s::registerPhpFunctions(): Argument #1 ($restrict) must be a callable, %s",
				DomXPathClassName(PH7_ContextThis(pCtx)),zWhy);
		}
		zName = ph7_value_to_string(apArg[0],&nName);
		DomXPathMapPut(pVm,pMap,zName,nName,apArg[0]);
	}
	return PH7_OK;
}
/* php's callback NAME grammar for registerPhpFunctionNS: an XML NCName, which
 * is what an expression can spell as a function name. */
static int DomXPathIsCallbackName(const char *zName,int nName)
{
	int i;
	if( nName < 1 ){
		return 0;
	}
	if( xmlValidateNCName((const xmlChar *)zName,0) != 0 ){
		return 0;
	}
	/* xmlValidateNCName reads to the NUL, and a name may not carry one. */
	for( i = 0 ; i < nName ; ++i ){
		if( zName[i] == 0 ){
			return 0;
		}
	}
	return (int)SyStrlen(zName) == nName;
}
/*
 * DOMXPath::registerPhpFunctionNS(string $namespaceURI, string $name,
 *                                 callable $callable): void
 *
 * php 8.4's narrow door: one callable under one name in the caller's OWN
 * namespace -- no `php:function("name")` indirection, and independent of
 * registerPhpFunctions' mode (it neither needs it nor opens it). php's own
 * URI is refused, the name must be an NCName, and the callable is screened
 * here.
 */
DOM_METHOD(vm_builtin_DOMXPath_registerPhpFunctionNS)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nUri = 0,nName = 0;
	const char *zUri = nArg > 0 ? ph7_value_to_string(apArg[0],&nUri) : "";
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],&nName) : "";
	char zBuf[128];
	const char *zWhy;
	ph7_hashmap *pMap;
	SyBlob sKey;
	if( pThis == 0 || nArg < 3 ){
		return PH7_OK;
	}
	if( nUri == (int)sizeof(XP_PHPNS)-1 && SyMemcmp(zUri,XP_PHPNS,(sxu32)nUri) == 0 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::registerPhpFunctionNS(): Argument #1 ($namespaceURI) must not be "
			"\"%s\" because it is reserved by PHP",DomXPathClassName(pThis),XP_PHPNS);
	}
	if( !DomXPathIsCallbackName(zName,nName) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"%s::registerPhpFunctionNS(): Argument #2 ($name) must be a valid callback name",
			DomXPathClassName(pThis));
	}
	zWhy = PH7_VmCallableReason(pVm,apArg[2],zBuf,(int)sizeof(zBuf));
	if( zWhy ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"%s::registerPhpFunctionNS(): Argument #3 ($callable) must be a valid callback, %s",
			DomXPathClassName(pThis),zWhy);
	}
	pMap = DomXPathSlotMap(pVm,pThis,XP_NSFN);
	if( pMap == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* One key from the pair: a URI cannot carry \x01, so the join is
	 * unambiguous without escaping. */
	SyBlobInit(&sKey,&pVm->sAllocator);
	SyBlobAppend(&sKey,zUri,(sxu32)nUri);
	SyBlobAppend(&sKey,"\1",1);
	SyBlobAppend(&sKey,zName,(sxu32)nName);
	DomXPathMapPut(pVm,pMap,(const char *)SyBlobData(&sKey),(int)SyBlobLength(&sKey),apArg[2]);
	SyBlobRelease(&sKey);
	return PH7_OK;
}

/* ===== Schema validation ===== */

/*
 * The four schema doors -- {XML Schema, RelaxNG} x {a FILE, a STRING} -- and
 * php's `validate()` beside them, all one shape:
 *
 *   parse the schema (loudly: every libxml complaint reaches the caller's
 *   error handler), and if that fails say "Invalid Schema" / "Invalid RelaxNG"
 *   and answer false; otherwise validate the document and answer whether it
 *   came back clean.
 *
 * Only the pair of libxml families differs, so the switch is four calls wide
 * and the plumbing -- the argument screens, the diagnostic capture, the
 * refusals -- is written once.  The names a caller sees are php's: a filename
 * that is empty or carries a NUL is a ValueError naming the argument, raised
 * before anything is opened.
 */
#define DOM_VAL_SCHEMA 0
#define DOM_VAL_RELAX  1

/*
 * Schema, RelaxNG and DTD-validity diagnostics: onto the shared per-VM queue
 * through PH7_LibxmlQueueError, exactly like the global structured handler.
 *
 * php installs libxml's printf-style pair here instead, which is why its
 * validation diagnostics read as libxml writes them -- "I/O warning : failed
 * to load external entity ...", a parse error over three lines with the
 * offending source and a caret under it -- while every message this engine
 * drains is one structured record with its location appended.  The structured
 * handler is the one this file must keep: it is also what feeds
 * `libxml_get_errors()`, and php's own switches to exactly this shape once
 * `libxml_use_internal_errors(true)` is on.  The ANSWERS agree; the wording of
 * a failure does not (the error-format class).
 */
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
/* php's own last word when a schema will not parse, under the method's name. */
static void DomValidateSaySo(ph7_vm *pVm,const char *zFn,const char *zWhat)
{
	SyBlob sMsg;
	SyBlobInit(&sMsg,&pVm->sAllocator);
	SyBlobFormat(&sMsg,"%s(): %s",zFn,zWhat);
	SyBlobNullAppend(&sMsg);
	PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,(const char *)SyBlobData(&sMsg));
	SyBlobRelease(&sMsg);
}
/*
 * The argument every schema door takes: a filename or the schema itself. The
 * two refusals are php's own and answer before any parse.
 */
static int DomValidateArg(ph7_context *pCtx,int nArg,ph7_value **apArg,int bFile,
	const char *zFn,const char **pzSrc,int *pnSrc,int *pRc)
{
	int nSrc = 0;
	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nSrc) : "";
	const char *zParam = bFile ? "filename" : "source";
	if( bFile && nSrc != (int)SyStrlen(zSrc) ){
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($%s) must not contain any null bytes",zFn,zParam);
		return 0;
	}
	if( nSrc < 1 ){
		*pRc = PH7_VmThrowException(pCtx,"ValueError",
			"%s(): Argument #1 ($%s) must not be empty",zFn,zParam);
		return 0;
	}
	*pzSrc = zSrc;
	*pnSrc = nSrc;
	return 1;
}
static int DomValidateRun(ph7_context *pCtx,int nArg,ph7_value **apArg,int iKind,
	int bFile,const char *zFn)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = DomThisNode(pCtx);
	int nSrc = 0,rc = PH7_OK,iRc;
	const char *zSrc = "";
	sxu32 nMark;
	/* php reads the option word from the SCHEMA pair only; RelaxNG's two
	 * declare no second parameter at all. LIBXML_SCHEMA_CREATE is the one bit
	 * it acts on -- "write the schema's default values into the document". */
	int bCreate = iKind == DOM_VAL_SCHEMA && nArg > 1
		&& (ph7_value_to_int(apArg[1]) & XML_SCHEMA_VAL_VC_I_CREATE) != 0;
	if( !DomValidateArg(pCtx,nArg,apArg,bFile,zFn,&zSrc,&nSrc,&rc) ){
		return rc;
	}
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	if( iKind == DOM_VAL_SCHEMA ){
		xmlSchemaParserCtxtPtr pParser = bFile ? xmlSchemaNewParserCtxt(zSrc)
		                                       : xmlSchemaNewMemParserCtxt(zSrc,nSrc);
		xmlSchemaPtr pSchema;
		xmlSchemaValidCtxtPtr pValid;
		if( pParser == 0 ){
			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		xmlSchemaSetParserStructuredErrors(pParser,DomSchemaErr,pVm);
		pSchema = xmlSchemaParse(pParser);
		xmlSchemaFreeParserCtxt(pParser);
		if( pSchema == 0 ){
			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
			DomValidateSaySo(pVm,zFn,"Invalid Schema");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pValid = xmlSchemaNewValidCtxt(pSchema);
		if( pValid == 0 ){
			xmlSchemaFree(pSchema);
			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		if( bCreate ){
			xmlSchemaSetValidOptions(pValid,XML_SCHEMA_VAL_VC_I_CREATE);
		}
		xmlSchemaSetValidStructuredErrors(pValid,DomSchemaErr,pVm);
		iRc = xmlSchemaValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);
		xmlSchemaFreeValidCtxt(pValid);
		xmlSchemaFree(pSchema);
	}else{
		xmlRelaxNGParserCtxtPtr pParser = bFile ? xmlRelaxNGNewParserCtxt(zSrc)
		                                        : xmlRelaxNGNewMemParserCtxt(zSrc,nSrc);
		xmlRelaxNGPtr pSchema;
		xmlRelaxNGValidCtxtPtr pValid;
		if( pParser == 0 ){
			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		xmlRelaxNGSetParserStructuredErrors(pParser,DomSchemaErr,pVm);
		pSchema = xmlRelaxNGParse(pParser);
		xmlRelaxNGFreeParserCtxt(pParser);
		if( pSchema == 0 ){
			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
			DomValidateSaySo(pVm,zFn,"Invalid RelaxNG");
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		pValid = xmlRelaxNGNewValidCtxt(pSchema);
		if( pValid == 0 ){
			xmlRelaxNGFree(pSchema);
			PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
			ph7_result_bool(pCtx,0);
			return PH7_OK;
		}
		xmlRelaxNGSetValidStructuredErrors(pValid,DomSchemaErr,pVm);
		iRc = xmlRelaxNGValidateDoc(pValid,(xmlDocPtr)pDocNd->pNode);
		xmlRelaxNGFreeValidCtxt(pValid);
		xmlRelaxNGFree(pSchema);
	}
	PH7_LibxmlCaptureEnd(pVm,nMark,zFn);
	ph7_result_bool(pCtx,iRc == 0);
	return PH7_OK;
}
/* DOMDocument::schemaValidate(string $filename, int $flags = 0): bool */
DOM_METHOD(vm_builtin_DOMDocument_schemaValidate)
{
	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_SCHEMA,TRUE,"DOMDocument::schemaValidate");
}
/* DOMDocument::schemaValidateSource(string $source, int $flags = 0): bool */
DOM_METHOD(vm_builtin_DOMDocument_schemaValidateSource)
{
	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_SCHEMA,FALSE,"DOMDocument::schemaValidateSource");
}
/* DOMDocument::relaxNGValidate(string $filename): bool */
DOM_METHOD(vm_builtin_DOMDocument_relaxNGValidate)
{
	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_RELAX,TRUE,"DOMDocument::relaxNGValidate");
}
/* DOMDocument::relaxNGValidateSource(string $source): bool */
DOM_METHOD(vm_builtin_DOMDocument_relaxNGValidateSource)
{
	return DomValidateRun(pCtx,nArg,apArg,DOM_VAL_RELAX,FALSE,"DOMDocument::relaxNGValidateSource");
}
/*
 * DOMDocument::validate(): bool -- against the document's OWN DTD, which is
 * the one question of the five that takes no argument. libxml's validity
 * complaints ("no DTD found!", "root and DTD name do not match") reach the
 * caller through the same per-VM queue every other diagnostic here does.
 */
DOM_METHOD(vm_builtin_DOMDocument_validate)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = DomThisNode(pCtx);
	xmlValidCtxtPtr pValid;
	sxu32 nMark;
	int iRc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pValid = xmlNewValidCtxt();
	if( pValid == 0 ){
		PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::validate");
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	iRc = xmlValidateDocument(pValid,(xmlDocPtr)pDocNd->pNode);
	xmlFreeValidCtxt(pValid);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::validate");
	ph7_result_bool(pCtx,iRc != 0);
	return PH7_OK;
}
/*
 * The MARKERS libxml leaves around everything it substituted.
 *
 * An XInclude pass wraps each replacement in an XML_XINCLUDE_START /
 * XML_XINCLUDE_END pair, which are nodes in the tree like any other: they
 * answer from `childNodes`, they shift every index after them, and the first
 * child of an element whose only content was an `<xi:include>` is one of them
 * rather than what was included.  php takes them out before answering, so the
 * document a caller gets back is the substituted one and nothing else.  They
 * are parked on the orphan set rather than freed, like every other node this
 * file unlinks.
 */
static void DomDropXIncludeMarks(phl_xmldoc *pShell,xmlNodePtr pNode)
{
	xmlNodePtr pNext;
	while( pNode ){
		pNext = pNode->next;
		if( pNode->type == XML_XINCLUDE_START || pNode->type == XML_XINCLUDE_END ){
			xmlUnlinkNode(pNode);
			DomOrphanAdd(pShell,pNode);
		}else{
			DomDropXIncludeMarks(pShell,pNode->children);
		}
		pNode = pNext;
	}
}
/*
 * DOMDocument::xinclude(int $options = 0): int|false
 *
 * php answers the COUNT of substitutions libxml made, -1 when one of them
 * failed -- and FALSE when there were none at all, which is not an error and
 * is the one answer a caller has to screen for separately.
 */
DOM_METHOD(vm_builtin_DOMDocument_xinclude)
{
	ph7_vm *pVm = pCtx->pVm;
	phl_domnode *pDocNd = DomThisNode(pCtx);
	int iOpts = nArg > 0 ? ph7_value_to_int(apArg[0]) : 0;
	sxu32 nMark;
	int nDone;
	if( pDocNd == 0 ){
		ph7_result_bool(pCtx,0);
		return PH7_OK;
	}
	nMark = PH7_LibxmlCaptureBegin(pVm);
	nDone = xmlXIncludeProcessFlags((xmlDocPtr)pDocNd->pNode,iOpts);
	PH7_LibxmlCaptureEnd(pVm,nMark,"DOMDocument::xinclude");
	if( nDone >= 0 ){
		DomDropXIncludeMarks(pDocNd->pShell,
			((xmlDocPtr)pDocNd->pNode)->children);
	}
	if( nDone == 0 ){
		ph7_result_bool(pCtx,0);
	}else{
		ph7_result_int(pCtx,nDone);
	}
	return PH7_OK;
}

/*
 * DOMDocument::registerNodeClass(string $baseClass, ?string $extendedClass): true
 *
 * php lets a program say which class a node should be WRAPPED in, per
 * document: register `MyElement` against `DOMElement` and every element of
 * that document -- read from the tree or made by a factory -- comes back a
 * MyElement, so a walk can call the program's own methods on what it finds
 * instead of carrying a parallel table of its own.
 *
 * The lookup is by the class the extension would have used and by nothing
 * else: registering against `DOMNode` or `DOMCharacterData` changes NO
 * wrapping, because an element is wrapped as a DOMElement and a text node as a
 * DOMText, and neither name is the one registered.
 *
 * The map is the document's, stored in a hidden slot beside its identity cache
 * and carried by a document CLONE the way the parser directives are.
 */
/* The map, materialized on the document the way its identity cache is. */
static ph7_hashmap * DomNodeClassMap(ph7_vm *pVm,ph7_class_instance *pDoc,int bMake)
{
	ph7_value *pSlot = pDoc ? PH7_NativeAttr(pDoc,DOM_NCLS) : 0;
	if( pSlot == 0 || (!bMake && (pSlot->iFlags & MEMOBJ_HASHMAP) == 0) ){
		return 0;
	}
	if( (pSlot->iFlags & MEMOBJ_HASHMAP) == 0 ){
		if( PH7_MemObjToHashmap(pSlot) != SXRET_OK ){
			return 0;
		}
	}
	return PH7_HashmapCowSeparate(&(*pVm),pSlot);
}
/* The class a node of pDoc's tree is wrapped in: the registered one when the
 * document names it, php's own otherwise. */
static const char * DomWrapClassName(ph7_vm *pVm,ph7_class_instance *pDoc,int iKind,
	SyBlob *pOut)
{
	const char *zBase = DomClassOfKind(DomDocFlag(pDoc,DOM_F_MODERN),iKind);
	ph7_hashmap *pMap = DomNodeClassMap(&(*pVm),pDoc,FALSE);
	ph7_hashmap_node *pEntry = 0;
	ph7_value sKey,*pHit;
	if( pMap == 0 ){
		return zBase;
	}
	PH7_MemObjInitFromString(&(*pVm),&sKey,0);
	PH7_MemObjStringAppend(&sKey,zBase,(sxu32)SyStrlen(zBase));
	if( PH7_HashmapLookup(pMap,&sKey,&pEntry) == SXRET_OK && pEntry ){
		pHit = HashmapExtractNodeValue(pEntry);
		if( pHit && (pHit->iFlags & MEMOBJ_STRING) ){
			SyBlobAppend(pOut,SyBlobData(&pHit->sBlob),SyBlobLength(&pHit->sBlob));
			SyBlobNullAppend(pOut);
			zBase = (const char *)SyBlobData(pOut);
		}
	}
	PH7_MemObjRelease(&sKey);
	return zBase;
}
DOM_METHOD(vm_builtin_DOMDocument_registerNodeClass)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nBase = 0,nExt = 0;
	const char *zBase = nArg > 0 ? ph7_value_to_string(apArg[0],&nBase) : "";
	const char *zExt = (nArg > 1 && !ph7_value_is_null(apArg[1]))
		? ph7_value_to_string(apArg[1],&nExt) : 0;
	ph7_class *pBase,*pExt = 0,*pNode;
	ph7_hashmap *pMap;
	ph7_value sKey,sVal;
	pBase = PH7_VmExtractClass(pVm,zBase,(sxu32)nBase,FALSE,0);
	pNode = PH7_VmExtractClass(pVm,"DOMNode",sizeof("DOMNode")-1,FALSE,0);
	if( pBase == 0 || pNode == 0 || !PH7_VmInstanceOf(pBase,pNode) ){
		return PH7_VmThrowException(pCtx,"TypeError",
			"DOMDocument::registerNodeClass(): Argument #1 ($baseClass) must be a "
			"class name derived from DOMNode, %.*s given",nBase,zBase);
	}
	if( zExt ){
		pExt = PH7_VmExtractClass(pVm,zExt,(sxu32)nExt,FALSE,0);
		if( pExt == 0 ){
			return PH7_VmThrowException(pCtx,"TypeError",
				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be "
				"a valid class name or null, %.*s given",nExt,zExt);
		}
		if( !PH7_VmInstanceOf(pExt,pBase) ){
			/* php's plain Error here, not a TypeError: the name IS a class, it
			 * is simply the wrong one. */
			return PH7_VmThrowException(pCtx,"Error",
				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must be "
				"a class name derived from %z or null, %.*s given",
				&pBase->sDisp,nExt,zExt);
		}
		if( pExt->iFlags & PH7_CLASS_ABSTRACT ){
			return PH7_VmThrowException(pCtx,"ValueError",
				"DOMDocument::registerNodeClass(): Argument #2 ($extendedClass) must "
				"not be an abstract class");
		}
	}
	pMap = DomNodeClassMap(pVm,pThis,TRUE);
	if( pMap == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* Keyed by the base's OWN spelling, which is the one the wrap looks up. */
	PH7_MemObjInitFromString(pVm,&sKey,&pBase->sName);
	if( pExt ){
		PH7_MemObjInitFromString(pVm,&sVal,&pExt->sName);
		PH7_HashmapInsert(pMap,&sKey,&sVal);
		PH7_MemObjRelease(&sVal);
	}else{
		ph7_hashmap_node *pEntry = 0;
		if( PH7_HashmapLookup(pMap,&sKey,&pEntry) == SXRET_OK && pEntry ){
			PH7_HashmapUnlinkNode(pEntry,TRUE);
		}
	}
	PH7_MemObjRelease(&sKey);
	ph7_result_bool(pCtx,1);
	return PH7_OK;
}

/* ===== DOMImplementation ===== */

/*
 * php's factory for the two things that cannot be made from a document that
 * does not exist yet: a DOCTYPE, and a document with a namespaced root.
 *
 * It carries no state at all -- `new DOMImplementation` is enough, its three
 * methods are ordinary instance methods, and `$doc->implementation` answers a
 * FRESH one on every read.
 */

/* A node that belongs to NO document, wrapped and owned the way a constructed
 * one is: parked on the per-VM limbo shell, its own identity-cache holder. The
 * caller owns the reference. */
static ph7_class_instance * DomLimboWrap(ph7_vm *pVm,xmlNodePtr pNode)
{
	const char *zClass = DomClassOfKind(0,(int)pNode->type);
	ph7_class *pClass = PH7_VmExtractClass(&(*pVm),zClass,(sxu32)SyStrlen(zClass),FALSE,0);
	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(&(*pVm),pClass) : 0;
	phl_xmldoc *pShell = pObj ? DomLimboShell(&(*pVm)) : 0;
	phl_domnode *pRes = pShell ? DomNewRes(&(*pVm),pShell,pNode) : 0;
	if( pRes == 0 ){
		if( pObj ){
			PH7_ClassInstanceUnref(pObj);
		}
		return 0;
	}
	DomOrphanAdd(pShell,pNode);
	DomSetRes(&(*pVm),pObj,pRes);
	PH7_NativeSetAttrObj(&(*pVm),pObj,DOM_DOC,pObj);
	DomCacheStore(&(*pVm),pObj,pNode,pObj);
	return pObj;
}
/*
 * DOMImplementation::hasFeature(string $feature, string $version): bool
 *
 * php's table is two rows wide and the version is compared as a STRING: only
 * "1.0", "2.0" and "" are versions at all, and of those "Core" answers for
 * "1.0" alone where "XML" answers for every one. So `hasFeature('Core','2.0')`
 * is false while `hasFeature('XML','2.0')` is true, and `hasFeature('Core','1')`
 * -- a version that is not spelled the way the table spells it -- is false.
 */
DOM_METHOD(vm_builtin_DOMImplementation_hasFeature)
{
	const char *zFeature = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	const char *zVersion = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	int bKnown = DomNameIs(zVersion,"1.0") || DomNameIs(zVersion,"2.0")
		|| zVersion[0] == 0;
	ph7_result_bool(pCtx,bKnown
		&& (DomNameIsCi(zFeature,"XML")
			|| (DomNameIsCi(zFeature,"Core") && DomNameIs(zVersion,"1.0"))));
	return PH7_OK;
}
/*
 * DOMImplementation::createDocumentType(string $qualifiedName,
 *     string $publicId = '', string $systemId = ''): DOMDocumentType
 *
 * The name is not checked at ALL beyond being non-empty -- `1bad`, `a b` and
 * `p:q:r` are each a doctype php builds without a word -- because nothing has
 * parsed it: the name is the bytes the `<!DOCTYPE ...>` line will carry.
 */
DOM_METHOD(vm_builtin_DOMImplementation_createDocumentType)
{
	int nName = 0;
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0],&nName) : "";
	const char *zPub = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "";
	const char *zSys = nArg > 2 ? ph7_value_to_string(apArg[2],0) : "";
	ph7_class_instance *pObj;
	xmlDtdPtr pDtd;
	if( nName < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"DOMImplementation::createDocumentType(): Argument #1 ($qualifiedName) "
			"must not be empty");
	}
	pDtd = xmlNewDtd(0,(const xmlChar *)zName,
		zPub[0] ? (const xmlChar *)zPub : 0,
		zSys[0] ? (const xmlChar *)zSys : 0);
	if( pDtd == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	pObj = DomLimboWrap(pCtx->pVm,(xmlNodePtr)pDtd);
	if( pObj == 0 ){
		xmlFreeDtd(pDtd);
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pObj);
	return PH7_OK;
}
/*
 * DOMImplementation::createDocument(?string $namespace = null,
 *     string $qualifiedName = '', ?DOMDocumentType $doctype = null): DOMDocument
 *
 * An empty qualified name is a document with no root at all, which is what
 * makes the three-argument call with only a doctype meaningful.  The name is a
 * QName or nothing (`1bad` and `a:b:c` are the Namespace Error), and the
 * namespace decides what becomes of its PREFIX: with a URI the element is
 * declared under it, and WITHOUT one the prefix is simply dropped -- php
 * builds the element from the local name and hangs the declaration on it
 * afterwards, so `createDocument('', 'p:root')` is `<root/>`.
 *
 * A doctype that already belongs to a document is the Wrong Document Error,
 * so the same DOMDocumentType cannot seed two documents.
 */
DOM_METHOD(vm_builtin_DOMImplementation_createDocument)
{
	ph7_vm *pVm = pCtx->pVm;
	const xmlChar *zUri = DomArgUri(nArg,apArg,0);
	int nName = 0;
	const char *zName = nArg > 1 ? ph7_value_to_string(apArg[1],&nName) : "";
	phl_domnode *pDtdNd = (nArg > 2 && !ph7_value_is_null(apArg[2])) ? DomObjArg(apArg[2]) : 0;
	xmlDtdPtr pDtd = pDtdNd ? (xmlDtdPtr)pDtdNd->pNode : 0;
	ph7_class *pClass;
	ph7_class_instance *pObj;
	phl_xmldoc *pShell;
	phl_domnode *pRes;
	xmlDocPtr pDoc;
	xmlNodePtr pRoot = 0;
	xmlNsPtr pNs = 0;
	xmlChar *zPrefix = 0,*zLocal = 0;
	if( pDtd && pDtd->doc ){
		/* php's own screen, and the reason a doctype seeds ONE document. */
		return DomThrowAlways(pCtx,DOM_ERR_WRONG_DOC);
	}
	if( nName > 0 ){
		if( xmlValidateQName((const xmlChar *)zName,0) != 0 ){
			return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);
		}
		zLocal = xmlSplitQName2((const xmlChar *)zName,&zPrefix);
		if( zLocal == 0 ){
			zLocal = xmlStrdup((const xmlChar *)zName);
		}
		if( zUri && zUri[0] ){
			/* php asks libxml for the declaration BEFORE it has a node to hang
			 * it on, and takes a refusal (the `xml` prefix over its own URI is
			 * one) as the Namespace Error. */
			pNs = xmlNewNs(0,zUri,zPrefix);
			if( pNs == 0 ){
				if( zLocal ){
					xmlFree(zLocal);
				}
				if( zPrefix ){
					xmlFree(zPrefix);
				}
				return DomThrowAlways(pCtx,DOM_ERR_NAMESPACE);
			}
		}
	}
	pDoc = xmlNewDoc((const xmlChar *)"1.0");
	pClass = pDoc ? PH7_VmExtractClass(pVm,"DOMDocument",sizeof("DOMDocument")-1,FALSE,0) : 0;
	pObj = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
	pShell = pObj ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;
	pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;
	if( pRes == 0 ){
		if( pDoc && pShell == 0 ){
			xmlFreeDoc(pDoc);
		}
		if( pObj ){
			PH7_ClassInstanceUnref(pObj);
		}
		if( pNs ){
			xmlFreeNs(pNs);
		}
		if( zLocal ){
			xmlFree(zLocal);
		}
		if( zPrefix ){
			xmlFree(zPrefix);
		}
		return PH7_ContextMemoryError(pCtx);
	}
	DomSetRes(pVm,pObj,pRes);
	PH7_NativeSetAttrObj(pVm,pObj,DOM_DOC,pObj);
	if( pDtd ){
		/* The doctype MOVES: it leaves the limbo shell for this document's
		 * tree, and every wrapper of it -- its own and its declarations' --
		 * re-homes on adoptNode's machinery, which is what makes the doctype
		 * answer this document as its `ownerDocument` afterwards rather than
		 * the holder it was its own. */
		ph7_class_instance *pDtdObj = (ph7_class_instance *)apArg[2]->x.pOther;
		DomOrphanRemove(pDtdNd->pShell,(xmlNodePtr)pDtd);
		pDtd->doc = pDoc;
		pDoc->intSubset = pDtd;
		DomLinkLast((xmlNodePtr)pDoc,(xmlNodePtr)pDtd);
		DomAdoptWrappers(pVm,pDtdObj,pObj,pShell,(xmlNodePtr)pDtd);
	}
	if( nName > 0 ){
		pRoot = xmlNewDocNode(pDoc,0,zLocal,0);
		if( pRoot ){
			xmlDocSetRootElement(pDoc,pRoot);
			if( pNs ){
				pNs->next = pRoot->nsDef;
				pRoot->nsDef = pNs;
				xmlSetNs(pRoot,pNs);
				pNs = 0;
			}
		}
	}
	if( pNs ){
		xmlFreeNs(pNs);
	}
	if( zLocal ){
		xmlFree(zLocal);
	}
	if( zPrefix ){
		xmlFree(zPrefix);
	}
	PH7_NativeResultObject(pCtx,pObj);
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
		/* The 2004 reader answers an ELEMENT's descendant text here, which is
		 * what its `textContent` answers too. php 8.4 went back to the standard:
		 * `nodeValue` is null for everything that is not character data, a
		 * processing instruction or an attribute, and `textContent` is the only
		 * one of the pair that walks the subtree. */
		if( DomThisModern(pCtx) && pNode != 0
		 && pNode->type != XML_TEXT_NODE && pNode->type != XML_CDATA_SECTION_NODE
		 && pNode->type != XML_COMMENT_NODE && pNode->type != XML_PI_NODE
		 && pNode->type != XML_ATTRIBUTE_NODE ){
			ph7_result_null(pCtx);
		}else{
			DomNodeValue(pCtx,pNode);
		}
	}else if( DomNameIs(zName,"nodeType") ){
		ph7_result_int(pCtx,DomNodeTypeOf(pNode));
	}else if( DomNameIs(zName,"textContent") ){
		/* Null, in the new tree, for the two kinds the standard says have no
		 * text at all -- a document and a doctype -- where the 2004 reader
		 * answers "" for both. */
		if( DomThisModern(pCtx) && pNode != 0
		 && (pNode->type == XML_DTD_NODE || pNode->type == XML_DOCUMENT_TYPE_NODE
		  || pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE) ){
			ph7_result_null(pCtx);
		}else{
			DomTextContent(pCtx,pNode);
		}
	}else if( DomNameIs(zName,"parentNode") ){
		DomResultNodeOf(pCtx,pNd,pNode ? pNode->parent : 0);
	}else if( DomNameIs(zName,"firstChild") ){
		/* Through the entity-reference resolver: an ADOPTED constructed
		 * reference's raw children are cleared, and php's reader answers the
		 * document's declaration anyway.
		 *
		 * Both ends are gated on php's dom_node_children_valid, which the
		 * DOCTYPE is not: libxml links a DTD's declarations as its children
		 * and php's `firstChild`/`lastChild`/`hasChildNodes()` answer null,
		 * null and false there all the same -- while `childNodes` (which does
		 * NOT consult it) lists them. */
		DomResultNodeOf(pCtx,pNd,DomNodeChildFirst(pNode));
	}else if( DomNameIs(zName,"lastChild") ){
		DomResultNodeOf(pCtx,pNd,DomNodeChildLast(pNode));
	}else if( DomNameIs(zName,"nextSibling") ){
		DomResultNodeOf(pCtx,pNd,
			DomAttrSiblingNext(DomThisModern(pCtx),pNd ? pNd->pShell : 0,pNode));
	}else if( DomNameIs(zName,"previousSibling") ){
		DomResultNodeOf(pCtx,pNd,
			DomAttrSiblingPrev(DomThisModern(pCtx),pNd ? pNd->pShell : 0,pNode));
	}else if( DomNameIs(zName,"ownerDocument") ){
		/* A document has no owner document, which is also why DomWrap answers
		 * the document itself rather than a second wrapper for it. The NODE's
		 * document is the source of truth, not the $__doc slot: a constructed
		 * ownerless node's slot points at its own holder, and php answers
		 * null there until an insertion adopts it. */
		DomResultWrap(pCtx,(bIsDoc || pNode == 0 || pNode->doc == 0) ? 0 : pDoc);
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
		/* php 8.4 declares it `?string` on the element and the attribute, where
		 * the 2004 DOMNode declares plain `string`: the same absent prefix reads
		 * null in one tree and "" in the other. */
		if( DomThisModern(pCtx) && (pNode == 0 || pNode->ns == 0 || pNode->ns->prefix == 0) ){
			ph7_result_null(pCtx);
		}else{
			DomPrefix(pCtx,pNode);
		}
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
		ph7_class_instance *pList = DomNewCollection(pVm,
			DomCollClass(pDoc,"DOMNodeList","Dom\\NodeList"),pDoc,DNL_CHILD,pThis,0,0,0);
		if( pList == 0 ){
			return -1;
		}
		PH7_NativeResultObject(pCtx,pList);
	}else if( DomNameIs(zName,"attributes") ){
		/* php: NULL for anything that is not an element. */
		if( pNode == 0 || pNode->type != XML_ELEMENT_NODE ){
			ph7_result_null(pCtx);
		}else{
			ph7_class_instance *pMap = DomNewCollection(pVm,
				DomCollClass(pDoc,"DOMNamedNodeMap","Dom\\NamedNodeMap"),
				pDoc,DNL_CHILD,pThis,0,0,0);
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
/*
 * Is this receiver a node class with NO node behind it?
 *
 * `new DOMNode()`, `new DOMCharacterData()`, `new DOMEntity()`, `new DOMNotation()`,
 * `new DOMDocumentType()` and `new DOMNameSpaceNode()` all construct in php and none
 * of them builds a libxml node -- so every property handler on such an object fetches
 * a null pointer and answers php's `DOMException: Invalid State Error`, on a read and
 * on an `isset()` alike. A name the class does NOT declare never reaches a handler at
 * all and keeps the ordinary undefined-property warning, which is why the test is
 * against the DECLARATION rather than against the reader.
 */
static int DomNodeLess(ph7_context *pCtx,const char *zName)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	phl_domnode *pNd;
	if( pThis == 0 || PH7_NativeAttr(pThis,DOM_RES) == 0 ){
		return 0;   /* not one of the classes that carries a node at all */
	}
	pNd = DomResOf(pThis);
	if( pNd && pNd->pNode ){
		return 0;
	}
	return PH7_ClassExtractAttribute(pThis->pClass,zName,(sxu32)SyStrlen(zName)) != 0;
}
/*
 * The write half. A per-class WRITER answers one of these; the name it does
 * not write is looked up in the class's READER by the property HOOK, which
 * decides between php's two refusals -- a property the table carries is
 * read-only, one it does not is nothing this class answers and goes back on
 * the ordinary path (where PHL's scope policy meets a dynamic property).
 */
#define DOM_SET_UNKNOWN  0   /* not a property of this class */
#define DOM_SET_DONE     1   /* written, or a refusal already raised into *pRc */
/*
 * php's `Cannot modify readonly property C::$p`, worded under the INSTANCE's
 * class so a userland subclass of DOMElement is reported under its own name.
 *
 * Three writers refuse a name of their own HERE rather than by declining it: the
 * two DTD halves whose read-only properties are DEPRECATED (php's readonly Error
 * carries no deprecation notice, and the reader would have raised one) and the
 * document's read-only four for the same reason.
 */
static sxi32 DomRefuseWrite(ph7_context *pCtx,const char *zName)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis == 0 ){
		return PH7_OK;
	}
	return DomPropThrow(pCtx,"Error",0,"Cannot modify readonly property %z::$%s",
		&pThis->pClass->sName,zName);
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
/* Is the receiver a document of php 8.4's tree? Only that tree's five extra
 * names are answered, and only there does an absent encoding or URI read as ""
 * rather than null -- the new declarations are plain `string` where the 2004
 * ones are `?string`. */
static int DomThisModern(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pDoc = pThis ? PH7_NativeAttr(pThis,DOM_DOC) : 0;
	return pDoc != 0 && (pDoc->iFlags & MEMOBJ_OBJ) != 0
		&& DomDocFlag((ph7_class_instance *)pDoc->x.pOther,DOM_F_MODERN);
}
/*
 * The five names php 8.4's document has that the 2004 one does not: the WHATWG
 * spellings of the two things a document knows about itself, and the three HTML
 * ones. `characterSet`, `charset` and `inputEncoding` are one answer under three
 * names, and it is the EFFECTIVE encoding -- a document parsed from source that
 * carries no declaration still reads "UTF-8", where the 2004 `xmlEncoding`
 * reads null for the same document.
 *
 * `body`, `head` and `title` are the HTML document's, and an XML one answers the
 * empty shape for each rather than refusing: null, null and "".
 */
static int DomModernDocProp(ph7_context *pCtx,const char *zName,xmlDocPtr pDoc)
{
	if( DomNameIs(zName,"URL") || DomNameIs(zName,"documentURI") ){
		const xmlChar *zUrl = pDoc ? pDoc->URL : 0;
		ph7_result_string(pCtx,zUrl ? (const char *)zUrl : "",-1);
		return 1;
	}
	if( DomNameIs(zName,"characterSet") || DomNameIs(zName,"charset")
	 || DomNameIs(zName,"inputEncoding") || DomNameIs(zName,"xmlEncoding") ){
		const xmlChar *zEnc = pDoc ? pDoc->encoding : 0;
		ph7_result_string(pCtx,zEnc ? (const char *)zEnc : "UTF-8",-1);
		return 1;
	}
	if( DomNameIs(zName,"title") ){
		ph7_result_string(pCtx,"",0);
		return 1;
	}
	if( DomNameIs(zName,"body") || DomNameIs(zName,"head") ){
		ph7_result_null(pCtx);
		return 1;
	}
	return 0;
}
static int DomDocStateProp(ph7_context *pCtx,const char *zName,xmlDocPtr pDoc,int bDepr)
{
	int bDeprAe = DomNameIs(zName,"actualEncoding");
	if( DomThisModern(pCtx) && DomModernDocProp(pCtx,zName,pDoc) ){
		return 1;
	}
	if( bDeprAe || DomNameIs(zName,"config") ){
		/* ...and NOT on the get_debug_info walk either: php marks the DECLARATION
		 * deprecated and its debug handler reads the C function behind it, so
		 * print_r()/var_dump() of a document raise nothing (bDepr is 0 there). */
		if( bDepr ){
			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,
				bDeprAe ? "Property DOMDocument::$actualEncoding is deprecated"
				        : "Property DOMDocument::$config is deprecated");
		}
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
/*
 * The two DOMChildNode-side properties.  php declares them on DOMElement and
 * DOMCharacterData only -- an attribute, a PI or the document warns Undefined
 * property -- and they skip every node kind that is not an element.
 */
static int DomChildNodeProp(ph7_context *pCtx,const char *zName)
{
	int bNext = DomNameIs(zName,"nextElementSibling");
	if( bNext || DomNameIs(zName,"previousElementSibling") ){
		phl_domnode *pNd = DomThisNode(pCtx);
		xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
		xmlNodePtr pSib = pNode ? (bNext ? pNode->next : pNode->prev) : 0;
		while( pSib && pSib->type != XML_ELEMENT_NODE ){
			pSib = bNext ? pSib->next : pSib->prev;
		}
		DomResultNodeOf(pCtx,pNd,pSib);
		return 1;
	}
	return 0;
}
/* DOMDocument adds documentElement and the state block above. */
static int DomDocPropEx(ph7_context *pCtx,const char *zName,int bDepr)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	if( DomNameIs(zName,"documentElement") ){
		DomResultNodeOf(pCtx,pNd,pNd ? xmlDocGetRootElement((xmlDocPtr)pNd->pNode) : 0);
		return 1;
	}
	if( DomNameIs(zName,"implementation") ){
		/* A FRESH object on every read, which is php's: the class has no state
		 * and nothing ties one to a document. */
		ph7_class *pClass = PH7_VmExtractClass(pCtx->pVm,"DOMImplementation",
			sizeof("DOMImplementation")-1,FALSE,0);
		ph7_class_instance *pImpl = pClass ? PH7_NewClassInstance(pCtx->pVm,pClass) : 0;
		if( pImpl ){
			PH7_NativeResultObject(pCtx,pImpl);
		}else{
			ph7_result_null(pCtx);
		}
		return 1;
	}
	if( DomNameIs(zName,"doctype") ){
		/* The INTERNAL subset alone, which is php's: a DTD pulled in from the
		 * SYSTEM identifier lands in `extSubset` and is not what `doctype`
		 * answers. Null for a document that declares none. */
		DomResultNodeOf(pCtx,pNd,
			pNd ? (xmlNodePtr)xmlGetIntSubset((xmlDocPtr)pNd->pNode) : 0);
		return 1;
	}
	if( DomDocStateProp(pCtx,zName,pNd ? (xmlDocPtr)pNd->pNode : 0,bDepr) ){
		return 1;
	}
	{
		/* The seven directives, out of the one hidden word that holds them. */
		sxu32 i;
		for( i = 0 ; i < SX_ARRAYSIZE(aDomDocFlag) ; ++i ){
			if( DomNameIs(zName,aDomDocFlag[i].zName) ){
				ph7_result_bool(pCtx,
					DomDocFlag(PH7_ContextThis(pCtx),aDomDocFlag[i].iBit));
				return 1;
			}
		}
	}
	if( DomParentNodeProp(pCtx,zName) ){
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
static int DomDocProp(ph7_context *pCtx,const char *zName)
{
	return DomDocPropEx(pCtx,zName,1);
}
/* The same reader with php's two #[\Deprecated] notices held back: the
 * get_debug_info walk below reads the handler, not the declaration. */
static int DomDocPropQuiet(ph7_context *pCtx,const char *zName)
{
	return DomDocPropEx(pCtx,zName,0);
}
/*
 * DOMDocumentType: what the `<!DOCTYPE ...>` line SAYS.
 *
 * The DTD node has been reachable all along (`$doc->firstChild` on any document
 * carrying a doctype), so what was missing was not the node but every question
 * about it: the class, so `instanceof DOMDocumentType` and `get_class()`
 * answer, and the four identifiers a program reads off one.
 *
 * php's `publicId`/`systemId` here are plain strings that answer "" when the
 * declaration carries none -- unlike DOMEntity's, which are `?string` and null
 * for the same absence -- so the two classes cannot share a reader.
 */
static xmlDtdPtr DomThisDtd(ph7_context *pCtx)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pNode == 0
	 || (pNode->type != XML_DTD_NODE && pNode->type != XML_DOCUMENT_TYPE_NODE) ){
		return 0;
	}
	return (xmlDtdPtr)pNode;
}
/*
 * `internalSubset`: the bytes BETWEEN the brackets, rebuilt by dumping each
 * declaration the subset holds.  php reads them off the DOCUMENT's internal
 * subset rather than the receiver's own children -- so a doctype cloned out of
 * a document that has none answers null -- and answers null, not "", when
 * there is no subset to dump at all.
 */
static void DomInternalSubset(ph7_context *pCtx,xmlDtdPtr pDtd)
{
	xmlDtdPtr pSub = (pDtd && pDtd->doc) ? xmlGetIntSubset(pDtd->doc) : 0;
	xmlNodePtr pChild = pSub ? pSub->children : 0;
	xmlBufferPtr pBuf;
	xmlOutputBufferPtr pOut;
	if( pChild == 0 ){
		ph7_result_null(pCtx);
		return;
	}
	pBuf = xmlBufferCreate();
	pOut = pBuf ? xmlOutputBufferCreateBuffer(pBuf,0) : 0;
	if( pOut == 0 ){
		if( pBuf ){
			xmlBufferFree(pBuf);
		}
		ph7_result_null(pCtx);
		return;
	}
	for( ; pChild ; pChild = pChild->next ){
		xmlNodeDumpOutput(pOut,pSub->doc,pChild,0,0,0);
	}
	xmlOutputBufferFlush(pOut);
	ph7_result_string(pCtx,(const char *)xmlBufferContent(pBuf),(int)xmlBufferLength(pBuf));
	xmlOutputBufferClose(pOut);
	xmlBufferFree(pBuf);
}
static int DomDocTypeProp(ph7_context *pCtx,const char *zName)
{
	xmlDtdPtr pDtd = DomThisDtd(pCtx);
	if( DomNameIs(zName,"name") ){
		/* The name the DOCTYPE declares, which is also its nodeName. */
		ph7_result_string(pCtx,(pDtd && pDtd->name) ? (const char *)pDtd->name : "",-1);
		return 1;
	}
	if( DomNameIs(zName,"publicId") ){
		ph7_result_string(pCtx,(pDtd && pDtd->ExternalID) ? (const char *)pDtd->ExternalID : "",-1);
		return 1;
	}
	if( DomNameIs(zName,"systemId") ){
		ph7_result_string(pCtx,(pDtd && pDtd->SystemID) ? (const char *)pDtd->SystemID : "",-1);
		return 1;
	}
	if( DomNameIs(zName,"internalSubset") ){
		DomInternalSubset(pCtx,pDtd);
		return 1;
	}
	if( DomNameIs(zName,"entities") || DomNameIs(zName,"notations") ){
		/* The DTD half has its OWN map class in the new tree -- a second class
		 * over the same declaration walk, so an entity table and an attribute
		 * table are no longer the same type. */
		ph7_class_instance *pMap = DomNewCollection(pCtx->pVm,
			DomCollClass(DomThisDoc(pCtx),"DOMNamedNodeMap","Dom\\DtdNamedNodeMap"),
			DomThisDoc(pCtx),DomNameIs(zName,"notations") ? DNL_NOTS : DNL_ENTS,
			PH7_ContextThis(pCtx),0,0,0);
		if( pMap ){
			PH7_NativeResultObject(pCtx,pMap);
		}else{
			ph7_result_null(pCtx);
		}
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
/*
 * DOMEntity: an `<!ENTITY ...>` declaration of the internal subset.
 *
 * Its three identifiers are the DOM's "for an UNPARSED entity" rule, which php
 * follows to the letter: `publicId`, `systemId` and `notationName` answer null
 * for every entity that is not `NDATA`-declared, so the external-but-parsed
 * `<!ENTITY e SYSTEM "e.xml">` reads null from all three while
 * `<!ENTITY g SYSTEM "g.gif" NDATA gif>` reads its own two and the notation's
 * name.  (`baseURI`, which DOMNode answers, is where the resolved system
 * identifier does show for both.)
 *
 * The other three are php 8.4's deprecated block, and like DOMDocument's the
 * notice fires on a READ and on an `isset()` alike -- both go through php's
 * property handler -- and not on a write, where the readonly refusal comes
 * first and never consults this reader.
 */
static xmlEntityPtr DomThisEntity(ph7_context *pCtx)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	/* Only a real entity DECLARATION is xmlEntity-shaped. An ELEMENT
	 * declaration wears this class in php and is an xmlElement underneath,
	 * whose fields past the node header are another struct's. */
	if( pNode == 0 || pNode->type != XML_ENTITY_DECL ){
		return 0;
	}
	return (xmlEntityPtr)pNode;
}
static int DomEntityPropEx(ph7_context *pCtx,const char *zName,int bDepr)
{
	xmlEntityPtr pEnt = DomThisEntity(pCtx);
	int bUnparsed = pEnt && pEnt->etype == XML_EXTERNAL_GENERAL_UNPARSED_ENTITY;
	if( DomNameIs(zName,"publicId") ){
		DomResultXmlStr(pCtx,bUnparsed ? pEnt->ExternalID : 0);
		return 1;
	}
	if( DomNameIs(zName,"systemId") ){
		DomResultXmlStr(pCtx,bUnparsed ? pEnt->SystemID : 0);
		return 1;
	}
	if( DomNameIs(zName,"notationName") ){
		/* libxml keeps an unparsed entity's notation name in `content`. */
		DomResultXmlStr(pCtx,bUnparsed ? pEnt->content : 0);
		return 1;
	}
	if( DomNameIs(zName,"actualEncoding") ){
		if( bDepr ){
			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,
				"Property DOMEntity::$actualEncoding is deprecated");
		}
		ph7_result_null(pCtx);
		return 1;
	}
	if( DomNameIs(zName,"encoding") ){
		if( bDepr ){
			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,
				"Property DOMEntity::$encoding is deprecated");
		}
		ph7_result_null(pCtx);
		return 1;
	}
	if( DomNameIs(zName,"version") ){
		/* php has never filled any of the three in: the handler answers NULL
		 * and does nothing else. */
		if( bDepr ){
			PH7_VmThrowError(pCtx->pVm,0,8192 /* E_DEPRECATED */,
				"Property DOMEntity::$version is deprecated");
		}
		ph7_result_null(pCtx);
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
static int DomEntityProp(ph7_context *pCtx,const char *zName)
{
	return DomEntityPropEx(pCtx,zName,1);
}
/* ...and the same reader without php's three #[\Deprecated] notices, for the
 * get_debug_info walk (which reads the handler, not the declaration). */
static int DomEntityPropQuiet(ph7_context *pCtx,const char *zName)
{
	return DomEntityPropEx(pCtx,zName,0);
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
	if( DomParentNodeProp(pCtx,zName) || DomChildNodeProp(pCtx,zName) ){
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
	/* The two trees answer `name` differently, and only here: the 2004 tree
	 * hands back libxml's node name, which for an attribute is the LOCAL name,
	 * so `p:b` reads as `b` while `nodeName` still reads as `p:b`. The modern
	 * tree answers the qualified name, so the two agree there. */
	if( DomNameIs(zName,"name") ){
		if( DomThisModern(pCtx) ){
			DomNodeName(pCtx,pNode);
		}else{
			ph7_result_string(pCtx,(pNode && pNode->name) ? (const char *)pNode->name : "",-1);
		}
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
		DomDataValue(pCtx,pNode);
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
	if( DomCharDataProp(pCtx,zName) || DomChildNodeProp(pCtx,zName) ){
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
			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sDisp);
			zGiven = zBuf;
		}else if( pVal ){
			zGiven = ph7_type_name(pVal);
		}
		*pRc = DomPropThrow(pCtx,"TypeError",0,
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
			SyBufferFormat(zBuf,sizeof(zBuf),"%z",&pObj->pClass->sDisp);
			zGiven = zBuf;
		}else if( pVal && (pVal->iFlags & MEMOBJ_NULL) == 0 ){
			zGiven = ph7_type_name(pVal);
		}
		*pRc = DomPropThrow(pCtx,"TypeError",0,
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
	/* A write to the stand-in attribute php 8.4's map answers a namespace
	 * DECLARATION through goes to the DECLARATION: the stand-in is a view of
	 * the xmlNs and nothing else reads its text child. */
	xmlNsPtr pNs = DomNsAttrStandIn(pShell,pNode);
	if( pNs ){
		if( pNs->href ){
			xmlFree((xmlChar *)pNs->href);
		}
		pNs->href = xmlStrdup((const xmlChar *)zText);
	}
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
	if( pDoc == 0 || DomDocFlag(pDoc,DOM_F_STRICT_ERR) ){
		return DomThrowAlways(pCtx,iCode);
	}
	SyBlobInit(&sFn,&pCtx->pVm->sAllocator);
	PH7_VmActiveFuncName(pCtx->pVm,&sFn);
	SyStringInitFromBuf(&sName,SyBlobData(&sFn),SyBlobLength(&sFn));
	rc = PH7_VmThrowError(pCtx->pVm,&sName,PH7_CTX_WARNING,DomErrText(iCode));
	SyBlobRelease(&sFn);
	return rc;
}
/*
 * The node kinds a value write REACHES.
 *
 * php's two writers do not accept the same list, and everything off it is a
 * silent NO-OP -- the write is accepted and the node is left exactly as it was.
 * PHL had one exclusion, the document, and wrote to everything else, which cost
 * three answers and one crash:
 *
 *   - an ENTITY REFERENCE's children are the entity DECLARATION's, shared by
 *     every reference to it and owned by the DTD; dropping them freed nodes the
 *     document frees again at teardown -- `$ref->nodeValue = 'x'` aborted the
 *     process on a double free;
 *   - a DOCTYPE's children are the declarations of the internal SUBSET, so
 *     `$doc->doctype->nodeValue = 'x'` silently emptied `<!DOCTYPE r [ ... ]>`
 *     of every entity, element and attribute declaration in it;
 *   - a FRAGMENT is emptied by `textContent` and left alone by `nodeValue`,
 *     which is the one kind where the two writers really do disagree.
 */
static int DomValueWritable(xmlNodePtr pNode,int bValue)
{
	if( pNode == 0 ){
		return 0;
	}
	switch( pNode->type ){
	case XML_ELEMENT_NODE:
	case XML_ATTRIBUTE_NODE:
	case XML_TEXT_NODE:
	case XML_COMMENT_NODE:
	case XML_CDATA_SECTION_NODE:
	case XML_PI_NODE:
		return 1;
	case XML_DOCUMENT_FRAG_NODE:
		return !bValue;
	default:
		return 0;
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
		/* php's one REFUSAL among the no-ops, and it answers before the type
		 * check does: `$ref->textContent = []` is the readonly Error where
		 * `$ref->nodeValue = []` is the TypeError. Left unwritten here, the
		 * accessor macro falls through to it. */
		if( !bValue && pNode && pNode->type == XML_ENTITY_REF_NODE ){
			return DOM_SET_UNKNOWN;
		}
		if( DomWriteText(pCtx,"DOMNode",bValue ? "nodeValue" : "textContent",
			bValue ? "?string" : "string",pVal,&sVal,pRc) == 0 ){
			return DOM_SET_DONE;
		}
		if( DomValueWritable(pNode,bValue) ){
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
		*pRc = DomRefuseWrite(pCtx,zName);
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
				*pRc = DomPropThrow(pCtx,"ValueError",0,"Invalid document encoding");
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
	{
		/* The seven directives. php declares each `bool`, so the screen is the
		 * typed-property one a real slot would have applied, and the value lands
		 * in the hidden word rather than in a property of its own. */
		sxu32 i;
		for( i = 0 ; i < SX_ARRAYSIZE(aDomDocFlag) ; ++i ){
			if( DomNameIs(zName,aDomDocFlag[i].zName) ){
				ph7_class_instance *pThis = PH7_ContextThis(pCtx);
				sxi64 iWord;
				if( DomWriteBool(pCtx,"DOMDocument",zName,pVal,pRc) == 0 ){
					return DOM_SET_DONE;
				}
				iWord = pThis ? PH7_NativeAttrInt(pThis,DOM_DFLAGS) : 0;
				if( ph7_value_to_bool(pVal) ){
					iWord |= aDomDocFlag[i].iBit;
				}else{
					iWord &= ~(sxi64)aDomDocFlag[i].iBit;
				}
				if( pThis ){
					PH7_NativeSetAttrInt(pCtx->pVm,pThis,DOM_DFLAGS,iWord);
				}
				return DOM_SET_DONE;
			}
		}
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
		DomDataValue(pCtx,pNode);
		return 1;
	}
	/* php 8.4 puts the processing instruction UNDER `Dom\\CharacterData`, where
	 * the 2004 one sits directly under DOMNode -- so `$pi->length` and the two
	 * element siblings are answers in that tree and undefined properties in
	 * this one. The reader is the character data's own; nothing about a PI's
	 * content differs, only where php filed the class. */
	if( DomThisModern(pCtx)
	 && (DomCharDataProp(pCtx,zName) || DomChildNodeProp(pCtx,zName)) ){
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
	if( DomNameIs(zName,"nodeValue") ){
		/* php builds its wrapper as a fake node whose text CHILD carries the
		 * URI, and an EMPTY href writes no child at all -- so the xmlns=""
		 * UNDECLARATION answers null here while namespaceURI below answers
		 * the empty string off the href itself. Both doors (the attribute
		 * lookups and the namespace:: axis) share this recognizer. */
		if( pNs->href && pNs->href[0] ){
			ph7_result_string(pCtx,(const char *)pNs->href,-1);
		}else{
			ph7_result_null(pCtx);
		}
		return 1;
	}
	if( DomNameIs(zName,"namespaceURI") ){
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
/*
 * Every property DOMEntity adds is read-only, and the refusal is raised HERE
 * rather than by falling through to the reader: the reader is where the three
 * deprecated names raise their notice, and php's write never reaches it -- the
 * readonly Error comes first and says nothing about deprecation.
 */
static int DomSetEntityProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	if( DomNameIs(zName,"publicId") || DomNameIs(zName,"systemId")
	 || DomNameIs(zName,"notationName") || DomNameIs(zName,"actualEncoding")
	 || DomNameIs(zName,"encoding") || DomNameIs(zName,"version") ){
		*pRc = DomRefuseWrite(pCtx,zName);
		return DOM_SET_DONE;
	}
	return DomSetNodeProp(pCtx,zName,pVal,pRc);
}
/*
 * DOMNotation: the two identifiers a `<!NOTATION ...>` declares.
 *
 * Both are plain strings that answer "" for the half that is absent -- a
 * SYSTEM-only notation reads "" from `publicId` -- where DOMEntity's same-named
 * pair are `?string`. The node under them is the stand-in DomNotationNode
 * built, which shares the entity's layout, so both identifiers are read from
 * the same two fields.
 */
static int DomNotationProp(ph7_context *pCtx,const char *zName)
{
	phl_domnode *pNd = DomThisNode(pCtx);
	xmlNodePtr pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	xmlEntityPtr pNot = (pNode && pNode->type == XML_NOTATION_NODE)
		? (xmlEntityPtr)pNode : 0;
	if( DomNameIs(zName,"publicId") ){
		ph7_result_string(pCtx,(pNot && pNot->ExternalID) ? (const char *)pNot->ExternalID : "",-1);
		return 1;
	}
	if( DomNameIs(zName,"systemId") ){
		ph7_result_string(pCtx,(pNot && pNot->SystemID) ? (const char *)pNot->SystemID : "",-1);
		return 1;
	}
	return DomNodeProp(pCtx,zName);
}
static int DomSetNotationProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	if( DomNameIs(zName,"publicId") || DomNameIs(zName,"systemId") ){
		*pRc = DomRefuseWrite(pCtx,zName);
		return DOM_SET_DONE;
	}
	return DomSetNodeProp(pCtx,zName,pVal,pRc);
}
/*
 * DOMXPath's two, out of the hidden slots that hold them: php declares both
 * VIRTUAL, so `document` is read-only because its handler has no writer (and not
 * because the slot is `readonly`, which is why php's isReadOnly() is false there)
 * and the write refusal is the reader's, exactly as it is for a node's `nodeName`.
 */
/* Which of the two XPath classes a diagnostic has to name. The 2004 class and
 * php 8.4's `Dom\XPath` share every C body here -- one evaluator, one
 * registration table -- and differ only in what they SAY and in the family of
 * the wrappers a result carries.
 *
 * The DOCUMENT answers it, not the receiver's own name: php names the declaring
 * SCOPE, so a user subclass of the 2004 class is still "DOMXPath" in its
 * messages however it is spelled -- including one declared inside a `Dom\`
 * namespace of its own, which a name test gets wrong. Each class can only ever
 * hold its own tree's document, the constructors' signatures seeing to that, so
 * the family flag IS the scope; `Dom\XPath` is final and has no subclass to
 * confuse it. Before a constructor runs there is no document and the answer is
 * the 2004 name, which is the only class reachable in that state. */
static const char * DomXPathClassName(ph7_class_instance *pThis)
{
	return DomDocFlag(pThis ? PH7_NativeAttrObj(pThis,XP_DOC) : 0,DOM_F_MODERN)
		? "Dom\\XPath" : "DOMXPath";
}
static int DomXPathProp(ph7_context *pCtx,const char *zName)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( DomNameIs(zName,"document") ){
		DomResultWrap(pCtx,pThis ? PH7_NativeAttrObj(pThis,XP_DOC) : 0);
		return 1;
	}
	if( DomNameIs(zName,"registerNodeNamespaces") ){
		ph7_result_bool(pCtx,pThis ? PH7_NativeAttrTruthy(pThis,XP_NSDEF) : 1);
		return 1;
	}
	return 0;
}
static int DomSetXPathProp(ph7_context *pCtx,const char *zName,ph7_value *pVal,int *pRc)
{
	if( DomNameIs(zName,"registerNodeNamespaces") ){
		ph7_class_instance *pThis = PH7_ContextThis(pCtx);
		if( DomWriteBool(pCtx,DomXPathClassName(pThis),zName,pVal,pRc) == 0 ){
			return DOM_SET_DONE;
		}
		if( pThis ){
			PH7_NativeSetAttrBool(pCtx->pVm,pThis,XP_NSDEF,ph7_value_to_bool(pVal));
		}
		return DOM_SET_DONE;
	}
	/* `document` falls through to the shared refusal, which reads the class's own
	 * recognizer and words php's `Cannot modify readonly property`. */
	return DOM_SET_UNKNOWN;
}
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
	if( DomNodeReadOnly(pFrag) ){
		/* A CONSTRUCTED fragment -- `new DOMDocumentFragment()` -- refuses
		 * this door the way every child-list door refuses an ownerless
		 * receiver, where its append() takes the same chunk's nodes. */
		return DomThrow(pCtx,DOM_ERR_NO_MOD);
	}
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
	pList = DomNewCollection(pCtx->pVm,
		DomCollClass(DomThisDoc(pCtx),"DOMNodeList","Dom\\HTMLCollection"),
		DomThisDoc(pCtx),DNL_GEBTN,pThis,zName,0,0);
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
	pList = DomNewCollection(pCtx->pVm,
		DomCollClass(DomThisDoc(pCtx),"DOMNodeList","Dom\\HTMLCollection"),
		DomThisDoc(pCtx),DNL_GEBTNNS,pThis,
		zName,zUri ? zUri : "",0);
	if( pList == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	PH7_NativeResultObject(pCtx,pList);
	return PH7_OK;
}

/*
 * php's get_debug_info for the DOM (ph7_class::xPresent), and the ONLY table any
 * presentation surface of a node class shows.
 *
 * Every property php declares on these classes is VIRTUAL -- a read_property
 * handler over libxml's own state -- so php's get_properties answers a node's REAL
 * table and nothing else: `(array)`, `get_object_vars()`, `foreach`,
 * `json_encode()`, `var_export()`, `serialize()` and `get_mangled_object_vars()`
 * see an EMPTY document, and only a SUBCLASS's own properties ever appear there.
 * That half needs no hook at all (the slot walk already answers it), which is why
 * the non-debug call declines and lets the engine fall back to it.
 *
 * The DEBUG half is php's dom_get_debug_info_helper: the object's real properties
 * first (a subclass's own, under php's mangled keys), then the class's
 * property-handler table walked own-entries-first with the parent chain behind it.
 * Two rules come out of that helper and are reproduced here. An OBJECT value is
 * never recursed into -- php substitutes the literal `(object value omitted)`, so
 * `print_r($doc)` does not print the whole tree through `documentElement`. And a
 * handler that FAILS contributes no row at all, which in ext/dom is exactly one
 * case: `ownerDocument` is missing from a node libxml never gave a document (one
 * built with `new`, and the stand-in a NOTATION is read through).
 *
 * The values are read through the class's own recognizer rather than through
 * __get: php's walk is the C handler, so it runs no userland code and raises none
 * of the deprecations a php-level read of `actualEncoding`/`config` would.
 */
#define DOM_NODE_DEBUG \
	"nodeName", "nodeValue", "nodeType", "parentNode", "parentElement", "childNodes", \
	"firstChild", "lastChild", "previousSibling", "nextSibling", "attributes", \
	"isConnected", "ownerDocument", "namespaceURI", "prefix", "localName", \
	"baseURI", "textContent"
#define DOM_MNODE_DEBUG \
	"nodeType", "nodeName", "baseURI", "isConnected", "ownerDocument", \
	"parentNode", "parentElement", "childNodes", "firstChild", "lastChild", \
	"previousSibling", "nextSibling", "nodeValue", "textContent"
#define DOM_MPARENT_DEBUG \
	"firstElementChild", "lastElementChild", "childElementCount"
#define DOM_MCHILD_DEBUG \
	"previousElementSibling", "nextElementSibling"
#define DOM_CHARDATA_DEBUG \
	"data", "length", "previousElementSibling", "nextElementSibling"
static const char * const azDomNodeDebug[] = { DOM_NODE_DEBUG };
static const char * const azDomMNodeDebug[] = { DOM_MNODE_DEBUG };
static const char * const azDomMDocDebug[] = {
	"URL", "documentURI", "characterSet", "charset", "inputEncoding",
	"doctype", "documentElement", DOM_MPARENT_DEBUG, "body", "head", "title",
	DOM_MNODE_DEBUG
};
static const char * const azDomMXmlDocDebug[] = {
	"xmlEncoding", "xmlStandalone", "xmlVersion", "formatOutput",
	"URL", "documentURI", "characterSet", "charset", "inputEncoding",
	"doctype", "documentElement", DOM_MPARENT_DEBUG, "body", "head", "title",
	DOM_MNODE_DEBUG
};
static const char * const azDomMElemDebug[] = {
	"namespaceURI", "prefix", "localName", "tagName", "id", "className",
	"attributes", DOM_MPARENT_DEBUG, DOM_MCHILD_DEBUG, DOM_MNODE_DEBUG
};
static const char * const azDomMAttrDebug[] = {
	"namespaceURI", "prefix", "localName", "name", "value", "ownerElement",
	"specified", DOM_MNODE_DEBUG
};
static const char * const azDomMCharDebug[] = {
	"data", "length", DOM_MCHILD_DEBUG, DOM_MNODE_DEBUG
};
static const char * const azDomMTextDebug[] = {
	"wholeText", "data", "length", DOM_MCHILD_DEBUG, DOM_MNODE_DEBUG
};
static const char * const azDomMPiDebug[] = {
	"target", "data", "length", DOM_MCHILD_DEBUG, DOM_MNODE_DEBUG
};
static const char * const azDomMFragDebug[] = { DOM_MPARENT_DEBUG, DOM_MNODE_DEBUG };
static const char * const azDomMDocTypeDebug[] = {
	"name", "entities", "notations", "publicId", "systemId", "internalSubset",
	DOM_MNODE_DEBUG
};
static const char * const azDomMEntityDebug[] = {
	"publicId", "systemId", "notationName", DOM_MNODE_DEBUG
};
static const char * const azDomMNotationDebug[] = {
	"publicId", "systemId", DOM_MNODE_DEBUG
};
static const char * const azDomDocDebug[] = {
	"doctype", "implementation", "documentElement", "actualEncoding", "encoding",
	"xmlEncoding", "standalone", "xmlStandalone", "version", "xmlVersion",
	"strictErrorChecking", "documentURI", "config", "formatOutput", "validateOnParse",
	"resolveExternals", "preserveWhiteSpace", "recover", "substituteEntities",
	"firstElementChild", "lastElementChild", "childElementCount", DOM_NODE_DEBUG
};
static const char * const azDomElemDebug[] = {
	"tagName", "className", "id", "schemaTypeInfo", "firstElementChild",
	"lastElementChild", "childElementCount", "previousElementSibling",
	"nextElementSibling", DOM_NODE_DEBUG
};
static const char * const azDomAttrDebug[] = {
	"name", "specified", "value", "ownerElement", "schemaTypeInfo", DOM_NODE_DEBUG
};
static const char * const azDomCharDebug[] = { DOM_CHARDATA_DEBUG, DOM_NODE_DEBUG };
static const char * const azDomTextDebug[] = {
	"wholeText", DOM_CHARDATA_DEBUG, DOM_NODE_DEBUG
};
static const char * const azDomPiDebug[] = { "target", "data", DOM_NODE_DEBUG };
static const char * const azDomFragDebug[] = {
	"firstElementChild", "lastElementChild", "childElementCount", DOM_NODE_DEBUG
};
static const char * const azDomDocTypeDebug[] = {
	"name", "entities", "notations", "publicId", "systemId", "internalSubset",
	DOM_NODE_DEBUG
};
static const char * const azDomEntityDebug[] = {
	"publicId", "systemId", "notationName", "actualEncoding", "encoding", "version",
	DOM_NODE_DEBUG
};
static const char * const azDomNotationDebug[] = { "publicId", "systemId", DOM_NODE_DEBUG };
static const char * const azDomListDebug[] = { "length" };
static const char * const azDomNsNodeDebug[] = {
	"nodeName", "nodeValue", "nodeType", "prefix", "localName", "namespaceURI",
	"isConnected", "ownerDocument", "parentNode", "parentElement"
};
static const char * const azDomXPathDebug[] = { "document", "registerNodeNamespaces" };
/*
 * The DECLARATIONS behind those names. php declares every one of them on the class
 * -- Reflection lists them, `property_exists()` answers true, `isVirtual()` is true
 * and `hasDefaultValue()` false -- and keeps no slot for any: PH7_MOD_VIRTUAL is
 * that pair of facts. Each row states php's own declared type, and the rows are in
 * php's own declaration order, which is the order Reflection reports and (own class
 * first) the order the debug table above walks.
 */
#define DOM_VPROP(NAME,TYPE) \
	{ NAME, PH7_MOD_PUBLIC|PH7_MOD_VIRTUAL, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, TYPE }
#define DOM_NODE_VPROPS \
	DOM_VPROP("nodeName","string"), \
	DOM_VPROP("nodeValue","?string"), \
	DOM_VPROP("nodeType","int"), \
	DOM_VPROP("parentNode","?DOMNode"), \
	DOM_VPROP("parentElement","?DOMElement"), \
	DOM_VPROP("childNodes","DOMNodeList"), \
	DOM_VPROP("firstChild","?DOMNode"), \
	DOM_VPROP("lastChild","?DOMNode"), \
	DOM_VPROP("previousSibling","?DOMNode"), \
	DOM_VPROP("nextSibling","?DOMNode"), \
	DOM_VPROP("attributes","?DOMNamedNodeMap"), \
	DOM_VPROP("isConnected","bool"), \
	DOM_VPROP("ownerDocument","?DOMDocument"), \
	DOM_VPROP("namespaceURI","?string"), \
	DOM_VPROP("prefix","string"), \
	DOM_VPROP("localName","?string"), \
	DOM_VPROP("baseURI","?string"), \
	DOM_VPROP("textContent","string")
/* php's DOMParentNode trio, declared on each of the three classes that carry it. */
#define DOM_PARENT_VPROPS \
	DOM_VPROP("firstElementChild","?DOMElement"), \
	DOM_VPROP("lastElementChild","?DOMElement"), \
	DOM_VPROP("childElementCount","int")
/* ...and its DOMChildNode pair. */
#define DOM_CHILD_VPROPS \
	DOM_VPROP("previousElementSibling","?DOMElement"), \
	DOM_VPROP("nextElementSibling","?DOMElement")
/*
 * php 8.4's tree states its OWN names, and they are not a rename of the 2004
 * ones: `Dom\Node` has fourteen where DOMNode has eighteen (the four it drops --
 * `attributes`, `prefix`, `localName`, `namespaceURI` -- moved DOWN onto the two
 * classes that can answer them), every type names a `Dom\` class, and the order
 * is php's own declaration order, which Reflection and the debug walk both show.
 */
#define DOM_MNODE_VPROPS \
	DOM_VPROP("nodeType","int"), \
	DOM_VPROP("nodeName","string"), \
	DOM_VPROP("baseURI","string"), \
	DOM_VPROP("isConnected","bool"), \
	DOM_VPROP("ownerDocument","?Dom\\Document"), \
	DOM_VPROP("parentNode","?Dom\\Node"), \
	DOM_VPROP("parentElement","?Dom\\Element"), \
	DOM_VPROP("childNodes","Dom\\NodeList"), \
	DOM_VPROP("firstChild","?Dom\\Node"), \
	DOM_VPROP("lastChild","?Dom\\Node"), \
	DOM_VPROP("previousSibling","?Dom\\Node"), \
	DOM_VPROP("nextSibling","?Dom\\Node"), \
	DOM_VPROP("nodeValue","?string"), \
	DOM_VPROP("textContent","?string")
/* The namespaced ParentNode trio, on the three classes that carry it. php's
 * `children` is a real declared SLOT there, not a virtual one, so it is not in
 * this macro -- see the note on each class's table. */
#define DOM_MPARENT_VPROPS \
	DOM_VPROP("firstElementChild","?Dom\\Element"), \
	DOM_VPROP("lastElementChild","?Dom\\Element"), \
	DOM_VPROP("childElementCount","int")
/* ...and its ChildNode pair. */
#define DOM_MCHILD_VPROPS \
	DOM_VPROP("previousElementSibling","?Dom\\Element"), \
	DOM_VPROP("nextElementSibling","?Dom\\Element")
/* php declares the WHATWG mixins' methods on every class that carries them,
 * not on the two interfaces -- `Dom\ChildNode` and `Dom\ParentNode` state
 * them abstract and each implementing class restates them with a body. The
 * bodies are the 2004 ones: `before()` on a `Dom\Element` and on a
 * DOMElement do the same thing to the same libxml tree, and the only thing
 * the family changes is the type an argument must be under, which the screen
 * reads off the receiver. Both are variadic `Dom\Node|string`, so the
 * DECLARED type is what refuses here -- the 2004 pair are untyped `...$nodes`
 * screened inside the body, and php's namespaced pair are not. */
#define DOM_MCHILD_METHODS \
	{ "remove",      PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf }, \
	{ "before",      PH7_MOD_PUBLIC, "Dom\\Node|string ...$nodes", "void", \
	  vm_builtin_Dom_before }, \
	{ "after",       PH7_MOD_PUBLIC, "Dom\\Node|string ...$nodes", "void", \
	  vm_builtin_Dom_after }, \
	{ "replaceWith", PH7_MOD_PUBLIC, "Dom\\Node|string ...$nodes", "void", \
	  vm_builtin_Dom_replaceWith }
#define DOM_MPARENT_METHODS \
	{ "append",          PH7_MOD_PUBLIC, "Dom\\Node|string ...$nodes", "void", \
	  vm_builtin_Dom_append }, \
	{ "prepend",         PH7_MOD_PUBLIC, "Dom\\Node|string ...$nodes", "void", \
	  vm_builtin_Dom_prepend }, \
	{ "replaceChildren", PH7_MOD_PUBLIC, "Dom\\Node|string ...$nodes", "void", \
	  vm_builtin_Dom_replaceChildren }

/*
 * One row per class that HAS a property-handler table -- php's own
 * `dom_xxx_prop_handlers`, which is what its read_property / has_property /
 * write_property consult before anything else about the object.
 *
 * xRead is the READ handler and xWrite the write one; xDebug is xRead with php's
 * two #[\Deprecated] notices held back, because get_debug_info reads the C
 * function behind the declaration and not the declaration. bNode says the
 * receiver's $__res is an xmlNode, which is what the `ownerDocument` rule may be
 * asked about -- a namespace declaration carries an xmlNs instead and has no such
 * field to read.
 */
typedef struct DomPropSpec DomPropSpec;
struct DomPropSpec {
	const char *zClass;
	int (*xRead)(ph7_context *,const char *);
	int (*xWrite)(ph7_context *,const char *,ph7_value *,int *);
	int (*xDebug)(ph7_context *,const char *);
	const char * const *azName;
	sxu32 nName;
	int bNode;
};
static const DomPropSpec aDomProp[] = {
	{ "DOMDocument", DomDocProp, DomSetDocProp, DomDocPropQuiet,
	  azDomDocDebug, SX_ARRAYSIZE(azDomDocDebug), 1 },
	{ "DOMElement", DomElemProp, DomSetElemProp, DomElemProp,
	  azDomElemDebug, SX_ARRAYSIZE(azDomElemDebug), 1 },
	{ "DOMAttr", DomAttrProp, DomSetAttrProp, DomAttrProp,
	  azDomAttrDebug, SX_ARRAYSIZE(azDomAttrDebug), 1 },
	{ "DOMText", DomTextProp, DomSetCharProp, DomTextProp,
	  azDomTextDebug, SX_ARRAYSIZE(azDomTextDebug), 1 },
	{ "DOMCharacterData", DomCharProp, DomSetCharProp, DomCharProp,
	  azDomCharDebug, SX_ARRAYSIZE(azDomCharDebug), 1 },
	{ "DOMProcessingInstruction", DomPiProp, DomSetPiProp, DomPiProp,
	  azDomPiDebug, SX_ARRAYSIZE(azDomPiDebug), 1 },
	/* The fragment writes what DOMNode writes; only its READ set is wider. */
	{ "DOMDocumentFragment", DomFragProp, DomSetNodeProp, DomFragProp,
	  azDomFragDebug, SX_ARRAYSIZE(azDomFragDebug), 1 },
	/* The DTD half's OWN properties are all read-only, so its writer states none
	 * of them and each lands on the hook's readonly Error. DOMNode's three still
	 * write here -- `nodeValue` and `textContent` are accepted and ignored on a
	 * doctype, which is not the same answer as refusing them. */
	{ "DOMDocumentType", DomDocTypeProp, DomSetNodeProp, DomDocTypeProp,
	  azDomDocTypeDebug, SX_ARRAYSIZE(azDomDocTypeDebug), 1 },
	{ "DOMEntity", DomEntityProp, DomSetEntityProp, DomEntityPropQuiet,
	  azDomEntityDebug, SX_ARRAYSIZE(azDomEntityDebug), 1 },
	{ "DOMNotation", DomNotationProp, DomSetNotationProp, DomNotationProp,
	  azDomNotationDebug, SX_ARRAYSIZE(azDomNotationDebug), 1 },
	{ "DOMNodeList", DomListProp, DomSetNothing, DomListProp,
	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },
	/* php's 8.4 collection declares the same single virtual `length`, over the
	 * same count, so it names DOMNodeList's reader rather than a copy of it. */
	{ "Dom\\HTMLCollection", DomListProp, DomSetNothing, DomListProp,
	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },
	/* Its three siblings: the same single virtual `length`, over the list walk
	 * for the node list and the attribute/declaration walk for the two maps. */
	{ "Dom\\NodeList", DomListProp, DomSetNothing, DomListProp,
	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },
	{ "Dom\\NamedNodeMap", DomMapProp, DomSetNothing, DomMapProp,
	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },
	{ "Dom\\DtdNamedNodeMap", DomMapProp, DomSetNothing, DomMapProp,
	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },
	{ "DOMNamedNodeMap", DomMapProp, DomSetNothing, DomMapProp,
	  azDomListDebug, SX_ARRAYSIZE(azDomListDebug), 0 },
	{ "DOMNameSpaceNode", DomNsNodeProp, DomSetNothing, DomNsNodeProp,
	  azDomNsNodeDebug, SX_ARRAYSIZE(azDomNsNodeDebug), 0 },
	{ "DOMXPath", DomXPathProp, DomSetXPathProp, DomXPathProp,
	  azDomXPathDebug, SX_ARRAYSIZE(azDomXPathDebug), 0 },
	/* php 8.4's XPath presents the same two names over the same two readers --
	 * the class is not a DOMXPath and never meets it, so it names its own row. */
	{ "Dom\\XPath", DomXPathProp, DomSetXPathProp, DomXPathProp,
	  azDomXPathDebug, SX_ARRAYSIZE(azDomXPathDebug), 0 },
	/* DOMComment, DOMCdataSection and DOMEntityReference name no row of their own:
	 * they declare no property php's table does not already carry, so the base-chain
	 * walk below reaches their parent's -- which is php's answer for them too. */
	/* php 8.4's tree over the same nodes: the readers are the 2004 ones -- the
	 * question "what is this node's first child" has one answer -- under the new
	 * tree's own NAME LISTS, which are not the old ones. The wrap the readers
	 * make is family-aware on its own (DOM_F_MODERN), so a `Dom\\Element`'s
	 * `firstChild` is a `Dom\\Text` and never a DOMText.
	 *
	 * `Dom\\HTMLDocument`, `Dom\\HTMLElement`, `Dom\\CDATASection`,
	 * `Dom\\Comment` and `Dom\\EntityReference` name no row: they declare
	 * nothing their parent does not, so the base-chain walk reaches it -- php's
	 * own answer for them too. */
	{ "Dom\\XMLDocument", DomDocProp, DomSetDocProp, DomDocPropQuiet,
	  azDomMXmlDocDebug, SX_ARRAYSIZE(azDomMXmlDocDebug), 1 },
	{ "Dom\\Document", DomDocProp, DomSetDocProp, DomDocPropQuiet,
	  azDomMDocDebug, SX_ARRAYSIZE(azDomMDocDebug), 1 },
	{ "Dom\\Element", DomElemProp, DomSetElemProp, DomElemProp,
	  azDomMElemDebug, SX_ARRAYSIZE(azDomMElemDebug), 1 },
	{ "Dom\\Attr", DomAttrProp, DomSetAttrProp, DomAttrProp,
	  azDomMAttrDebug, SX_ARRAYSIZE(azDomMAttrDebug), 1 },
	{ "Dom\\Text", DomTextProp, DomSetCharProp, DomTextProp,
	  azDomMTextDebug, SX_ARRAYSIZE(azDomMTextDebug), 1 },
	/* The namespaced PI is a CharacterData, so its `data` and `length` are the
	 * character reader's; only `target` is its own. */
	{ "Dom\\ProcessingInstruction", DomPiProp, DomSetPiProp, DomPiProp,
	  azDomMPiDebug, SX_ARRAYSIZE(azDomMPiDebug), 1 },
	{ "Dom\\CharacterData", DomCharProp, DomSetCharProp, DomCharProp,
	  azDomMCharDebug, SX_ARRAYSIZE(azDomMCharDebug), 1 },
	{ "Dom\\DocumentFragment", DomFragProp, DomSetNodeProp, DomFragProp,
	  azDomMFragDebug, SX_ARRAYSIZE(azDomMFragDebug), 1 },
	{ "Dom\\DocumentType", DomDocTypeProp, DomSetNodeProp, DomDocTypeProp,
	  azDomMDocTypeDebug, SX_ARRAYSIZE(azDomMDocTypeDebug), 1 },
	{ "Dom\\Entity", DomEntityProp, DomSetEntityProp, DomEntityPropQuiet,
	  azDomMEntityDebug, SX_ARRAYSIZE(azDomMEntityDebug), 1 },
	{ "Dom\\Notation", DomNotationProp, DomSetNotationProp, DomNotationProp,
	  azDomMNotationDebug, SX_ARRAYSIZE(azDomMNotationDebug), 1 },
	{ "Dom\\Node", DomNodeProp, DomSetNodeProp, DomNodeProp,
	  azDomMNodeDebug, SX_ARRAYSIZE(azDomMNodeDebug), 1 },
	{ "DOMNode", DomNodeProp, DomSetNodeProp, DomNodeProp,
	  azDomNodeDebug, SX_ARRAYSIZE(azDomNodeDebug), 1 }
};
/* The nearest ancestor with a handler table -- php's own lookup, which is why a
 * userland subclass of DOMElement shows DOMElement's twenty-seven. */
static const DomPropSpec * DomPropSpecOf(ph7_class *pClass)
{
	for( ; pClass ; pClass = pClass->pBase ){
		sxu32 nName = SyStringLength(&pClass->sName);
		sxu32 i;
		for( i = 0 ; i < SX_ARRAYSIZE(aDomProp) ; ++i ){
			if( SyStrlen(aDomProp[i].zClass) == nName
			 && SyStrncmp(SyStringData(&pClass->sName),aDomProp[i].zClass,nName) == 0 ){
				return &aDomProp[i];
			}
		}
	}
	return 0;
}
/*
 * php's read_property / has_property / write_property for every DOM class, as
 * ph7_class::xProp.
 *
 * ext/dom has no `__get`/`__set`/`__isset` anywhere: each class carries a table
 * of property handlers and php's object handlers consult it FIRST, so a name the
 * table holds is answered by the handler and only a name it does NOT hold falls
 * through to the standard path -- which is where a subclass's own magic accessor
 * finally gets a say. Routing the surface through the magic trio had the order
 * exactly backwards: a subclass that wrote `__get` without delegating replaced
 * the whole DOM surface for its instances, and every DOM class carried three
 * methods php does not.
 *
 * The recognizers below ARE the table: one per class, answering 1 for a name it
 * knows. A name none of them knows leaves bAnswered at 0, and the member opcode
 * takes the ordinary path from there -- php's own fall-through, undefined-property
 * warning and all.
 *
 * UNSET is not answered here. php's unset_property for a virtual property has
 * nothing to remove and refuses with `Cannot unset C::$p`, which the opcode
 * already words off the declaration itself; declining leaves that answer, and
 * leaves a name the class does NOT declare on the __unset path php sends it to.
 *
 * Neither is WRITE, the member opcode's question -- it is asked before the value
 * exists, and a handler that really STORES needs it. The opcode recognizes the
 * name as this table's (PH7_ClassNativePropOwns) and routes the write to STORE
 * below, at the point php's write_property gets its zval.
 */
static void DomPropHook(ph7_vm *pVm,ph7_class_instance *pThis,PH7_NativePropCtx *pCtx)
{
	const DomPropSpec *pSpec = pThis ? DomPropSpecOf(pThis->pClass) : 0;
	sxu32 nName = SyStringLength(pCtx->pName);
	ph7_context sCtx;
	ph7_value sVal;
	char zName[128];
	int bKnown;
	if( pSpec == 0
	 || pCtx->iMode == PH7_NATIVE_PROP_UNSET || pCtx->iMode == PH7_NATIVE_PROP_WRITE ){
		return;
	}
	if( nName >= sizeof(zName) ){
		/* Longer than any name ext/dom declares: the ordinary path owns it. */
		return;
	}
	SyMemcpy(SyStringData(pCtx->pName),zName,nName);
	zName[nName] = 0;
	if( pCtx->iMode == PH7_NATIVE_PROP_OWNS ){
		/* php's table membership, which for ext/dom is the DECLARATION: every one
		 * of its ninety names is declared virtual on the class, and nothing else
		 * is the handler's. Answered without running a recognizer -- a write shape
		 * asks this before the value exists, and the deprecated names would raise
		 * their notice on a read the program has not made. */
		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,zName,nName);
		if( pDecl && (pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){
			pCtx->bAnswered = 1;
		}
		return;
	}
	/* The recognizers answer by WRITING a result, so the scratch context carries
	 * one slot of its own -- and the refusal channel, which is what stops a
	 * readonly Error or a DOMException from running the enclosing catch in the
	 * middle of this access. */
	PH7_MemObjInit(pVm,&sVal);
	VmInitCallContext(&sCtx,pVm,0,&sVal,0);
	sCtx.pThis = pThis;
	sCtx.pCalledClass = pThis->pClass;
	sCtx.pPropCtx = pCtx;
	if( pCtx->iMode == PH7_NATIVE_PROP_STORE ){
		/* php's write_property, with the value. A name the WRITER does not state
		 * is read-only when the class declares it at all -- and the refusal is
		 * php's own, raised where the store would have landed -- and nothing this
		 * class knows otherwise, which puts the write back on the ordinary path. */
		int rcW = PH7_OK;
		if( pSpec->xWrite(&sCtx,zName,pCtx->pResult,&rcW) == DOM_SET_DONE ){
			pCtx->bAnswered = 1;
			if( pCtx->zThrowClass == 0 && DomNodeLess(&sCtx,zName) ){
				/* A writable name on an object with no node behind it: php's
				 * handler reaches its DOM_GET_OBJ and answers Invalid State --
				 * AFTER the declared type has had its say, which is why a
				 * TypeError already recorded stands. */
				DomPropRefuse(&sCtx,"DOMException",DOM_ERR_INVALID_STATE,
					DomErrText(DOM_ERR_INVALID_STATE));
			}
		}else if( pSpec->xRead(&sCtx,zName) ){
			pCtx->bAnswered = 1;
			DomRefuseWrite(&sCtx,zName);
		}
		VmReleaseCallContext(&sCtx);
		PH7_MemObjRelease(&sVal);
		return;
	}
	if( DomNodeLess(&sCtx,zName) ){
		/* A name the class DECLARES, on an object libxml gave no node -- every
		 * handler would fetch a null pointer, and php answers Invalid State on a
		 * read and on an isset() alike. */
		DomPropRefuse(&sCtx,"DOMException",DOM_ERR_INVALID_STATE,
			DomErrText(DOM_ERR_INVALID_STATE));
		VmReleaseCallContext(&sCtx);
		PH7_MemObjRelease(&sVal);
		return;
	}
	bKnown = pSpec->xRead(&sCtx,zName) != 0;
	if( bKnown && pCtx->zThrowClass == 0 ){
		pCtx->bAnswered = 1;
		if( pCtx->iMode == PH7_NATIVE_PROP_READ ){
			PH7_MemObjStore(&sVal,pCtx->pResult);
		}else{
			/* php's has_property fetches the value and judges it: by NULL-ness for
			 * isset() (`isset($n->nextSibling)` is false on a last child while
			 * `isset($n->nodeName)` is true) and by TRUTH for the two check_empty
			 * questions -- ext/dom's handler makes no distinction between them. */
			int bSet = pCtx->iMode == PH7_NATIVE_PROP_ISSET
				? (sVal.iFlags & MEMOBJ_NULL) == 0
				: ph7_value_to_bool(&sVal);
			PH7_MemObjRelease(pCtx->pResult);
			ph7_value_bool(pCtx->pResult,bSet);
		}
	}
	VmReleaseCallContext(&sCtx);
	PH7_MemObjRelease(&sVal);
}
/* php's `dom_node_owner_document_read` answers FAILURE -- and the debug walk then
 * writes no row -- for a node libxml gave no document: one built with `new`, and
 * the entity-shaped stand-in a NOTATION declaration is read through. A DOCUMENT
 * itself answers null and keeps its row. */
static int DomDebugSkip(ph7_class_instance *pThis,const char *zName,int bNode)
{
	phl_domnode *pNd;
	xmlNodePtr pNode;
	if( !bNode || !DomNameIs(zName,"ownerDocument") ){
		return 0;
	}
	pNd = DomResOf(pThis);
	pNode = pNd ? (xmlNodePtr)pNd->pNode : 0;
	if( pNode == 0
	 || pNode->type == XML_DOCUMENT_NODE || pNode->type == XML_HTML_DOCUMENT_NODE ){
		return 0;
	}
	return pNode->doc == 0;
}
static sxi32 DomPresent(ph7_vm *pVm,ph7_class_instance *pThis,ph7_value *pOut,int bDebug)
{
	const DomPropSpec *pSpec;
	ph7_context sCtx;
	ph7_value sVal,sKey;
	sxu32 i;
	if( !bDebug ){
		/* php's get_properties for a DOM object is the object's own table -- which
		 * for anything but a subclass is empty -- so the engine's slot walk IS the
		 * answer and this hook has nothing to add. */
		return SXERR_NOTFOUND;
	}
	pSpec = pThis ? DomPropSpecOf(pThis->pClass) : 0;
	if( pSpec == 0 || (pOut->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return SXERR_NOTFOUND;
	}
	if( pSpec->bNode && PH7_NativeAttr(pThis,DOM_RES) != 0 ){
		phl_domnode *pNd = DomResOf(pThis);
		if( pNd == 0 || pNd->pNode == 0 ){
			/* An object with no node behind it (one built with `new`): every handler
			 * would refuse, so the walk contributes nothing and the object shows its
			 * real table alone. php's dump THROWS out of the debug handler here
			 * instead, after printing the header. */
			return SXERR_NOTFOUND;
		}
	}
	/* A subclass's own properties come FIRST and under php's mangled keys, which
	 * is what prints `[p:MyDoc:private]` beside the fabricated rows. */
	PH7_ClassInstanceToHashmapRaw(pThis,(ph7_hashmap *)pOut->x.pOther);
	PH7_MemObjInit(pVm,&sVal);
	/* One scratch call context for the whole walk: the recognizers answer by
	 * WRITING a result, and rule 54 says a second call on the same context would
	 * append to the first answer -- so the slot is blanked between rows. */
	VmInitCallContext(&sCtx,pVm,0,&sVal,0);
	sCtx.pThis = pThis;
	sCtx.pCalledClass = pThis->pClass;
	for( i = 0 ; i < pSpec->nName ; ++i ){
		const char *zName = pSpec->azName[i];
		if( DomDebugSkip(pThis,zName,pSpec->bNode) ){
			continue;
		}
		PH7_MemObjRelease(&sVal);
		PH7_MemObjInit(pVm,&sVal);
		if( pSpec->xDebug(&sCtx,zName) == 0 ){
			continue;
		}
		if( sVal.iFlags & MEMOBJ_OBJ ){
			/* php prints the literal rather than the object: a document would
			 * otherwise dump its whole tree through `documentElement`. */
			ph7_value sOmit;
			PH7_MemObjInitFromString(pVm,&sOmit,0);
			PH7_MemObjStringAppend(&sOmit,"(object value omitted)",
				sizeof("(object value omitted)")-1);
			PH7_MemObjInitFromString(pVm,&sKey,0);
			PH7_MemObjStringAppend(&sKey,zName,(sxu32)SyStrlen(zName));
			ph7_array_add_elem(pOut,&sKey,&sOmit);
			PH7_MemObjRelease(&sKey);
			PH7_MemObjRelease(&sOmit);
			continue;
		}
		PH7_MemObjInitFromString(pVm,&sKey,0);
		PH7_MemObjStringAppend(&sKey,zName,(sxu32)SyStrlen(zName));
		ph7_array_add_elem(pOut,&sKey,&sVal);
		PH7_MemObjRelease(&sKey);
	}
	VmReleaseCallContext(&sCtx);
	PH7_MemObjRelease(&sVal);
	return SXRET_OK;
}
/*
 * Install the DOM library: every class declared from C, no embedded chunk and
 * no globally visible thunk left.  Called from PH7_VmInit inside the
 * bCompilingBuiltin window, after PH7_VmInstallLibxml (the capture plumbing must
 * exist) and after the Reflection install (DOMException needs Exception).
 */

/* ===== php 8.4's namespaced tree ===== */

/*
 * The three static producers on `Dom\XMLDocument` are the ONLY door into that
 * tree: `Dom\Document` is abstract, `Dom\Node::__construct()` is final private,
 * and every node below is made by a document that already exists. So this is
 * also the only place DOM_F_MODERN is ever set, and setting it there is what
 * makes every wrapper the tree hands out afterwards a `Dom\` one.
 *
 * OWNED: the caller answers with DomResultOwned and the reference goes back.
 */
static ph7_class_instance * DomNewModernDoc(ph7_context *pCtx,xmlDocPtr pDoc)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm,"Dom\\XMLDocument",
		sizeof("Dom\\XMLDocument")-1,FALSE,0);
	ph7_class_instance *pThis = pClass ? PH7_NewClassInstance(pVm,pClass) : 0;
	phl_xmldoc *pShell = pThis ? PH7_LibxmlNewDoc(pVm,pDoc) : 0;
	phl_domnode *pRes = pShell ? DomNewRes(pVm,pShell,pDoc) : 0;
	if( pRes == 0 ){
		if( pShell == 0 ){
			xmlFreeDoc(pDoc);
		}
		if( pThis ){
			PH7_ClassInstanceUnref(pThis);
		}
		return 0;
	}
	PH7_NativeSetAttrInt(pVm,pThis,DOM_DFLAGS,DOM_F_DEFAULT|DOM_F_MODERN);
	DomSetRes(pVm,pThis,pRes);
	PH7_NativeSetAttrObj(pVm,pThis,DOM_DOC,pThis);
	return pThis;
}
/*
 * Dom\XMLDocument::createEmpty(string $version = '1.0', string $encoding = 'UTF-8')
 *
 * Unlike `new DOMDocument`, whose encoding defaults to NOTHING and whose URI is
 * null, php gives this one a real encoding and the URI `about:blank` -- the
 * WHATWG spelling for a document that came from nowhere, which is what its
 * `$URL` and `$documentURI` both read.
 */
DOM_METHOD(vm_builtin_DomXMLDocument_createEmpty)
{
	const char *zVersion = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "1.0";
	const char *zEncoding = nArg > 1 ? ph7_value_to_string(apArg[1],0) : "UTF-8";
	ph7_class_instance *pThis;
	xmlDocPtr pDoc = xmlNewDoc((const xmlChar *)zVersion);
	if( pDoc == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( zEncoding[0] ){
		pDoc->encoding = xmlStrdup((const xmlChar *)zEncoding);
	}
	pDoc->URL = xmlStrdup((const xmlChar *)"about:blank");
	pThis = DomNewModernDoc(pCtx,pDoc);
	if( pThis == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	return DomResultOwned(pCtx,pThis);
}
/*
 * php's namespaced parsers name an encoding on a document that declared none:
 * the override if one was given, else UTF-8. The 2004 loaders leave it NULL,
 * and the difference is visible in the declaration the writers emit.
 */
static void DomModernDefaultEncoding(xmlDocPtr pDoc,const char *zEnc)
{
	if( pDoc->encoding == 0 ){
		pDoc->encoding = xmlStrdup((const xmlChar *)(zEnc ? zEnc : "UTF-8"));
	}
}
/*
 * Dom\XMLDocument::createFromString(string $source, int $options = 0,
 *                                   ?string $overrideEncoding = null)
 *
 * The 2004 loader answers `false` and leaves the receiver as it was; this one
 * has no receiver to leave, so a parse that does not produce a document is a
 * THROW -- php's DOMException 12 (SYNTAX_ERR), raised after the parser's own
 * warnings have already been drained. The empty string never reaches libxml at
 * all: php refuses it by argument, the way loadXML() does.
 *
 * `$overrideEncoding` is libxml's parse encoding, which is the third argument
 * the memory parser has always taken and the 2004 API never exposed.
 */
DOM_METHOD(vm_builtin_DomXMLDocument_createFromString)
{
	ph7_vm *pVm = pCtx->pVm;
	int nLen = 0;
	const char *zSrc = nArg > 0 ? ph7_value_to_string(apArg[0],&nLen) : "";
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	const char *zEnc = (nArg > 2 && !ph7_value_is_null(apArg[2]))
		? ph7_value_to_string(apArg[2],0) : 0;
	ph7_class_instance *pThis;
	phl_dom_errsave sErr;
	xmlDocPtr pDoc;
	sxu32 nMark;
	if( nLen < 1 ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Dom\\XMLDocument::createFromString(): Argument #1 ($source) must not be empty");
	}
	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pDoc = xmlReadMemory(zSrc,nLen,0,zEnc,iOpts);
	PH7_LibxmlCaptureEndOpts(pVm,nMark,"Dom\\XMLDocument::createFromString",iOpts);
	DomRestoreWarnings(pVm,sErr);
	if( pDoc == 0 ){
		return PH7_VmThrowExceptionCode(pCtx,"DOMException",DOM_ERR_SYNTAX,
			"XML fragment is not well-formed");
	}
	DomStampCwd(pCtx,pDoc);
	DomModernDefaultEncoding(pDoc,zEnc);
	pThis = DomNewModernDoc(pCtx,pDoc);
	if( pThis == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	return DomResultOwned(pCtx,pThis);
}
/*
 * Dom\XMLDocument::createFromFile(string $path, int $options = 0,
 *                                 ?string $overrideEncoding = null)
 *
 * The same parse from a file, and the two ways it can fail are two DIFFERENT
 * refusals: a file that cannot be opened is a plain `Exception` naming the path
 * (php's own, code 0 -- it is not a DOM error), and a file that opens but does
 * not parse is the DOMException the string producer raises. Both come after the
 * I/O or parse warning the reader has already emitted.
 */
DOM_METHOD(vm_builtin_DomXMLDocument_createFromFile)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zFile = nArg > 0 ? ph7_value_to_string(apArg[0],0) : "";
	int nFile = 0;
	int iOpts = nArg > 1 ? ph7_value_to_int(apArg[1]) : 0;
	const char *zEnc = (nArg > 2 && !ph7_value_is_null(apArg[2]))
		? ph7_value_to_string(apArg[2],0) : 0;
	ph7_class_instance *pThis;
	phl_dom_errsave sErr;
	SyBlob sBody,sPath;
	xmlDocPtr pDoc;
	sxu32 nMark;
	zFile = nArg > 0 ? ph7_value_to_string(apArg[0],&nFile) : "";
	if( nFile != (int)SyStrlen(zFile) ){
		return PH7_VmThrowException(pCtx,"ValueError",
			"Dom\\XMLDocument::createFromFile(): Argument #1 ($path) must not contain "
			"any null bytes");
	}
	if( !PH7_DomReadFile(pCtx,zFile,nFile,"Dom\\XMLDocument::createFromFile",
			&sBody,&sPath) ){
		return PH7_VmThrowException(pCtx,"Exception","Cannot open file '%.*s'",
			nFile,zFile);
	}
	sErr = DomForceWarnings(pVm,(iOpts & XML_PARSE_RECOVER) != 0);
	nMark = PH7_LibxmlCaptureBegin(pVm);
	pDoc = xmlReadMemory((const char *)SyBlobData(&sBody),(int)SyBlobLength(&sBody),
		(const char *)SyBlobData(&sPath),zEnc,iOpts);
	PH7_LibxmlCaptureEndOpts(pVm,nMark,"Dom\\XMLDocument::createFromFile",iOpts);
	DomRestoreWarnings(pVm,sErr);
	SyBlobRelease(&sBody);
	SyBlobRelease(&sPath);
	if( pDoc == 0 ){
		return PH7_VmThrowExceptionCode(pCtx,"DOMException",DOM_ERR_SYNTAX,
			"XML fragment is not well-formed");
	}
	DomModernDefaultEncoding(pDoc,zEnc);
	pThis = DomNewModernDoc(pCtx,pDoc);
	if( pThis == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	return DomResultOwned(pCtx,pThis);
}
/*
 * `Dom\Node::__construct()`, which php declares FINAL PRIVATE: the whole tree is
 * unconstructible from PHP, `new Dom\Element` is "Call to private
 * Dom\Node::__construct() from global scope", and no subclass can reopen it.
 * The body can never run, and states so rather than pretending to build one.
 */
DOM_METHOD(vm_builtin_DomNode_construct)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SXUNUSED(pCtx);
	return PH7_OK;
}

PH7_PRIVATE sxi32 PH7_VmInstallDom(ph7_vm *pVm)
{
	/* php's eighteen DOMNode properties -- all VIRTUAL there, so the object holds
	 * none of them and every one is answered by DomNodeProp -- followed by the two
	 * slots every wrapper really carries and the identity cache, which are PHL's
	 * own storage and hidden from every surface. */
	static const PH7_NativePropDef aNodeProp[] = {
		DOM_NODE_VPROPS,
		{ DOM_RES, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_DOC, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		/* The identity cache. Only a DOCUMENT'S is a document's; a CONSTRUCTED
		 * ownerless node is its own holder (its $__doc points at itself) and
		 * caches its tree's wrappers HERE until an insertion adopts them into
		 * a real document's cache. Empty and unread on every owned node. */
		{ DOM_NODES, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
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
		 * "?object" message instead -- recorded, the error-format class.) */
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
	/* php's twenty-two, in its own declaration order. Nine are the XML declaration
	 * and the document's URI read straight off libxml, three are the DOMParentNode
	 * trio, and seven are the directives DOM_DFLAGS holds -- none of them a slot,
	 * which is what makes php's `(array)$doc` an EMPTY array. */
	static const PH7_NativePropDef aDocProp[] = {
		DOM_VPROP("doctype","?DOMDocumentType"),
		DOM_VPROP("implementation","DOMImplementation"),
		DOM_VPROP("documentElement","?DOMElement"),
		DOM_VPROP("actualEncoding","?string"),
		DOM_VPROP("encoding","?string"),
		DOM_VPROP("xmlEncoding","?string"),
		DOM_VPROP("standalone","bool"),
		DOM_VPROP("xmlStandalone","bool"),
		DOM_VPROP("version","?string"),
		DOM_VPROP("xmlVersion","?string"),
		DOM_VPROP("strictErrorChecking","bool"),
		DOM_VPROP("documentURI","?string"),
		DOM_VPROP("config","mixed"),
		DOM_VPROP("formatOutput","bool"),
		DOM_VPROP("validateOnParse","bool"),
		DOM_VPROP("resolveExternals","bool"),
		DOM_VPROP("preserveWhiteSpace","bool"),
		DOM_VPROP("recover","bool"),
		DOM_VPROP("substituteEntities","bool"),
		DOM_PARENT_VPROPS,
		/* The seven directives, as the one word that holds them. */
		{ DOM_DFLAGS,           PH7_MOD_PUBLIC|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_INT, DOM_F_DEFAULT, 0, 0.0 }, 0 },
		/* The identity cache DomWrap keys by node pointer. */
		{ DOM_NODES,            PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		/* ...and the base-class => user-class table registerNodeClass writes. */
		{ DOM_NCLS,             PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
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
		{ "createDocumentFragment", PH7_MOD_PUBLIC, "", "@DOMDocumentFragment",
		  vm_builtin_DOMDocument_createFragment },
		{ "normalizeDocument",    PH7_MOD_PUBLIC, "", "@void", vm_builtin_DOMDocument_normalizeDocument },
		{ "registerNodeClass",    PH7_MOD_PUBLIC, "string $baseClass, ?string $extendedClass",
		  "@true", vm_builtin_DOMDocument_registerNodeClass },
		{ "schemaValidate",       PH7_MOD_PUBLIC, "string $filename, int $flags = 0", "@bool",
		  vm_builtin_DOMDocument_schemaValidate },
		{ "schemaValidateSource", PH7_MOD_PUBLIC, "string $source, int $flags = 0", "@bool",
		  vm_builtin_DOMDocument_schemaValidateSource },
		/* php declares no option word on the RelaxNG pair at all. */
		{ "relaxNGValidate",       PH7_MOD_PUBLIC, "string $filename", "@bool",
		  vm_builtin_DOMDocument_relaxNGValidate },
		{ "relaxNGValidateSource", PH7_MOD_PUBLIC, "string $source", "@bool",
		  vm_builtin_DOMDocument_relaxNGValidateSource },
		{ "validate",             PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_DOMDocument_validate },
		{ "xinclude",             PH7_MOD_PUBLIC, "int $options = 0", "@int|false",
		  vm_builtin_DOMDocument_xinclude },
		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName", "@DOMNodeList",
		  vm_builtin_Dom_getElementsByTagName },
		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "@DOMNodeList", vm_builtin_Dom_getElementsByTagNameNS },
		/* The DOMParentNode three: real (non-tentative) void, one untyped
		 * variadic -- php's own rows, screened inside the body. */
		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },
		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },
		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },
	};
	static const PH7_NativeMethodDef aElemMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC,
		  "string $qualifiedName, ?string $value = null, string $namespace = ''", "",
		  vm_builtin_DOMElement_construct },
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
		/* The DOMChildNode four, php's order on this class. */
		{ "remove",          PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf },
		{ "before",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_before },
		{ "after",           PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_after },
		{ "replaceWith",     PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceWith },
		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },
		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },
		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },
		/* The 8.3 pair. php declares the first's return as a plain ?DOMElement
		 * and the second's as a real void. */
		{ "insertAdjacentElement", PH7_MOD_PUBLIC, "string $where, DOMElement $element",
		  "?DOMElement", vm_builtin_DOMElement_insertAdjacentElement },
		{ "insertAdjacentText",    PH7_MOD_PUBLIC, "string $where, string $data", "void",
		  vm_builtin_DOMElement_insertAdjacentText },
	};
	static const PH7_NativeMethodDef aAttrMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $name, string $value = ''", "",
		  vm_builtin_DOMAttr_construct },
		{ "isId",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_DOMAttr_isId },
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
		/* The DOMChildNode four, php's order on THIS class -- replaceWith
		 * leads here where DOMElement's list starts at remove. */
		{ "replaceWith", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceWith },
		{ "remove",      PH7_MOD_PUBLIC, "", "void", vm_builtin_Dom_removeSelf },
		{ "before",      PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_before },
		{ "after",       PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_after },
	};
	static const PH7_NativeMethodDef aPiMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $name, string $value = ''", "",
		  vm_builtin_DOMProcessingInstruction_construct },
	};
	static const PH7_NativeMethodDef aFragMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_DOMDocumentFragment_construct },
		{ "appendXML", PH7_MOD_PUBLIC, "string $data", "@bool",
		  vm_builtin_DOMDocumentFragment_appendXML },
		{ "append",          PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_append },
		{ "prepend",         PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_prepend },
		{ "replaceChildren", PH7_MOD_PUBLIC, "...$nodes", "void", vm_builtin_Dom_replaceChildren },
	};
	static const PH7_NativeMethodDef aTextMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $data = ''", "",
		  vm_builtin_DOMText_construct },
		{ "splitText", PH7_MOD_PUBLIC, "int $offset", "", vm_builtin_DOMText_splitText },
		/* php's 8.x rename and the name it renamed, one body. */
		{ "isWhitespaceInElementContent", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_DOMText_isWhitespace },
		{ "isElementContentWhitespace",   PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_DOMText_isWhitespace },
	};
	static const PH7_NativeMethodDef aEntRefMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",
		  vm_builtin_DOMEntityReference_construct },
	};
	/* The DTD half declares no method of its own at all -- php's whole
	 * DOMDocumentType surface is properties over DOMNode's method list. */
	/* php's factory class: three ordinary instance methods and no state. */
	static const PH7_NativeMethodDef aImplMethod[] = {
		{ "createDocumentType", PH7_MOD_PUBLIC,
		  "string $qualifiedName, string $publicId = '', string $systemId = ''", "",
		  vm_builtin_DOMImplementation_createDocumentType },
		{ "createDocument", PH7_MOD_PUBLIC,
		  "?string $namespace = null, string $qualifiedName = '', "
		  "?DOMDocumentType $doctype = null", "@DOMDocument",
		  vm_builtin_DOMImplementation_createDocument },
		{ "hasFeature", PH7_MOD_PUBLIC, "string $feature, string $version", "@bool",
		  vm_builtin_DOMImplementation_hasFeature },
	};
	/* The comment and CDATA constructors -- the only method either class
	 * declares of its own; php's CDATA data is REQUIRED where the other two
	 * default. */
	static const PH7_NativeMethodDef aCommentMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $data = ''", "",
		  vm_builtin_DOMComment_construct },
	};
	static const PH7_NativeMethodDef aCdataMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $data", "",
		  vm_builtin_DOMCdataSection_construct },
	};
	/* DOMNodeList and DOMNamedNodeMap share a slot layout: what a live view is OF
	 * ($__owner), the document to wrap results against ($__doc), and -- for the
	 * two node-list kinds -- the tag name or the frozen snapshot. */
	static const PH7_NativePropDef aListProp[] = {
		/* php's one declared property on either class, and virtual there too. */
		DOM_VPROP("length","int"),
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
	/* php 8.4's Dom\ChildNode and Dom\ParentNode: the standard's own two mixins,
	 * stated as php states them. Neither is the old DOMChildNode/DOMParentNode --
	 * those keep the 2004 names and the 2004 signatures, and php declares both
	 * pairs side by side. */
	static const PH7_NativeMethodDef aDomChildNodeIf[] = {
		{ "remove",      PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "void", 0 },
		{ "before",      PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "Dom\\Node|string ...$nodes", "void", 0 },
		{ "after",       PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "Dom\\Node|string ...$nodes", "void", 0 },
		{ "replaceWith", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "Dom\\Node|string ...$nodes", "void", 0 },
	};
	static const PH7_NativeMethodDef aDomParentNodeIf[] = {
		{ "append",          PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "Dom\\Node|string ...$nodes", "void", 0 },
		{ "prepend",         PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "Dom\\Node|string ...$nodes", "void", 0 },
		{ "replaceChildren", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "Dom\\Node|string ...$nodes", "void", 0 },
		{ "querySelector",   PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "string $selectors", "?Dom\\Element", 0 },
		{ "querySelectorAll",PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "string $selectors", "Dom\\NodeList", 0 },
	};
	/* The collection's four, in php's declaration order. `count` and `item` are
	 * DOMNodeList's bodies -- the count and the index walk are the same -- under
	 * the new API's own return types, which are not tentative the way the 2004
	 * class's `count(): int` still is. */
	static const PH7_NativeMethodDef aDomCollMethod[] = {
		{ "item",        PH7_MOD_PUBLIC, "int $index", "?Dom\\Element",
		  vm_builtin_DOMNodeList_item },
		{ "namedItem",   PH7_MOD_PUBLIC, "string $key", "?Dom\\Element",
		  vm_builtin_DomHTMLCollection_namedItem },
		{ "count",       PH7_MOD_PUBLIC, "", "int", vm_builtin_DOMNodeList_count },
		{ "getIterator", PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
	};
	/* The namespaced list and the two namespaced maps, each in php's own
	 * declaration order -- which is not the 2004 classes': the node list puts
	 * `item` LAST and the maps put it first. Their `count(): int` is not
	 * tentative the way DOMNodeList::count() still is, and every node type
	 * they name is a `Dom\` one. */
	static const PH7_NativeMethodDef aDomNodeListMethod[] = {
		{ "count",       PH7_MOD_PUBLIC, "", "int", vm_builtin_DOMNodeList_count },
		{ "getIterator", PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
		{ "item",        PH7_MOD_PUBLIC, "int $index", "?Dom\\Node",
		  vm_builtin_DOMNodeList_item },
	};
	static const PH7_NativeMethodDef aDomMapMethod[] = {
		{ "item",           PH7_MOD_PUBLIC, "int $index", "?Dom\\Attr",
		  vm_builtin_DomNamedNodeMap_item },
		{ "getNamedItem",   PH7_MOD_PUBLIC, "string $qualifiedName", "?Dom\\Attr",
		  vm_builtin_DOMNamedNodeMap_getNamedItem },
		{ "getNamedItemNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "?Dom\\Attr", vm_builtin_DOMNamedNodeMap_getNamedItemNS },
		{ "count",          PH7_MOD_PUBLIC, "", "int", vm_builtin_DOMNamedNodeMap_count },
		{ "getIterator",    PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
	};
	/* The DTD half's map answers ENTITIES and NOTATIONS, so php's three readers
	 * carry a union return rather than the attribute one. */
	static const PH7_NativeMethodDef aDomDtdMapMethod[] = {
		{ "item",           PH7_MOD_PUBLIC, "int $index", "Dom\\Entity|Dom\\Notation|null",
		  vm_builtin_DomDtdNamedNodeMap_item },
		{ "getNamedItem",   PH7_MOD_PUBLIC, "string $qualifiedName",
		  "Dom\\Entity|Dom\\Notation|null", vm_builtin_DOMNamedNodeMap_getNamedItem },
		{ "getNamedItemNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "Dom\\Entity|Dom\\Notation|null", vm_builtin_DOMNamedNodeMap_getNamedItemNS },
		{ "count",          PH7_MOD_PUBLIC, "", "int", vm_builtin_DOMNamedNodeMap_count },
		{ "getIterator",    PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
	};
	static const PH7_NativeMethodDef aListMethod[] = {
		{ "count",       PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNodeList_count },
		{ "item",        PH7_MOD_PUBLIC, "int $index", "", vm_builtin_DOMNodeList_item },
		{ "getIterator", PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
	};
	static const PH7_NativeMethodDef aMapMethod[] = {
		{ "count",        PH7_MOD_PUBLIC, "", "@int", vm_builtin_DOMNamedNodeMap_count },
		{ "item",         PH7_MOD_PUBLIC, "int $index", "@?DOMNode", vm_builtin_DOMNamedNodeMap_item },
		{ "getNamedItem", PH7_MOD_PUBLIC, "string $qualifiedName", "@?DOMNode",
		  vm_builtin_DOMNamedNodeMap_getNamedItem },
		{ "getNamedItemNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName", "@?DOMNode",
		  vm_builtin_DOMNamedNodeMap_getNamedItemNS },
		{ "getIterator",  PH7_MOD_PUBLIC, "", "Iterator", vm_builtin_Dom_getIterator },
	};
	/* The declaration itself, the document it belongs to, and the element that
	 * MAKES it -- php's parentNode/parentElement. */
	static const PH7_NativePropDef aNsNodeProp[] = {
		/* php's ten, in its order: a namespace declaration is not a DOMNode there,
		 * so the class states its own subset rather than inheriting one. */
		DOM_VPROP("nodeName","string"),
		DOM_VPROP("nodeValue","?string"),
		DOM_VPROP("nodeType","int"),
		DOM_VPROP("prefix","string"),
		DOM_VPROP("localName","?string"),
		DOM_VPROP("namespaceURI","?string"),
		DOM_VPROP("isConnected","bool"),
		DOM_VPROP("ownerDocument","?DOMDocument"),
		DOM_VPROP("parentNode","?DOMNode"),
		DOM_VPROP("parentElement","?DOMElement"),
		{ DOM_RES,      PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_DOC,      PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_NS_OWNER, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aNsNodeMethod[] = {
		{ "__sleep",  PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },
		{ "__wakeup", PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },
	};
#define XP_HIDDEN_SLOTS \
		{ XP_DOC,    PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }, \
		{ XP_NSDEF,  PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL, 1, 0, 0.0 }, 0 }, \
		{ XP_NSREG,  PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }, \
		{ XP_FNMODE, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, \
		  { 0, 0, PH7_NATIVE_VAL_INT, XP_MODE_NONE, 0, 0.0 }, 0 }, \
		{ XP_FNREG,  PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 }, \
		{ XP_NSFN,   PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	static const PH7_NativePropDef aXPathProp[] = {
		/* php models both as VIRTUAL: `document` is read-only because its handler
		 * has no writer -- not because the slot is `readonly`, which is why php's
		 * isReadOnly() answers false and its modifiers are 513. The values live in
		 * the two hidden slots below. */
		DOM_VPROP("document","DOMDocument"),
		DOM_VPROP("registerNodeNamespaces","bool"),
		XP_HIDDEN_SLOTS
	};
	/* php's namespaced XPath declares the same two virtuals, over the same hidden
	 * slots; only the document's declared type moves with the tree. */
	static const PH7_NativePropDef aMXPathProp[] = {
		DOM_VPROP("document","Dom\\Document"),
		DOM_VPROP("registerNodeNamespaces","bool"),
		XP_HIDDEN_SLOTS
	};
	static const PH7_NativeMethodDef aXPathMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "DOMDocument $document, bool $registerNodeNS = true", "",
		  vm_builtin_DOMXPath_construct },
		{ "query",       PH7_MOD_PUBLIC,
		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",
		  vm_builtin_DOMXPath_query },
		{ "evaluate",    PH7_MOD_PUBLIC,
		  "string $expression, ?DOMNode $contextNode = null, bool $registerNodeNS = true", "@mixed",
		  vm_builtin_DOMXPath_evaluate },
		{ "registerNamespace", PH7_MOD_PUBLIC, "string $prefix, string $namespace", "@bool",
		  vm_builtin_DOMXPath_registerNamespace },
		{ "registerPhpFunctions", PH7_MOD_PUBLIC, "array|string|null $restrict = null", "@void",
		  vm_builtin_DOMXPath_registerPhpFunctions },
		{ "registerPhpFunctionNS", PH7_MOD_PUBLIC,
		  "string $namespaceURI, string $name, callable $callable", "void",
		  vm_builtin_DOMXPath_registerPhpFunctionNS },
		{ "quote", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string $str", "string",
		  vm_builtin_DOMXPath_quote },
	};
	/* php 8.4's `Dom\XPath`: the same seven members over the same C bodies, and
	 * the differences are all in what the signatures SAY. The document and the
	 * context node are of the namespaced tree; query() promises a `Dom\NodeList`
	 * where the 2004 one may answer false, and evaluate()'s union has no false
	 * in it either -- so an evaluation that cannot happen raises there. */
	static const PH7_NativeMethodDef aMXPathMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC,
		  "Dom\\Document $document, bool $registerNodeNS = true", "",
		  vm_builtin_DOMXPath_construct },
		{ "evaluate",    PH7_MOD_PUBLIC,
		  "string $expression, ?Dom\\Node $contextNode = null, bool $registerNodeNS = true",
		  "Dom\\NodeList|string|float|bool|null", vm_builtin_DOMXPath_evaluate },
		{ "query",       PH7_MOD_PUBLIC,
		  "string $expression, ?Dom\\Node $contextNode = null, bool $registerNodeNS = true",
		  "Dom\\NodeList", vm_builtin_DOMXPath_query },
		{ "registerNamespace", PH7_MOD_PUBLIC, "string $prefix, string $namespace", "bool",
		  vm_builtin_DOMXPath_registerNamespace },
		{ "registerPhpFunctions", PH7_MOD_PUBLIC, "array|string|null $restrict = null", "void",
		  vm_builtin_DOMXPath_registerPhpFunctions },
		{ "registerPhpFunctionNS", PH7_MOD_PUBLIC,
		  "string $namespaceURI, string $name, callable $callable", "void",
		  vm_builtin_DOMXPath_registerPhpFunctionNS },
		{ "quote", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string $str", "string",
		  vm_builtin_DOMXPath_quote },
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
	static const PH7_NativeMethodDef aChildNodeIf[] = {
		{ "remove",      PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "", "void", 0 },
		{ "before",      PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },
		{ "after",       PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },
		{ "replaceWith", PH7_MOD_PUBLIC|PH7_MOD_ABSTRACT, "...$nodes", "void", 0 },
	};
	/* The remaining classes' OWN declarations, each php's list in php's order.
	 * They add no storage: every row is virtual, so these tables exist only to put
	 * the names on the class -- for Reflection, for property_exists(), and for the
	 * debug table above to walk. */
	static const PH7_NativePropDef aElemProp[] = {
		DOM_VPROP("tagName","string"),
		DOM_VPROP("className","string"),
		DOM_VPROP("id","string"),
		DOM_VPROP("schemaTypeInfo","mixed"),
		DOM_PARENT_VPROPS,
		DOM_CHILD_VPROPS
	};
	static const PH7_NativePropDef aAttrProp[] = {
		DOM_VPROP("name","string"),
		DOM_VPROP("specified","bool"),
		DOM_VPROP("value","string"),
		DOM_VPROP("ownerElement","?DOMElement"),
		DOM_VPROP("schemaTypeInfo","mixed")
	};
	static const PH7_NativePropDef aCharProp[] = {
		DOM_VPROP("data","string"),
		DOM_VPROP("length","int"),
		DOM_CHILD_VPROPS
	};
	static const PH7_NativePropDef aTextProp[] = {
		DOM_VPROP("wholeText","string")
	};
	static const PH7_NativePropDef aPiProp[] = {
		DOM_VPROP("target","string"),
		DOM_VPROP("data","string")
	};
	static const PH7_NativePropDef aFragProp[] = { DOM_PARENT_VPROPS };
	static const PH7_NativePropDef aDocTypeProp[] = {
		DOM_VPROP("name","string"),
		DOM_VPROP("entities","DOMNamedNodeMap"),
		DOM_VPROP("notations","DOMNamedNodeMap"),
		DOM_VPROP("publicId","string"),
		DOM_VPROP("systemId","string"),
		DOM_VPROP("internalSubset","?string")
	};
	static const PH7_NativePropDef aEntityProp[] = {
		DOM_VPROP("publicId","?string"),
		DOM_VPROP("systemId","?string"),
		DOM_VPROP("notationName","?string"),
		DOM_VPROP("actualEncoding","?string"),
		DOM_VPROP("encoding","?string"),
		DOM_VPROP("version","?string")
	};
	/* php's pair here are plain `string`, unlike DOMEntity's same-named `?string`. */
	static const PH7_NativePropDef aNotationProp[] = {
		DOM_VPROP("publicId","string"),
		DOM_VPROP("systemId","string")
	};
	/* php's DOMException REDECLARES Exception's `$code` as a PUBLIC untyped slot --
	 * which is why `(array)$e` and `get_object_vars($e)` show a plain `code` there
	 * where every other exception mangles it, and why Reflection reports it as
	 * DOMException's own with modifiers 1. The instance keeps the position the BASE
	 * declared it in, so var_dump still prints it third. */
	static const PH7_NativePropDef aExcProp[] = {
		{ "code", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_INT, 0, 0, 0.0 }, 0 }
	};
	/* php 8.4's node tree. Every property below is one php declares on that
	 * class and on no other -- the four DOMNode carries that `Dom\Node` does
	 * not (`attributes`, `prefix`, `localName`, `namespaceURI`) moved down onto
	 * the element and the attribute, which are the two that can answer them.
	 *
	 * php declares `children`, `classList` and `implementation` as real typed
	 * SLOTS rather than virtual ones; those three are not stated yet and are
	 * the tree's open residual, together with its methods. */
	static const PH7_NativePropDef aMNodeProp[] = {
		DOM_MNODE_VPROPS,
		{ DOM_RES,   PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_DOC,   PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_NODES, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	/*
	 * php 8.4's twenty-two, in its own declaration order -- and they are the
	 * 2004 bodies, because the WALK is the same walk: what changed between the
	 * two trees is the types on the outside, not the libxml underneath.
	 *
	 * Four differences the rows have to carry, none of them cosmetic:
	 *
	 *  - Every return type is REAL here where DOMNode's is tentative (`@`), and
	 *    the three php never wrote down at all on the old class -- cloneNode,
	 *    insertBefore's family -- are written down on this one. So a subclass
	 *    that widens a return is a compile error under `Dom\` and merely a
	 *    deprecation under `DOM`.
	 *  - Four parameters gained a `?`: isSameNode, lookupPrefix and
	 *    isDefaultNamespace take null where the 2004 pair took a bare string,
	 *    which is the standard's own signature finally stated. The bodies were
	 *    already null-tolerant -- they read an absent argument as the empty
	 *    string and screen on that -- so no body changes.
	 *  - `insertBefore`'s $child lost its default. php requires TWO arguments
	 *    here and one there; passing one is an ArgumentCountError, not a null.
	 *  - `getRootNode` takes a non-nullable `array $options = []` where the old
	 *    one takes `?array = null`. The body ignores it in both trees, exactly
	 *    as php's does.
	 *
	 * Two of DOMNode's rows are ABSENT: `hasAttributes` moved to `Dom\Element`,
	 * where only an element can answer it, and `isSupported` -- DOM Level 1's
	 * feature test -- was not carried forward at all.
	 */
	static const PH7_NativeMethodDef aMNodeMethod[] = {
		{ "__construct", PH7_MOD_PRIVATE|PH7_MOD_FINAL, "", "", vm_builtin_DomNode_construct },
		{ "getRootNode", PH7_MOD_PUBLIC, "array $options = []", "Dom\\Node",
		  vm_builtin_DOMNode_getRootNode },
		{ "hasChildNodes", PH7_MOD_PUBLIC, "", "bool", vm_builtin_DOMNode_hasChildNodes },
		{ "normalize", PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMDocument_normalizeDocument },
		{ "cloneNode", PH7_MOD_PUBLIC, "bool $deep = false", "Dom\\Node",
		  vm_builtin_DOMNode_cloneNode },
		{ "isEqualNode", PH7_MOD_PUBLIC, "?Dom\\Node $otherNode", "bool",
		  vm_builtin_DOMNode_isEqualNode },
		{ "isSameNode", PH7_MOD_PUBLIC, "?Dom\\Node $otherNode", "bool",
		  vm_builtin_DOMNode_isSameNode },
		{ "compareDocumentPosition", PH7_MOD_PUBLIC, "Dom\\Node $other", "int",
		  vm_builtin_DOMNode_compareDocumentPosition },
		{ "contains", PH7_MOD_PUBLIC, "?Dom\\Node $other", "bool",
		  vm_builtin_DOMNode_contains },
		{ "lookupPrefix", PH7_MOD_PUBLIC, "?string $namespace", "?string",
		  vm_builtin_DOMNode_lookupPrefix },
		{ "lookupNamespaceURI", PH7_MOD_PUBLIC, "?string $prefix", "?string",
		  vm_builtin_DOMNode_lookupNamespaceURI },
		{ "isDefaultNamespace", PH7_MOD_PUBLIC, "?string $namespace", "bool",
		  vm_builtin_DOMNode_isDefaultNamespace },
		/* `= !`: php declares $child with no default -- Reflection reports two
		 * required parameters, where DOMNode's reports one -- and then lets the
		 * call omit it anyway, so `$p->insertBefore($n)` appends under both
		 * names and a no-argument call says "expects at least 1". */
		{ "insertBefore", PH7_MOD_PUBLIC, "Dom\\Node $node, ?Dom\\Node $child = !", "Dom\\Node",
		  vm_builtin_DOMNode_insertBefore },
		{ "appendChild", PH7_MOD_PUBLIC, "Dom\\Node $node", "Dom\\Node",
		  vm_builtin_DOMNode_appendChild },
		{ "replaceChild", PH7_MOD_PUBLIC, "Dom\\Node $node, Dom\\Node $child", "Dom\\Node",
		  vm_builtin_DOMNode_replaceChild },
		{ "removeChild", PH7_MOD_PUBLIC, "Dom\\Node $child", "Dom\\Node",
		  vm_builtin_DOMNode_removeChild },
		{ "getLineNo", PH7_MOD_PUBLIC, "", "int", vm_builtin_DOMNode_getLineNo },
		{ "getNodePath", PH7_MOD_PUBLIC, "", "string", vm_builtin_DOMNode_getNodePath },
		{ "C14N", PH7_MOD_PUBLIC,
		  "bool $exclusive = false, bool $withComments = false, ?array $xpath = null, "
		  "?array $nsPrefixes = null", "string|false", vm_builtin_DOMNode_C14N },
		{ "C14NFile", PH7_MOD_PUBLIC,
		  "string $uri, bool $exclusive = false, bool $withComments = false, "
		  "?array $xpath = null, ?array $nsPrefixes = null", "int|false",
		  vm_builtin_DOMNode_C14NFile },
		{ "__sleep", PH7_MOD_PUBLIC, "", "array", vm_builtin_DOMNode_sleep },
		{ "__wakeup", PH7_MOD_PUBLIC, "", "void", vm_builtin_DOMNode_wakeup },
	};
	/* php's five, in its own declaration order -- the 2004 bodies again, because
	 * the CUT is the same cut. What the rows carry that DOMCharacterData's do not
	 * is a real return: the four writers are `void` where the old ones answer
	 * `true`, so `$t->appendData('x')` is null under this name and true under the
	 * other, and substringData's `string` is no longer tentative. */
	static const PH7_NativeMethodDef aMCharMethod[] = {
		{ "substringData", PH7_MOD_PUBLIC, "int $offset, int $count", "string",
		  vm_builtin_DOMCharacterData_substringData },
		{ "appendData",    PH7_MOD_PUBLIC, "string $data", "void",
		  vm_builtin_DOMCharacterData_appendData },
		{ "insertData",    PH7_MOD_PUBLIC, "int $offset, string $data", "void",
		  vm_builtin_DOMCharacterData_insertData },
		{ "deleteData",    PH7_MOD_PUBLIC, "int $offset, int $count", "void",
		  vm_builtin_DOMCharacterData_deleteData },
		{ "replaceData",   PH7_MOD_PUBLIC, "int $offset, int $count, string $data", "void",
		  vm_builtin_DOMCharacterData_replaceData },
		DOM_MCHILD_METHODS
	};
	/* `Dom\Element` carries BOTH mixins, child side first -- php's order, and
	 * the reverse of the order its `implements` clause names them in. */
	/* The attribute API, php's declaration order. It is the 2004 element's set
	 * of C bodies over the same libxml attributes; what the new tree changed is
	 * the SHAPE of four answers -- an absent attribute reads null rather than
	 * "" or false, and the two writes that handed back the node they touched
	 * are plain `void` here. Each body asks its receiver's family. */
	static const PH7_NativeMethodDef aMElemMethod[] = {
		{ "hasAttributes",    PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_DOMNode_hasAttributes },
		{ "getAttributeNames", PH7_MOD_PUBLIC, "", "array",
		  vm_builtin_DOMElement_getAttributeNames },
		{ "getAttribute",     PH7_MOD_PUBLIC, "string $qualifiedName", "?string",
		  vm_builtin_DOMElement_getAttribute },
		{ "getAttributeNS",   PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "?string", vm_builtin_DOMElement_getAttributeNS },
		{ "setAttribute",     PH7_MOD_PUBLIC, "string $qualifiedName, string $value",
		  "void", vm_builtin_DOMElement_setAttribute },
		{ "setAttributeNS",   PH7_MOD_PUBLIC,
		  "?string $namespace, string $qualifiedName, string $value", "void",
		  vm_builtin_DOMElement_setAttributeNS },
		{ "removeAttribute",  PH7_MOD_PUBLIC, "string $qualifiedName", "void",
		  vm_builtin_DOMElement_removeAttribute },
		{ "removeAttributeNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "void", vm_builtin_DOMElement_removeAttributeNS },
		{ "toggleAttribute",  PH7_MOD_PUBLIC, "string $qualifiedName, ?bool $force = null",
		  "bool", vm_builtin_DOMElement_toggleAttribute },
		{ "hasAttribute",     PH7_MOD_PUBLIC, "string $qualifiedName", "bool",
		  vm_builtin_DOMElement_hasAttribute },
		{ "hasAttributeNS",   PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "bool", vm_builtin_DOMElement_hasAttributeNS },
		{ "getAttributeNode", PH7_MOD_PUBLIC, "string $qualifiedName", "?Dom\\Attr",
		  vm_builtin_DOMElement_getAttributeNode },
		{ "getAttributeNodeNS", PH7_MOD_PUBLIC, "?string $namespace, string $localName",
		  "?Dom\\Attr", vm_builtin_DOMElement_getAttributeNodeNS },
		{ "setAttributeNode", PH7_MOD_PUBLIC, "Dom\\Attr $attr", "?Dom\\Attr",
		  vm_builtin_DOMElement_setAttributeNode },
		{ "setAttributeNodeNS", PH7_MOD_PUBLIC, "Dom\\Attr $attr", "?Dom\\Attr",
		  vm_builtin_DOMElement_setAttributeNodeNS },
		{ "removeAttributeNode", PH7_MOD_PUBLIC, "Dom\\Attr $attr", "Dom\\Attr",
		  vm_builtin_DOMElement_removeAttributeNode },
		/* The two live lookups, which the 2004 element has had all along: the
		 * body already picks the collection class off the document's family, so
		 * the same C answers a DOMNodeList there and a `Dom\HTMLCollection`
		 * here. What the row states is the TYPE -- php's namespaced tree is
		 * fully typed where the 2004 one is tentative, so no `@`. */
		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName",
		  "Dom\\HTMLCollection", vm_builtin_Dom_getElementsByTagName },
		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC,
		  "?string $namespace, string $localName", "Dom\\HTMLCollection",
		  vm_builtin_Dom_getElementsByTagNameNS },
		{ "setIdAttribute",   PH7_MOD_PUBLIC, "string $qualifiedName, bool $isId", "void",
		  vm_builtin_DOMElement_setIdAttribute },
		{ "setIdAttributeNS", PH7_MOD_PUBLIC,
		  "?string $namespace, string $qualifiedName, bool $isId", "void",
		  vm_builtin_DOMElement_setIdAttributeNS },
		{ "setIdAttributeNode", PH7_MOD_PUBLIC, "Dom\\Attr $attr, bool $isId", "void",
		  vm_builtin_DOMElement_setIdAttributeNode },
		DOM_MCHILD_METHODS,
		DOM_MPARENT_METHODS,
		/* php declares it LAST on this class, behind both mixins -- the one row
		 * whose position the sorted sweeps could not have told us. */
		{ "rename", PH7_MOD_PUBLIC, "?string $namespaceURI, string $qualifiedName", "void",
		  vm_builtin_Dom_rename }
	};
	static const PH7_NativeMethodDef aMDocTypeMethod[] = {
		DOM_MCHILD_METHODS
	};
	static const PH7_NativeMethodDef aMFragMethod[] = {
		DOM_MPARENT_METHODS
	};
	/* On the ABSTRACT document, not on its two final subclasses: php declares
	 * them once and both documents inherit the one declaration.
	 *
	 * The factory, in php's own declaration order, is the 2004 bodies under the
	 * new API's signatures -- the node a factory builds is wrapped by family
	 * already (DomWrapClassName reads the document's own flag), so
	 * `$doc->createElement('x')` answers a `Dom\Element` here and a DOMElement
	 * there without the bodies knowing. What the signatures change is real:
	 * `createElement` has no `$value` second parameter at all, so the entity
	 * parse the 2004 one carries is unreachable from this tree, and
	 * `createProcessingInstruction`'s `$data` is REQUIRED, which is what makes
	 * the empty-versus-absent distinction observable on every call.
	 * `createAttributeNS` is the one that is NOT a shared body: php's
	 * namespaced one is a different factory rather than a retyped one, and the
	 * one function carries both under DomThisModern. */
	static const PH7_NativeMethodDef aMDocMethod[] = {
		/* php declares the two lookups BEFORE the factory on this tree, where
		 * the 2004 document declares them after it. */
		{ "getElementsByTagName", PH7_MOD_PUBLIC, "string $qualifiedName",
		  "Dom\\HTMLCollection", vm_builtin_Dom_getElementsByTagName },
		{ "getElementsByTagNameNS", PH7_MOD_PUBLIC,
		  "?string $namespace, string $localName", "Dom\\HTMLCollection",
		  vm_builtin_Dom_getElementsByTagNameNS },
		{ "createElement",        PH7_MOD_PUBLIC, "string $localName", "Dom\\Element",
		  vm_builtin_DOMDocument_createElement },
		{ "createElementNS",      PH7_MOD_PUBLIC,
		  "?string $namespace, string $qualifiedName", "Dom\\Element",
		  vm_builtin_DOMDocument_createElementNS },
		{ "createDocumentFragment", PH7_MOD_PUBLIC, "", "Dom\\DocumentFragment",
		  vm_builtin_DOMDocument_createFragment },
		{ "createTextNode",       PH7_MOD_PUBLIC, "string $data", "Dom\\Text",
		  vm_builtin_DOMDocument_createTextNode },
		{ "createCDATASection",   PH7_MOD_PUBLIC, "string $data", "Dom\\CDATASection",
		  vm_builtin_DOMDocument_createCDATASection },
		{ "createComment",        PH7_MOD_PUBLIC, "string $data", "Dom\\Comment",
		  vm_builtin_DOMDocument_createComment },
		{ "createProcessingInstruction", PH7_MOD_PUBLIC, "string $target, string $data",
		  "Dom\\ProcessingInstruction", vm_builtin_DOMDocument_createPI },
		{ "createAttribute",      PH7_MOD_PUBLIC, "string $localName", "Dom\\Attr",
		  vm_builtin_DOMDocument_createAttribute },
		{ "createAttributeNS",    PH7_MOD_PUBLIC,
		  "?string $namespace, string $qualifiedName", "Dom\\Attr",
		  vm_builtin_DOMDocument_createAttributeNS },
		{ "getElementById",       PH7_MOD_PUBLIC, "string $elementId", "?Dom\\Element",
		  vm_builtin_DOMDocument_getElementById },
		DOM_MPARENT_METHODS
	};
	/* php's return here is `Dom\Text` and not `Dom\Text|false`, which is what
	 * makes the past-end offset an Index Size Error rather than a false. A
	 * `Dom\CDATASection` splits into a `Dom\Text` exactly as the 2004 pair do. */
	static const PH7_NativeMethodDef aMTextMethod[] = {
		{ "splitText", PH7_MOD_PUBLIC, "int $offset", "Dom\\Text",
		  vm_builtin_DOMText_splitText },
	};
	static const PH7_NativePropDef aMCharProp[] = {
		DOM_MCHILD_VPROPS,
		DOM_VPROP("data","string"),
		DOM_VPROP("length","int")
	};
	/* php's namespaced attribute declares exactly two, in this order. `isId` is
	 * the 2004 body unchanged -- the ID flag is libxml's and the tree asking
	 * makes no difference to it. */
	static const PH7_NativeMethodDef aMAttrMethod[] = {
		{ "isId",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_DOMAttr_isId },
		{ "rename", PH7_MOD_PUBLIC, "?string $namespaceURI, string $qualifiedName", "void",
		  vm_builtin_Dom_rename }
	};
	static const PH7_NativePropDef aMAttrProp[] = {
		DOM_VPROP("namespaceURI","?string"),
		DOM_VPROP("prefix","?string"),
		DOM_VPROP("localName","string"),
		DOM_VPROP("name","string"),
		DOM_VPROP("value","string"),
		DOM_VPROP("ownerElement","?Dom\\Element"),
		DOM_VPROP("specified","bool")
	};
	static const PH7_NativePropDef aMElemProp[] = {
		DOM_VPROP("namespaceURI","?string"),
		DOM_VPROP("prefix","?string"),
		DOM_VPROP("localName","string"),
		DOM_VPROP("tagName","string"),
		DOM_VPROP("id","string"),
		DOM_VPROP("className","string"),
		DOM_VPROP("attributes","Dom\\NamedNodeMap"),
		DOM_MPARENT_VPROPS,
		DOM_MCHILD_VPROPS
	};
	static const PH7_NativePropDef aMTextProp[] = { DOM_VPROP("wholeText","string") };
	static const PH7_NativePropDef aMPiProp[] = { DOM_VPROP("target","string") };
	static const PH7_NativePropDef aMFragProp[] = { DOM_MPARENT_VPROPS };
	static const PH7_NativePropDef aMDocTypeProp[] = {
		DOM_VPROP("name","string"),
		DOM_VPROP("entities","Dom\\DtdNamedNodeMap"),
		DOM_VPROP("notations","Dom\\DtdNamedNodeMap"),
		DOM_VPROP("publicId","string"),
		DOM_VPROP("systemId","string"),
		DOM_VPROP("internalSubset","?string")
	};
	/* Three where DOMEntity states six: the new class drops the pair libxml
	 * cannot answer for an entity and the XML-declaration echo beside them. */
	static const PH7_NativePropDef aMEntityProp[] = {
		DOM_VPROP("publicId","?string"),
		DOM_VPROP("systemId","?string"),
		DOM_VPROP("notationName","?string")
	};
	static const PH7_NativePropDef aMNotationProp[] = {
		DOM_VPROP("publicId","string"),
		DOM_VPROP("systemId","string")
	};
	/* The abstract document. Its flag word is DOMDocument's -- the parse
	 * directives are still directives here, and DOM_F_MODERN rides in it. */
	static const PH7_NativePropDef aMDocProp[] = {
		DOM_VPROP("URL","string"),
		DOM_VPROP("documentURI","string"),
		DOM_VPROP("characterSet","string"),
		DOM_VPROP("charset","string"),
		DOM_VPROP("inputEncoding","string"),
		DOM_VPROP("doctype","?Dom\\DocumentType"),
		DOM_VPROP("documentElement","?Dom\\Element"),
		DOM_MPARENT_VPROPS,
		DOM_VPROP("body","?Dom\\HTMLElement"),
		DOM_VPROP("head","?Dom\\HTMLElement"),
		DOM_VPROP("title","string"),
		{ DOM_DFLAGS, PH7_MOD_PUBLIC|PH7_MOD_HIDDEN,
		  { 0, 0, PH7_NATIVE_VAL_INT, DOM_F_DEFAULT, 0, 0.0 }, 0 },
		{ DOM_NODES,  PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ DOM_NCLS,   PH7_MOD_PUBLIC|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativePropDef aMXmlDocProp[] = {
		DOM_VPROP("xmlEncoding","string"),
		DOM_VPROP("xmlStandalone","bool"),
		DOM_VPROP("xmlVersion","string"),
		DOM_VPROP("formatOutput","bool")
	};
	/* The three static producers, which are the whole door into the tree. */
	static const PH7_NativeMethodDef aMXmlDocMethod[] = {
		{ "createEmpty", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $version = '1.0', string $encoding = 'UTF-8'", "Dom\\XMLDocument",
		  vm_builtin_DomXMLDocument_createEmpty },
		{ "createFromFile", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $path, int $options = 0, ?string $overrideEncoding = null",
		  "Dom\\XMLDocument", vm_builtin_DomXMLDocument_createFromFile },
		{ "createFromString", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "string $source, int $options = 0, ?string $overrideEncoding = null",
		  "Dom\\XMLDocument", vm_builtin_DomXMLDocument_createFromString },
		/* The entity reference is the XML document's alone -- php declares it
		 * here and not on the abstract document above, because an HTML document
		 * has no entity declarations to refer to. */
		{ "createEntityReference", PH7_MOD_PUBLIC, "string $name", "Dom\\EntityReference",
		  vm_builtin_DOMDocument_createEntityRef },
		/* The writers. php declares them on each final document rather than on
		 * the abstract one above, so the name they answer under is the final
		 * class's -- and the HTML document states two more beside these. */
		{ "saveXml", PH7_MOD_PUBLIC, "?Dom\\Node $node = null, int $options = 0",
		  "@string|false", vm_builtin_DomXMLDocument_saveXml },
		{ "saveXmlFile", PH7_MOD_PUBLIC, "string $filename, int $options = 0",
		  "@int|false", vm_builtin_DomXMLDocument_saveXmlFile },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "DOMException", "Exception", 0, PH7_CLASS_FINAL, 0, 0, 0, 0,
		  aExcProp, SX_ARRAYSIZE(aExcProp), 0, 0, 0 },
		{ "DOMParentNode", 0, 0, PH7_CLASS_INTERFACE,
		  aParentNodeIf, SX_ARRAYSIZE(aParentNodeIf), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMChildNode", 0, 0, PH7_CLASS_INTERFACE,
		  aChildNodeIf, SX_ARRAYSIZE(aChildNodeIf), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aNodeMethod, SX_ARRAYSIZE(aNodeMethod), aNodeConst, SX_ARRAYSIZE(aNodeConst),
		  aNodeProp, SX_ARRAYSIZE(aNodeProp), 0, 0, DomPresent },
		{ "DOMDocument", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,
		  aDocMethod, SX_ARRAYSIZE(aDocMethod), 0, 0, aDocProp, SX_ARRAYSIZE(aDocProp),
		  DomDocRelease, 0, DomPresent },
		{ "DOMElement", "DOMNode", "DOMParentNode,DOMChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,
		  aElemMethod, SX_ARRAYSIZE(aElemMethod), 0, 0, aElemProp, SX_ARRAYSIZE(aElemProp),
		  0, 0, DomPresent },
		{ "DOMAttr", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aAttrMethod, SX_ARRAYSIZE(aAttrMethod), 0, 0, aAttrProp, SX_ARRAYSIZE(aAttrProp),
		  0, 0, DomPresent },
		{ "DOMCharacterData", "DOMNode", "DOMChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,
		  aCharMethod, SX_ARRAYSIZE(aCharMethod), 0, 0, aCharProp, SX_ARRAYSIZE(aCharProp),
		  0, 0, DomPresent },
		{ "DOMText", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aTextMethod, SX_ARRAYSIZE(aTextMethod), 0, 0, aTextProp, SX_ARRAYSIZE(aTextProp),
		  0, 0, DomPresent },
		{ "DOMComment", "DOMCharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aCommentMethod, SX_ARRAYSIZE(aCommentMethod), 0, 0, 0, 0, 0, 0, DomPresent },
		{ "DOMCdataSection", "DOMText", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aCdataMethod, SX_ARRAYSIZE(aCdataMethod), 0, 0, 0, 0, 0, 0, DomPresent },
		/* php declares the PI under DOMNode (its `data` is its own property, not
		 * DOMCharacterData's), the fragment and the entity reference plainly. */
		{ "DOMProcessingInstruction", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aPiMethod, SX_ARRAYSIZE(aPiMethod), 0, 0, aPiProp, SX_ARRAYSIZE(aPiProp),
		  0, 0, DomPresent },
		{ "DOMDocumentFragment", "DOMNode", "DOMParentNode", PH7_CLASS_NOSERIALIZE_SUBOK,
		  aFragMethod, SX_ARRAYSIZE(aFragMethod), 0, 0, aFragProp, SX_ARRAYSIZE(aFragProp),
		  0, 0, DomPresent },
		{ "DOMEntityReference", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aEntRefMethod, SX_ARRAYSIZE(aEntRefMethod), 0, 0, 0, 0, 0, 0, DomPresent },
		/* The DTD trio state no method of their own -- every name php declares on
		 * them is a property, and the class's handler answers it. */
		{ "DOMDocumentType", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, aDocTypeProp,
		  SX_ARRAYSIZE(aDocTypeProp), 0, 0, DomPresent },
		{ "DOMEntity", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, aEntityProp,
		  SX_ARRAYSIZE(aEntityProp), 0, 0, DomPresent },
		{ "DOMImplementation", 0, 0, 0,
		  aImplMethod, SX_ARRAYSIZE(aImplMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMNotation", "DOMNode", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, aNotationProp,
		  SX_ARRAYSIZE(aNotationProp), 0, 0, DomPresent },
		/* php's own two: IteratorAggregate (NOT Iterator -- the chunk had the
		 * list carry its own cursor) and Countable. */
		{ "DOMNodeList", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,
		  aListMethod, SX_ARRAYSIZE(aListMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),
		  0, &sDomListIterVtab, DomPresent },
		{ "DOMNamedNodeMap", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,
		  aMapMethod, SX_ARRAYSIZE(aMapMethod), 0, 0, aListProp, SX_ARRAYSIZE(aListProp),
		  0, &sDomMapIterVtab, DomPresent },
		/* php's own: a class of its OWN, with no parent at all -- a namespace
		 * declaration is not a DOMNode there, and `$ns instanceof DOMNode` is
		 * false. Its refusal to serialize is the same soft kind the node
		 * classes carry. */
		{ "DOMNameSpaceNode", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aNsNodeMethod, SX_ARRAYSIZE(aNsNodeMethod), 0, 0,
		  aNsNodeProp, SX_ARRAYSIZE(aNsNodeProp), 0, 0, DomPresent },
		/* php 8.4's namespaced API. The two interfaces carry no property; the
		 * collection shares DOMNodeList's slot layout, its iterator vtable and
		 * its clone refusal, because it is the same live view under a new name. */
		{ "Dom\\ChildNode", 0, 0, PH7_CLASS_INTERFACE,
		  aDomChildNodeIf, SX_ARRAYSIZE(aDomChildNodeIf), 0, 0, 0, 0, 0, 0, 0 },
		{ "Dom\\ParentNode", 0, 0, PH7_CLASS_INTERFACE,
		  aDomParentNodeIf, SX_ARRAYSIZE(aDomParentNodeIf), 0, 0, 0, 0, 0, 0, 0 },
		{ "Dom\\HTMLCollection", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,
		  aDomCollMethod, SX_ARRAYSIZE(aDomCollMethod), 0, 0,
		  aListProp, SX_ARRAYSIZE(aListProp), 0, &sDomListIterVtab, DomPresent },
		/* And its three siblings. Each is php's own class, unrelated to the
		 * 2004 one it shadows -- `Dom\NodeList` is not a DOMNodeList and the two
		 * trees never meet -- but the live view underneath is the same, so all
		 * four wear DOMNodeList's slot layout and its iterator. php serializes a
		 * bare one (`O:12:"Dom\NodeList":0:{}`) and refuses to clone it, which is
		 * NOCLONE alone. */
		{ "Dom\\NodeList", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,
		  aDomNodeListMethod, SX_ARRAYSIZE(aDomNodeListMethod), 0, 0,
		  aListProp, SX_ARRAYSIZE(aListProp), 0, &sDomListIterVtab, DomPresent },
		{ "Dom\\NamedNodeMap", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,
		  aDomMapMethod, SX_ARRAYSIZE(aDomMapMethod), 0, 0,
		  aListProp, SX_ARRAYSIZE(aListProp), 0, &sDomMapIterVtab, DomPresent },
		{ "Dom\\DtdNamedNodeMap", 0, "IteratorAggregate,Countable", PH7_CLASS_NOCLONE,
		  aDomDtdMapMethod, SX_ARRAYSIZE(aDomDtdMapMethod), 0, 0,
		  aListProp, SX_ARRAYSIZE(aListProp), 0, &sDomMapIterVtab, DomPresent },
		/* The namespaced NODE tree. php's shapes, which are not the 2004 ones:
		 * the PI is a CharacterData, the fragment is a ParentNode and not a
		 * ChildNode, `Dom\Document` is ABSTRACT with two final documents under
		 * it, and no row here has a DOM* class anywhere in its chain. */
		{ "Dom\\Node", 0, 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMNodeMethod, SX_ARRAYSIZE(aMNodeMethod), aNodeConst, SX_ARRAYSIZE(aNodeConst),
		  aMNodeProp, SX_ARRAYSIZE(aMNodeProp), 0, 0, DomPresent },
		{ "Dom\\CharacterData", "Dom\\Node", "Dom\\ChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMCharMethod, SX_ARRAYSIZE(aMCharMethod), 0, 0,
		  aMCharProp, SX_ARRAYSIZE(aMCharProp), 0, 0, DomPresent },
		{ "Dom\\Attr", "Dom\\Node", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMAttrMethod, SX_ARRAYSIZE(aMAttrMethod), 0, 0,
		  aMAttrProp, SX_ARRAYSIZE(aMAttrProp), 0, 0, DomPresent },
		{ "Dom\\Element", "Dom\\Node", "Dom\\ChildNode,Dom\\ParentNode",
		  PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMElemMethod, SX_ARRAYSIZE(aMElemMethod), 0, 0,
		  aMElemProp, SX_ARRAYSIZE(aMElemProp), 0, 0, DomPresent },
		/* php's HTML element states nothing of its own; it is the class an HTML
		 * document's elements wear, and the difference is the family. */
		{ "Dom\\HTMLElement", "Dom\\Element", "Dom\\ChildNode,Dom\\ParentNode",
		  PH7_CLASS_NOSERIALIZE_SUBOK, 0, 0, 0, 0, 0, 0, 0, 0, DomPresent },
		{ "Dom\\Text", "Dom\\CharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMTextMethod, SX_ARRAYSIZE(aMTextMethod), 0, 0,
		  aMTextProp, SX_ARRAYSIZE(aMTextProp), 0, 0, DomPresent },
		{ "Dom\\CDATASection", "Dom\\Text", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, 0, 0, 0, 0, DomPresent },
		{ "Dom\\Comment", "Dom\\CharacterData", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, 0, 0, 0, 0, DomPresent },
		{ "Dom\\ProcessingInstruction", "Dom\\CharacterData", 0,
		  PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, aMPiProp, SX_ARRAYSIZE(aMPiProp), 0, 0, DomPresent },
		{ "Dom\\DocumentFragment", "Dom\\Node", "Dom\\ParentNode",
		  PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMFragMethod, SX_ARRAYSIZE(aMFragMethod), 0, 0,
		  aMFragProp, SX_ARRAYSIZE(aMFragProp), 0, 0, DomPresent },
		{ "Dom\\DocumentType", "Dom\\Node", "Dom\\ChildNode", PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMDocTypeMethod, SX_ARRAYSIZE(aMDocTypeMethod), 0, 0,
		  aMDocTypeProp, SX_ARRAYSIZE(aMDocTypeProp), 0, 0, DomPresent },
		{ "Dom\\Entity", "Dom\\Node", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, aMEntityProp, SX_ARRAYSIZE(aMEntityProp), 0, 0, DomPresent },
		{ "Dom\\EntityReference", "Dom\\Node", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, 0, 0, 0, 0, DomPresent },
		{ "Dom\\Notation", "Dom\\Node", 0, PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, aMNotationProp, SX_ARRAYSIZE(aMNotationProp), 0, 0, DomPresent },
		{ "Dom\\Document", "Dom\\Node", "Dom\\ParentNode",
		  PH7_CLASS_ABSTRACT|PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMDocMethod, SX_ARRAYSIZE(aMDocMethod), 0, 0,
		  aMDocProp, SX_ARRAYSIZE(aMDocProp), DomDocRelease, 0, DomPresent },
		{ "Dom\\XMLDocument", "Dom\\Document", "Dom\\ParentNode",
		  PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE_SUBOK,
		  aMXmlDocMethod, SX_ARRAYSIZE(aMXmlDocMethod), 0, 0,
		  aMXmlDocProp, SX_ARRAYSIZE(aMXmlDocProp), DomDocRelease, 0, DomPresent },
		{ "Dom\\HTMLDocument", "Dom\\Document", "Dom\\ParentNode",
		  PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE_SUBOK,
		  0, 0, 0, 0, 0, 0, DomDocRelease, 0, DomPresent },
		{ "Dom\\Implementation", 0, 0, PH7_CLASS_NOCLONE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "DOMXPath", 0, 0, PH7_CLASS_NOSERIALIZE|PH7_CLASS_NOCLONE,
		  aXPathMethod, SX_ARRAYSIZE(aXPathMethod), 0, 0, aXPathProp, SX_ARRAYSIZE(aXPathProp),
		  0, 0, DomPresent },
		/* php 8.4's evaluator over the namespaced tree. FINAL where the 2004 one
		 * may be extended, and the same two refusals: `Serialization of
		 * 'Dom\XPath' is not allowed`, and an uncloneable object. */
		{ "Dom\\XPath", 0, 0,
		  PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE|PH7_CLASS_NOCLONE,
		  aMXPathMethod, SX_ARRAYSIZE(aMXPathMethod), 0, 0,
		  aMXPathProp, SX_ARRAYSIZE(aMXPathProp), 0, 0, DomPresent },
	};
	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc == SXRET_OK ){
		/* The clone hook (ph7_class::xClone, php's clone_obj): stated on every
		 * node class -- rule 29, a hook is per-row and never inherited between
		 * native rows -- and assigned HERE because PH7_NativeClassSpec carries
		 * no field for it. The document's copies the whole document; a user
		 * subclass reaches the nearest ancestor's hook through the engine's
		 * chain walk, php's handler inheritance. */
		static const char * const azNodeClone[] = {
			"DOMNode", "DOMElement", "DOMAttr", "DOMCharacterData", "DOMText",
			"DOMComment", "DOMCdataSection", "DOMProcessingInstruction",
			"DOMDocumentFragment", "DOMEntityReference", "DOMDocumentType",
			/* And the namespaced tree's own, for the same two reasons: a hook is
			 * per-row and never inherited between native rows, and every node
			 * here holds an entry in a document's identity cache that its
			 * teardown has to take back out. */
			"Dom\\Node", "Dom\\Element", "Dom\\HTMLElement", "Dom\\Attr",
			"Dom\\CharacterData", "Dom\\Text", "Dom\\CDATASection",
			"Dom\\Comment", "Dom\\ProcessingInstruction",
			"Dom\\DocumentFragment", "Dom\\EntityReference",
			"Dom\\DocumentType", "Dom\\Entity", "Dom\\Notation"
		};
		sxu32 n;
		ph7_class *pClass;
		for( n = 0 ; n < SX_ARRAYSIZE(azNodeClone) ; ++n ){
			pClass = PH7_VmExtractClass(&(*pVm),azNodeClone[n],
				(sxu32)SyStrlen(azNodeClone[n]),FALSE,0);
			if( pClass ){
				pClass->xClone = DomInstanceClone;
			}
		}
		pClass = PH7_VmExtractClass(&(*pVm),"DOMDocument",sizeof("DOMDocument")-1,FALSE,0);
		if( pClass ){
			pClass->xClone = DomInstanceCloneDoc;
		}
		/* php's two final namespaced documents copy the whole document the same
		 * way; the abstract one between them can never be an instance. */
		pClass = PH7_VmExtractClass(&(*pVm),"Dom\\XMLDocument",
			sizeof("Dom\\XMLDocument")-1,FALSE,0);
		if( pClass ){
			pClass->xClone = DomInstanceCloneDoc;
		}
		pClass = PH7_VmExtractClass(&(*pVm),"Dom\\HTMLDocument",
			sizeof("Dom\\HTMLDocument")-1,FALSE,0);
		if( pClass ){
			pClass->xClone = DomInstanceCloneDoc;
		}
		/* And the teardown hook (ph7_class::xRelease), on the same rows for the
		 * same reason: the document's identity cache borrows its wrappers, so
		 * each node class has to take its own entry back out and free the
		 * libxml node it was the last holder of. DOMDocument keeps its own,
		 * which forgets the tree's back-pointer instead; DOMNameSpaceNode is
		 * not a node class here (it is not a DOMNode in php either) and its
		 * handle is the document's, shared. */
		for( n = 0 ; n < SX_ARRAYSIZE(azNodeClone) ; ++n ){
			pClass = PH7_VmExtractClass(&(*pVm),azNodeClone[n],
				(sxu32)SyStrlen(azNodeClone[n]),FALSE,0);
			if( pClass ){
				pClass->xRelease = DomNodeRelease;
			}
		}
		/* The dimension handlers (ph7_class::xDim, php's read_dimension /
		 * has_dimension), assigned here for the same reason the clone hook is:
		 * PH7_NativeClassSpec carries no field for them, and php's own two
		 * classes wear them without declaring ArrayAccess. */
		pClass = PH7_VmExtractClass(&(*pVm),"DOMNodeList",sizeof("DOMNodeList")-1,FALSE,0);
		if( pClass ){
			pClass->xDim = DomListDim;
		}
		pClass = PH7_VmExtractClass(&(*pVm),"DOMNamedNodeMap",sizeof("DOMNamedNodeMap")-1,FALSE,0);
		if( pClass ){
			pClass->xDim = DomMapDim;
		}
		/* php gave the namespaced four handlers of THEIR own, and the offset
		 * rules differ from the 2004 pair's in both directions -- see
		 * DomModernDim. `Dom\HTMLCollection` had none at all and answered the
		 * engine's `Cannot use object of type ... as array` to a subscript php
		 * reads. */
		{
			static const struct { const char *zName; void (*xDim)(ph7_vm *,
				ph7_class_instance *,PH7_NativeDimCtx *); } aModernDim[] = {
				{ "Dom\\NodeList",         DomModernListDim },
				{ "Dom\\HTMLCollection",   DomModernCollDim },
				{ "Dom\\NamedNodeMap",     DomModernMapDim  },
				{ "Dom\\DtdNamedNodeMap",  DomModernMapDim  }
			};
			for( n = 0 ; n < SX_ARRAYSIZE(aModernDim) ; ++n ){
				pClass = PH7_VmExtractClass(&(*pVm),aModernDim[n].zName,
					(sxu32)SyStrlen(aModernDim[n].zName),FALSE,0);
				if( pClass ){
					pClass->xDim = aModernDim[n].xDim;
				}
			}
		}
		/* The property handlers (ph7_class::xProp, php's read_property /
		 * has_property / write_property), assigned for the same reason. One per
		 * ROOT: the engine walks the base chain for the hook exactly as php's
		 * handlers are inherited, and the hook then picks the per-class table off
		 * aDomProp[] -- so a subclass of DOMElement reaches DOMElement's. */
		{
			static const char * const azPropRoot[] = {
				"DOMNode", "DOMNodeList", "DOMNamedNodeMap", "DOMNameSpaceNode", "DOMXPath",
				"Dom\\XPath", "Dom\\HTMLCollection",
				"Dom\\NodeList", "Dom\\NamedNodeMap", "Dom\\DtdNamedNodeMap",
				/* The namespaced tree's root. It is a root and not a branch of
				 * DOMNode's: the two trees never meet, so the chain walk from a
				 * `Dom\\Element` reaches nothing the 2004 one installed. */
				"Dom\\Node"
			};
			for( n = 0 ; n < SX_ARRAYSIZE(azPropRoot) ; ++n ){
				PH7_NativeClassInstallPropHook(&(*pVm),azPropRoot[n],DomPropHook);
			}
		}
		/*
		 * php 8.4's `Dom\AdjacentPosition` -- the four insertion points
		 * `Dom\Element::insertAdjacentElement()` and its two siblings name.
		 *
		 * It is STRING-backed, and the backing values are the HTML standard's
		 * lowercase spellings and not the case names: `from('beforebegin')` is
		 * the case and `from('BeforeBegin')` is a ValueError. A program that
		 * spells the position by hand is reading the standard, not the enum, so
		 * getting that pairing wrong would fail exactly the call the enum exists
		 * to type.
		 *
		 * The case table is `static` because the installer keeps a POINTER to
		 * each backing literal for the life of the class. A stack array is safe
		 * only for a PURE enum, which carries no literal at all -- which is why
		 * the engine's other two native enums build theirs on the stack.
		 */
		if( rc == SXRET_OK ){
			static const PH7_NativeEnumCase aAdjacent[] = {
				{ "BeforeBegin", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "beforebegin", 0.0 } },
				{ "AfterBegin",  { 0, 0, PH7_NATIVE_VAL_STRING, 0, "afterbegin",  0.0 } },
				{ "BeforeEnd",   { 0, 0, PH7_NATIVE_VAL_STRING, 0, "beforeend",   0.0 } },
				{ "AfterEnd",    { 0, 0, PH7_NATIVE_VAL_STRING, 0, "afterend",    0.0 } }
			};
			rc = PH7_InstallNativeEnum(&(*pVm),"Dom\\AdjacentPosition",MEMOBJ_STRING,
				aAdjacent,SX_ARRAYSIZE(aAdjacent),0,0);
		}
		/*
		 * `Dom\DOMException` is php's 2004 DOMException under a second name, not
		 * a class of its own: `get_class()` on one caught as `Dom\DOMException`
		 * answers `DOMException`, and `catch (DOMException)` catches what the
		 * namespaced surface throws. So it is an ALIAS -- a second key over the
		 * same class -- and php's own listing shows the key folded, which is the
		 * spelling entered here.
		 */
		if( rc == SXRET_OK ){
			static const char zAlias[] = "dom\\domexception";
			pClass = PH7_VmExtractClass(&(*pVm),"DOMException",sizeof("DOMException")-1,FALSE,0);
			if( pClass && SyHashGet(&pVm->hClass,(const void *)zAlias,
				sizeof(zAlias)-1) == 0 ){
				SyHashInsert(&pVm->hClass,(const void *)zAlias,sizeof(zAlias)-1,pClass);
			}
		}
	}
	return rc;
}

#else
/* Ensure non-empty translation unit when libxml is disabled (MSVC C4206) */
typedef int vm_dom_unused;
#endif /* PH7_ENABLE_LIBXML */
