--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test eq and ne operators for strict string comparison
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test eq operator (strict string equality)
echo "eq_same_strings: " . (("123" eq "123") ? "true" : "false") . "\n";
echo "eq_string_vs_int: " . (("123" eq 123) ? "true" : "false") . "\n";
echo "eq_case_diff: " . (("abc" eq "ABC") ? "true" : "false") . "\n";
echo "eq_empty: " . (("" eq "") ? "true" : "false") . "\n";

// Test ne operator (strict string inequality)
echo "ne_same_strings: " . (("123" ne "123") ? "true" : "false") . "\n";
echo "ne_string_vs_int: " . (("123" ne 123) ? "true" : "false") . "\n";
echo "ne_case_diff: " . (("abc" ne "ABC") ? "true" : "false") . "\n";
echo "ne_empty: " . (("" ne "") ? "true" : "false") . "\n";
?>
--EXPECT--
eq_same_strings: true
eq_string_vs_int: true
eq_case_diff: false
eq_empty: true
ne_same_strings: true
ne_string_vs_int: true
ne_case_diff: false
ne_empty: true
--CLEAN--
<?php

