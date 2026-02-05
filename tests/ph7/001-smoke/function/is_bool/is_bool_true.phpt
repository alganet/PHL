--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_bool(true) returns true
--FILE--
<?php
$val = true;
echo "is_bool_true=" . (is_bool($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_bool_true=true
--CLEAN--
<?php
unset($val);
