--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Missing '=' in const declaration
--FILE--
<?php
const FOO;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";", expecting "="%A
--CLEAN--
<?php

