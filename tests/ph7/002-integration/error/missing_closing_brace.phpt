--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Syntax error: Missing closing brace in variable name
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$var = ${unclosed;
?>
--EXPECTF--
%s %d Error:  Syntax error: Missing closing brace '}'
Compile error
--CLEAN--
<?php
unset($var);
