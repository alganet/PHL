--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
floor(-2.9) should round down to -3
--FILE--
<?php
$val = floor(-2.9);
echo "floor=" . (int)$val . "\n";
?>
--EXPECT--
floor=-3
--CLEAN--
<?php
unset($val);
