--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: function receives array by value
--FILE--
<?php
function cow_function_pass_by_value_modify($arr) {
    $arr[0] = 99;
    return $arr[0];
}
$a = [1, 2, 3];
echo cow_function_pass_by_value_modify($a) . "\n";
echo $a[0] . "\n";
?>
--EXPECT--
99
1
--CLEAN--
<?php
unset($a);
