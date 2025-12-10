--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
restore_exception_handler restores previous handler
--SKIPIF--
<?php
if (!function_exists('restore_exception_handler')) { echo 'skip: restore_exception_handler not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP restore_exception_handler behavior differs'; }
?>
--FILE--
<?php
$result = restore_exception_handler();
if ($result === false) { echo "no_handler_false\n"; } else { echo "no_handler_true\n"; }
$old = set_exception_handler(function($e) { echo "Handler: " . $e->getMessage() . "\n"; });
if ($old === null) { echo "old_handler_null\n"; } else { echo "old_handler_not_null\n"; }
$result = restore_exception_handler();
if ($result) { echo "restore_ok\n"; } else { echo "restore_failed\n"; }
?>
--EXPECT--
no_handler_false
old_handler_null
restore_failed

