--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: declare statement with invalid syntax (missing opening parenthesis)
--FILE--
<?php
declare ticks=1) {
    echo "This should not execute\n";
}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected identifier "ticks", expecting "("%A
--CLEAN--
<?php

