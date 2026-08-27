--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
user_error alias for trigger_error
--SKIPIF--
<?php
if (!function_exists('user_error')) { echo 'skip: user_error not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP error message format differs'; }
?>
--FILE--
<?php
$result = user_error("Test error", E_USER_NOTICE);
if ($result) { echo "user_error_ok\n"; } else { echo "user_error_failed\n"; }
?>
--EXPECTF--
Notice: Test error in %s on line %d
user_error_ok
--CLEAN--
<?php
unset($result);
