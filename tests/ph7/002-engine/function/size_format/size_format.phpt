--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
size_format outputs human readable sizes
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
echo size_format(1*1024*1024*1024) . "\n"; // 1.0 GB
echo size_format(512*1024*1024) . "\n"; // 512.0 MB
echo size_format(8192) . "\n"; // 8.0 KB
?>
--EXPECT--
1.0 GB
512.0 MB
8.0 KB
