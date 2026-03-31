--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: returned array is isolated from caller mutations
--FILE--
<?php
function cow_return_value_make() {
    return [1, 2, 3];
}
$a = cow_return_value_make();
$b = $a;
$b[0] = 99;
echo $a[0] . "\n";
echo $b[0] . "\n";
?>
--EXPECT--
1
99
--CLEAN--
<?php
unset($a, $b);
