--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode()/json_validate() throw ValueError for an out-of-range $depth (PHP 8)
--FILE--
<?php
foreach ([0, -1, -7, 2147483648] as $d) {
    try {
        json_decode('[1]', true, $d);
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
foreach ([0, -3, 2147483648] as $d) {
    try {
        json_validate('[1]', $d);
    } catch (\ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
// INT_MAX and a normal depth are accepted; empty string is handled before $depth.
echo json_encode(json_decode('[1]', true, 2147483647)), "\n";
echo var_export(json_decode('', true, 0), true), "\n";
echo var_export(json_validate('[1]', 5), true), "\n";
// php clears json_last_error before validating $depth, so a caught depth
// ValueError leaves the error state at JSON_ERROR_NONE (0).
json_decode('{bad');
try { json_decode('1', true, 0); } catch (\ValueError $e) {}
echo "last_error=", json_last_error(), "\n";
?>
--EXPECT--
json_decode(): Argument #3 ($depth) must be greater than 0
json_decode(): Argument #3 ($depth) must be greater than 0
json_decode(): Argument #3 ($depth) must be greater than 0
json_decode(): Argument #3 ($depth) must be less than 2147483647
json_validate(): Argument #2 ($depth) must be greater than 0
json_validate(): Argument #2 ($depth) must be greater than 0
json_validate(): Argument #2 ($depth) must be less than 2147483647
[1]
NULL
true
last_error=0
--CLEAN--
<?php
