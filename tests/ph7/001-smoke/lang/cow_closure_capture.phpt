--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: closure captures array by value
--FILE--
<?php
$a = [1, 2];
$f = function() use ($a) {
    $a[0] = 99;
    return $a[0];
};
echo $f() . "\n";
echo $a[0] . "\n";
?>
--EXPECT--
99
1
--CLEAN--
<?php
unset($a, $f);
