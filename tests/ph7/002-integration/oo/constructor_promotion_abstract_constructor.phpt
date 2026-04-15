--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Constructor property promotion: visibility in abstract constructor is a fatal
--FILE--
<?php
abstract class CppAbs {
    abstract public function __construct(public int $x);
}
?>
--EXPECTF--
%s Fatal error:  Cannot declare promoted property in an abstract constructor %s
--CLEAN--
<?php
