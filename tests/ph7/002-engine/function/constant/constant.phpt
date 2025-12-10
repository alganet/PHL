--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
constant retrieves defined constant value
--SKIPIF--
<?php
if (!function_exists('constant')) { echo 'skip: constant not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP throws fatal error for undefined constants, PHL returns null'; }
?>
--FILE--
<?php
define('TEST_CONST', 42);
$val = constant('TEST_CONST');
if ($val === 42) { echo "defined_ok\n"; } else { echo "defined_failed\n"; }
$undef = constant('UNDEFINED_CONST');
if ($undef === null) { echo "undefined_ok\n"; } else { echo "undefined_failed\n"; }
?>
--EXPECTF--
defined_ok
%s Notice: constant(): 'UNDEFINED_CONST': Undefined constant
undefined_ok

