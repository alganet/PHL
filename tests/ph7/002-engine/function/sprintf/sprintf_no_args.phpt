--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: sprintf with no arguments returns empty string
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = sprintf();
if ($result == "") {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS
