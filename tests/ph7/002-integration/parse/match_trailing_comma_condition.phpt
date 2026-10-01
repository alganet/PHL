--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: a condition list may end in a trailing comma before '=>'
--FILE--
<?php
// php's possible_comma: the last condition, or 'default', may be followed by ','
$x = 'b';
echo match ($x) { 'a', 'b', => "hit", default => "miss" }, "\n";
$x = 'a';
echo match ($x) { 'a', => "hit", default => "miss" }, "\n";
echo match ($x) { 'z', => "no", default, => "dflt" }, "\n";
$x = 'c';
echo match ($x) {
    'a', 'b', => "one",
    'c', 'd', => "two",
}, "\n";
echo match (true) { $x === 'c', => "yes" }, "\n";
echo match ($x) { 'a', 'b',
    => "no", 'c',
    => "split" }, "\n";
?>
--EXPECT--
hit
hit
dflt
two
yes
split
