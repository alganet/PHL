--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getcwd() with extra arg should ignore extras and return string
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$cwd = getcwd();
// Call with an extra argument (not type correct, but function must still return string)
echo "getcwd_type=" . gettype(getcwd("unexpected")) . "\n";
echo "cwd_eq=" . ($cwd === getcwd() ? 'same' : 'diff') . "\n";
?>
--EXPECT--
getcwd_type=string
cwd_eq=same

