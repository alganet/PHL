--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An element in php's namespaced DOM tree could not read or write its substituted value
--FILE--
<?php
/* php 8.4's element declares `substitutedNodeValue` beside `textContent`, and
 * the two run the SAME content walk on the way out -- every entity reference
 * below the node is read as its declaration's replacement text, so the pair
 * always agree on a read.
 *
 * They part on the WRITE, and that is the whole reason the name exists:
 * `textContent` stores its bytes literally as one raw text child, while this
 * one PARSES them, so a declared entity leaves an entity reference in the tree
 * and a character reference leaves the character it names. Serializing the
 * element back is what shows the difference the read cannot.
 *
 * An UNDECLARED entity is not a refusal: libxml drops the reference and keeps
 * the text on either side of it. What a reference libxml cannot TERMINATE does
 * is deliberately not pinned here -- 2.9 refuses it and empties the element,
 * 2.13 accepts it and keeps the text, so that row measures the library's
 * version rather than this property. */

function doc(): Dom\XMLDocument {
    return Dom\XMLDocument::createFromString(
        '<!DOCTYPE r [<!ENTITY e "EXP">]><r><a>x&e;y</a></r>');
}

echo "== the read is the content walk, entity references substituted\n";
$d = doc();
$a = $d->documentElement->firstElementChild;
var_dump($a->substitutedNodeValue, $a->textContent, $a->nodeValue);

echo "== and the write parses what the other stores literally\n";
foreach (['plain', '', '&e;', 'A&e;B', '&#x31;', '&lt;&gt;', 'a&nope;b', '<x/>'] as $v) {
    $d = doc();
    $a = $d->documentElement->firstElementChild;
    $a->substitutedNodeValue = $v;
    printf("%-10s -> %-14s read=%-8s children=%d\n", var_export($v, true),
        $d->saveXml($a), var_export($a->substitutedNodeValue, true),
        $a->childNodes->length);
}

echo "== the same bytes through textContent, which parses nothing\n";
$d = doc();
$a = $d->documentElement->firstElementChild;
$a->textContent = 'A&e;B';
echo $d->saveXml($a), " read=", var_export($a->substitutedNodeValue, true), "\n";

echo "== the write replaces every child, so a held grandchild is orphaned\n";
$d = Dom\XMLDocument::createFromString('<r><a><k/><m>t</m></a></r>');
$a = $d->documentElement->firstElementChild;
$k = $a->firstElementChild;
$a->substitutedNodeValue = 'flat';
var_dump($k->parentNode === null, $d->saveXml($a));

echo "== it is a non-nullable string, and virtual\n";
$d = doc();
$a = $d->documentElement->firstElementChild;
foreach ([1, 1.5, true, null, [1], new stdClass] as $v) {
    try {
        $a->substitutedNodeValue = $v;
        echo "ok ", var_export($a->substitutedNodeValue, true), "\n";
    } catch (\Throwable $t) {
        echo get_class($t), ": ", $t->getMessage(), "\n";
    }
}
try { unset($a->substitutedNodeValue); } catch (\Throwable $t) {
    echo get_class($t), ": ", $t->getMessage(), "\n";
}
var_dump(isset($a->substitutedNodeValue),
    array_key_exists('substitutedNodeValue', get_object_vars($a)));
$p = new ReflectionProperty('Dom\Element', 'substitutedNodeValue');
var_dump($p->isVirtual(), (string) $p->getType(), $p->hasDefaultValue());

echo "== and the 2004 element declares no such name\n";
var_dump(property_exists('DOMElement', 'substitutedNodeValue'));
?>
--EXPECT--
== the read is the content walk, entity references substituted
string(5) "xEXPy"
string(5) "xEXPy"
NULL
== and the write parses what the other stores literally
'plain'    -> <a>plain</a>   read='plain'  children=1
''         -> <a></a>        read=''       children=1
'&e;'      -> <a>&e;</a>     read='EXP'    children=1
'A&e;B'    -> <a>A&e;B</a>   read='AEXPB'  children=3
'&#x31;'   -> <a>1</a>       read='1'      children=1
'&lt;&gt;' -> <a>&lt;&gt;</a> read='<>'     children=1
'a&nope;b' -> <a>a&nope;b</a> read='ab'     children=3
'<x/>'     -> <a>&lt;x/&gt;</a> read='<x/>'   children=1
== the same bytes through textContent, which parses nothing
<a>A&amp;e;B</a> read='A&e;B'
== the write replaces every child, so a held grandchild is orphaned
bool(true)
string(11) "<a>flat</a>"
== it is a non-nullable string, and virtual
ok '1'
ok '1.5'
ok '1'
TypeError: Cannot assign null to property Dom\Element::$substitutedNodeValue of type string
TypeError: Cannot assign array to property Dom\Element::$substitutedNodeValue of type string
TypeError: Cannot assign stdClass to property Dom\Element::$substitutedNodeValue of type string
Error: Cannot unset Dom\Element::$substitutedNodeValue
bool(true)
bool(false)
bool(true)
string(6) "string"
bool(false)
== and the 2004 element declares no such name
bool(false)
