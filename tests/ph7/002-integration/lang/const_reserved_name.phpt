--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Declaring a reserved constant name should be forbidden
--FILE--
<?php
const true = 1;
?>
--EXPECTF--
%AFatal error:%ACannot redeclare constant 'true'%A
--CLEAN--
<?php

