--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
eval with empty string returns null
--SKIPIF--
<?php
if (!function_exists('eval')) { echo 'skip: eval not available'; }
?>
--FILE--
<?php
$result = eval('');
if ($result === null) { echo "empty_eval_null\n"; } else { echo "empty_eval_not_null\n"; }
$result = eval('return null;');
if ($result === null) { echo "return_null_ok\n"; } else { echo "return_null_failed\n"; }
?>
--EXPECT--
empty_eval_null
return_null_ok
--CLEAN--
<?php
unset($result);
