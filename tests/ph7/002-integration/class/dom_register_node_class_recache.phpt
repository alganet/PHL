--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOM: registerNodeClass reaches a node already read, because its wrapper died with the last reference to it
--FILE--
<?php
/* the wrapper is cached on the node and freed when nothing holds it, so each
 * read below builds a fresh one under whatever class is registered at that
 * moment */
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
