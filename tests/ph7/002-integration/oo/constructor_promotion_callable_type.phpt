--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: callable type on promoted property is a fatal
--FILE--
<?php
class CppCall {
    public function __construct(public callable $cb) {}
}
?>
--EXPECTF--
%s Fatal error:  Property CppCall::$cb cannot have type callable %s
--CLEAN--
<?php
