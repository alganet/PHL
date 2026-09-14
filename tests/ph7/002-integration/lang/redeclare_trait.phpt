--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring a trait is a fatal error
--FILE--
<?php
trait RedeclTr {}
trait RedeclTr {}
echo "unreached";
--EXPECTF--
%AFatal error:%ACannot redeclare trait RedeclTr %Ain %A
--CLEAN--
<?php
