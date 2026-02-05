--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Invalid token after 'use' in anonymous function
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$f = function() use invalid { };
?>
--EXPECTF--
%s %d Error: Syntax error while declaring annonymous function
Compile error
--CLEAN--
<?php
unset($f);
