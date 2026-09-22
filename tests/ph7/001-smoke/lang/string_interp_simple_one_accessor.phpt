--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Simple "$var" interpolation takes exactly one accessor and never "::"
--FILE--
<?php
// php's simple syntax is a LEXER rule: the variable name plus AT MOST ONE
// accessor ("[offset]" or "->prop"), then literal text. PHL used to chain
// accessors greedily, so every shape below silently answered something else.
set_error_handler(function ($no, $msg) { echo "Warning: $msg\n"; return true; });

class SimpInterpK { const C = 1; public static $s = 7; }

$c = 'SimpInterpK';
$s = 'sv';
$u = 'SimpInterpNoSuchClass';

// "::" is never an accessor: the VALUE of $c, then literal text.
echo "$c::C\n";
echo "$c::$s\n";
echo "$c::\n";
echo "$u::C\n";

$o = new stdClass();
$o->p = 1;
$o->arr = [10, 20];
$o->obj = new stdClass();
$o->obj->q = 5;

// One "->" only; the second one is literal.
echo "$o->p->q\n";
echo "$o->p->\n";

// One accessor TOTAL: a subscript after a property is literal.
echo "$o->p[0]\n";
echo "$o->arr[0]\n";

// ...and a second subscript after a subscript is literal too.
$a = ['x' => 1, 0 => 'zero'];
$n = [['x' => 9]];
echo "$a[x][y]\n";
echo "$n[0][x]\n";

// "{...}" is not an accessor either.
$x = 1;
echo "$x{'a'}\n";
echo "$a{0}\n";

// The one-accessor forms themselves still work, in strings and heredocs.
$k = 0;
echo "$a[x] $a[0] $a[$k] $o->p\n";
echo <<<EOT
$c::C $o->p->q $a[x][y] $o->p
EOT;
echo "\n";

// Complex syntax is the form that DOES reach chained accessors.
echo "{$o->obj->q} {$c}::C ", SimpInterpK::C, " ", SimpInterpK::$s, "\n";
restore_error_handler();
?>
--EXPECT--
SimpInterpK::C
SimpInterpK::sv
SimpInterpK::
SimpInterpNoSuchClass::C
1->q
1->
1[0]
Warning: Array to string conversion
Array[0]
1[y]
Warning: Array to string conversion
Array[x]
1{'a'}
Warning: Array to string conversion
Array{0}
1 zero zero 1
SimpInterpK::C 1->q 1[y] 1
5 SimpInterpK::C 1 7
--CLEAN--
<?php
unset($c, $s, $u, $o, $a, $n, $x, $k);
