--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
empty() with no operand is a parse error on the ')' (was a bare skip asserting PH7's empty() === true)
--FILE--
<?php
if (empty() === true) {
    echo "true";
}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token ")"%A
--CLEAN--
<?php
