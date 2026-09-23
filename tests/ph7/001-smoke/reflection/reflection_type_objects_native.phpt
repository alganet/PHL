--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionType objects are engine values: uncloneable, and a variadic has no default
--FILE--
<?php
function reflTypeObjNative(?array $a, int|string $b, Countable&Stringable $c, mixed ...$rest): ?string { return null; }

$rf = new ReflectionFunction('reflTypeObjNative');
$ps = $rf->getParameters();

// php hands these out as VALUES: `clone` is an Error, not a copy.
foreach ([$ps[0]->getType(), $ps[1]->getType(), $ps[2]->getType(), $rf->getReturnType()] as $t) {
    try { $x = clone $t; echo "cloned ", get_class($x), "\n"; }
    catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

// A nullable name keeps its '?' in the rendered text whatever its length is.
foreach ([$ps[0]->getType(), $rf->getReturnType()] as $t) {
    echo (string)$t, ' name=', $t->getName(), ' null=', var_export($t->allowsNull(), true), "\n";
}

// A variadic parameter is optional but never has a default -- in a compiled
// function and in a C builtin alike, whose parameters come from a signature
// string rather than from byte-code.
$probe = [$ps[3], (new ReflectionFunction('printf'))->getParameters()[1],
          (new ReflectionFunction('array_merge'))->getParameters()[0]];
foreach ($probe as $p) {
    echo $p->getName(), ' variadic=', var_export($p->isVariadic(), true),
         ' optional=', var_export($p->isOptional(), true),
         ' hasDefault=', var_export($p->isDefaultValueAvailable(), true), "\n";
}

// The parameter list of a C builtin is the one its signature declares.
$sub = new ReflectionFunction('substr');
echo $sub->getNumberOfParameters(), ' ', $sub->getNumberOfRequiredParameters(), "\n";
foreach ($sub->getParameters() as $p) {
    echo '  ', $p->getType(), ' $', $p->getName(),
         $p->isDefaultValueAvailable() ? ' = ' . var_export($p->getDefaultValue(), true) : '', "\n";
}
--EXPECT--
Error: Trying to clone an uncloneable object of class ReflectionNamedType
Error: Trying to clone an uncloneable object of class ReflectionUnionType
Error: Trying to clone an uncloneable object of class ReflectionIntersectionType
Error: Trying to clone an uncloneable object of class ReflectionNamedType
?array name=array null=true
?string name=string null=true
rest variadic=true optional=true hasDefault=false
values variadic=true optional=true hasDefault=false
arrays variadic=true optional=true hasDefault=false
3 2
  string $string
  int $offset
  ?int $length = NULL
