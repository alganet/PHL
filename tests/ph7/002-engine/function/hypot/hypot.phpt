--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: hypot(3,4) returns 5.00000000
--FILE--
<?php
$val = hypot(3,4);
echo "hypot=" . sprintf('%.8f', $val) . "\n";
?>
--EXPECT--
hypot=5.00000000
--CLEAN--
<?php
unset($val);
?>
