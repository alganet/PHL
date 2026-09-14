--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring a builtin class is a fatal error
--FILE--
<?php
class Exception {}
echo "unreached";
--EXPECTF--
%AFatal error:%ACannot redeclare class Exception%A
--CLEAN--
<?php
