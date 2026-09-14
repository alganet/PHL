--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring a class is a fatal error (php parity)
--FILE--
<?php
class RedeclCls {}
class RedeclCls {}
echo "unreached";
--EXPECTF--
%AFatal error:%ACannot redeclare class RedeclCls %Ain %A
--CLEAN--
<?php
