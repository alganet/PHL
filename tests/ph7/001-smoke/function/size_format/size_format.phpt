--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
size_format outputs human readable sizes
--SKIPIF--
<?php
// PHL extension: `size_format()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: size_format() is not a php symbol'; }
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
--CLEAN--
<?php

