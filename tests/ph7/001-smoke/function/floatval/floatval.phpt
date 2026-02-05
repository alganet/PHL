--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: floatval('123.45xyz') returns 123.45
--FILE--
<?php
$val = floatval('123.45xyz');
/* Normalize to two decimals for stability */
echo "floatval=" . sprintf('%.2f', $val) . "\n";
?>
--EXPECT--
floatval=123.45
--CLEAN--
<?php
unset($val);
