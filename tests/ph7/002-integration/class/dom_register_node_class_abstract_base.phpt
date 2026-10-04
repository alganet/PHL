--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOM: registerNodeClass refuses a base class nothing can be instantiated as
--FILE--
<?php
/* a base no node can ever BE registers nothing, so php screens it before it
 * writes the table -- and the 2004 tree, whose own classes are all concrete,
 * reaches the refusal only through a user class */
abstract class AbstractElem extends DOMElement {}
$d = new DOMDocument;
try {
    $d->registerNodeClass('AbstractElem', null);
} catch (Throwable $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}
/* the concrete ancestor it was declared from is still registrable */
class ConcreteElem extends DOMElement {}
var_dump($d->registerNodeClass('DOMElement', 'ConcreteElem'));
$d->loadXML('<r/>');
var_dump(get_class($d->documentElement));
--EXPECT--
ValueError: DOMDocument::registerNodeClass(): Argument #1 ($baseClass) must not be an abstract class
bool(true)
string(12) "ConcreteElem"
