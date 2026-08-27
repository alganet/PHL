--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
json_decode with malformed JSON inputs
--SKIPIF--
<?php if (!function_exists('json_decode') || !function_exists('json_last_error')) { die('skip'); } ?>
<?php if (function_exists('zend_version')) { die('skip PHL and PHP json_decode differ'); } ?>
--FILE--
<?php
// Test malformed JSON that should return NULL and set error
$malformed = array(
    'invalid json',    // completely invalid
    '',                // empty string
);

foreach ($malformed as $json) {
    $result = json_decode($json);
    $error = json_last_error();
    echo ($result === null && $error != 0) ? "malformed_ok\n" : "malformed_fail\n";
}

// Test that some malformed JSON still sets error
$result = json_decode('{');
$error = json_last_error();
echo ($error != 0) ? "error_ok\n" : "error_fail\n";

// Test that valid JSON still works
$valid = '{"test": "value"}';
$result = json_decode($valid, true);
echo ($result !== null && isset($result['test']) && $result['test'] == 'value') ? "valid_ok\n" : "valid_fail\n";
?>
--EXPECTF--
malformed_ok
malformed_ok
Warning: json_decode(): JSON Objects are always returned as an associative array in %s on line %d
error_fail
valid_ok
--CLEAN--
<?php
unset($malformed, $result, $error, $valid);
