--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 with zero arguments returns float 0
--FILE--
<?php
$result = atan2(0, 0);
echo $result . "\n";
echo is_float($result) ? "FLOAT" : "NOT_FLOAT";
echo "\n";
?>
--EXPECT--
0
FLOAT
--CLEAN--
<?php
unset($result);
