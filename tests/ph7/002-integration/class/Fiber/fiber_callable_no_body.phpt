--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
POLICY DIVERGENCE §10: a fiber over a callable with no PHP body is refused (PHL half)
--DESCRIPTION--
php's fiber switches a real machine stack, so ANY callable can be its body — an
internal function, and a name it routes through `__call`/`__callStatic`. A fiber
here is an execution context over a compiled body, and neither of those has one:
an internal function is a C routine, and a magic route is the HANDLER's body run
with a packed argument array. Both are refused loudly, at start(), naming the
reason — never silently, and never by running something else. The visibility half
matters most: php does not reach a private method through a callable, it reaches
`__call` INSTEAD, so running the private body would be a hole and not a shortcut.
php's half is the `_zend` twin. Everything a fiber CAN run is in
fiber_callable_argument.phpt.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of the twin pair; php's behaviour is in the _zend twin";
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
internal: FiberError: Fiber callable is an internal function, which cannot be a fiber body here
private-array: FiberError: Fiber callable routes through __call(), which cannot be a fiber body here
private-static: FiberError: Fiber callable routes through __callStatic(), which cannot be a fiber body here
missing-array: FiberError: Fiber callable routes through __call(), which cannot be a fiber body here
missing-static: FiberError: Fiber callable routes through __callStatic(), which cannot be a fiber body here
END
