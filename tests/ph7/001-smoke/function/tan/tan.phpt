--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: tan(0) returns 0.00000000
--FILE--
<?php
$val = tan(0);
echo "tan=" . sprintf('%.8f', $val) . "\n";
?>
--EXPECT--
tan=0.00000000
--CLEAN--
<?php
unset($val);
