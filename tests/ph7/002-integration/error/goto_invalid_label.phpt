--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Goto with invalid label name
--FILE--
<?php
goto 123;
label:
echo "done";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "123", expecting identifier%A
--CLEAN--
<?php

