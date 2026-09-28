--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An enum case backing value is a constant expression: a call in one is a compile-time fatal
--FILE--
<?php
enum CesEnum: int {
    case A = strlen('ab');
}
echo "unreachable\n";
?>
--EXPECTF--
%AConstant expression contains invalid operations%A
--CLEAN--
<?php
