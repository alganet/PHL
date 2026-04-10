--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal with consecutive underscores is a parse error
--FILE--
<?php
$x = 1__2;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected identifier "__2" %s
--CLEAN--
<?php
