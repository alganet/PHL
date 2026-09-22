--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-public magic method is a compile WARNING and is then dispatched anyway
--DESCRIPTION--
The one rule of the magic-method declaration family that is not an acceptance
gap: it changes what a program php RUNS does. php warns
`The magic method C::__get() must have public visibility` at the declaration and
then calls the method regardless — the engine reaching for `__get` is not the
outside world reaching for a private member. PHL said nothing at the declaration
and threw `Call to private method C::__get() from global scope` at the ACCESS,
which killed the script.

Every row php requires public is exercised here through the surface that
DISPATCHES it, because the fix is at the dispatch, not at the declaration:
property access, `isset`/`unset`, the two catch-alls, `$o()` directly, through a
callback argument and through the Closure `fromCallable()` wraps an object in,
`var_dump`, and both serialization protocols.
Inheritance and traits are included because the warning belongs to the
DECLARING class — a base class or a trait warns once, where it is written, not
once per subclass or per `use`.

`__construct`, `__destruct` and `__clone` are deliberately NOT in this set: a
private constructor is the singleton idiom, and php enforces those three at the
call like any other method.
--FILE--
<?php
class Access
{
    public $kept = 1;

    private function __get($name)
    {
        return "get:$name";
    }

    protected function __set($name, $value)
    {
        echo "set:$name=$value\n";
    }

    private function __isset($name)
    {
        return $name === "there";
    }

    private function __unset($name)
    {
        echo "unset:$name\n";
    }

    private function __call($name, $arguments)
    {
        return "call:$name/" . count($arguments);
    }

    private static function __callStatic($name, $arguments)
    {
        return "callStatic:$name";
    }

    private function __debugInfo()
    {
        return ["shown" => "by debugInfo"];
    }
}

class Invokable
{
    protected function __invoke($n)
    {
        return $n * 2;
    }
}

class Sleeper
{
    public $p = "kept";

    private function __sleep()
    {
        return ["p"];
    }

    private function __wakeup()
    {
        echo "woke\n";
    }
}

class Serializer
{
    private function __serialize(): array
    {
        return ["a" => 1];
    }

    private function __unserialize(array $data): void
    {
        echo "unserialized:" . count($data) . "\n";
    }
}

class Base
{
    private function __get($name)
    {
        return "base:$name";
    }
}

class Derived extends Base
{
}

trait Hidden
{
    private function __get($name)
    {
        return "trait:$name";
    }
}

class UsesTrait
{
    use Hidden;
}

$o = new Access();
echo $o->missing, "\n";
$o->missing = "v";
var_dump(isset($o->there), isset($o->elsewhere));
unset($o->gone);
echo $o->absent(1, 2, 3), "\n";
echo Access::absentStatic(), "\n";
var_dump($o);

$i = new Invokable();
echo $i(21), "\n";
echo implode(",", array_map($i, [1, 2, 3])), "\n";
echo Closure::fromCallable($i)(50), "\n";

echo serialize(new Sleeper()), "\n";
$woken = unserialize(serialize(new Sleeper()));
echo $woken->p, "\n";

$s = serialize(new Serializer());
echo $s, "\n";
unserialize($s);

echo (new Derived())->x, "\n";
echo (new UsesTrait())->y, "\n";
?>
--EXPECTF--
%AWarning:%AThe magic method Access::__get() must have public visibility in %s on line %d
%AWarning:%AThe magic method Access::__set() must have public visibility in %s on line %d
%AWarning:%AThe magic method Access::__isset() must have public visibility in %s on line %d
%AWarning:%AThe magic method Access::__unset() must have public visibility in %s on line %d
%AWarning:%AThe magic method Access::__call() must have public visibility in %s on line %d
%AWarning:%AThe magic method Access::__callStatic() must have public visibility in %s on line %d
%AWarning:%AThe magic method Access::__debugInfo() must have public visibility in %s on line %d
%AWarning:%AThe magic method Invokable::__invoke() must have public visibility in %s on line %d
%AWarning:%AThe magic method Sleeper::__sleep() must have public visibility in %s on line %d
%AWarning:%AThe magic method Sleeper::__wakeup() must have public visibility in %s on line %d
%AWarning:%AThe magic method Serializer::__serialize() must have public visibility in %s on line %d
%AWarning:%AThe magic method Serializer::__unserialize() must have public visibility in %s on line %d
%AWarning:%AThe magic method Base::__get() must have public visibility in %s on line %d
%AWarning:%AThe magic method Hidden::__get() must have public visibility in %s on line %d
%Aget:missing
set:missing=v
bool(true)
bool(false)
unset:gone
call:absent/3
callStatic:absentStatic
object(Access)#%d (1) {
  ["shown"]=>
  string(12) "by debugInfo"
}
42
2,4,6
100
O:7:"Sleeper":1:{s:1:"p";s:4:"kept";}
woke
kept
O:10:"Serializer":1:{s:1:"a";i:1;}
unserialized:1
base:x
trait:y
