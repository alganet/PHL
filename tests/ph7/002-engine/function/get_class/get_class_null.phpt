--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_class returns class name of object
--SKIPIF--
<?php
if (!function_exists('get_class')) { echo 'skip: get_class not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP get_class(null) throws fatal error, PHL returns false'; }
?>
--FILE--
<?php
$result = get_class(null);
if ($result === false) { echo "null_false_ok\n"; } else { echo "null_false_failed\n"; }
?>
--EXPECT--
null_false_ok
