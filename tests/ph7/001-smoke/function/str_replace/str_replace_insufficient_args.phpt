--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
str_replace insufficient arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = str_replace();
if ($result === null) {
    echo "null";
} else {
    echo "not null";
}
?>
--EXPECT--
null
--CLEAN--
<?php
unset($result);
