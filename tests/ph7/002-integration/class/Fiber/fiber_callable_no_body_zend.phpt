--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: a fiber over a callable with no PHP body runs (php half)
--DESCRIPTION--
The php half of fiber_callable_no_body.phpt: php's fiber switches a real machine
stack, so an internal function and a `__call`/`__callStatic`-routed name are
bodies like any other. A fiber in PHL is an execution context over a COMPILED
body, which neither of those has, so the PHL half refuses all five loudly at
start(). Note what php does with the two PRIVATE members: it does not reach them,
it reaches the magic handler instead — which is why refusing is the honest answer
there, and running the private body would not be.
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip php half of the twin pair; PHL's behaviour is in the non-_zend member";
}
?>
--FILE--
<?php
class FnbHost {
    private function priv() { return 'priv'; }
    private static function privStatic() { return 'privStatic'; }
    public function __call($n, $a) { return "call:$n"; }
    public static function __callStatic($n, $a) { return "static:$n"; }
}
$o = new FnbHost;

foreach (['internal'        => 'strtoupper',
          'private-array'   => [$o, 'priv'],
          'private-static'  => 'FnbHost::privStatic',
          'missing-array'   => [$o, 'nope'],
          'missing-static'  => 'FnbHost::nope'] as $label => $cb) {
    /* Every one of these is a valid callable, so the constructor takes it. */
    $f = new Fiber($cb);
    try { $f->start(); echo "$label: ", var_export($f->getReturn(), true), "\n"; }
    catch (Throwable $e) { echo "$label: ", get_class($e), ': ', $e->getMessage(), "\n"; }
}
echo "END\n";
?>
--EXPECT--
internal: ArgumentCountError: strtoupper() expects exactly 1 argument, 0 given
private-array: 'call:priv'
private-static: 'static:privStatic'
missing-array: 'call:nope'
missing-static: 'static:nope'
END
