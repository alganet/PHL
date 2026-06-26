--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A scalar in an intersection type is rejected at compile time
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip PHL-specific diagnostic wording'; ?>
--FILE--
<?php
function bad(int&string $x) {}
echo "unreachable\n";
?>
--EXPECTF--
%AType int cannot be part of an intersection type%A
--CLEAN--
<?php
