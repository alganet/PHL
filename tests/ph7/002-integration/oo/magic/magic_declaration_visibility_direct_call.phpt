--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A non-public magic method is still denied to a call the USER wrote
--DESCRIPTION--
The boundary of the rule next door. php dispatches a non-public magic method
because the ENGINE is the caller — it is not the outside world reaching for a
private member. Written out by hand, `$o->__get('x')` from global scope IS the
outside world, and php refuses it with the ordinary
`Call to private method C::__get() from global scope`, catchable like any other.

So the two are pinned together: the same declaration answers a property read
and refuses the spelled-out call, and calling it from INSIDE the class — where
a private method is legitimately reachable — works either way. What separates
them is who built the call, which is exactly what a blanket "treat magic
methods as public" would lose.

The first-class form `$o->__get(...)` is here because it is the case that tells
the two apart the hardest: it reaches the same C dispatcher the engine's own
magic dispatch goes through, so a rule inferred from the CALL rather than set by
the caller would hand a private method — and, through it, every private property
the body can read — to global scope.
--FILE--
<?php
class Guarded
{
    private function __get($name)
    {
        return "get:$name";
    }

    public function fromInside()
    {
        return $this->__get("inside");
    }
}

$o = new Guarded();
echo $o->viaEngine, "\n";
echo $o->fromInside(), "\n";

try {
    echo $o->__get("byHand"), "\n";
} catch (Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}

try {
    $callable = $o->__get(...);
    echo $callable("byFirstClass"), "\n";
} catch (Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
echo "still running\n";
?>
--EXPECTF--
%AWarning:%AThe magic method Guarded::__get() must have public visibility in %s on line %d
%Aget:viaEngine
get:inside
Error: Call to private method Guarded::__get() from global scope
Error: Call to private method Guarded::__get() from global scope
still running
