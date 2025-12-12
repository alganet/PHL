--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
define() returns false when attempting to redefine an existing constant and original value remains
--SKIPIF--
<?php
if (!function_exists('define')) { echo 'skip: define not available'; }
?>
--FILE--
<?php
define('PH7_TEST_CONST_OVERWRITE', 'first');
$ok2 = @define('PH7_TEST_CONST_OVERWRITE', 'second');
echo $ok2 ? 'redefine_ok' : 'redefine_failed';
echo PHP_EOL;
echo constant('PH7_TEST_CONST_OVERWRITE') . PHP_EOL;
?>
--EXPECTF--
redefine_%s
%s
