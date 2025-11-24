--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: floor(2.9) returns 2
--FILE--
<?php
$val = floor(2.9);
echo "floor=" . (int)$val . "\n";
?>
--EXPECT--
floor=2
--CLEAN--
<?php
unset($val);
?>
