--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: explicit property after promoted with same name is a fatal
--FILE--
<?php
class CppDupAfter {
    public function __construct(public int $x) {}
    public int $x = 0;
}
?>
--EXPECTF--
%s Fatal error:  Cannot redeclare CppDupAfter::$x %s
--CLEAN--
<?php
