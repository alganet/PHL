--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An abstract method in a class not declared abstract is a fatal (was a bare skip whose EXPECTED output was literally "Should not reach here")
--DESCRIPTION--
php words this case as "declares abstract method m()", distinct from the
"contains N abstract methods" wording it uses for an unimplemented INHERITED one.
PHL used to silently promote the class to abstract instead, which is why its
existing unimplemented-abstract check never fired here.
--FILE--
<?php
class InvalidClassSyntaxTestClass {
    abstract function abstractMethod();
}
echo "Should not reach here\n";
?>
--EXPECTF--
%AFatal error:%AClass InvalidClassSyntaxTestClass declares abstract method abstractMethod() and must therefore be declared abstract%A
--CLEAN--
<?php
