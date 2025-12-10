--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
error_reporting get and set error reporting level
--SKIPIF--
<?php
if (!function_exists('error_reporting')) { echo 'skip: error_reporting not available'; }
?>
--FILE--
<?php
$old = error_reporting();
if ($old > 0) { echo "old_level_ok\n"; } else { echo "old_level_zero\n"; }
error_reporting(0);
$new = error_reporting();
if ($new == 0) { echo "set_zero_ok\n"; } else { echo "set_zero_failed\n"; }
error_reporting(32767);
$restored = error_reporting();
if ($restored > 0) { echo "restore_ok\n"; } else { echo "restore_failed\n"; }
?>
--EXPECT--
old_level_ok
set_zero_ok
restore_ok

