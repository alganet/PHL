--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An arrow function in a constant expression is always rejected -- there is no static-fn escape ("Constant expression contains invalid operations")
--FILE--
<?php
const X = static fn () => 1;
echo "unreachable\n";
?>
--EXPECTF--
%AConstant expression contains invalid operations%A
--CLEAN--
<?php
