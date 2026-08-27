--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
trigger_error user error triggering
--SKIPIF--
<?php
if (!function_exists('trigger_error')) { echo 'skip: trigger_error not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP error message format differs'; }
?>
--FILE--
<?php
$result = trigger_error("Test notice", E_USER_NOTICE);
if ($result) { echo "notice_ok\n"; } else { echo "notice_failed\n"; }
$result = trigger_error("Test warning", E_USER_WARNING);
if ($result) { echo "warning_ok\n"; } else { echo "warning_failed\n"; }
?>
--EXPECTF--
Notice: Test notice in %s on line %d
notice_ok
Warning: Test warning in %s on line %d
warning_ok
--CLEAN--
<?php
unset($result);
