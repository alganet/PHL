--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class implementing interface without providing required methods
--FILE--
<?php
interface Greet {
    public function hello();
}
class Foo implements Greet {
}
?>
--EXPECTF--
%s Fatal error:  Class Foo contains 1 abstract method and must therefore be declared abstract or implement the remaining %s (Greet::hello) %s
--CLEAN--
<?php
