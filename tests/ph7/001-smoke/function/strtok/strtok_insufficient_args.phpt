--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtok insufficient arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = strtok();
if ($result === false) {
    echo "false";
} else {
    echo "not false";
}
?>
--EXPECT--
false
--CLEAN--
<?php
unset($result);
