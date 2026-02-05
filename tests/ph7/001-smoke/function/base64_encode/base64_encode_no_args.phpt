--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base64_encode with no arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = base64_encode();
if ($result === false) {
    echo "PASS\n";
} else {
    echo "FAIL\n";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
