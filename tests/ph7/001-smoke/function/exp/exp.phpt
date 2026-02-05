--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: exp(1) returns e (approx)
--FILE--
<?php
$val = exp(1);
echo "exp=" . sprintf('%.8f', $val) . "\n";
?>
--EXPECT--
exp=2.71828183
--CLEAN--
<?php
unset($val);
