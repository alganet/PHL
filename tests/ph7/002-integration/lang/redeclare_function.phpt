--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Redeclaring a function is a fatal error (php parity)
--FILE--
<?php
function redecl_fn() {}
function redecl_fn() {}
echo "unreached";
--EXPECTF--
%AFatal error:%ACannot redeclare function redecl_fn() %Ain %A
--CLEAN--
<?php
