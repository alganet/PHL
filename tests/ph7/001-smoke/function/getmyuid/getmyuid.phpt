--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: getmyuid returns integer
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' && !function_exists('zend_version')) {
    echo 'skip';
}
?>
--FILE--
<?php
$uid = getmyuid();
echo "getmyuid_isint=" . (is_int($uid) ? 'true' : 'false') . "\n";
?>
--EXPECT--
getmyuid_isint=true
--CLEAN--
<?php
unset($uid);
