--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A space after a namespace separator ends the name
--FILE--
<?php
namespace A\ B;
echo "should not reach here\n";
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected token "\", expecting "{" in %s on line 2
