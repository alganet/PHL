--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Expected '(' after function name
--FILE--
<?php
function foo {}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "{", expecting "("%A
--CLEAN--
<?php

