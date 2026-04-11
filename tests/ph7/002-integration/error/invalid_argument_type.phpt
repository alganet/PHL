--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
function with invalid argument type
--FILE--
<?php
function foo(echo $a) {
}
--EXPECTF--
PHP Parse error:  syntax error, unexpected token "echo", expecting variable in %s on line %d
--CLEAN--
<?php

