--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a node already WRAPPED keeps the class it was wrapped in (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* the document's identity cache owns its wrappers for the document's whole
 * life, which is what makes `$d->documentElement === $d->documentElement`
 * hold; php's die with the last reference a program holds and are remade on
 * the next read, so a registration that lands AFTER a node was touched
 * reaches it there and not here. A registration made before the walk -- the
 * documented order, and the one every example uses -- agrees on both. */
class RcElem extends DOMElement {}
$d = new DOMDocument;
$d->loadXML('<r><k/></r>');
var_dump(get_class($d->documentElement));       // wrapped as php's class
$d->registerNodeClass('DOMElement', 'RcElem');
var_dump(get_class($d->documentElement));       // ...and keeps it here
var_dump(get_class($d->documentElement->firstChild));   // untouched: the new one
$d->registerNodeClass('DOMElement', null);
var_dump(get_class($d->documentElement->firstChild));
--EXPECT--
string(10) "DOMElement"
string(10) "DOMElement"
string(6) "RcElem"
string(6) "RcElem"
--CLEAN--
<?php
