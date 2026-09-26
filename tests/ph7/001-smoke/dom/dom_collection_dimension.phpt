--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A collection subscript reads without ArrayAccess, and its offset is not an array key
--FILE--
<?php
// php 8.3 gave DOMNodeList and DOMNamedNodeMap dimension HANDLERS without
// declaring ArrayAccess, so `$list[0]` and `$map['href']` read while
// `$list instanceof ArrayAccess` is false -- which is how every modern DOM
// example is written, and which was `Cannot use object of type DOMNodeList as
// array` here.
//
// What an offset MEANS is one rule for both and it is not the array one: a
// STRING starting with a number is an INDEX ("1x" is 1, " 2 " is 2), one that
// does not is a NAME -- and the list has no name door at all, so `$list['k']`
// is null on a document whose child element IS named k. Every other type takes
// the ordinary int cast. Then the two classes disagree about an index outside
// [0, INT_MAX]: the list answers null, the map raises item()'s own ValueError
// with no function frame to name the argument in front of it.
$doc = new DOMDocument;
$doc->loadXML('<r xmlns:p="urn:p" a="1" p:b="2" a1="3"><k/><j/><q/></r>');
$el = $doc->documentElement;
$list = $el->childNodes;
$map = $el->attributes;

var_dump($list instanceof ArrayAccess, $map instanceof ArrayAccess);

function dom_dim_say($collection, $offset) {
    try {
        $hit = $collection[$offset];
        return $hit === null ? 'NULL' : $hit->nodeName;
    } catch (Throwable $ex) {
        return get_class($ex) . ': ' . $ex->getMessage();
    }
}
$offsets = ['0' => 0, '1' => 1, '2' => 2, '3' => 3, '-1' => -1,
    'INT_MAX32' => 2147483647, 'INT_MAX32+1' => 2147483648,
    'PHP_INT_MAX' => PHP_INT_MAX, 'PHP_INT_MIN' => PHP_INT_MIN,
    "'0'" => '0', "'1'" => '1', "'01'" => '01', "' 1 '" => ' 1 ', "'1x'" => '1x',
    "'.5'" => '.5', "'1e0'" => '1e0', "'+1'" => '+1', "'-1'" => '-1',
    "'2147483648'" => '2147483648', "''" => '', "'  '" => '  ', "'k'" => 'k',
    "'a'" => 'a', "'b'" => 'b', "'p:b'" => 'p:b', "'a1'" => 'a1', "'A'" => 'A',
    'null' => null, 'true' => true, 'false' => false, '1.0' => 1.0,
    '1.9' => 1.9, '-0.5' => -0.5, '[]' => [], '[1,2]' => [1, 2]];
foreach ($offsets as $label => $offset) {
    printf("%-14s list %-14s map %s\n", $label,
        dom_dim_say($list, $offset), dom_dim_say($map, $offset));
}

// isset() is php's has_dimension, a second handler that answers presence and
// NEVER refuses -- `isset($map[-1])` is a plain false where reading the same
// offset raises. empty() asks it first and reads the value only on a hit, so
// it does not refuse either; `??` READS, so it does.
echo "=== isset / empty / ?? ===\n";
foreach ([0, 3, -1, 'a', 'zz', 2147483648] as $offset) {
    $coalesce = 'list ';
    try { $coalesce .= var_export($list[$offset] === null ? null : 'node', true); }
    catch (Throwable $ex) { $coalesce .= get_class($ex); }
    $coalesce .= ' map ';
    try { $coalesce .= var_export($map[$offset] === null ? null : 'node', true); }
    catch (Throwable $ex) { $coalesce .= get_class($ex); }
    printf("%-12s isset %d/%d  empty %d/%d  ?? %s\n", var_export($offset, true),
        isset($list[$offset]), isset($map[$offset]),
        empty($list[$offset]), empty($map[$offset]), $coalesce);
}

// Every list flavour and both declaration tables go through the same door.
echo "=== flavours ===\n";
$doc2 = new DOMDocument;
$doc2->loadXML('<!DOCTYPE r [<!ENTITY e "v"><!NOTATION n SYSTEM "s">]><r><k/></r>');
$xpath = new DOMXPath($doc2);
var_dump($doc2->getElementsByTagName('k')[0]->nodeName,
         $xpath->query('//k')[0]->nodeName,
         $doc2->doctype->entities['e']->nodeName,
         $doc2->doctype->notations['n']->nodeName,
         $doc2->doctype->entities[0]->nodeName,
         $doc2->documentElement->firstChild->attributes['nope']);

// The wrapper the subscript answers is the SAME object item() answers, and a
// document mutation is visible through a live list's subscript.
echo "=== identity / liveness ===\n";
var_dump($list[0] === $list->item(0), $map['a'] === $map->getNamedItem('a'));
$el->appendChild($doc->createElement('late'));
var_dump(count($list), $list[3]->nodeName);
?>
--EXPECT--
bool(false)
bool(false)
0              list k              map a
1              list j              map p:b
2              list q              map a1
3              list NULL           map NULL
-1             list NULL           map ValueError: must be between 0 and 2147483647
INT_MAX32      list NULL           map NULL
INT_MAX32+1    list NULL           map ValueError: must be between 0 and 2147483647
PHP_INT_MAX    list NULL           map ValueError: must be between 0 and 2147483647
PHP_INT_MIN    list NULL           map ValueError: must be between 0 and 2147483647
'0'            list k              map a
'1'            list j              map p:b
'01'           list j              map p:b
' 1 '          list j              map p:b
'1x'           list j              map p:b
'.5'           list k              map a
'1e0'          list j              map p:b
'+1'           list j              map p:b
'-1'           list NULL           map ValueError: must be between 0 and 2147483647
'2147483648'   list NULL           map ValueError: must be between 0 and 2147483647
''             list NULL           map NULL
'  '           list NULL           map NULL
'k'            list NULL           map NULL
'a'            list NULL           map a
'b'            list NULL           map p:b
'p:b'          list NULL           map NULL
'a1'           list NULL           map a1
'A'            list NULL           map NULL
null           list k              map a
true           list j              map p:b
false          list k              map a
1.0            list j              map p:b
1.9            list j              map p:b
-0.5           list k              map a
[]             list k              map a
[1,2]          list j              map p:b
=== isset / empty / ?? ===
0            isset 1/1  empty 0/0  ?? list 'node' map 'node'
3            isset 0/0  empty 1/1  ?? list NULL map NULL
-1           isset 0/0  empty 1/1  ?? list NULL map ValueError
'a'          isset 0/1  empty 1/0  ?? list NULL map 'node'
'zz'         isset 0/0  empty 1/1  ?? list NULL map NULL
2147483648   isset 0/0  empty 1/1  ?? list NULL map ValueError
=== flavours ===
string(1) "k"
string(1) "k"
string(1) "e"
string(1) "n"
string(1) "e"
NULL
=== identity / liveness ===
bool(true)
bool(true)
int(4)
string(4) "late"
