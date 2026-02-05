--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file_exists() should return FALSE for non-existent files
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test with a non-existent file
$nonexistent = __DIR__ . '/ph7_nonexistent_file_' . uniqid();
echo file_exists($nonexistent) ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php
unset($nonexistent);
