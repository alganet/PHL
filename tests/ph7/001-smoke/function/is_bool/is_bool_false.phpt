--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: is_bool(0) returns false
--FILE--
<?php
$val = 0;
echo "is_bool_false=" . (is_bool($val) ? 'true' : 'false') . "\n";
?>
--EXPECT--
is_bool_false=false
--CLEAN--
<?php
unset($val);
