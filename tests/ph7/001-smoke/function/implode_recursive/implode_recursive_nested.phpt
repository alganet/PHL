--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
implode_recursive with nested arrays
--SKIPIF--
<?php
// PHL extension: `implode_recursive()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: implode_recursive() is not a php symbol'; }
?>
--FILE--
<?php
// Test implode_recursive with nested arrays
$result = implode_recursive('-', array('a', array('b', 'c'), 'd'));
echo $result . "\n";
?>
--EXPECT--
a-b-c-d
--CLEAN--
<?php
unset($result);
