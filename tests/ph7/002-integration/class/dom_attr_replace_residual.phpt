--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: appendChild's attribute replacement parks the displaced node DETACHED AND ALIVE (setAttributeNode's after-state); an attribute reference detaches the argument cleanly (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* the displaced attribute stays a working node: php frees it under its wrapper
 * (a held one answers Invalid State there), which nothing here does before the
 * document goes */
$d = new DOMDocument;
$d->loadXML('<r k="old"/>');
$held = $d->documentElement->getAttributeNode('k');
$a = $d->createAttribute('k');
$a->value = 'new';
$d->documentElement->appendChild($a);
echo $d->saveXML($d->documentElement), "\n";
var_dump($held->value, $held->ownerElement);

/* a non-attribute argument against an ATTRIBUTE reference: php splices it into
 * libxml's PROPERTY chain -- garbage state whose exact shape is the library
 * version's, invisible to the bytes and the child list. PHL detaches the
 * argument instead, so its parentNode answers null where php's answers the
 * receiver; everything either engine serializes agrees. */
$x = new DOMDocument;
$x->loadXML('<r><c b="2">t</c></r>');
$c = $x->documentElement->firstChild;
$e = $x->createElement('nE');
$ret = $c->insertBefore($e, $c->getAttributeNode('b'));
echo get_class($ret), "\n";
echo $x->saveXML($x->documentElement), "\n";
var_dump($c->childNodes->length, $e->parentNode === $c);
--EXPECT--
<r k="new"/>
string(3) "old"
NULL
DOMElement
<r><c b="2">t</c></r>
int(1)
bool(false)
--CLEAN--
<?php
