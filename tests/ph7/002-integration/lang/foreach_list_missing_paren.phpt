--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach (... as list) without parentheses is a syntax error expecting '(' (was a bare skip freezing PHL's invented "foreach: Expected '(' after 'list'" fatal)

--FILE--
<?php
$rows = [[1,2]];
foreach ($rows as list) {
    echo "bad\n";
}
?>
--EXPECTF--
%AParse error:%Asyntax error,%Aexpecting "("%A
--CLEAN--
<?php
