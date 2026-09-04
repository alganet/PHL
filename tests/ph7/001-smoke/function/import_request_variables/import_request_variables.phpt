--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
import_request_variables imports GET values into globals
--SKIPIF--
<?php
// PHL extension: `import_request_variables()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: import_request_variables() is not a php symbol'; }
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
