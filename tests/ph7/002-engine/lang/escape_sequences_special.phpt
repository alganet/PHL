--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Special escape sequences in double-quoted strings
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test \a (alert/bell) - ASCII 7
$alert = "\a";
if (ord($alert) === 7) {
    echo "alert OK\n";
} else {
    echo "alert FAIL: " . ord($alert) . "\n";
}

// Test \b (backspace) - ASCII 8
$backspace = "\b";
if (ord($backspace) === 8) {
    echo "backspace OK\n";
} else {
    echo "backspace FAIL: " . ord($backspace) . "\n";
}

// Test \f (form-feed) - ASCII 12
$formfeed = "\f";
if (ord($formfeed) === 12) {
    echo "formfeed OK\n";
} else {
    echo "formfeed FAIL: " . ord($formfeed) . "\n";
}

// Test \v (vertical tab) - ASCII 11
$vtab = "\v";
if (ord($vtab) === 11) {
    echo "vtab OK\n";
} else {
    echo "vtab FAIL: " . ord($vtab) . "\n";
}

// Test \' (single quote) in double-quoted string
$squote = "\'";
if ($squote === "'") {
    echo "squote OK\n";
} else {
    echo "squote FAIL\n";
}
?>
--EXPECT--
alert OK
backspace OK
formfeed OK
vtab OK
squote OK