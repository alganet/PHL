--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
const: Invalid constant name
--FILE--
<?php
const 123 = 5;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "123", expecting identifier%A
--CLEAN--
<?php

