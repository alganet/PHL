--CREDITS--
Test for VM expression evaluation edge cases
--TEST--
VM expression evaluation with complex operations
--FILE--
<?php
// Test complex expression evaluation
$a = 10;
$b = 20;
$c = ($a + $b) * 2 / 3;
echo "c: $c\n";

// Test with ternary operator
$d = $a > 5 ? $b : $a;
echo "d: $d\n";

// Test logical operations
$e = ($a && $b) ? 'true' : 'false';
echo "e: $e\n";

// Test bitwise operations
$f = $a | $b;
$g = $a & $b;
echo "f: $f, g: $g\n";

// Test shift operations
$h = $a << 1;
$i = $b >> 1;
echo "h: $h, i: $i\n";

// Test modulo
$j = $b % 7;
echo "j: $j\n";
?>
--EXPECT--
c: 20
d: 20
e: true
f: 30, g: 0
h: 20, i: 10
j: 6