--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: array with invalid reference to literal
--FILE--
<?php
$a = array(&1);
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "1"%A
--CLEAN--
<?php
unset($a);
