--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
define creates constants
--SKIPIF--
<?php
if (!function_exists('define')) { echo 'skip: define not available'; }
?>
--FILE--
<?php
$result = define('MY_CONST', 'hello');
if ($result) { echo "define_ok\n"; } else { echo "define_failed\n"; }
if (defined('MY_CONST')) { echo "defined_check_ok\n"; } else { echo "defined_check_failed\n"; }
$val = constant('MY_CONST');
if ($val === 'hello') { echo "value_ok\n"; } else { echo "value_failed\n"; }
?>
--EXPECT--
define_ok
defined_check_ok
value_ok

