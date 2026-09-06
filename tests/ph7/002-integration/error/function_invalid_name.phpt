--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid function name
--FILE--
<?php
function 123() {}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "123", expecting "("%A
--CLEAN--
<?php

