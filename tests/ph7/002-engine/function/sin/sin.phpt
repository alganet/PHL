--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: sin(0) returns 0.00000000
--FILE--
<?php
$val = sin(0);
echo "sin=" . sprintf('%.8f', $val) . "\n";
?>
--EXPECT--
sin=0.00000000
--CLEAN--
<?php
unset($val);
?>
