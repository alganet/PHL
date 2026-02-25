--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
floor(5) should return the integer unchanged
--FILE--
<?php
$val = floor(5);
echo "floor=" . (int)$val . "\n";
?>
--EXPECT--
floor=5
--CLEAN--
<?php
unset($val);
?>
