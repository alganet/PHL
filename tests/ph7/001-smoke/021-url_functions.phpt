--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test URL functions availability
--FILE--
<?php
echo "URL functions test: Basic availability check\n";

// Test that functions exist
$functions = array('parse_url', 'urlencode', 'urldecode', 'rawurlencode', 'rawurldecode');
foreach ($functions as $func) {
    if (function_exists($func)) {
        echo "$func exists\n";
    } else {
        echo "$func does not exist\n";
    }
}

echo "URL functions test completed.\n";
?>
--EXPECT--
URL functions test: Basic availability check
parse_url exists
urlencode exists
urldecode exists
rawurlencode exists
rawurldecode exists
URL functions test completed.