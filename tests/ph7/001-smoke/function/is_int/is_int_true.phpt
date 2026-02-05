--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_int(123) returns true
--FILE--
<?php
$val = 123;
echo "is_int_true=" . (is_int($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_int_true=true
--CLEAN--
<?php
unset($val);
