--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A diagnostic names the two types `iterable` stands for, where Reflection keeps the name
--FILE--
<?php
class HtiHolder {
    public iterable $plain;
    public ?iterable $nullable;
    public iterable|int $compound;
}
function hti_plain(iterable $x) {}
function hti_nullable(?iterable $x) {}
function hti_ret(): iterable { return 1; }

$o = new HtiHolder;
foreach (['plain', 'nullable', 'compound'] as $p) {
    try { $o->$p = new stdClass; } catch (Throwable $t) { echo $t->getMessage(), "\n"; }
    echo 'reflection: ', (new ReflectionProperty('HtiHolder', $p))->getType(), "\n";
}
try { hti_plain(1); } catch (Throwable $t) { echo $t->getMessage(), "\n"; }
try { hti_nullable(1); } catch (Throwable $t) { echo $t->getMessage(), "\n"; }
try { hti_ret(); } catch (Throwable $t) { echo $t->getMessage(), "\n"; }
?>
--EXPECTF--
Cannot assign stdClass to property HtiHolder::$plain of type Traversable|array
reflection: iterable
Cannot assign stdClass to property HtiHolder::$nullable of type Traversable|array|null
reflection: ?iterable
Cannot assign stdClass to property HtiHolder::$compound of type Traversable|array|int
reflection: Traversable|array|int
hti_plain(): Argument #1 ($x) must be of type Traversable|array, int given, called in %s on line %d
hti_nullable(): Argument #1 ($x) must be of type Traversable|array|null, int given, called in %s on line %d
hti_ret(): Return value must be of type Traversable|array, int returned
--CLEAN--
<?php
