--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset checks variable existence
--SKIPIF--
<?php
if (!function_exists('isset')) { echo 'skip: isset not available'; }
?>
--FILE--
<?php
$x = 10;
if (isset($x)) { echo "x_set\n"; } else { echo "x_not_set\n"; }
if (isset($y)) { echo "y_set\n"; } else { echo "y_not_set\n"; }
$z = null;
if (isset($z)) { echo "z_set\n"; } else { echo "z_not_set\n"; }
?>
--EXPECT--
x_set
y_not_set
z_not_set
--CLEAN--
<?php
unset($x, $z);
