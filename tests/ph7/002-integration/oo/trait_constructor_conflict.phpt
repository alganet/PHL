--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class constructor takes precedence over trait constructor
--FILE--
<?php
trait HasInit {
    public function __construct() { echo "trait\n"; }
}
class Foo {
    use HasInit;
    public function __construct() { echo "class\n"; }
}
$f = new Foo();
?>
--EXPECT--
class
--CLEAN--
<?php
