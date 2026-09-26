--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMElement's attribute NODE surface: the nodes, the names, the ID flag
--FILE--
<?php
$show = static function ($x) {
    if ($x instanceof DOMAttr) {
        return 'DOMAttr(' . $x->nodeName . '=' . var_export($x->value, true)
            . ',ns=' . var_export($x->namespaceURI, true)
            . ',owner=' . ($x->ownerElement ? $x->ownerElement->nodeName : 'NULL')
            . ',isId=' . var_export($x->isId(), true) . ')';
    }
    if (is_array($x)) return '[' . implode('|', $x) . ']';
    return var_export($x, true);
};
$say = static function (callable $f) use ($show) {
    try { return $show($f()); }
    catch (Throwable $e) { return get_class($e) . '(' . $e->getCode() . '): ' . $e->getMessage(); }
};
$mk = static function () {
    $d = new DOMDocument;
    $d->loadXML('<r xmlns:x="urn:x" a="1" x:b="2"><c a="9" id="i1"/></r>');
    return $d;
};

$doc = $mk(); $root = $doc->documentElement; $kid = $root->firstChild;

// A qualified name is resolved in the element's scope: the prefix names a
// namespace, and a bare local name does NOT reach a namespaced attribute.
var_dump($say(fn() => $root->getAttributeNode('a')));
var_dump($say(fn() => $root->getAttributeNode('x:b')));
var_dump($say(fn() => $root->getAttributeNode('b')));
var_dump($say(fn() => $root->getAttributeNode('nope')));
var_dump($root->getAttribute('x:b'), $root->getAttribute('b'), $root->hasAttribute('x:b'));
var_dump($root->getAttributeNS(null, 'a'), $root->getAttributeNS('urn:x', 'b'));

// The NS spelling answers null rather than false, and an empty URI is not null.
var_dump($say(fn() => $root->getAttributeNodeNS('urn:x', 'b')));
var_dump($say(fn() => $root->getAttributeNodeNS(null, 'a')));
var_dump($say(fn() => $root->getAttributeNodeNS('', 'a')));
var_dump($root->hasAttributeNS(null, 'a'), $root->hasAttributeNS('urn:x', 'b'),
    $root->hasAttributeNS('urn:x', 'a'));

// getAttributeNames answers the qualified names in document order.
$plain = new DOMDocument;
$plain->loadXML('<r a="1" b="2" c="3"><k/></r>');
var_dump($say(fn() => $plain->documentElement->getAttributeNames()));
var_dump($say(fn() => $plain->documentElement->firstChild->getAttributeNames()));
var_dump($say(fn() => $kid->getAttributeNames()));

// A created attribute has this document and NO element until it is set.
$doc = $mk(); $root = $doc->documentElement; $kid = $root->firstChild;
$fresh = $doc->createAttribute('q');
var_dump($say(fn() => $fresh));
var_dump($fresh->ownerDocument === $doc, $fresh->parentNode === null, $fresh->specified,
    $fresh->schemaTypeInfo);
var_dump($say(fn() => $doc->createAttribute('1bad')));
var_dump($say(fn() => $doc->createAttribute('')));

// Setting one answers what it DISPLACED, and moves an attribute off its owner.
$fresh->value = 'qv';
var_dump($say(fn() => $root->setAttributeNode($fresh)));
var_dump($doc->saveXML($root));
$second = $doc->createAttribute('a'); $second->value = 'new';
var_dump($say(fn() => $root->setAttributeNode($second)));
var_dump($say(fn() => $root->setAttributeNode($second)));
var_dump($doc->saveXML($root));
var_dump($say(fn() => $root->setAttributeNode($kid->getAttributeNode('a'))));
var_dump($doc->saveXML($root));
$foreign = (new DOMDocument)->createAttribute('zz');
var_dump($say(fn() => $root->setAttributeNode($foreign)));

// Removing one hands the node back, alive and ownerless; twice is Not Found.
$doc = $mk(); $root = $doc->documentElement; $kid = $root->firstChild;
$taken = $root->getAttributeNode('a');
var_dump($say(fn() => $root->removeAttributeNode($taken)));
var_dump($doc->saveXML($root), $taken->ownerElement === null, $taken->value);
var_dump($say(fn() => $root->removeAttributeNode($taken)));
var_dump($say(fn() => $root->removeAttributeNode($kid->getAttributeNode('a'))));

// The NS remover is silent about an absent one.
$doc = $mk(); $root = $doc->documentElement;
var_dump($say(fn() => $root->removeAttributeNS('urn:x', 'b')));
var_dump($doc->saveXML($root));
var_dump($say(fn() => $root->removeAttributeNS('urn:nope', 'zz')));
var_dump($say(fn() => $root->removeAttributeNS(null, 'a')));
var_dump($doc->saveXML($root));

// toggleAttribute: flip, force on, force off -- and a name libxml will not take.
$doc = $mk(); $root = $doc->documentElement;
var_dump($say(fn() => $root->toggleAttribute('a')), $doc->saveXML($root));
var_dump($say(fn() => $root->toggleAttribute('a')), $doc->saveXML($root));
var_dump($say(fn() => $root->toggleAttribute('x:b', true)), $doc->saveXML($root));
var_dump($say(fn() => $root->toggleAttribute('x:b', false)), $doc->saveXML($root));
var_dump($say(fn() => $root->toggleAttribute('1bad')));

