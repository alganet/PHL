--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mismatched parenthesis
--FILE--
<?php
echo (1 + 2;
?>
--EXPECTF--
%APHP Parse error:  syntax error, unexpected token ";" in %s on line 2%A
--CLEAN--
<?php

