--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A magic method declared with the wrong argument COUNT is a compile fatal (__get, one required)
--DESCRIPTION--
php validates a magic method's DECLARATION at compile time
(zend_check_magic_method_implementation): the ENGINE builds the arguments and
calls these methods on its own, so a shape it cannot call is rejected where it
is written. PHL accepted every shape php rejects, and then dispatched whatever
had been declared — `__get($name, $extra)` ran with `$extra` unset.

The one-argument row (`__get`/`__isset`/`__unset`/`__unserialize`/`__set_state`)
uses php's SINGULAR wording. The count is of DECLARED parameters, so an optional
one counts too: `__get($a = null)` is the accepted shape, `__get()` is not.
--FILE--
<?php
class Bad { public function __get($name, $extra) { return 1; } }
echo "not reached\n";
?>
--EXPECTF--
%AMethod Bad::__get() must take exactly 1 argument in %s on line 2%A
