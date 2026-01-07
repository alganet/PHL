--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test eq and ne operators case sensitivity
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo "case_diff_eq: " . (("abc" eq "ABC") ? "true" : "false") . "\n";
echo "case_diff_ne: " . (("abc" ne "ABC") ? "true" : "false") . "\n";
echo "case_same_eq: " . (("abc" eq "abc") ? "true" : "false") . "\n";
echo "case_same_ne: " . (("abc" ne "abc") ? "true" : "false") . "\n";
?>
--EXPECT--
case_diff_eq: false
case_diff_ne: false
case_same_eq: true
case_same_ne: true