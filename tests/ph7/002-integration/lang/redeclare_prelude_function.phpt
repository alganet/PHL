--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring a prelude function is a fatal error
--FILE--
<?php
function ini_get() {}
echo "unreached";
--EXPECTF--
%AFatal error:%ACannot redeclare function ini_get()%A
--CLEAN--
<?php
