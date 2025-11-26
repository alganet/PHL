--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_defined_functions returns an array and contains built-in functions
--FILE--
<?php
$funcs = get_defined_functions();
// Check for known built-in functions
if (is_array($funcs) && isset($funcs['internal']) && in_array('strlen', $funcs['internal'])) {
    echo "get_defined_functions_ok\n";
} else {
    echo "get_defined_functions_fail\n";
}
?>
--EXPECT--
get_defined_functions_ok
