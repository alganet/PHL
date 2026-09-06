--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php 8 removed curly-brace offsets: $a{0} is a parse error, not a subscript
--SKIPIF--
<?php
// Both engines reject it; php's bison parser adds an "expecting ..." tail that a
// recursive-descent parser cannot reproduce (NEWPLAN section 7), so the message text
// is only pinned under PHL.
if (function_exists('zend_version')) echo 'skip PHL pins the message; php adds a bison expectation tail';
?>
--FILE--
<?php
$a = [1, 2];
echo $a{0};
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "{"%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php
