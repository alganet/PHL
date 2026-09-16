--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a do block with no while is a syntax error expecting "while" (was a bare skip freezing PHL's invented "Missing 'while' statement after 'do' block" fatal)

--DESCRIPTION--
Both engines raise a syntax error expecting "while"; they name a different
offending token because php treats the closing `?>` as an implicit statement
terminator (so it names ";") while PHL reads it as end of file. A recorded
divergence; the expectation matches what both agree on.
--FILE--
<?php
do {
    echo "loop\n";
}
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected%A expecting "while"%A
--CLEAN--
<?php

