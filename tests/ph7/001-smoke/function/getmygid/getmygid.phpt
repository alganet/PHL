--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: getmygid returns integer
--SKIPIF--
<?php
if (PHP_OS == 'WINNT' && !function_exists('zend_version')) {
    echo 'skip';
}
?>
--FILE--
<?php
$gid = getmygid();
echo "getmygid_isint=" . (is_int($gid) ? 'true' : 'false') . "\n";
?>
--EXPECT--
getmygid_isint=true
--CLEAN--
<?php
unset($gid);
