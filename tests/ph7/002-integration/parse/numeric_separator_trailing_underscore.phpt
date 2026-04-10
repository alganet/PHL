--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal with trailing underscore is a parse error
--FILE--
<?php
$x = 1_;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected identifier "_" %s
--CLEAN--
<?php
