--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a label declared twice in the same scope is a compile-time fatal, as php says
--FILE--
<?php
goto again;
again:
echo "first;";
again:
echo "second;";
?>
--EXPECTF--
%ALabel 'again' already defined in %s on line 5%A
--CLEAN--
<?php
