--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test eq and ne operators with different string lengths
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo "different_lengths_eq: " . (("abc" eq "abcd") ? "true" : "false") . "\n";
echo "different_lengths_ne: " . (("abc" ne "abcd") ? "true" : "false") . "\n";
echo "same_length_eq: " . (("abc" eq "abc") ? "true" : "false") . "\n";
echo "same_length_ne: " . (("abc" ne "abc") ? "true" : "false") . "\n";
?>
--EXPECT--
different_lengths_eq: false
different_lengths_ne: false
same_length_eq: true
same_length_ne: true
--CLEAN--
<?php

