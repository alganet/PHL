--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ph7version returns the PH7_VERSION value
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// Compare constant PH7_VERSION to the return value of ph7version
if (defined('PH7_VERSION')) {
    // ph7version() is expected to be a substring of PH7_VERSION (e.g. "2.1.4" vs "PH7/2.1.4")
    echo (strpos(PH7_VERSION, ph7version()) !== false) ? "ok\n" : "fail\n";
} else {
    echo "no_const\n";
}
?>
--EXPECT--
ok