// The ID three. The plain spelling does NOT resolve a prefix, so a namespaced
// attribute is Not Found under its qualified name.
$doc = $mk(); $root = $doc->documentElement; $kid = $root->firstChild;
var_dump($kid->getAttributeNode('id')->isId());
var_dump($say(fn() => $kid->setIdAttribute('id', true)));
var_dump($kid->getAttributeNode('id')->isId());
var_dump($say(fn() => $kid->setIdAttribute('id', false)));
var_dump($kid->getAttributeNode('id')->isId());
var_dump($say(fn() => $kid->setIdAttribute('nope', true)));
var_dump($say(fn() => $root->setIdAttribute('x:b', true)));
var_dump($say(fn() => $root->setIdAttributeNS('urn:x', 'b', true)));
var_dump($root->getAttributeNode('x:b')->isId());
var_dump($say(fn() => $root->setIdAttributeNode($root->getAttributeNode('a'), true)));
var_dump($root->getAttributeNode('a')->isId());
var_dump($say(fn() => $kid->setIdAttributeNode($root->getAttributeNode('a'), true)));

// The MAP asks a different question than the element does: a null namespace is
// ANY there, and getNamedItem matches the stored name whatever its namespace.
$doc = $mk(); $root = $doc->documentElement;
$map = $root->attributes;
var_dump($say(fn() => $map->getNamedItem('x:b')));
var_dump($say(fn() => $map->getNamedItem('b')));
var_dump($say(fn() => $map->getNamedItemNS(null, 'b')));
var_dump($say(fn() => $map->getNamedItemNS('', 'b')));
var_dump($say(fn() => $map->getNamedItemNS('urn:x', 'b')));
var_dump($say(fn() => $map->getNamedItemNS('urn:x', 'a')));
--EXPECT--
string(41) "DOMAttr(a='1',ns=NULL,owner=r,isId=false)"
string(46) "DOMAttr(x:b='2',ns='urn:x',owner=r,isId=false)"
string(5) "false"
string(5) "false"
string(1) "2"
string(0) ""
bool(true)
string(1) "1"
string(1) "2"
string(46) "DOMAttr(x:b='2',ns='urn:x',owner=r,isId=false)"
string(41) "DOMAttr(a='1',ns=NULL,owner=r,isId=false)"
string(4) "NULL"
bool(true)
bool(true)
bool(false)
string(7) "[a|b|c]"
string(2) "[]"
string(6) "[a|id]"
string(43) "DOMAttr(q='',ns=NULL,owner=NULL,isId=false)"
bool(true)
bool(true)
bool(true)
NULL
string(40) "DOMException(5): Invalid Character Error"
string(40) "DOMException(5): Invalid Character Error"
string(4) "NULL"
string(62) "<r xmlns:x="urn:x" a="1" x:b="2" q="qv"><c a="9" id="i1"/></r>"
string(44) "DOMAttr(a='1',ns=NULL,owner=NULL,isId=false)"
string(4) "NULL"
string(64) "<r xmlns:x="urn:x" x:b="2" q="qv" a="new"><c a="9" id="i1"/></r>"
string(46) "DOMAttr(a='new',ns=NULL,owner=NULL,isId=false)"
string(56) "<r xmlns:x="urn:x" x:b="2" q="qv" a="9"><c id="i1"/></r>"
string(37) "DOMException(4): Wrong Document Error"
string(44) "DOMAttr(a='1',ns=NULL,owner=NULL,isId=false)"
string(49) "<r xmlns:x="urn:x" x:b="2"><c a="9" id="i1"/></r>"
bool(true)
string(1) "1"
string(32) "DOMException(8): Not Found Error"
string(32) "DOMException(8): Not Found Error"
string(4) "NULL"
string(47) "<r xmlns:x="urn:x" a="1"><c a="9" id="i1"/></r>"
string(4) "NULL"
string(4) "NULL"
string(41) "<r xmlns:x="urn:x"><c a="9" id="i1"/></r>"
string(5) "false"
string(49) "<r xmlns:x="urn:x" x:b="2"><c a="9" id="i1"/></r>"
string(4) "true"
string(54) "<r xmlns:x="urn:x" x:b="2" a=""><c a="9" id="i1"/></r>"
string(4) "true"
string(54) "<r xmlns:x="urn:x" x:b="2" a=""><c a="9" id="i1"/></r>"
string(5) "false"
string(46) "<r xmlns:x="urn:x" a=""><c a="9" id="i1"/></r>"
string(40) "DOMException(5): Invalid Character Error"
bool(false)
string(4) "NULL"
bool(true)
string(4) "NULL"
bool(false)
string(32) "DOMException(8): Not Found Error"
string(32) "DOMException(8): Not Found Error"
string(4) "NULL"
bool(true)
string(4) "NULL"
bool(true)
string(32) "DOMException(8): Not Found Error"
string(4) "NULL"
string(46) "DOMAttr(x:b='2',ns='urn:x',owner=r,isId=false)"
string(46) "DOMAttr(x:b='2',ns='urn:x',owner=r,isId=false)"
string(4) "NULL"
string(46) "DOMAttr(x:b='2',ns='urn:x',owner=r,isId=false)"
string(4) "NULL"
