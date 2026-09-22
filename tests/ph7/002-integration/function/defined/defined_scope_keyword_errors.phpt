--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
defined() refuses a scope keyword it cannot resolve, with php's two distinct Errors
--FILE--
<?php
foreach (["self::A", "static::A", "parent::A"] as $n) {
    try { var_dump(defined($n)); }
    catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
// a class scope IS active here, it just has no parent — php says so differently
class DskNoParent {
    const A = 1;
    static function t() {
        try { var_dump(defined("parent::A")); }
        catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
    }
}
DskNoParent::t();
// a plain function body is not a class scope either
function dskFn() {
    try { var_dump(defined("self::A")); }
    catch (\Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
dskFn();
// ...and the Error is catchable, so execution carries on
echo "end\n";
?>
--EXPECT--
Error: Cannot access "self" when no class scope is active
Error: Cannot access "static" when no class scope is active
Error: Cannot access "parent" when no class scope is active
Error: Cannot access "parent" when current class scope has no parent
Error: Cannot access "self" when no class scope is active
end
--CLEAN--
<?php
