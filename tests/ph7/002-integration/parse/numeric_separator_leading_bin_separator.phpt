--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal with underscore immediately after prefix is a parse error
--FILE--
<?php
$x = 0b_10;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected identifier "b_10" %s
--CLEAN--
<?php
