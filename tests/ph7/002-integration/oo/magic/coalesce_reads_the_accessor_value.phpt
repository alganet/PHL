--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`$o->p ?? $default` takes the ACCESSOR's value, not the truth of __isset()/the hook
--DESCRIPTION--
php has three levels of property access, not two. A plain read calls `__get`.
`isset()` calls `__isset` and stops at its truth. `??` sits between them: it is
SILENT on a miss like `isset()`, but the expression takes the property's VALUE —
`__isset` only GATES the access and `__get` answers it, and a class with no
`__isset` at all is read straight through `__get`.

PHL compiled the member left of `??` as isset() context, so every accessor path
handed the coalesce a BOOLEAN: `$config->timeout ?? 30` answered `bool(true)`,
a value that appears nowhere in the program, and the default was never reached
because `true` is not null. With no `__isset` declared it went the other way and
always took the default, never calling `__get` at all. Property HOOKS had the
same two answers for the same reason. `$o->p ?? $d` is how optional data is read
from a model or a config object, so both were silent wrong answers on ordinary
code — while `isset()`, `empty()` and `??=` on the same property were already
php-exact, which is what made it look deliberate.

The last row is the neighbour that fell out of the same encoding: `isset()` on a
hooked property returning null answered TRUE, because the branch stored
`bool(false)` where the trailing `isset` builtin tests NULL-ness.
--FILE--
<?php
class Gated
{
    public function __get($name)
    {
        echo "  get($name)\n";
        return $name === "nothing" ? null : "value:$name";
    }

    public function __isset($name)
    {
        echo "  isset($name)\n";
        return $name !== "absent";
    }
}

class Ungated
{
    public function __get($name)
    {
        echo "  get($name)\n";
        return "ungated:$name";
    }
}

class Hooked
{
    public $shown { get { return "hooked"; } }
    public $empty { get { return null; } }
}

echo "gated, __isset true:\n";
var_dump((new Gated())->present ?? "default");
echo "gated, __isset false:\n";
var_dump((new Gated())->absent ?? "default");
echo "gated, __get null:\n";
var_dump((new Gated())->nothing ?? "default");
echo "no __isset at all:\n";
var_dump((new Ungated())->anything ?? "default");

echo "chained through __get:\n";
class Chain
{
    public function __get($name) { return $name === "leaf" ? "end" : new Chain(); }
    public function __isset($name) { return true; }
}
var_dump((new Chain())->a->b->leaf ?? "default");

echo "hooks:\n";
$h = new Hooked();
var_dump($h->shown ?? "default");
var_dump($h->empty ?? "default");

echo "the other two levels are unchanged:\n";
$g = new Gated();
var_dump(isset($g->present), isset($g->absent));
var_dump(empty($g->nothing), empty($g->present));
var_dump(isset($h->shown), isset($h->empty), empty($h->empty));
?>
--EXPECT--
gated, __isset true:
  isset(present)
  get(present)
string(13) "value:present"
gated, __isset false:
  isset(absent)
string(7) "default"
gated, __get null:
  isset(nothing)
  get(nothing)
string(7) "default"
no __isset at all:
  get(anything)
string(16) "ungated:anything"
chained through __get:
string(3) "end"
hooks:
string(6) "hooked"
string(7) "default"
the other two levels are unchanged:
  isset(present)
  isset(absent)
bool(true)
bool(false)
  isset(nothing)
  get(nothing)
  isset(present)
  get(present)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
