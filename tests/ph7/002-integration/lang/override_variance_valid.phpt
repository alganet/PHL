--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Valid method overrides (covariant return, contravariant param, optional adds, …) compile clean
--FILE--
<?php
class OvAnimal {}
class OvDog extends OvAnimal {}
class OvP {
    public function same(int $a): int { return $a; }
    public function covRet(): OvAnimal { return new OvAnimal; }
    public function contraParam(OvDog $d): void {}
    public function widenParam(int $a): void {}
    public function addOptional(int $a): void {}
    public function dropOptionalKeepReq(int $a): void {}
    public function unionRet(): int|string { return 1; }
    public function selfRet(): self { return $this; }
    public function untypedRet() { return 1; }
    public function nullableParam(?int $a): void {}
    public function __construct(int $x) {}
}
class OvC extends OvP {
    public function same(int $a): int { return $a; }                  // identical
    public function covRet(): OvDog { return new OvDog; }             // covariant return
    public function contraParam(OvAnimal $d): void {}                 // contravariant param widen
    public function widenParam($a): void {}                           // type -> no type (param widen)
    public function addOptional(int $a, string $b = ""): void {}      // added optional param
    public function dropOptionalKeepReq(int $a): void {}              // unchanged
    public function unionRet(): int { return 1; }                     // union -> member (skipped)
    public function selfRet(): static { return $this; }              // self -> static (skipped)
    public function untypedRet(): int { return 1; }                  // no type -> type (return narrow)
    public function nullableParam(?int $a): void {}                  // unchanged
    public function __construct() {}                                 // __construct exempt
}
$c = new OvC();
echo $c->same(5), " ", get_class($c->covRet()), "\n";
echo "ok\n";
?>
--EXPECT--
5 OvDog
ok
--CLEAN--
<?php
