--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMElement attributes: get/set/has/remove, NS variants, DOMNamedNodeMap
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<e a="1" b="2" c="3"/>');
$r = $d->documentElement;
var_dump($r->getAttribute('a'), $r->getAttribute('nope'));
var_dump($r->hasAttribute('b'), $r->hasAttribute('nope'));
$ret = $r->setAttribute('dd', 'x&y');
var_dump($ret instanceof DOMAttr, $ret->value);
var_dump($r->removeAttribute('b'), $r->removeAttribute('nope'));
echo $d->saveXML($r), "\n";
var_dump($r->attributes->length);
$attr = $r->attributes->item(0);
var_dump($attr instanceof DOMAttr, $attr->name, $attr->value, $attr->nodeType);
var_dump($r->attributes->item(99));
var_dump($r->attributes->getNamedItem('dd')->value);
$d2 = new DOMDocument;
$d2->loadXML('<phpunit/>');
$d2->documentElement->setAttributeNS('http://www.w3.org/2001/XMLSchema-instance', 'xsi:noNamespaceSchemaLocation', 'https://schema.phpunit.de/11.5/phpunit.xsd');
echo $d2->saveXML();
var_dump($d2->documentElement->getAttributeNS('http://www.w3.org/2001/XMLSchema-instance', 'noNamespaceSchemaLocation'));
var_dump($d2->documentElement->getAttributeNS('http://example.com/none', 'noNamespaceSchemaLocation'));
--EXPECT--
string(1) "1"
string(0) ""
bool(true)
bool(false)
bool(true)
string(3) "x&y"
bool(true)
bool(false)
<e a="1" c="3" dd="x&amp;y"/>
int(3)
bool(true)
string(1) "a"
string(1) "1"
int(2)
NULL
string(3) "x&y"
<?xml version="1.0"?>
<phpunit xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="https://schema.phpunit.de/11.5/phpunit.xsd"/>
string(42) "https://schema.phpunit.de/11.5/phpunit.xsd"
string(0) ""
