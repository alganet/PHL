--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A function call in a constant expression is a compile-time fatal (was a bare skip asserting PH7 evaluated the call)
--FILE--
<?php
const TEST = strlen("hello");
var_dump(TEST);
?>
--EXPECTF--
%AConstant expression contains invalid operations%A
--CLEAN--
<?php
