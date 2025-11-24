--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: cos(0) returns 1.00000000
--FILE--
<?php
$val = cos(0);
echo "cos=" . sprintf('%.8f', $val) . "\n";
?>
--EXPECT--
cos=1.00000000
--CLEAN--
<?php
unset($val);
?>
