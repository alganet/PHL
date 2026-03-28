--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Elvis operator ?: with various types
--FILE--
<?php
// Truthy values - should return the left operand
echo "hello" ?: "fallback", "\n";
echo 1 ?: "fallback", "\n";
echo 1.5 ?: "fallback", "\n";
echo true ?: "fallback", "\n";

// Falsy values - should return the right operand
echo "" ?: "fallback", "\n";
echo 0 ?: "fallback", "\n";
echo 0.0 ?: "fallback", "\n";
echo false ?: "fallback", "\n";
echo null ?: "fallback", "\n";

// Nested elvis
echo null ?: "" ?: "final", "\n";
echo 0 ?: null ?: "last", "\n";
echo "first" ?: "second" ?: "third", "\n";

// Elvis with variables
$a = "value";
$b = null;
$c = 0;
echo $a ?: "default", "\n";
echo $b ?: "default", "\n";
echo $c ?: "default", "\n";

// Elvis in assignment
$result = null ?: "assigned";
echo $result, "\n";

// Elvis preserves type
$x = 42 ?: 0;
echo $x + 1, "\n";

$y = 3.14 ?: 0;
echo $y, "\n";

// Single evaluation: LHS must be evaluated exactly once
$counter = 0;
function inc() {
    global $counter;
    $counter++;
    return $counter;
}

// Truthy: inc() called once, returns 1, RHS not evaluated
echo inc() ?: "never", "\n";
echo $counter, "\n";

// Falsy: LHS evaluated once (returns 0), RHS evaluated
$counter = -1;
echo inc() ?: "was zero", "\n";
echo $counter, "\n";

// Side-effect via increment: evaluated once
$n = 5;
echo $n++ ?: "zero", "\n";
echo $n, "\n";

// Elvis in function argument
function show($v) { echo $v, "\n"; }
show("hi" ?: "bye");
show(0 ?: "fallback");

// Elvis in condition
if ("yes" ?: "no") {
    echo "truthy branch", "\n";
}
?>
--EXPECT--
hello
1
1.5
1
fallback
fallback
fallback
fallback
fallback
final
last
first
value
default
default
assigned
43
3.14
1
1
was zero
0
5
6
hi
fallback
truthy branch
--CLEAN--
<?php
