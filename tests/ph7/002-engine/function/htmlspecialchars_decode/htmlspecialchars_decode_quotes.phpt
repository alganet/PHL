--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode with quote entities
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test decoding of &quot; entity
$result = htmlspecialchars_decode('&quot;hello&quot;');
echo $result . "\n";
?>
--EXPECT--
"hello"
