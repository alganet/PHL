--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOM: a detached node is freed with its last wrapper, and a held descendant is unlinked rather than freed
--FILE--
<?php
/* the orphan parent's last wrapper dies at the statement's end; the subtree is
 * freed then and there, and the wrapped child is UNLINKED to keep it alive */
$d = new DOMDocument;
$d->loadXML('<r/>');
$t = $d->createTextNode('T');
$d->createElement('gone')->appendChild($t);
var_dump($t->parentNode, $t->data, $t->ownerDocument === $d);

/* the identity cache outlives any one wrapper: a node still in the tree is
 * wrapped afresh after its object goes, and is still the same node */
$r = $d->documentElement;
var_dump($r === $d->documentElement, $d->documentElement->nodeName);
unset($r);
var_dump($d->documentElement->nodeName);

/* an attribute outlives the element it was read from */
$e = $d->createElement('e');
$e->setAttribute('k', 'v');
$a = $e->getAttributeNode('k');
unset($e);
var_dump($a->value, $a->ownerElement === null ? 'detached' : $a->ownerElement->nodeName);

/* a node dropped and rebuilt reports itself, not whatever the allocator last
 * put at that address */
for ($i = 0; $i < 3; $i++) {
    $n = $d->createElement('n' . $i);
    var_dump($n->nodeName);
    unset($n);
}

/* only a PARENTLESS node is its wrapper's to free: an appended one is the
 * tree's, and dropping the object that appended it changes nothing */
$p = $d->documentElement;
$c = $d->createElement('c');
$p->appendChild($c);
unset($c);
var_dump($p->firstChild->nodeName, $d->saveXML($d->documentElement));
--EXPECT--
NULL
string(1) "T"
bool(true)
bool(true)
string(1) "r"
string(1) "r"
string(1) "v"
string(8) "detached"
string(2) "n0"
string(2) "n1"
string(2) "n2"
string(1) "c"
string(11) "<r><c/></r>"
--CLEAN--
<?php
