--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match_all PREG_PATTERN_ORDER stores named groups under their name, interleaved with the numbered keys
--FILE--
<?php
/* PREG_PATTERN_ORDER (the default) used to omit named-group keys entirely, so
 * $m['name'] / $m['value'] were undefined — breaking code that reads them
 * (PHPUnit's DocBlock annotation parser: $matches['name'][$i] /
 * $matches['value'][$i]). php stores each named group under BOTH its name and
 * its number, and interleaves them per group: 0, name, 1, value, 2. An optional
 * named group that does not participate contributes an empty string. */
$doc = "@covers Foo\n@small\n@dataProvider bar";
$n = preg_match_all('/@(?P<name>[A-Za-z_-]+)(?:[ \t]+(?P<value>.*?))?[ \t]*\r?$/m', $doc, $m);
echo $n, "\n";
/* keys present in insertion order */
echo implode(",", array_keys($m)), "\n";
/* named arrays are the same as their numbered twins */
echo implode("|", $m['name']), "\n";
echo implode("|", $m['value']), "\n";   // @small -> "" in the middle
var_dump($m['name'] === $m[1]);
var_dump($m['value'] === $m[2]);
/* PREG_SET_ORDER also carries the named keys per set. */
preg_match_all('/(?P<k>\w)(?P<v>\d)/', 'a1b2', $s, PREG_SET_ORDER);
echo $s[0]['k'], $s[0]['v'], " ", $s[1]['k'], $s[1]['v'], "\n";
?>
--EXPECT--
3
0,name,1,value,2
covers|small|dataProvider
Foo||bar
bool(true)
bool(true)
a1 b2
--CLEAN--
<?php
