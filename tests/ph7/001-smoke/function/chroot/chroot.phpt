--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chroot() changes the root directory
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' || function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
// Try to change to root directory (likely to fail on most systems, but tests the code path)
echo chroot("/") ? "true\n" : "false\n";
?>
--EXPECT--
false
--CLEAN--
<?php

