--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class-name callable reaches __call on the caller's $this, and the direct form refuses it
--DESCRIPTION--
php's get_static_method_fallback and the fcc->object half of
zend_is_callable_check_func are one rule: a method named through a CLASS that
the class cannot answer directly resolves to __call on the CALLER's own $this
whenever that receiver is an instance of the class. The `::` SYNTAX half of this
shipped first; the CALLABLE half is here, and it comes with php's
direct-vs-callback asymmetry. A callback (call_user_func, array_map) binds the
receiver and dispatches; the DIRECT `$cb = ['C','m']; $cb()` spelling carries no
object, so the trampoline it resolved to — a NON-static function — is refused
with php's staticness Error, naming the class the callable wrote and the name as
written, even when that name is a declared static method. With no compatible
$this in scope (an unrelated class, a static method, the top level) every
spelling answers __callStatic as before.
--FILE--
<?php
class CcwtBoth {
    private function ccwtPriv() { return 'real'; }
    private static function ccwtPrivStatic() { return 'realStatic'; }
    public function __call($n, $a) { return "__call($n) on " . get_class($this); }
    public static function __callStatic($n, $a) { return "__callStatic($n)"; }
}
class CcwtOnlyStatic {
    public static function __callStatic($n, $a) { return "onlyStatic($n)"; }
}
function ccwtShow($label, $fn) {
    /* Build the whole line before echoing it: `echo $label, $fn()` would have printed the
     * label already when $fn() throws. */
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', $out, "\n";
}
class CcwtInside extends CcwtBoth {
    public function probe($what) {
        ccwtShow("$what direct-arr", function () use ($what) { $cb = ['CcwtBoth', $what]; return $cb(); });
        ccwtShow("$what direct-str", function () use ($what) { $s = "CcwtBoth::$what"; return $s(); });
        echo "$what cuf-arr => ", call_user_func(['CcwtBoth', $what]), "\n";
        echo "$what cuf-str => ", call_user_func("CcwtBoth::$what"), "\n";
        echo "$what cufa => ", call_user_func_array(['CcwtBoth', $what], [1]), "\n";
        echo "$what map => ", implode('', array_map(['CcwtBoth', $what], [1])), "\n";
        echo "$what is_callable => ", var_export(is_callable(['CcwtBoth', $what]), true), "\n";
    }
    public function noCall() {
        echo 'onlyStatic cuf => ', call_user_func(['CcwtOnlyStatic', 'zz']), "\n";
        ccwtShow('onlyStatic direct', function () { $cb = ['CcwtOnlyStatic', 'zz']; return $cb(); });
    }
}
class CcwtAlien {
    public function probe($what) {
        echo "alien $what cuf-arr => ", call_user_func(['CcwtBoth', $what]), "\n";
        ccwtShow("alien $what direct-arr", function () use ($what) { $cb = ['CcwtBoth', $what]; return $cb(); });
    }
}
$in = new CcwtInside;
/* Missing, inaccessible instance, inaccessible static — the trampoline replaces all three. */
$in->probe('ccwtMissing');
$in->probe('ccwtPriv');
$in->probe('ccwtPrivStatic');
$in->noCall();
/* No compatible $this: the __callStatic answers stand. */
(new CcwtAlien)->probe('ccwtMissing');
echo 'top cuf => ', call_user_func(['CcwtBoth', 'ccwtMissing']), "\n";
$cb = ['CcwtBoth', 'ccwtMissing'];
echo 'top direct => ', $cb(), "\n";
echo "end\n";
?>
--EXPECT--
ccwtMissing direct-arr => Error: Non-static method CcwtBoth::ccwtMissing() cannot be called statically
ccwtMissing direct-str => Error: Non-static method CcwtBoth::ccwtMissing() cannot be called statically
ccwtMissing cuf-arr => __call(ccwtMissing) on CcwtInside
ccwtMissing cuf-str => __call(ccwtMissing) on CcwtInside
ccwtMissing cufa => __call(ccwtMissing) on CcwtInside
ccwtMissing map => __call(ccwtMissing) on CcwtInside
ccwtMissing is_callable => true
ccwtPriv direct-arr => Error: Non-static method CcwtBoth::ccwtPriv() cannot be called statically
ccwtPriv direct-str => Error: Non-static method CcwtBoth::ccwtPriv() cannot be called statically
ccwtPriv cuf-arr => __call(ccwtPriv) on CcwtInside
ccwtPriv cuf-str => __call(ccwtPriv) on CcwtInside
ccwtPriv cufa => __call(ccwtPriv) on CcwtInside
ccwtPriv map => __call(ccwtPriv) on CcwtInside
ccwtPriv is_callable => true
ccwtPrivStatic direct-arr => Error: Non-static method CcwtBoth::ccwtPrivStatic() cannot be called statically
ccwtPrivStatic direct-str => Error: Non-static method CcwtBoth::ccwtPrivStatic() cannot be called statically
ccwtPrivStatic cuf-arr => __call(ccwtPrivStatic) on CcwtInside
ccwtPrivStatic cuf-str => __call(ccwtPrivStatic) on CcwtInside
ccwtPrivStatic cufa => __call(ccwtPrivStatic) on CcwtInside
ccwtPrivStatic map => __call(ccwtPrivStatic) on CcwtInside
ccwtPrivStatic is_callable => true
onlyStatic cuf => onlyStatic(zz)
onlyStatic direct => 'onlyStatic(zz)'
alien ccwtMissing cuf-arr => __callStatic(ccwtMissing)
alien ccwtMissing direct-arr => '__callStatic(ccwtMissing)'
top cuf => __callStatic(ccwtMissing)
top direct => __callStatic(ccwtMissing)
end
