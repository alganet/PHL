--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
size_format handles edge cases for better coverage
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test very small values (less than 100 bytes)
echo size_format(50) . "\n"; // Should return "0.1 KB"

// Test values that trigger overflow condition
echo size_format(999*1024*1024*1024*1024*1024) . "\n"; // Large value

// Test boundary values around 100 bytes
echo size_format(99) . "\n";  // Just under 100
echo size_format(100) . "\n"; // Exactly 100
echo size_format(101) . "\n"; // Just over 100

// Test different units with precise values
echo size_format(1536) . "\n"; // 1.5 KB
echo size_format(1048576) . "\n"; // 1.0 MB
echo size_format(1073741824) . "\n"; // 1.0 GB
?>
--EXPECT--
0.1 KB
999.0 PB
0.1 KB
0.1 KB
0.1 KB
1.5 KB
1.0 MB
1.0 GB
--CLEAN--
<?php

