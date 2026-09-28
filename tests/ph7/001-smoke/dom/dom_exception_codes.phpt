--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A DOMException carries php's DOM error code, and the DOM_*_ERR constants name it
--FILE--
<?php
// The whole DOM level-2 numbering, plus the one name that carries no DOM_ prefix.
$codes = [
    'DOM_PHP_ERR', 'DOM_INDEX_SIZE_ERR', 'DOMSTRING_SIZE_ERR',
    'DOM_HIERARCHY_REQUEST_ERR', 'DOM_WRONG_DOCUMENT_ERR',
    'DOM_INVALID_CHARACTER_ERR', 'DOM_NO_DATA_ALLOWED_ERR',
    'DOM_NO_MODIFICATION_ALLOWED_ERR', 'DOM_NOT_FOUND_ERR',
    'DOM_NOT_SUPPORTED_ERR', 'DOM_INUSE_ATTRIBUTE_ERR', 'DOM_INVALID_STATE_ERR',
    'DOM_SYNTAX_ERR', 'DOM_INVALID_MODIFICATION_ERR', 'DOM_NAMESPACE_ERR',
    'DOM_INVALID_ACCESS_ERR', 'DOM_VALIDATION_ERR',
];
$seen = [];
foreach ($codes as $name) {
    // DOM_PHP_ERR is php-deprecated: naming it is a notice this sweep is not about.
    $seen[] = defined($name) ? $name . '=' . @constant($name) : $name . '=MISSING';
}
var_dump(implode(' ', $seen));

// The node-type and DTD attribute-type constants ext/dom declares beside them.
$types = [
    'XML_ELEMENT_DECL_NODE', 'XML_ATTRIBUTE_DECL_NODE', 'XML_ENTITY_DECL_NODE',
    'XML_NAMESPACE_DECL_NODE', 'XML_LOCAL_NAMESPACE',
    'XML_ATTRIBUTE_CDATA', 'XML_ATTRIBUTE_ID', 'XML_ATTRIBUTE_IDREF',
    'XML_ATTRIBUTE_IDREFS', 'XML_ATTRIBUTE_ENTITY', 'XML_ATTRIBUTE_NMTOKEN',
    'XML_ATTRIBUTE_NMTOKENS', 'XML_ATTRIBUTE_ENUMERATION', 'XML_ATTRIBUTE_NOTATION',
];
$seen = [];
foreach ($types as $name) {
    // DOM_PHP_ERR is php-deprecated: naming it is a notice this sweep is not about.
    $seen[] = defined($name) ? $name . '=' . @constant($name) : $name . '=MISSING';
}
var_dump(implode(' ', $seen));

// Every refusal answers the code its sentence names, so a catch can branch on it.
$say = static function (callable $f) {
    try { $f(); return 'no throw'; }
    catch (DOMException $e) { return $e->getCode() . ':' . $e->getMessage(); }
};
$doc = new DOMDocument;
$doc->loadXML('<r a="1"><c>hi</c></r>');
$root = $doc->documentElement;
$other = new DOMDocument;
$other->loadXML('<s/>');

var_dump($say(fn() => $root->appendChild($other->documentElement)));
var_dump($say(fn() => $root->firstChild->appendChild($root)));
var_dump($say(fn() => $root->removeChild($root->firstChild->firstChild)));
var_dump($say(fn() => $root->setAttribute('1bad', 'v')));
var_dump($say(fn() => $doc->createElement('1bad')));
var_dump($say(fn() => $root->firstChild->firstChild->substringData(99, 1)));

// ...and a caller tests it against the constant rather than the sentence.
try {
    $root->removeChild($doc->createElement('z'));
} catch (DOMException $e) {
    var_dump($e->getCode() === DOM_NOT_FOUND_ERR, $e->getCode() === @constant('DOM_PHP_ERR'));
}
--EXPECT--
string(411) "DOM_PHP_ERR=0 DOM_INDEX_SIZE_ERR=1 DOMSTRING_SIZE_ERR=2 DOM_HIERARCHY_REQUEST_ERR=3 DOM_WRONG_DOCUMENT_ERR=4 DOM_INVALID_CHARACTER_ERR=5 DOM_NO_DATA_ALLOWED_ERR=6 DOM_NO_MODIFICATION_ALLOWED_ERR=7 DOM_NOT_FOUND_ERR=8 DOM_NOT_SUPPORTED_ERR=9 DOM_INUSE_ATTRIBUTE_ERR=10 DOM_INVALID_STATE_ERR=11 DOM_SYNTAX_ERR=12 DOM_INVALID_MODIFICATION_ERR=13 DOM_NAMESPACE_ERR=14 DOM_INVALID_ACCESS_ERR=15 DOM_VALIDATION_ERR=16"
string(337) "XML_ELEMENT_DECL_NODE=15 XML_ATTRIBUTE_DECL_NODE=16 XML_ENTITY_DECL_NODE=17 XML_NAMESPACE_DECL_NODE=18 XML_LOCAL_NAMESPACE=18 XML_ATTRIBUTE_CDATA=1 XML_ATTRIBUTE_ID=2 XML_ATTRIBUTE_IDREF=3 XML_ATTRIBUTE_IDREFS=4 XML_ATTRIBUTE_ENTITY=6 XML_ATTRIBUTE_NMTOKEN=7 XML_ATTRIBUTE_NMTOKENS=8 XML_ATTRIBUTE_ENUMERATION=9 XML_ATTRIBUTE_NOTATION=10"
string(22) "4:Wrong Document Error"
string(25) "3:Hierarchy Request Error"
string(17) "8:Not Found Error"
string(25) "5:Invalid Character Error"
string(25) "5:Invalid Character Error"
string(18) "1:Index Size Error"
bool(true)
bool(false)
