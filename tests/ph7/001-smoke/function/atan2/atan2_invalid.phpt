--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
atan2 returns float type
--FILE--
<?php
$result = atan2(1, 1);
echo is_float($result) ? "FLOAT" : "NOT_FLOAT";
echo "\n";
?>
--EXPECT--
FLOAT
--CLEAN--
<?php
unset($result);
