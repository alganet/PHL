--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring an enum is a fatal error
--FILE--
<?php
enum RedeclEn {}
enum RedeclEn {}
echo "unreached";
--EXPECTF--
%AFatal error:%ACannot redeclare enum RedeclEn %Ain %A
--CLEAN--
<?php
