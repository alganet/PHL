--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOM node identity: same underlying node yields the same object
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<a><b><c/></b></a>');
var_dump($d->documentElement === $d->documentElement);
var_dump($d->documentElement->firstChild === $d->documentElement->firstChild);
var_dump($d->documentElement->parentNode === $d);
var_dump($d->documentElement->firstChild->parentNode === $d->documentElement);
var_dump($d->documentElement->isSameNode($d->documentElement));
$e = $d->getElementsByTagName('c')->item(0);
var_dump($e === $d->documentElement->firstChild->firstChild);
var_dump($d->documentElement instanceof DOMElement);
var_dump($d instanceof DOMNode, $d instanceof DOMDocument);
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
