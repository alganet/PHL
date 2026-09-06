--CREDITS--
SPDX-FileCopyrightText: 2025 Test Coverage Improvement
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec handles non-hex prefix characters correctly
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo hexdec('x1a') . "\n";  // 'x' is non-hex, '1a' should be converted
echo hexdec('0xFF') . "\n";  // '0x' prefix, 'FF' should be converted
?>
--EXPECTF--
Error [8192]: Invalid characters passed for attempted conversion, these have been ignored in %s on line %d
26
Error [8192]: Invalid characters passed for attempted conversion, these have been ignored in %s on line %d
255
--CLEAN--
<?php

