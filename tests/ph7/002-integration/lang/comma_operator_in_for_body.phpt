--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A for() body is ordinary php: the comma operator is rejected there too
--FILE--
<?php
for ($i = 0; $i < 1; $i++) {
    $q = (1, 2);
}
echo $q;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token ","%A
--CLEAN--
<?php
