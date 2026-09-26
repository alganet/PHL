--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A namespaced attribute is written with php's grammar, prefix and declaration rules
--FILE--
<?php
$XMLNS = 'http://www.w3.org/2000/xmlns/';
$XML   = 'http://www.w3.org/XML/1998/namespace';

$set = static function (string $doc, ?string $uri, string $qname, string $value = 'V') {
    $d = new DOMDocument;
    $d->loadXML($doc);
    $el = $d->documentElement->firstChild instanceof DOMElement
        ? $d->documentElement->firstChild : $d->documentElement;
    try { $el->setAttributeNS($uri, $qname, $value); }
    catch (Throwable $e) { return get_class($e) . '(' . $e->getCode() . '): ' . $e->getMessage(); }
    return $d->saveXML($d->documentElement);
};
$make = static function (string $doc, ?string $uri, string $qname) {
    $d = new DOMDocument;
    $d->loadXML($doc);
    try { $a = $d->createAttributeNS($uri, $qname); }
    catch (Throwable $e) { return get_class($e) . '(' . $e->getCode() . '): ' . $e->getMessage(); }
    if ($a === false) return 'false';
    return $a->nodeName . '@' . var_export($a->namespaceURI, true)
        . ' | ' . $d->saveXML($d->documentElement);
};

$plain = '<r xmlns:p="urn:a" a="1"><c/></r>';

// The name grammar. A prefixed name is two NCNames and every failure there is
// the Namespace Error; an unprefixed one is a plain Name, where a character
// libxml will not take is the Invalid Character Error.
var_dump($set($plain, 'urn:b', '1:x'));
var_dump($set($plain, 'urn:b', 'a:b:c'));
var_dump($set($plain, 'urn:b', 'x:'));
var_dump($set($plain, 'urn:b', ':x'));
var_dump($set($plain, null, ':x'));
var_dump($set($plain, null, 'x y'));
var_dump($set($plain, null, 'p:x'));
var_dump($set($plain, 'urn:b', ''));

// Finding the namespace: a prefixed binding in scope is REUSED whatever prefix
// was asked for, a free prefix is declared, and a taken one is answered with a
// generated `default`.
var_dump($set($plain, 'urn:a', 'p:x'));
var_dump($set($plain, 'urn:a', 'q:x'));
var_dump($set($plain, 'urn:b', 'q:x'));
var_dump($set($plain, 'urn:b', 'p:x'));
var_dump($set($plain, 'urn:b', 'x'));
var_dump($set('<r xmlns="urn:b" xmlns:pp="urn:b"><c/></r>', 'urn:b', 'k'));
var_dump($set('<r xmlns="urn:def"><c/></r>', 'urn:zz', 'k'));

// The two RESERVED prefixes may be REUSED but never declared.
var_dump($set($plain, $XML, 'xml:lang'));
var_dump($set($plain, $XML, 'lang'));
var_dump($set($plain, $XML, 'xmlns:z'));
var_dump($set($plain, 'urn:b', 'xml:lang'));
var_dump($set($plain, 'urn:b', 'xmlns:z'));
var_dump($set($plain, $XMLNS, 'p:z'));

// The xmlns NAMESPACE plus an xmlns name is the DECLARATION spelling, and it
// rebinds the declaration already there instead of adding a second.
var_dump($set($plain, $XMLNS, 'xmlns:z', 'urn:z'));
var_dump($set($plain, $XMLNS, 'xmlns:p', 'urn:new'));
var_dump($set($plain, $XMLNS, 'xmlns', 'urn:z'));
var_dump($set('<r xmlns="urn:old" a="1"/>', $XMLNS, 'xmlns', 'urn:z'));
var_dump($set($plain, $XMLNS, 'q', 'v'));

// A declaration that SHADOWS one the nodes under it were using re-spells them,
// so the document still says what it said.
var_dump($set('<r xmlns="urn:d" xmlns:p="urn:p" p:k="v"><c p:k="z"/></r>', $XMLNS, 'xmlns', 'urn:new'));
var_dump($set('<r xmlns="urn:d" xmlns:p="urn:p" p:k="v"><c p:k="z"/></r>', 'urn:b', 'p:x'));
var_dump($set('<r xmlns="urn:b" xmlns:pp="urn:b"><c/></r>', $XMLNS, 'xmlns', 'V'));

