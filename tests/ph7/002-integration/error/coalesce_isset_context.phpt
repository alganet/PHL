--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
?? evaluates its whole left operand in isset-context: no diagnostics, NULL on a miss
--DESCRIPTION--
php reads the entire left operand of ?? as an isset-style probe. PHL leaked three
diagnostics there and, worse, returned a WRONG VALUE for an out-of-range string offset:
the warning path yielded "" instead of NULL, so `$s[99] ?? $d` evaluated to "" rather than
$d. Subscript reads in the chain now carry LOAD_IDX iP2=8 (quiet probe; offsetExists then
offsetGet on a hit for ArrayAccess), marked by the compiler across the whole chain -- a
peek at the next instruction only catches the outermost access, and a forward scan would
false-suppress an unrelated sibling access sitting just before a coalesce.
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "[$no] $str\n"; return true; });
$s = 'str';
$d = ['a' => ['b' => 1]];
$o = new ArrayObject(['k' => 'v']);

echo "== values ==\n";
var_dump($s[99] ?? 'D', $s[1] ?? 'D');
var_dump($d['a']['b'] ?? 'D', $d['x']['y'] ?? 'D', $undef['a']['b'] ?? 'D');
var_dump($o['k'] ?? 'D', $o['nope'] ?? 'D');
$n = 42;
var_dump($n['k'] ?? 'D');

echo "== sibling access still warns ==\n";
$a = ['k' => 1];
$pair = [$a['miss'], $b['j'] ?? 'D'];
var_dump($pair[1]);
?>
--EXPECT--
== values ==
string(1) "D"
string(1) "t"
int(1)
string(1) "D"
string(1) "D"
string(1) "v"
string(1) "D"
string(1) "D"
== sibling access still warns ==
[2] Undefined array key "miss"
string(1) "D"
