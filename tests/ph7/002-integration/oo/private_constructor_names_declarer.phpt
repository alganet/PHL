--TEST--
A refused constructor names its declaring class, not the class instantiated
--FILE--
<?php
function t(string $l, callable $f) {
    try { $f(); echo "$l: no error\n"; }
    catch (Throwable $e) { echo "$l: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}

/* Inherited private constructor: php names the DECLARER, not the class asked for. */
class Base { private function __construct() {} }
class Mid extends Base {}
class Leaf extends Mid {}
t('own', fn() => new Base());
t('child', fn() => new Mid());
t('grandchild', fn() => new Leaf());

/* A child that redeclares the constructor is itself the declarer. */
class WideBase { public function __construct() {} }
class NarrowChild extends WideBase { private function __construct() {} }
t('redeclared', fn() => new NarrowChild());

/* Protected reads the same way. */
class ProtBase { protected function __construct() {} }
class ProtChild extends ProtBase {}
t('protected', fn() => new ProtChild());

/* A dynamic class name resolves to the same declarer. */
t('dynamic', function () { $c = 'Leaf'; new $c(); });

/* The calling scope is named beside it, and does not change the declarer. */
class Outsider { public static function go() { new Leaf(); } }
t('from-scope', fn() => Outsider::go());

/* A trait is composed INTO the using class, so php names that class, never the trait. */
trait Ctor { private function __construct() {} }
class User { use Ctor; }
class UserChild extends User {}
t('trait-user', fn() => new User());
t('trait-child', fn() => new UserChild());

/* Re-using the trait in the child makes the child the composing class. */
trait Ctor2 { private function __construct() {} }
class ReBase { use Ctor2; }
class ReChild extends ReBase { use Ctor2; }
t('trait-recomposed', fn() => new ReChild());

/* Widening in the child is allowed and constructs. */
class HiddenBase { private function __construct() {} }
class OpenChild extends HiddenBase { public function __construct() {} }
t('widened', fn() => new OpenChild());

/* A factory inside the declaring class still reaches it through a subclass. */
class FactBase {
    private function __construct() {}
    public static function make(): static { return new static(); }
}
class FactChild extends FactBase {}
t('factory', fn() => FactChild::make());
--EXPECT--
own: Error: Call to private Base::__construct() from global scope
child: Error: Call to private Base::__construct() from global scope
grandchild: Error: Call to private Base::__construct() from global scope
redeclared: Error: Call to private NarrowChild::__construct() from global scope
protected: Error: Call to protected ProtBase::__construct() from global scope
dynamic: Error: Call to private Base::__construct() from global scope
from-scope: Error: Call to private Base::__construct() from scope Outsider
trait-user: Error: Call to private User::__construct() from global scope
trait-child: Error: Call to private User::__construct() from global scope
trait-recomposed: Error: Call to private ReChild::__construct() from global scope
widened: no error
factory: no error
