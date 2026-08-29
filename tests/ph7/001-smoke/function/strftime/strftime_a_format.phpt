--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strftime with %a format specifier returns abbreviated day name

--FILE--
<?php
set_error_handler(function(){ return true; }); // suppress the 8.1 strftime deprecation; the notice has its own test
$result = strftime("%a");
if (is_string($result) && strlen($result) === 3) {
    echo "PASS\n";
} else {
    echo "FAIL: expected 3-char string, got " . var_export($result, true) . "\n";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
