--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto with invalid label name
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
goto 123;
label:
echo "done";
?>
--EXPECTF--
%s Error:  goto: Invalid label name: '123' %s
--CLEAN--
<?php

