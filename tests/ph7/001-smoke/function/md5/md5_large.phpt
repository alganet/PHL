--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
md5 computes md5 hash for a large string (>64 bytes)
--FILE--
<?php
// Test with a string larger than MD5 block size (64 bytes) to trigger multi-block processing
$large_input = str_repeat("The quick brown fox jumps over the lazy dog. ", 3);
echo md5($large_input) . "\n";
?>
--EXPECT--
e6f263d514adf884ef6cc864d206d462
--CLEAN--
<?php
unset($large_input);
