--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
import_request_variables imports GET values into globals
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip: not PH7\n"; }
?>
--FILE--
<?php
// Prepare a GET value
$_GET['userid'] = '42';
// Import into global scope with prefix
import_request_variables('G','pref_');
// Check result
echo isset($pref_userid) && $pref_userid === '42' ? "ok\n" : "fail: $pref_userid\n";
?>
--EXPECT--
ok
--CLEAN--
<?php
unset($_GET);
