--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DTD's notations are DOMNotation, and belong to no document
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<!DOCTYPE r ['
    . '<!NOTATION sys SYSTEM "v.exe">'
    . '<!NOTATION pub PUBLIC "-//N//EN">'
    . '<!NOTATION both PUBLIC "-//B//EN" "b.exe">'
    . '<!ENTITY e SYSTEM "e.gif" NDATA sys>'
    . ']><r/>');
$dt = $d->doctype;
$map = $dt->notations;

// A notation is NOT a child of the DTD the way every other declaration is:
// libxml keeps it in a table of its own, which is the only place php reads.
printf("%s len=%d count=%d children=%d\n", get_class($map), $map->length,
    $map->count(), $dt->childNodes->length);
$seen = [];
foreach ($map as $name => $n) {
    $seen[] = $name . ':' . get_class($n);
}
sort($seen);   // the map's own order is libxml's hash order
echo implode(' ', $seen), "\n";

// Both identifiers are plain strings that answer "" for the half a
// declaration leaves out -- where DOMEntity's same-named pair are ?string and
// answer null. And the node behind one has NO document: php builds it out of
// the three strings a notation declaration is, so `ownerDocument` is null,
// `isConnected` false and `getRootNode()` the notation itself.
foreach (['sys', 'pub', 'both'] as $name) {
    $n = $map->getNamedItem($name);
    printf("%-4s type=%d name=%s pub=%-12s sys=%-9s value=%s text=%s\n", $name,
        $n->nodeType, var_export($n->nodeName, true), var_export($n->publicId, true),
        var_export($n->systemId, true), var_export($n->nodeValue, true),
        var_export($n->textContent, true));
    printf("     parent=%s owner=%s connected=%s children=%d base=%s path=%s\n",
        var_export($n->parentNode, true), var_export($n->ownerDocument, true),
        var_export($n->isConnected, true), $n->childNodes->length,
        var_export($n->baseURI, true), var_export($n->getNodePath(), true));
}

$s = $map->getNamedItem('sys');
var_dump($map->getNamedItem('zz'), $map->item(99),
    get_class($map->getNamedItemNS(null, 'sys')), $dt->notations === $dt->notations,
    $s instanceof DOMNode, get_parent_class('DOMNotation'),
    isset($s->publicId), isset($s->parentNode),
    $dt->entities->getNamedItem('e')->notationName);

// Both properties are read-only, and the node-kind taxonomy is the one php
// answers: a notation takes no children, copying one is false, and it belongs
// to no document, so canonicalizing it is php's plain Error and serializing it
// through a document is the Wrong Document refusal.
foreach (['publicId', 'systemId'] as $prop) {
    try {
        $s->$prop = 'x';
    } catch (Throwable $ex) {
        printf("%-9s %s: %s\n", $prop, get_class($ex), $ex->getMessage());
    }
}
var_dump($s->appendChild($d->createElement('x')), $s->cloneNode(true),
    $s->isEqualNode($s), get_class($s->getRootNode()));
foreach (['C14N', 'saveXML'] as $call) {
    try {
        var_dump($call === 'C14N' ? $s->C14N() : $d->saveXML($s));
    } catch (Throwable $ex) {
        printf("%-8s %s(%d): %s\n", $call, get_class($ex), $ex->getCode(), $ex->getMessage());
    }
}

$empty = new DOMDocument;
$empty->loadXML('<!DOCTYPE r><r/>');
var_dump($empty->doctype->notations->length);
?>
--EXPECT--
DOMNamedNodeMap len=3 count=3 children=1
both:DOMNotation pub:DOMNotation sys:DOMNotation
sys  type=12 name='sys' pub=''           sys='v.exe'   value=NULL text=''
     parent=NULL owner=NULL connected=false children=0 base=NULL path=NULL
pub  type=12 name='pub' pub='-//N//EN'   sys=''        value=NULL text=''
     parent=NULL owner=NULL connected=false children=0 base=NULL path=NULL
both type=12 name='both' pub='-//B//EN'   sys='b.exe'   value=NULL text=''
     parent=NULL owner=NULL connected=false children=0 base=NULL path=NULL
NULL
NULL
string(11) "DOMNotation"
bool(false)
bool(true)
string(7) "DOMNode"
bool(true)
bool(false)
string(3) "sys"
publicId  Error: Cannot modify readonly property DOMNotation::$publicId
systemId  Error: Cannot modify readonly property DOMNotation::$systemId
bool(false)
bool(false)
bool(true)
string(11) "DOMNotation"
C14N     Error(0): Node must be associated with a document
saveXML  DOMException(4): Wrong Document Error
int(0)
