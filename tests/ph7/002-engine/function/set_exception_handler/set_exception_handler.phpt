--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
set_exception_handler sets handler for uncaught exceptions
--SKIPIF--
<?php
if (!function_exists('set_exception_handler')) { echo 'skip: set_exception_handler not available'; }
?>
--FILE--
<?php
$old = set_exception_handler(function($e) {
    echo "Caught: " . $e->getMessage() . "\n";
});
if ($old === null) { echo "old_handler_null\n"; } else { echo "old_handler_not_null\n"; }
throw new Exception("test exception");
echo "after_throw\n";
?>
--EXPECT--
old_handler_null
Caught: test exception

