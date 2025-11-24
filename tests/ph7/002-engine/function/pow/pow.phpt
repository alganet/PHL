--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: pow(2,3) returns 8
--FILE--
<?php
$val = pow(2, 3);
echo "pow=" . (int)$val . "\n";
?>
--EXPECT--
pow=8
--CLEAN--
<?php
unset($val);
?>
