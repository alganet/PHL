--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class using trait with abstract method without implementing it
--FILE--
<?php
trait T {
    abstract public function doWork();
}
class C {
    use T;
}
?>
--EXPECTF--
%s Fatal error:  Class C contains 1 abstract method and must therefore be declared abstract or implement the remaining %s (C::doWork) %s
--CLEAN--
<?php
