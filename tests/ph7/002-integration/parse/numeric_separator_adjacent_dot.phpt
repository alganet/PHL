--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal underscore adjacent to decimal point is a parse error
--FILE--
<?php
$x = 1_.5;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected identifier "_" %s
--CLEAN--
<?php
