--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Missing closing parenthesis in expression
--FILE--
<?php
echo (1 + 2;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php

