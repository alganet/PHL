--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
trigger_error with no arguments returns false
--SKIPIF--
<?php
if (!function_exists('trigger_error')) { echo 'skip: trigger_error not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP error message format differs'; }
?>
--FILE--
<?php
$result = trigger_error();
if ($result) { echo "no_args_true\n"; } else { echo "no_args_false\n"; }
?>
--EXPECT--
no_args_false
--CLEAN--
<?php
unset($result);
