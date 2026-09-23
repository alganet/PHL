--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A first-class callable over a non-callable value raises the call's own Error
--FILE--
<?php
class FvncTarget {
    public function pub() {}
    private function priv() {}
    public static function stat() {}
}
$obj = new FvncTarget();
$cases = [
    'int' => 5,
    'float' => 1.5,
    'bool' => true,
    'null' => null,
    'array-3' => [1, 2, 3],
    'array-0' => [],
    'array-bad-first' => [1, 2],
    'array-bad-keys' => ['a' => 'FvncTarget', 'b' => 'pub'],
    'undefined-function' => 'fvncNoSuchFunction',
    'undefined-method' => 'FvncTarget::nope',
    'undefined-class' => 'FvncNoSuchClass::m',
    'plain-object' => new stdClass(),
];
foreach ($cases as $label => $value) {
    /* `($v)(...)` raises exactly what `($v)()` raises: the ellipsis defers the CALL, it
     * does not make a bad callable acceptable. */
    try {
        $f = ($value)(...);
        echo $label, "-fcc:no error\n";
    } catch (Throwable $e) {
        echo $label, "-fcc:", get_class($e), ": ", $e->getMessage(), "\n";
    }
    try {
        ($value)();
        echo $label, "-call:no error\n";
    } catch (Throwable $e) {
        echo $label, "-call:", get_class($e), ": ", $e->getMessage(), "\n";
    }
}
/* A method the calling scope cannot reach is refused where the callable is BUILT. */
$pair = [$obj, 'priv'];
try {
    $f = ($pair)(...);
    echo "private-pair:no error\n";
} catch (Throwable $e) {
    echo "private-pair:", get_class($e), ": ", $e->getMessage(), "\n";
}
$missing = [$obj, 'nope'];
try {
    $f = ($missing)(...);
    echo "missing-pair:no error\n";
} catch (Throwable $e) {
    echo "missing-pair:", get_class($e), ": ", $e->getMessage(), "\n";
}
/* ...and everything that IS callable still wraps. */
$ok = ['strtoupper', [$obj, 'pub'], 'FvncTarget::stat', fn() => 1];
foreach ($ok as $i => $value) {
    echo "callable$i:", var_export(($value)(...) instanceof Closure, true), "\n";
}
?>
--EXPECT--
int-fcc:Error: Value of type int is not callable
int-call:Error: Value of type int is not callable
float-fcc:Error: Value of type float is not callable
float-call:Error: Value of type float is not callable
bool-fcc:Error: Value of type bool is not callable
bool-call:Error: Value of type bool is not callable
null-fcc:Error: Value of type null is not callable
null-call:Error: Value of type null is not callable
array-3-fcc:Error: Array callback must have exactly two elements
array-3-call:Error: Array callback must have exactly two elements
array-0-fcc:Error: Array callback must have exactly two elements
array-0-call:Error: Array callback must have exactly two elements
array-bad-first-fcc:Error: First array member is not a valid class name or object
array-bad-first-call:Error: First array member is not a valid class name or object
array-bad-keys-fcc:Error: Array callback has to contain indices 0 and 1
array-bad-keys-call:Error: Array callback has to contain indices 0 and 1
undefined-function-fcc:Error: Call to undefined function fvncNoSuchFunction()
undefined-function-call:Error: Call to undefined function fvncNoSuchFunction()
undefined-method-fcc:Error: Call to undefined method FvncTarget::nope()
undefined-method-call:Error: Call to undefined method FvncTarget::nope()
undefined-class-fcc:Error: Class "FvncNoSuchClass" not found
undefined-class-call:Error: Class "FvncNoSuchClass" not found
plain-object-fcc:Error: Object of type stdClass is not callable
plain-object-call:Error: Object of type stdClass is not callable
private-pair:Error: Call to private method FvncTarget::priv() from global scope
missing-pair:Error: Call to undefined method FvncTarget::nope()
callable0:true
callable1:true
callable2:true
callable3:true
--CLEAN--
<?php
