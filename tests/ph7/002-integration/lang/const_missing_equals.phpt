--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
const without equals (covers compile.c lines 492,494)
--FILE--
<?php
const MYCONST;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ";", expecting "="%A
--CLEAN--
<?php

