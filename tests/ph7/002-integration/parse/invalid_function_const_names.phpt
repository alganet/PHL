--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
invalid function and const names
--FILE--
<?php
function 123() {}
const 456 = 789;
echo "Should not reach here\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "123", expecting "("%A
--CLEAN--
<?php

