--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: callable as a union alternative is a fatal
--FILE--
<?php
class CppCallUnion {
    public function __construct(public int|callable $x) {}
}
?>
--EXPECTF--
%s Fatal error:  Property CppCallUnion::$x cannot have type callable|int %s
--CLEAN--
<?php
