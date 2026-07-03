--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Appending past an occupied PHP_INT_MAX key throws a catchable Error (php-exact)
--FILE--
<?php
$a = [PHP_INT_MAX - 1 => 'x'];
$a[] = 'y'; // lands on PHP_INT_MAX, like php
var_export(array_keys($a));
echo "\n";
try {
    $a[] = 'z';
    echo "appended, count=", count($a), "\n";
} catch (Error $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
// The failed append must not corrupt the array (no aliased overwrite).
var_export($a[PHP_INT_MAX]);
echo "\n";
// Append lvalue and array_push take the same catchable path.
try {
    $a[]['k'] = 1;
} catch (Error $e) {
    echo "lvalue: ", $e->getMessage(), "\n";
}
try {
    array_push($a, 'w');
} catch (Error $e) {
    echo "push: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
array (
  0 => 9223372036854775806,
  1 => 9223372036854775807,
)
caught: Cannot add element to the array as the next element is already occupied
'y'
lvalue: Cannot add element to the array as the next element is already occupied
push: Cannot add element to the array as the next element is already occupied
--CLEAN--
<?php
unset($a);
