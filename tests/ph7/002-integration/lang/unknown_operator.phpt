--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test unknown operator parsing
--FILE--
<?php
$a @@ $b;
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "@"%A
--CLEAN--
<?php

