--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A namespace declaration is answered from the attribute surface, as a DOMNameSpaceNode
--FILE--
<?php
$XMLNS = 'http://www.w3.org/2000/xmlns/';
$show = static function ($x) {
    if ($x instanceof DOMNameSpaceNode) {
        return 'NS(' . $x->nodeName . '=' . $x->nodeValue . ',type=' . $x->nodeType
            . ',prefix=' . var_export($x->prefix, true) . ',local=' . $x->localName
            . ',ns=' . var_export($x->namespaceURI, true)
            . ',parent=' . ($x->parentNode ? $x->parentNode->nodeName : 'NULL') . ')';
    }
    if ($x instanceof DOMNode) return get_class($x) . '(' . $x->nodeName . ')';
    if (is_array($x)) return '[' . implode('|', $x) . ']';
    return var_export($x, true);
};
$say = static function (callable $f) use ($show) {
    try { return $show($f()); }
    catch (Throwable $e) { return get_class($e) . '(' . $e->getCode() . '): ' . $e->getMessage(); }
};
$mk = static function () {
    $d = new DOMDocument;
    $d->loadXML('<r xmlns:x="urn:x" xmlns="urn:d" a="1"><c xmlns:y="urn:y"/></r>');
    return $d;
};
$doc = $mk(); $root = $doc->documentElement; $kid = $root->firstChild;

// The declaration is reached under the NAME it is written with, on the element
// that makes it -- a child answers false for its parent's.
var_dump($say(fn() => $root->getAttributeNode('xmlns:x')));
var_dump($say(fn() => $root->getAttributeNode('xmlns')));
var_dump($say(fn() => $kid->getAttributeNode('xmlns:x')));
var_dump($say(fn() => $kid->getAttributeNode('xmlns:y')));
var_dump($root->getAttribute('xmlns:x'), $root->getAttribute('xmlns'),
    $root->hasAttribute('xmlns:x'), $kid->hasAttribute('xmlns:x'));

// Through the NS spelling the local name is the PREFIX, and the default
// declaration is not reachable that way at all.
var_dump($say(fn() => $root->getAttributeNodeNS($XMLNS, 'x')));
var_dump($say(fn() => $root->getAttributeNodeNS($XMLNS, 'xmlns')));
var_dump($root->hasAttributeNS($XMLNS, 'x'), $root->hasAttributeNS($XMLNS, 'xmlns'));

// The MAP does not see declarations at all.
var_dump($say(fn() => $root->attributes->getNamedItem('xmlns:x')));
var_dump($say(fn() => $root->attributes->getNamedItemNS($XMLNS, 'x')));
var_dump($root->attributes->length);

// getAttributeNames lists them FIRST, in the order the element makes them.
var_dump($say(fn() => $root->getAttributeNames()));
var_dump($say(fn() => $kid->getAttributeNames()));

// php hands back a FRESH object every time, and it is not a DOMNode.
$ns = $root->getAttributeNode('xmlns:x');
var_dump($ns === $root->getAttributeNode('xmlns:x'), $ns == $root->getAttributeNode('xmlns:x'));
var_dump($ns instanceof DOMNode, get_parent_class($ns), $ns->ownerDocument === $doc,
    $ns->isConnected, $ns->parentElement->nodeName);
set_error_handler(static function ($n, $msg) { echo 'WARN: ', $msg, "\n"; return true; });
var_dump($say(fn() => $ns->zzz));
restore_error_handler();
var_dump(isset($ns->prefix), isset($ns->zzz), $say(function () use ($ns) {
    $ns->nodeValue = 'q'; return 'written';
}));
var_dump($say(fn() => serialize($ns)));
var_dump($root->contains($ns));

