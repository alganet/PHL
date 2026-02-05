--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid operator parsing to cover uncovered lines in PH7_ExprExtractOperator
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a invalid_op $b;
?>
--EXPECTF--
%s 2 Error: Unexpected token 'invalid_op'
Compile error
--CLEAN--
<?php

