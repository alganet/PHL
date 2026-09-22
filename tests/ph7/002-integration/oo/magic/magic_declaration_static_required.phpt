--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The two magic methods php reaches without an object must be declared `static`
--DESCRIPTION--
The inverse of the rule next door, and php gives it its own message.
`__callStatic` is reached through a class name and nothing else, and
`__set_state` is called by the code `var_export()` writes — neither has an
instance to bind, so php requires the `static` keyword instead of forbidding it.

PHL accepted the instance form and then dispatched it unbound, which is the same
missing-`$this` hazard as the forbidden direction, arrived at from the other
side.
--FILE--
<?php
class Bad { public function __callStatic($name, $arguments) { return 1; } }
echo "not reached\n";
?>
--EXPECTF--
%AMethod Bad::__callStatic() must be static in %s on line 2%A