// Writing THROUGH a declaration is refused, and dropping one takes it out.
$doc = $mk(); $root = $doc->documentElement;
var_dump($say(fn() => $root->setAttribute('xmlns:x', 'urn:new')));
var_dump($doc->saveXML($root));
$doc = $mk(); $root = $doc->documentElement;
var_dump($say(fn() => $root->removeAttribute('xmlns:x')));
var_dump($doc->saveXML($root));
var_dump($say(fn() => $root->getAttributeNames()));

// ...except that what still USES it keeps it: php answers true and the document
// says what it said.
$used = new DOMDocument;
$used->loadXML('<r xmlns:x="urn:x" x:b="2" a="1"><c x:k="3"/></r>');
var_dump($say(fn() => $used->documentElement->removeAttribute('xmlns:x')));
var_dump($say(fn() => $used->documentElement->getAttributeNames()));
var_dump($used->saveXML($used->documentElement));

// toggleAttribute reaches them too, and toggling one ON writes a DECLARATION
// bound to the empty URI rather than an attribute.
$doc = $mk(); $root = $doc->documentElement;
var_dump($say(fn() => $root->toggleAttribute('xmlns:x')), $doc->saveXML($root));
var_dump($say(fn() => $root->toggleAttribute('xmlns:q')), $doc->saveXML($root));
var_dump($say(fn() => $root->getAttributeNames()));

// A declaration that is dropped where a DESCENDANT still needs it is re-declared
// where it is needed.
$deep = new DOMDocument;
$deep->loadXML('<r xmlns:x="urn:x"><x:c x:b="1" b="2"/></r>');
var_dump($say(fn() => $deep->documentElement->toggleAttribute('xmlns:x')));
var_dump($deep->saveXML($deep->documentElement));
var_dump($say(fn() => $deep->documentElement->getAttributeNames()));
--EXPECT--
string(64) "NS(xmlns:x=urn:x,type=18,prefix='x',local=x,ns='urn:x',parent=r)"
string(65) "NS(xmlns=urn:d,type=18,prefix='',local=xmlns,ns='urn:d',parent=r)"
string(5) "false"
string(64) "NS(xmlns:y=urn:y,type=18,prefix='y',local=y,ns='urn:y',parent=c)"
string(5) "urn:x"
string(5) "urn:d"
bool(true)
bool(false)
string(64) "NS(xmlns:x=urn:x,type=18,prefix='x',local=x,ns='urn:x',parent=r)"
string(4) "NULL"
bool(true)
bool(false)
string(4) "NULL"
string(4) "NULL"
int(1)
string(17) "[xmlns:x|xmlns|a]"
string(9) "[xmlns:y]"
bool(false)
bool(true)
bool(false)
bool(false)
bool(true)
bool(true)
string(1) "r"
WARN: Undefined property: DOMNameSpaceNode::$zzz
string(4) "NULL"
bool(true)
bool(false)
string(70) "Error(0): Cannot modify readonly property DOMNameSpaceNode::$nodeValue"
string(124) "Exception(0): Serialization of 'DOMNameSpaceNode' is not allowed, unless serialization methods are implemented in a subclass"
bool(true)
string(5) "false"
string(63) "<r xmlns:x="urn:x" xmlns="urn:d" a="1"><c xmlns:y="urn:y"/></r>"
string(4) "true"
string(47) "<r xmlns="urn:d" a="1"><c xmlns:y="urn:y"/></r>"
string(9) "[xmlns|a]"
string(4) "true"
string(15) "[xmlns:x|x:b|a]"
string(49) "<r xmlns:x="urn:x" x:b="2" a="1"><c x:k="3"/></r>"
string(5) "false"
string(47) "<r xmlns="urn:d" a="1"><c xmlns:y="urn:y"/></r>"
string(4) "true"
string(58) "<r xmlns="urn:d" xmlns:q="" a="1"><c xmlns:y="urn:y"/></r>"
string(17) "[xmlns|xmlns:q|a]"
string(5) "false"
string(43) "<r><x:c xmlns:x="urn:x" x:b="1" b="2"/></r>"
string(2) "[]"
