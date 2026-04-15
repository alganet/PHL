--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: variadic promoted parameter is a fatal
--FILE--
<?php
class CppVar {
    public function __construct(public int ...$xs) {}
}
?>
--EXPECTF--
%s Fatal error:  Cannot declare variadic promoted property %s
--CLEAN--
<?php
