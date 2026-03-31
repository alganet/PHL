--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: function receives array by reference with &
--FILE--
<?php
function cow_function_pass_by_ref_modify(&$arr) {
    $arr[0] = 99;
}
$a = [1, 2, 3];
cow_function_pass_by_ref_modify($a);
echo $a[0] . "\n";
?>
--EXPECT--
99
--CLEAN--
<?php
unset($a);
