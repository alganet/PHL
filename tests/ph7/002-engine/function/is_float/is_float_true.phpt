--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_float(1.2) returns true
--FILE--
<?php
$val = 1.2;
echo "is_float_true=" . (is_float($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_float_true=true
--CLEAN--
<?php
unset($val);
?>
