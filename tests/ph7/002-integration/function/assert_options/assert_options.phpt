--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert_options configure assertion behavior
--SKIPIF--
<?php
if (!function_exists('assert_options')) { echo 'skip: assert_options not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP assert() behavior differs'; }
?>
--FILE--
<?php
// Test getting old flags
$old = assert_options(1); // PH7_ASSERT_DISABLE
echo "old_flags_set\n";
// Test setting disable flag
assert_options(1, true); // Disable assertions
$result = assert(true);
if ($result) { echo "disabled_assert_ok\n"; } else { echo "disabled_assert_failed\n"; }
// Restore
assert_options(1, false);
?>
--EXPECT--
old_flags_set
disabled_assert_ok
--CLEAN--
<?php
unset($old, $result);