// createAttributeNS declares on the document's ROOT, and has no root to declare
// on before the document has one.
var_dump($make($plain, 'urn:z', 'z:zz'));
var_dump($make($plain, 'urn:z', 'zz'));
var_dump($make($plain, 'urn:a', 'q:yy'));
var_dump($make($plain, null, 'zz'));
var_dump($make($plain, 'urn:b', 'p:x'));
var_dump($make('<r xmlns="urn:def"/>', 'urn:def', 'zz'));
var_dump($make($plain, 'urn:z', '1:x'));
var_dump($make($plain, 'urn:z', ':x'));
var_dump($make($plain, 'urn:z', 'x y'));
var_dump($make($plain, null, 'p:x'));
var_dump($make($plain, 'urn:z', 'xmlns:p'));
var_dump($make($plain, $XMLNS, 'p:x'));
var_dump($make($plain, $XML, 'xml:lang'));

$empty = new DOMDocument;
try { var_dump(@$empty->createAttributeNS('urn:z', 'zz')); }
catch (Throwable $e) { var_dump(get_class($e) . ': ' . $e->getMessage()); }
--EXPECT--
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(40) "<r xmlns:p="urn:a" a="1"><c :x="V"/></r>"
string(40) "DOMException(5): Invalid Character Error"
string(33) "DOMException(14): Namespace Error"
string(91) "ValueError(0): DOMElement::setAttributeNS(): Argument #2 ($qualifiedName) must not be empty"
string(41) "<r xmlns:p="urn:a" a="1"><c p:x="V"/></r>"
string(41) "<r xmlns:p="urn:a" a="1"><c p:x="V"/></r>"
string(57) "<r xmlns:p="urn:a" a="1"><c xmlns:q="urn:b" q:x="V"/></r>"
string(57) "<r xmlns:p="urn:a" a="1"><c xmlns:p="urn:b" p:x="V"/></r>"
string(53) "<r xmlns:p="urn:a" a="1"><c xmlns="urn:b" x="V"/></r>"
string(51) "<r xmlns="urn:b" xmlns:pp="urn:b"><c pp:k="V"/></r>"
string(80) "<r xmlns="urn:def"><default:c xmlns="urn:zz" xmlns:default="urn:def" k="V"/></r>"
string(46) "<r xmlns:p="urn:a" a="1"><c xml:lang="V"/></r>"
string(46) "<r xmlns:p="urn:a" a="1"><c xml:lang="V"/></r>"
string(43) "<r xmlns:p="urn:a" a="1"><c xml:z="V"/></r>"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(49) "<r xmlns:p="urn:a" a="1"><c xmlns:z="urn:z"/></r>"
string(51) "<r xmlns:p="urn:a" a="1"><c xmlns:p="urn:new"/></r>"
string(47) "<r xmlns:p="urn:a" a="1"><c xmlns="urn:z"/></r>"
string(24) "<r xmlns="urn:z" a="1"/>"
string(77) "<r xmlns:p="urn:a" a="1"><c xmlns="http://www.w3.org/2000/xmlns/" q="v"/></r>"
string(103) "<r xmlns="urn:d" xmlns:p="urn:p" p:k="v"><default:c xmlns="urn:new" xmlns:default="urn:d" p:k="z"/></r>"
string(99) "<r xmlns="urn:d" xmlns:p="urn:p" p:k="v"><c xmlns:p="urn:b" xmlns:p1="urn:p" p1:k="z" p:x="V"/></r>"
string(55) "<r xmlns="urn:b" xmlns:pp="urn:b"><pp:c xmlns="V"/></r>"
string(64) "z:zz@'urn:z' | <r xmlns:p="urn:a" xmlns:z="urn:z" a="1"><c/></r>"
string(76) "default:zz@'urn:z' | <r xmlns:p="urn:a" xmlns:default="urn:z" a="1"><c/></r>"
string(48) "p:yy@'urn:a' | <r xmlns:p="urn:a" a="1"><c/></r>"
string(43) "zz@NULL | <r xmlns:p="urn:a" a="1"><c/></r>"
string(75) "default:x@'urn:b' | <r xmlns:p="urn:a" xmlns:default="urn:b" a="1"><c/></r>"
string(67) "default:zz@'urn:def' | <r xmlns="urn:def" xmlns:default="urn:def"/>"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(33) "DOMException(14): Namespace Error"
string(83) "xml:lang@'http://www.w3.org/XML/1998/namespace' | <r xmlns:p="urn:a" a="1"><c/></r>"
bool(false)
