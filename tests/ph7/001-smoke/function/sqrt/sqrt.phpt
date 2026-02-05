--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: sqrt(9) returns 3
--FILE--
<?php
$val = sqrt(9);
echo "sqrt=" . sprintf('%.0f', $val) . "\n";
?>
--EXPECT--
sqrt=3
--CLEAN--
<?php
unset($val);
