--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A compile fatal's stack trace keeps the arguments when zend.exception_ignore_args is Off
--DESCRIPTION--
php's fatal-error backtrace reads the same directive as an exception's trace, and
prints a string argument cut at zend.exception_string_param_max_len.
--INI--
display_errors=1
log_errors=0
zend.exception_ignore_args=0
zend.exception_string_param_max_len=3
--FILE--
<?php
function h($n, $s) { eval('class K { function m() { return parent::m(); } }'); }
h(42, 'secret');
?>
--EXPECTF--
Fatal error: Cannot use "parent" when current class scope has no parent in %s(2) : eval()'d code on line 1
Stack trace:
#0 %s(3): h(42, 'sec...')
#1 {main}
--EXPECT_STDERR--
