--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
In a final class, `self` satisfies an inherited `static` return type
--DESCRIPTION--
php's one exception to "only static is under static": a final class cannot be
extended, so the called class can only ever be that class and `self` IS
`static` there. A library writes it (respect/fluent's NamespaceLookup), and
PHL refused the whole file. Naming any OTHER class is still a fatal, and a
class that is not final still cannot narrow.
--FILE--
<?php
interface VsfMaker { public function make(): static; }

final class VsfFinal implements VsfMaker
{
    public function make(): self { return $this; }
}

final readonly class VsfReadonly implements VsfMaker
{
    public function make(): VsfReadonly { return $this; }
}

abstract class VsfBase { abstract public function build(): static; }
final class VsfChild extends VsfBase
{
    public function build(): self { return $this; }
}

interface VsfUnion { public function pick(): static|int; }
final class VsfPicker implements VsfUnion
{
    public function pick(): self|int { return $this; }
}

interface VsfNullable { public function maybe(): ?static; }
final class VsfMaybe implements VsfNullable
{
    public function maybe(): ?self { return $this; }
}

class VsfStaticParent { public static function of(): static { return new static(); } }
final class VsfStaticChild extends VsfStaticParent
{
    public static function of(): self { return new self(); }
}

/* An enum carries the final flag, so its own name works the same way. */
interface VsfEnumMaker { public function same(): static; }
enum VsfEnum implements VsfEnumMaker
{
    case One;
    public function same(): self { return $this; }
}

var_dump((new VsfFinal())->make() instanceof VsfFinal);
var_dump((new VsfReadonly())->make() instanceof VsfReadonly);
var_dump((new VsfChild())->build() instanceof VsfChild);
var_dump((new VsfPicker())->pick() instanceof VsfPicker);
var_dump((new VsfMaybe())->maybe() instanceof VsfMaybe);
var_dump(VsfStaticChild::of() instanceof VsfStaticChild);
var_dump(VsfEnum::One->same() === VsfEnum::One);

/* And what the exception does NOT cover, refused at compile time either way. */
foreach ([
    'a class that is not final' =>
        'interface VsfI2 { public function m(): static; } class VsfPlain implements VsfI2 { public function m(): self { return $this; } }',
    'a final class naming its PARENT' =>
        'class VsfP3 { public function m(): static { return $this; } } final class VsfC3 extends VsfP3 { public function m(): VsfP3 { return $this; } }',
    'a nullable self under a bare static' =>
        'interface VsfI4 { public function m(): static; } final class VsfC4 implements VsfI4 { public function m(): ?self { return $this; } }',
] as $label => $code) {
    $out = shell_exec(PHP_BINARY . ' -r ' . escapeshellarg($code) . ' 2>&1');
    printf("%-36s %s\n", $label, preg_match('/must be compatible with/', (string) $out) ? 'refused' : 'ACCEPTED');
}
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
a class that is not final            refused
a final class naming its PARENT      refused
a nullable self under a bare static  refused
