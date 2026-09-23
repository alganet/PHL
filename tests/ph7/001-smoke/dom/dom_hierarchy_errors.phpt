--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Tree surgery refuses with php's DOMException taxonomy, cycles included
--FILE--
<?php
$d1 = new DOMDocument; $d1->loadXML('<r><a><deep/></a><b/></r>');
$d2 = new DOMDocument; $d2->loadXML('<s><c/></s>');
$r  = $d1->documentElement;
$a  = $r->firstChild;
$deep = $a->firstChild;

$say = static function (callable $f) {
    try { $x = $f(); return is_object($x) ? get_class($x) : var_export($x, true); }
    catch (Throwable $e) { return get_class($e) . ': ' . $e->getMessage(); }
};

// A node may not become its own descendant, and may not host itself.
var_dump($say(fn() => $deep->appendChild($r)));
var_dump($say(fn() => $r->appendChild($r)));
var_dump($say(fn() => $r->replaceChild($r, $a)));
// ...and the tree is untouched by the refusals.
var_dump($d1->saveXML($r));

// A node from another document is refused before anything else.
var_dump($say(fn() => $r->appendChild($d2->documentElement->firstChild)));
var_dump($say(fn() => $r->insertBefore($d2->documentElement->firstChild)));

// A reference/victim node that is not a child of the receiver is Not Found.
var_dump($say(fn() => $r->insertBefore($d1->createElement('z'), $deep)));
var_dump($say(fn() => $r->removeChild($deep)));
var_dump($say(fn() => $r->replaceChild($d1->createElement('z'), $deep)));

// The declared DOMNode parameter screens the argument before the body runs.
var_dump($say(fn() => $r->appendChild(1)));

// What IS allowed still works, and answers the php-documented node.
var_dump($say(fn() => $r->appendChild($d1->createElement('ok'))));
var_dump($say(fn() => $r->removeChild($r->lastChild)));
var_dump($d1->saveXML($r));
--EXPECT--
string(37) "DOMException: Hierarchy Request Error"
string(37) "DOMException: Hierarchy Request Error"
string(37) "DOMException: Hierarchy Request Error"
string(25) "<r><a><deep/></a><b/></r>"
string(34) "DOMException: Wrong Document Error"
string(34) "DOMException: Wrong Document Error"
string(29) "DOMException: Not Found Error"
string(29) "DOMException: Not Found Error"
string(29) "DOMException: Not Found Error"
string(89) "TypeError: DOMNode::appendChild(): Argument #1 ($node) must be of type DOMNode, int given"
string(10) "DOMElement"
string(10) "DOMElement"
string(25) "<r><a><deep/></a><b/></r>"
