--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a node's wrapper dies with the last reference, so a later registration reaches it (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php caches the wrapper on the node itself and frees it when nothing holds
 * it, so each read below builds a fresh one under whatever class is
 * registered at that moment */
class RcElem extends DOMElement {}
$d = new DOMDocument;
$d->loadXML('<r><k/></r>');
var_dump(get_class($d->documentElement));
$d->registerNodeClass('DOMElement', 'RcElem');
var_dump(get_class($d->documentElement));
var_dump(get_class($d->documentElement->firstChild));
$d->registerNodeClass('DOMElement', null);
var_dump(get_class($d->documentElement->firstChild));
--EXPECT--
string(10) "DOMElement"
string(6) "RcElem"
string(6) "RcElem"
string(10) "DOMElement"
--CLEAN--
<?php
