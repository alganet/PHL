--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`final private` is a WARNING at the declaration, and the child may reuse the name
--DESCRIPTION--
A private method is never overridden, so `final` on one says nothing: php warns
`Private methods cannot be final as they are never overridden by other classes`
where it is written (php 8.0+) and compiles the class. PHL was silent there and
then FATALED at the subclass that declared the same name
("Cannot override final method A::m()"), a class php accepts and runs — the two
methods are independent members, each private to the class that declares it, and
each dispatched from its own declaring scope.
--FILE--
<?php
class FinPrivBase
{
    final
    private
    function fpm() { return 'base'; }
    public function callBase() { return $this->fpm(); }
}
class FinPrivKid extends FinPrivBase
{
    private function fpm() { return 'child'; }
    public function callKid() { return $this->fpm(); }
}
$k = new FinPrivKid;
echo $k->callBase(), ' ', $k->callKid(), "\n";
try { $k->fpm(); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
?>
--EXPECTF--
%AWarning:%APrivate methods cannot be final as they are never overridden by other classes in %s on line %d
base child
Error: Call to private method FinPrivKid::fpm() from global scope
--CLEAN--
<?php
