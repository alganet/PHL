--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring an interface is a fatal error
--FILE--
<?php
interface RedeclIf {}
interface RedeclIf {}
echo "unreached";
--EXPECTF--
%AFatal error:%ACannot redeclare interface RedeclIf %Ain %A
--CLEAN--
<?php
