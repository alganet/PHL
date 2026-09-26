--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An out-of-range item() index answers nothing, never the wrong node
--FILE--
<?php
// php's collection position is an `int`, so everything outside
// [0, 2147483647] is out of range -- and the two classes disagree about what
// to do with one: the list answers null and the named map refuses, naming the
// bound. Narrowing the argument first was a silent wrong answer either way,
// since 4294967296 and PHP_INT_MIN both truncate to 0 and picked the FIRST
// entry of the collection.
$d = new DOMDocument;
$d->loadXML('<r a="1" b="2"><k/><j/></r>');
$map = $d->documentElement->attributes;
$list = $d->documentElement->childNodes;
foreach ([-1, PHP_INT_MIN, 0, 1, 2, 2147483647, 2147483648, PHP_INT_MAX,
          4294967296] as $index) {
    $said = 'map ';
    try {
        $n = $map->item($index);
        $said .= $n ? $n->nodeName : 'NULL';
    } catch (Throwable $ex) {
        $said .= get_class($ex) . ': ' . $ex->getMessage();
    }
    $said .= ' | list ';
    try {
        $n = $list->item($index);
        $said .= $n ? $n->nodeName : 'NULL';
    } catch (Throwable $ex) {
        $said .= get_class($ex) . ': ' . $ex->getMessage();
    }
    printf("%-24s %s\n", var_export($index, true), $said);
}
?>
--EXPECT--
-1                       map ValueError: DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647 | list NULL
-9223372036854775807-1   map ValueError: DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647 | list NULL
0                        map a | list k
1                        map b | list j
2                        map NULL | list NULL
2147483647               map NULL | list NULL
2147483648               map ValueError: DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647 | list NULL
9223372036854775807      map ValueError: DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647 | list NULL
4294967296               map ValueError: DOMNamedNodeMap::item(): Argument #1 ($index) must be between 0 and 2147483647 | list NULL
