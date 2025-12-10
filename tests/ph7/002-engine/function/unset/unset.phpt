--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unset removes variables
--SKIPIF--
<?php
if (!function_exists('unset')) { echo 'skip: unset not available'; }
?>
--FILE--
<?php
$x = 10;
$y = 20;
unset($x);
if (!isset($x)) { echo "unset_x_ok\n"; } else { echo "unset_x_failed\n"; }
if (isset($y)) { echo "y_still_set\n"; } else { echo "y_unset\n"; }
unset($y);
if (!isset($y)) { echo "unset_y_ok\n"; } else { echo "unset_y_failed\n"; }
?>
--EXPECT--
unset_x_ok
y_still_set
unset_y_ok

