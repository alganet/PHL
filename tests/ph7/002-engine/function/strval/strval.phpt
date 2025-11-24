--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: strval(123) returns "123"
--FILE--
<?php
$val = strval(123);
echo "strval=" . $val . "\n";
?>
--EXPECT--
strval=123
--CLEAN--
<?php
unset($val);
?>
