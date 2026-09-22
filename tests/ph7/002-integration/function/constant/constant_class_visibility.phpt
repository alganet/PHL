--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
constant("C::K") honours visibility and the scope keywords, like php
--FILE--
<?php
class CvC {
    const A = 1;
    private const P = 2;
    protected const Q = 3;
    static function inside() {
        foreach (["CvC::P", "CvC::Q", "self::P", "static::A", "CvSub::P"] as $n) {
            try { var_dump(constant($n)); }
            catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
        }
    }
}
class CvSub extends CvC {
    static function inside() {
        foreach (["self::P", "self::Q", "parent::A", "parent::P", "static::Q"] as $n) {
            try { var_dump(constant($n)); }
            catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
        }
    }
}
// from outside every non-public constant is refused, and php names the class as WRITTEN
foreach (["CvC::A", "CvC::P", "CvC::Q", "CvSub::Q", "cvc::P", "\\CvC::P", "CvSub::P"] as $n) {
    try { var_dump(constant($n)); }
    catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
echo "--- inside the declaring class\n";
CvC::inside();
echo "--- inside a subclass\n";
CvSub::inside();
echo "--- no class scope\n";
foreach (["self::A", "static::A", "parent::A"] as $n) {
    try { var_dump(constant($n)); }
    catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
echo "--- misses\n";
foreach (["CvNope::A", "CvC::NOPE", "CvC::", "::A"] as $n) {
    try { var_dump(constant($n)); }
    catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
// ...and the reading side still reads: enum cases and on-demand initializers materialize
enum CvE: string { case H = 'h'; }
class CvLazy { const K = 4; const L = self::K + 5; }
var_dump(constant("CvE::H") === CvE::H, constant("CvLazy::L"));
?>
--EXPECT--
int(1)
Error: Cannot access private constant CvC::P
Error: Cannot access protected constant CvC::Q
Error: Cannot access protected constant CvSub::Q
Error: Cannot access private constant cvc::P
Error: Cannot access private constant CvC::P
Error: Undefined constant CvSub::P
--- inside the declaring class
int(2)
int(3)
int(2)
int(1)
Error: Undefined constant CvSub::P
--- inside a subclass
Error: Undefined constant self::P
int(3)
int(1)
Error: Cannot access private constant parent::P
int(3)
--- no class scope
Error: Cannot access "self" when no class scope is active
Error: Cannot access "static" when no class scope is active
Error: Cannot access "parent" when no class scope is active
--- misses
Error: Class "CvNope" not found
Error: Undefined constant CvC::NOPE
Error: Undefined constant CvC::
Error: Class "" not found
bool(true)
int(9)
--CLEAN--
<?php
