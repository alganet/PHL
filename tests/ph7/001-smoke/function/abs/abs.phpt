--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: abs(-5) returns 5
--FILE--
<?php
$val = abs(-5);
echo "abs=" . $val . "\n";
?>
--EXPECT--
abs=5
--CLEAN--
<?php
unset($val);
