--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: promoted and explicit property with same name is a fatal
--FILE--
<?php
class CppDup {
    public int $x = 0;
    public function __construct(public int $x) {}
}
?>
--EXPECTF--
%s Fatal error:  Cannot redeclare CppDup::$x %s
--CLEAN--
<?php
