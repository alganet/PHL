--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: missing value after label is a parse error
--FILE--
<?php
function naemvf($a) {}
naemvf(a:);
?>
--EXPECTF--
%s Parse error:  syntax error,%A
--CLEAN--
<?php
