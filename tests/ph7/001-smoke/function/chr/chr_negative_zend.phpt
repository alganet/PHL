--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php deprecates an out-of-range chr() codepoint and constrains it with % 256 (zend half of the twin pair — PHL raises a ValueError, see chr_negative.phpt)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
try {
    echo ord(chr(-1)) . "\n";
} catch (ValueError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
?>
--EXPECTF--
%A255
--CLEAN--
<?php
unset($e);
