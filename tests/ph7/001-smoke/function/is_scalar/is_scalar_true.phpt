--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_scalar(123) returns true
--FILE--
<?php
$val = 123;
echo "is_scalar_true=" . (is_scalar($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_scalar_true=true
--CLEAN--
<?php
unset($val);
