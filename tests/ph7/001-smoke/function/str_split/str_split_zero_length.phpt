--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: str_split with zero split length returns FALSE
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = str_split("hello", 0);
if ($result === false) {
    echo "PASS";
} else {
    echo "FAIL";
}
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($result);
