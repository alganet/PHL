--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: intval('123abc') returns 123
--FILE--
<?php
$val = intval('123abc');
echo "intval=" . $val . "\n";
?>
--EXPECT--
intval=123
--CLEAN--
<?php
unset($val);
