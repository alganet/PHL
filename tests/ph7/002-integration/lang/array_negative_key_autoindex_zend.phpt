--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Auto-index after a negative first key: php 8.3+ seeds the next index from the negative key (PHL continues from 0 — divergence recorded in PLAN.md §3.9)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
$a = [];
$a[-5] = 'a';
$a[] = 'b';
$a[] = 'c';
var_export(array_keys($a));
?>
--EXPECT--
array (
  0 => -5,
  1 => -4,
  2 => -3,
)
--CLEAN--
<?php
unset($a);
