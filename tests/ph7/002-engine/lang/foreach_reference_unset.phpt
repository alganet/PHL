--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Foreach by reference + unset behavior
--FILE--
<?php
$arr = array(1, 2, 3);
foreach ($arr as &$v) {
    $v *= 2;
}
unset($v);

echo implode(',', $arr) . "\n";
?>
--EXPECT--
2,4,6

--CLEAN--
<?php
unset($arr);
?>
