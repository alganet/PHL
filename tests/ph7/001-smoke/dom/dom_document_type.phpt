--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DOCTYPE is a DOMDocumentType and says what it declares
--FILE--
<?php
$dom_dt_load = static function (string $xml): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML($xml);
    return $d;
};

// The node was always reachable; the class, the DOM's own nodeType and the
// four identifiers were not. A DTD is XML_DTD_NODE (14) to libxml and
// DOCUMENT_TYPE_NODE (10) to the DOM, and php reports the DOM's.
foreach ([
    'system'  => '<!DOCTYPE root SYSTEM "x.dtd"><root/>',
    'public'  => '<!DOCTYPE root PUBLIC "-//P//EN" "x.dtd"><root/>',
    'name'    => '<!DOCTYPE root><root/>',
    'empty'   => '<!DOCTYPE root []><root/>',
    'subset'  => '<!DOCTYPE root [<!ENTITY a "b">]><root/>',
] as $label => $xml) {
    $d = $dom_dt_load($xml);
    $dt = $d->doctype;
    printf("%-7s %s type=%d name=%s pub=%s sys=%s int=%s\n", $label, get_class($dt),
        $dt->nodeType, var_export($dt->name, true), var_export($dt->publicId, true),
        var_export($dt->systemId, true),
        var_export(str_replace("\n", '|', (string)$dt->internalSubset), true));
}
var_dump($dt->nodeType === XML_DOCUMENT_TYPE_NODE, $dt instanceof DOMNode,
    $dom_dt_load('<root/>')->doctype);

// It IS the document's first child, and the same object every time.
$d = $dom_dt_load('<!DOCTYPE r [<!ENTITY a "b">]><r/>');
$dt = $d->doctype;
var_dump($d->doctype === $dt, $d->firstChild === $dt, $dt->parentNode === $d,
    $dt->ownerDocument === $d, $dt->isConnected, $dt->nextSibling->nodeName,
    $dt->previousSibling);

// libxml links the declarations as CHILDREN and php lists them from
// childNodes -- while firstChild, lastChild and hasChildNodes() all answer as
// though there were none, which is php's dom_node_children_valid.
var_dump($dt->childNodes->length, $dt->firstChild, $dt->lastChild,
    $dt->hasChildNodes(), $dt->attributes, $dt->nodeName, $dt->nodeValue,
    $dt->textContent, $dt->localName, $dt->prefix, $dt->namespaceURI,
    $dt->getNodePath());

// A copy carries the whole internal subset; libxml's generic copier has no
// case for a DTD at all, so this used to answer false.
$c = $dt->cloneNode(true);
printf("clone %s name=%s int=%s parent=%s connected=%s\n", get_class($c),
    var_export($c->name, true),
    var_export(str_replace("\n", '|', (string)$c->internalSubset), true),
    var_export($c->parentNode, true), var_export($c->isConnected, true));
$c2 = clone $dt;
printf("clone-op %s same=%s name=%s\n", get_class($c2),
    var_export($c2 === $dt, true), var_export($c2->name, true));

// Every property is read-only, and the mutators answer the taxonomy php's do:
// an invalid-children receiver is a silent false, a DTD-owned parent is the
// No Modification refusal.
foreach (['name', 'publicId', 'systemId', 'internalSubset'] as $prop) {
    try {
        $dt->$prop = 'x';
    } catch (Throwable $ex) {
        printf("%-14s %s: %s\n", $prop, get_class($ex), $ex->getMessage());
    }
}
var_dump($dt->appendChild($d->createElement('x')),
    $dt->insertBefore($d->createElement('x')));
try {
    $dt->removeChild($dt->childNodes->item(0));
} catch (Throwable $ex) {
    printf("removeChild %s(%d): %s\n", get_class($ex), $ex->getCode(), $ex->getMessage());
}
var_dump(isset($dt->name), isset($dt->internalSubset), isset($dt->firstChild),
    $dt->C14N(), $dt->isEqualNode($dt), get_class($dt->getRootNode()));

// Taking it off the document leaves the document with no doctype at all.
var_dump(get_class($d->removeChild($dt)), $d->doctype,
    str_replace("\n", '', $d->saveXML()));
?>
--EXPECT--
system  DOMDocumentType type=10 name='root' pub='' sys='x.dtd' int=''
public  DOMDocumentType type=10 name='root' pub='-//P//EN' sys='x.dtd' int=''
name    DOMDocumentType type=10 name='root' pub='' sys='' int=''
empty   DOMDocumentType type=10 name='root' pub='' sys='' int=''
subset  DOMDocumentType type=10 name='root' pub='' sys='' int='<!ENTITY a "b">|'
bool(true)
bool(true)
NULL
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(1) "r"
NULL
int(1)
NULL
NULL
bool(false)
NULL
string(1) "r"
NULL
string(0) ""
NULL
string(0) ""
NULL
NULL
clone DOMDocumentType name='r' int='<!ENTITY a "b">|' parent=NULL connected=false
clone-op DOMDocumentType same=false name='r'
name           Error: Cannot modify readonly property DOMDocumentType::$name
publicId       Error: Cannot modify readonly property DOMDocumentType::$publicId
systemId       Error: Cannot modify readonly property DOMDocumentType::$systemId
internalSubset Error: Cannot modify readonly property DOMDocumentType::$internalSubset
bool(false)
bool(false)
removeChild DOMException(7): No Modification Allowed Error
bool(true)
bool(true)
bool(false)
string(0) ""
bool(true)
string(11) "DOMDocument"
string(15) "DOMDocumentType"
NULL
string(25) "<?xml version="1.0"?><r/>"
