--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Magic method __invoke (PH7 does not support arguments)
--FILE--
<?php
class FooInvoke {
    public function __invoke(){ echo "invoked\n"; }
}
$f = new FooInvoke();
$f();
?>
--EXPECT--
invoked
--CLEAN--
<?php
unset($f);
