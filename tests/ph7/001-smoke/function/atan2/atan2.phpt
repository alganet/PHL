--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: atan2(1,1) returns approx 0.78539816
--FILE--
<?php
$val = atan2(1,1);
echo "atan2=" . sprintf('%.8f', $val) . "\n";
?>
--EXPECT--
atan2=0.78539816
--CLEAN--
<?php
unset($val);
