--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Declare statement with invalid syntax
--FILE--
<?php
declare invalid;
echo "should not reach here\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected identifier "invalid", expecting "("%A
--CLEAN--
<?php

