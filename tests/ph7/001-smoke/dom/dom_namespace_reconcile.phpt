--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A node linked into a tree is reconciled: declarations follow it in, redundant ones go
--FILE--
<?php
// The node moved is always the grandchild /r/a/<first>, the target always /r.
$dom_rec_move = static function (string $xml, int $op) {
    $d = new DOMDocument;
    $d->loadXML($xml);
    $r = $d->documentElement;
    $src = $r->getElementsByTagName('*')->item(1);
    if ($op === 0) $r->appendChild($src);
    elseif ($op === 1) $r->insertBefore($src, $r->firstChild);
    else $r->replaceChild($src, $r->firstChild);
    $out = $d->saveXML($r);
    // The bytes have to parse back: a prefix bound nowhere is not XML at all.
    $rr = new DOMDocument;
    $ok = @$rr->loadXML($d->saveXML());
    return $out . ' | reparse=' . ($ok ? 'ok' : 'FAILED')
        . ' | ' . $src->nodeName . '@' . var_export($src->namespaceURI, true);
};

// A node whose namespace was declared on the ancestor it LEAVES takes a
// declaration with it -- without one the serialization is not XML.
$decl = '<r><a xmlns:p="urn:p"><p:b k="1"/></a></r>';
var_dump($dom_rec_move($decl, 0));
var_dump($dom_rec_move($decl, 1));
var_dump($dom_rec_move($decl, 2));

// The same for a namespaced ATTRIBUTE: the declaration lands on the element
// that carries the attribute.
var_dump($dom_rec_move('<r><a xmlns:p="urn:p"><b p:k="1"/></a></r>', 0));

// A DEFAULT namespace going out of scope.
var_dump($dom_rec_move('<r><a xmlns="urn:d"><b/></a></r>', 0));

// A whole subtree: the declaration is made once, on the node that moved.
var_dump($dom_rec_move('<r><a xmlns:p="urn:p"><p:b><p:c d="1"/></p:b></a></r>', 0));

// A URI two prefixes bind is respelled to the FIRST of them, all the way down.
var_dump($dom_rec_move('<r><a xmlns:p="urn:x" xmlns:q="urn:x"><q:b q:k="1"/></a></r>', 0));

// ...and the other direction: a node that carries its own declaration loses it
// when the new parent already makes the same one.
$dom_rec_frag = static function (string $xml, string $chunk) {
    $d = new DOMDocument;
    $d->loadXML($xml);
    $f = $d->createDocumentFragment();
    $f->appendXML($chunk);
    $d->documentElement->appendChild($f);
    return $d->saveXML($d->documentElement);
};
var_dump($dom_rec_frag('<r xmlns:p="urn:p" xmlns="urn:def"><c/></r>',
    '<p:y xmlns:p="urn:p"/><q:z xmlns:q="urn:q"/><w xmlns="urn:def"/>'));
// An unprefixed declaration is replaced by ANY in-scope binding of that URI,
// even a prefixed one -- which RESPELLS the node.
var_dump($dom_rec_frag('<r xmlns:p="urn:p"/>', '<y xmlns="urn:p"/>'));
// A prefixed one is kept when only the DEFAULT namespace binds the URI.
var_dump($dom_rec_frag('<r xmlns="urn:p"/>', '<p:y xmlns:p="urn:p"/>'));
// Only the moved node's OWN declarations are considered, never a descendant's.
var_dump($dom_rec_frag('<r xmlns:p="urn:p"/>', '<q:a xmlns:q="urn:q"><p:b xmlns:p="urn:p"/></q:a>'));

// A node re-appended where nothing changed keeps every answer.
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:p"><p:a><p:b/></p:a></r>');
$a = $d->documentElement->firstChild;
$d->documentElement->appendChild($a);
var_dump($d->saveXML($d->documentElement), $a->namespaceURI, $a->firstChild->namespaceURI);

// A node with no namespace at all is left alone.
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:p"><a><b/></a></r>');
$b = $d->documentElement->firstChild->firstChild;
$d->documentElement->appendChild($b);
var_dump($d->saveXML($d->documentElement), var_export($b->namespaceURI, true));

// The two paths differ in DEPTH, and both halves are php's. A single node
// insertion strips the node's OWN declarations and no deeper...
$dom_rec_deep = static function (string $doc, string $chunk, bool $viaFragment) {
    $d = new DOMDocument;
    $d->loadXML($doc);
    $f = $d->createDocumentFragment();
    $f->appendXML($chunk);
    $d->documentElement->appendChild($viaFragment ? $f : $f->firstChild);
    return $d->saveXML($d->documentElement);
};
$nested = '<q:a xmlns:q="urn:q"><p:b xmlns:p="urn:p"/></q:a>';
var_dump($dom_rec_deep('<r xmlns:p="urn:p"/>', $nested, false));
// ...while a FRAGMENT splice strips every depth of what it moved.
var_dump($dom_rec_deep('<r xmlns:p="urn:p"/>', $nested, true));
var_dump($dom_rec_deep('<r xmlns:p="urn:p"/>',
    '<q:a xmlns:q="urn:q"><m xmlns:p="urn:p"><n xmlns:p="urn:p"/></m></q:a>', true));
