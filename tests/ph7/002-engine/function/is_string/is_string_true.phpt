--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_string('hello') returns true
--FILE--
<?php
$val = 'hello';
echo "is_string_true=" . (is_string($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_string_true=true
--CLEAN--
<?php
unset($val);
?>
