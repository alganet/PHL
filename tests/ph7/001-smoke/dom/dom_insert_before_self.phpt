--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A node inserted before ITSELF is detached and refused, never made its own sibling
--FILE--
<?php
$dom_ibs_mk = static function (): DOMDocument {
    $d = new DOMDocument;
    $d->loadXML('<r><a/><b/></r>');
    return $d;
};
$dom_ibs_try = static function (callable $cb) {
    try { $r = $cb(); }
    catch (Throwable $e) { return get_class($e) . '(' . $e->getCode() . '): ' . $e->getMessage(); }
    return $r instanceof DOMNode ? get_class($r) . ' ' . $r->nodeName : var_export($r, true);
};

// The refusal is a plain Error with no DOM code, and php DETACHES the node
// before raising it: the tree loses it and keeps going.
$d = $dom_ibs_mk();
$r = $d->documentElement;
$a = $r->firstChild;
var_dump($dom_ibs_try(static fn() => $r->insertBefore($a, $a)));
var_dump($d->saveXML($r));
var_dump($a->parentNode, $a->nodeName);

// ...and the tree is still WALKABLE afterwards, which is the whole point: a
// node linked before itself closed the child list into a cycle, and every walk
// of it -- saveXML, childNodes, getNodePath -- ran forever.
var_dump($r->childNodes->length, $r->getNodePath(), $r->firstChild->nodeName);

// The last child is the same case from the other end.
$d = $dom_ibs_mk();
$r = $d->documentElement;
$b = $r->lastChild;
var_dump($dom_ibs_try(static fn() => $r->insertBefore($b, $b)));
var_dump($d->saveXML($r));

// A reference node that is not a child at all is Not Found first, and nothing
// is detached.
$d = $dom_ibs_mk();
$r = $d->documentElement;
$n = $d->createElement('n');
var_dump($dom_ibs_try(static fn() => $r->insertBefore($n, $n)));
var_dump($d->saveXML($r));

// The neighbours that are NOT refused: a real reference node, and replacing a
// node with itself.
$d = $dom_ibs_mk();
$r = $d->documentElement;
$a = $r->firstChild;
$b = $r->lastChild;
var_dump($dom_ibs_try(static fn() => $r->insertBefore($a, $b)));
var_dump($d->saveXML($r));
var_dump($dom_ibs_try(static fn() => $r->replaceChild($a, $a)));
var_dump($d->saveXML($r));
var_dump($dom_ibs_try(static fn() => $r->appendChild($a)));
var_dump($d->saveXML($r));
?>
--EXPECT--
string(63) "Error(0): Cannot add newnode as the previous sibling of refnode"
string(11) "<r><b/></r>"
NULL
string(1) "a"
int(1)
string(2) "/r"
string(1) "b"
string(63) "Error(0): Cannot add newnode as the previous sibling of refnode"
string(11) "<r><a/></r>"
string(32) "DOMException(8): Not Found Error"
string(15) "<r><a/><b/></r>"
string(12) "DOMElement a"
string(15) "<r><a/><b/></r>"
string(12) "DOMElement a"
string(15) "<r><a/><b/></r>"
string(12) "DOMElement a"
string(15) "<r><b/><a/></r>"