// The deep walk judges every node against the INSERTION POINT, not its own
// parent: a duplicate inside the moved subtree survives when the new parent
// makes no such declaration...
var_dump($dom_rec_deep('<r/>', '<z:a xmlns:z="urn:z"><z:m xmlns:z="urn:z"/></z:a>', true));
// ...or makes it under another prefix.
var_dump($dom_rec_deep('<r xmlns:w="urn:z"/>', '<z:a xmlns:z="urn:z"><z:m xmlns:z="urn:z"/></z:a>', true));
// A declaration the moved subtree shadows is re-declared under a numbered
// prefix, and the node respelled to it.
var_dump($dom_rec_deep('<r xmlns:p="urn:p"/>',
    '<q:a xmlns:q="urn:q" xmlns:p="urn:other"><p:m xmlns:p="urn:p"/></q:a>', true));
// A declaration only a DESCENDANT makes is re-declared on the node that moved,
// because the search only ever looks up.
var_dump($dom_rec_deep('<r/>', '<q:a xmlns:q="urn:q"><z:y xmlns:z="urn:z"/></q:a>', false));

// A removed declaration is parked where the document will free it, BEHIND
// libxml's own `xml` declaration -- which stays the answer for the xml prefix.
$d = new DOMDocument;
$d->loadXML('<r xmlns:p="urn:p"><a><p:b xml:lang="en"/></a></r>');
$pb = $d->documentElement->firstChild->firstChild;
$d->documentElement->appendChild($pb);
$f = $d->createDocumentFragment();
$f->appendXML('<p:c xmlns:p="urn:p"/>');
$d->documentElement->appendChild($f);
var_dump($d->saveXML($d->documentElement));
var_dump($pb->getAttribute('xml:lang'));
var_dump($pb->getAttributeNS('http://www.w3.org/XML/1998/namespace', 'lang'));
var_dump($d->documentElement->lookupNamespaceURI('xml'));
var_dump($d->documentElement->lookupNamespaceURI('p'));
$d->documentElement->setAttributeNS('http://www.w3.org/XML/1998/namespace', 'xml:space', 'preserve');
var_dump($d->saveXML($d->documentElement));
$dom_rec_xp = new DOMXPath($d);
var_dump($dom_rec_xp->query('//*[@xml:lang]')->length);
?>
--EXPECT--
string(82) "<r><a xmlns:p="urn:p"/><p:b xmlns:p="urn:p" k="1"/></r> | reparse=ok | p:b@'urn:p'"
string(82) "<r><p:b xmlns:p="urn:p" k="1"/><a xmlns:p="urn:p"/></r> | reparse=ok | p:b@'urn:p'"
string(62) "<r><p:b xmlns:p="urn:p" k="1"/></r> | reparse=ok | p:b@'urn:p'"
string(77) "<r><a xmlns:p="urn:p"/><b xmlns:p="urn:p" p:k="1"/></r> | reparse=ok | b@NULL"
string(92) "<r><a xmlns="urn:d"/><default:b xmlns:default="urn:d"/></r> | reparse=ok | default:b@'urn:d'"
string(93) "<r><a xmlns:p="urn:p"/><p:b xmlns:p="urn:p"><p:c d="1"/></p:b></r> | reparse=ok | p:b@'urn:p'"
string(100) "<r><a xmlns:p="urn:x" xmlns:q="urn:x"/><q:b xmlns:q="urn:x" q:k="1"/></r> | reparse=ok | q:b@'urn:x'"
string(75) "<r xmlns:p="urn:p" xmlns="urn:def"><c/><p:y/><q:z xmlns:q="urn:q"/><w/></r>"
string(29) "<r xmlns:p="urn:p"><p:y/></r>"
string(43) "<r xmlns="urn:p"><p:y xmlns:p="urn:p"/></r>"
string(56) "<r xmlns:p="urn:p"><q:a xmlns:q="urn:q"><p:b/></q:a></r>"
string(40) "<r xmlns:p="urn:p"><p:a><p:b/></p:a></r>"
string(5) "urn:p"
string(5) "urn:p"
string(31) "<r xmlns:p="urn:p"><a/><b/></r>"
string(4) "NULL"
string(72) "<r xmlns:p="urn:p"><q:a xmlns:q="urn:q"><p:b xmlns:p="urn:p"/></q:a></r>"
string(56) "<r xmlns:p="urn:p"><q:a xmlns:q="urn:q"><p:b/></q:a></r>"
string(61) "<r xmlns:p="urn:p"><q:a xmlns:q="urn:q"><m><n/></m></q:a></r>"
string(56) "<r><z:a xmlns:z="urn:z"><z:m xmlns:z="urn:z"/></z:a></r>"
string(72) "<r xmlns:w="urn:z"><z:a xmlns:z="urn:z"><z:m xmlns:z="urn:z"/></z:a></r>"
string(94) "<r xmlns:p="urn:p"><q:a xmlns:q="urn:q" xmlns:p="urn:other" xmlns:p1="urn:p"><p1:m/></q:a></r>"
string(72) "<r><q:a xmlns:q="urn:q" xmlns:z="urn:z"><z:y xmlns:z="urn:z"/></q:a></r>"
string(53) "<r xmlns:p="urn:p"><a/><p:b xml:lang="en"/><p:c/></r>"
string(2) "en"
string(2) "en"
string(36) "http://www.w3.org/XML/1998/namespace"
string(5) "urn:p"
string(74) "<r xmlns:p="urn:p" xml:space="preserve"><a/><p:b xml:lang="en"/><p:c/></r>"
int(1)
