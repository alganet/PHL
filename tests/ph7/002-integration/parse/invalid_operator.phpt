--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid operator parsing to cover uncovered lines in PH7_ExprExtractOperator
--FILE--
<?php
$a invalid_op $b;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected identifier "invalid_op"%A
--CLEAN--
<?php

