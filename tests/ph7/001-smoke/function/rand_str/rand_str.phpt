--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rand_str returns a string with requested length and default length on invalid argument
--SKIPIF--
<?php
// PHL extension: `rand_str()` does not exist in php (it is an added API surface,
// allowed by the section 10 scope policy as a documented PHL extension —
// it does not change the meaning of valid php source). Engine-specific by design.
if (function_exists('zend_version')) { echo 'skip PHL extension: rand_str() is not a php symbol'; }
?>
--FILE--
<?php
// test explicit length
$s = rand_str(8);
if (strlen($s) !== 8) echo "len8_fail\n"; else echo "len8_ok\n";
// test default length on invalid input (e.g. -1)
$s2 = rand_str(-1);
if (strlen($s2) === 0) echo "len_default_fail\n"; else echo "len_default_ok\n";
?>
--EXPECT--
len8_ok
len_default_ok
--CLEAN--
<?php
unset($s, $s2);
