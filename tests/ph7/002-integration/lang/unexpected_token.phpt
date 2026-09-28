--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unexpected token
--SKIPIF--
<?php
// Both engines now stop at the FIRST refusal and print one diagnostic, but they
// blame a different token for this source: php names the "*" it could not start an
// operand with, PHL names the ";" its own recovery reached. The severity, the file,
// the line and the one-message rule all match -- only the noun differs -- so this
// test pins PHL's wording and is skipped under php.
if (function_exists('zend_version')) { echo 'skip php blames the "*", PHL the ";" (parser recovery point)'; }
?>
--FILE--
<?php
echo 1 + * 2;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php

