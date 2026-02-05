--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
html_entity_decode with no arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = html_entity_decode();
if (is_null($result)) {
    echo "PASS";
} else {
    echo "FAIL: expected null, got " . var_export($result, true);
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
