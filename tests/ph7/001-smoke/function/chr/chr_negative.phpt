--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE (non-deprecated compatibility): php merely DEPRECATES an out-of-range chr() codepoint and constrains it with % 256, so PHL rejects it with a ValueError instead. php's deprecating half lives in the _zend twin.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
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
--EXPECT--
ValueError: chr(): Argument #1 ($codepoint) must be between 0 and 255
--CLEAN--
<?php
unset($e);
