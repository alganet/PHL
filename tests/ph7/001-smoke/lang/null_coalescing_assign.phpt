--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Null coalescing assignment operator ??=
--DESCRIPTION--
Object property cases (`$obj->p ??= 'v'`) are intentionally omitted: PHL has a
pre-existing limitation with stdClass dynamic property assignment that is
unrelated to ??=. Class constant LHS cases (`Foo::CONST ??= 1`) diverge between
PHP (parse error) and PHL (runtime error), so they are not asserted here.
--FILE--
<?php
// Null LHS gets assigned
$a = null;
$a ??= 'set';
echo "1: $a\n";

// Non-null LHS preserved
$b = 'kept';
$b ??= 'fallback';
echo "2: $b\n";

// Falsy but not null preserved
$c = 0;
$c ??= 99;
echo "3: $c\n";

$d = '';
$d ??= 'nope';
echo "4: '$d'\n";

$e = false;
$e ??= 'nope';
echo "5: ", ($e === false ? 'false' : 'changed'), "\n";

// Undefined variable treated as null
$undef ??= 'init';
echo "6: $undef\n";

// Array element, missing key
$arr = [];
$arr['x'] ??= 'new';
echo "7: {$arr['x']}\n";

// Array element with explicit null
$arr['n'] = null;
$arr['n'] ??= 'filled';
echo "8: {$arr['n']}\n";

// Array element with existing value preserved
$arr['k'] = 'orig';
$arr['k'] ??= 'fallback';
echo "9: {$arr['k']}\n";

// Nested array auto-vivification
$deep = [];
$deep['a']['b'] ??= 'leaf';
echo "10: {$deep['a']['b']}\n";

// Short-circuit: RHS not evaluated when LHS non-null
function side_effect() {
    echo "called!\n";
    return 'rhs';
}
$keep = 'present';
$keep ??= side_effect();
echo "11: $keep\n";

$null_lhs = null;
$null_lhs ??= side_effect();
echo "12: $null_lhs\n";

// Right-associative chaining: $x ??= ($y ??= 'end')
$x = null;
$y = null;
$x ??= $y ??= 'end';
echo "13: x=$x y=$y\n";

// Chain stops when middle is set
$p = null;
$q = 'mid';
$p ??= $q ??= 'never';
echo "14: p=$p q=$q\n";

// Result of expression is the final LHS value
$src = null;
$result = ($src ??= 42);
echo "15: result=$result src=$src\n";

// Reference variable: assigning through the alias updates the referent
$base = null;
$alias = &$base;
$alias ??= 'via-ref';
echo "16: base=$base alias=$alias\n";

// COW: shared array, ??= on existing non-null key must not leak through
$src = ['k' => 'orig'];
$copy = $src;
$copy['k'] ??= 'overwritten';
echo "17: src=" . $src['k'] . " copy=" . $copy['k'] . "\n";

// COW: shared array, ??= on null key writes only to the copy
$src2 = ['k' => null];
$copy2 = $src2;
$copy2['k'] ??= 'set';
echo "18: src2=" . ($src2['k'] === null ? 'null' : $src2['k']) . " copy2=" . $copy2['k'] . "\n";

// COW: shared array, ??= on missing key writes only to the copy
$src3 = [];
$copy3 = $src3;
$copy3['k'] ??= 'new';
echo "19: src3=" . (isset($src3['k']) ? $src3['k'] : 'unset') . " copy3=" . $copy3['k'] . "\n";

// COW: nested shared, ??= on existing non-null leaf must not leak
$src4 = ['a' => ['b' => 'orig']];
$copy4 = $src4;
$copy4['a']['b'] ??= 'overwritten';
echo "20: src4=" . $src4['a']['b'] . " copy4=" . $copy4['a']['b'] . "\n";

// COW: nested shared, ??= on null leaf writes only to the copy
$src5 = ['a' => ['b' => null]];
$copy5 = $src5;
$copy5['a']['b'] ??= 'set';
echo "21: src5=" . ($src5['a']['b'] === null ? 'null' : $src5['a']['b']) . " copy5=" . $copy5['a']['b'] . "\n";
?>
--EXPECT--
1: set
2: kept
3: 0
4: ''
5: false
6: init
7: new
8: filled
9: orig
10: leaf
11: present
called!
12: rhs
13: x=end y=end
14: p=mid q=mid
15: result=42 src=42
16: base=via-ref alias=via-ref
17: src=orig copy=orig
18: src2=null copy2=set
19: src3=unset copy3=new
20: src4=orig copy4=orig
21: src5=null copy5=set
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $undef, $arr, $deep, $keep, $null_lhs, $x, $y, $p, $q, $src, $result, $base, $alias, $src2, $src3, $src4, $src5, $copy, $copy2, $copy3, $copy4, $copy5);
