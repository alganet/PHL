--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static property cannot answer an interface property
--DESCRIPTION--
Interface properties are instance properties; a static one of the same name
is a redeclaration php refuses before it would report the missing hook.
--FILE--
<?php
interface PimStI { public int $p { get; } }
class PimStC implements PimStI { public static int $p = 0; }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Cannot redeclare non static PimStI::$p as static PimStC::$p %s
