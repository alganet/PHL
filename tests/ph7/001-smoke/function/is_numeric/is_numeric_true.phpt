--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_numeric('123') returns true
--FILE--
<?php
$val = '123';
echo "is_numeric_true=" . (is_numeric($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_numeric_true=true
--CLEAN--
<?php
unset($val);
