--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
set_error_handler with suppress (return true)
--FILE--
<?php
$restore_error_handler = set_error_handler(function($errno, $errstr, $errfile, $errline) {
    $fname = basename($errfile);
    echo "Handler called: $errno, $errstr, $fname, $errline" . PHP_EOL;
    return true; // Suppress normal error reporting
});
trigger_error("Test error suppressed", E_USER_NOTICE);
set_error_handler($restore_error_handler);
echo "After error\n";
?>
--EXPECTF--
Handler called: %d, Test error suppressed, set_error_handler_suppress.phpt.file, %d
After error