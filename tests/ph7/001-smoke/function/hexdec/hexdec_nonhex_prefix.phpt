--CREDITS--
SPDX-FileCopyrightText: 2025 Test Coverage Improvement
SPDX-License-Identifier: BSD-3-Clause
--TEST--
hexdec handles non-hex prefix characters correctly
--FILE--
<?php
echo hexdec('x1a') . "\n";  // 'x' is non-hex, '1a' should be converted
echo hexdec('0xFF') . "\n";  // '0x' prefix, 'FF' should be converted
?>
--EXPECTF--
%AInvalid characters passed for attempted conversion, these have been ignored%A26%A255%A
--CLEAN--
<?php

