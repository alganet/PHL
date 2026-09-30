--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`$o(...)` on an __invoke object is a PHP->PHP call: it recurses past the native-nesting cap, and a Fiber can suspend inside it
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '8.1.0', '<')) echo 'skip Requires PHP 8.1+'; ?>
--FILE--
<?php
/* An invokable object dispatched through a synthetic OP_CALL burned one C
 * activation per call: recursion died at the 256-frame native cap, and a
 * Fiber::suspend() inside it was refused as an internal call boundary. Both
 * are the same fact -- `$o(...)` is `$o->__invoke(...)`, an ordinary PHP call. */

class IisDepth
{
    public int $n = 0;
    public function __invoke(): int
    {
        if (++$this->n >= 1000) {
            return $this->n;
        }
        return ($this)();
    }
}
$iisD = new IisDepth();
echo "depth: ", $iisD(), "\n";

/* The same one level down, so the deep frames sit under a nested activation. */
class IisDepthArg
{
    public function __invoke(int $k): int
    {
        return $k <= 0 ? 0 : 1 + $this($k - 1);
    }
}
$iisA = new IisDepthArg();
echo "depth via argument: ", $iisA(900), "\n";

/* The suspend boundaries a call SITE can spell. php takes all four; the ones
 * that go through a C callback (call_user_func, array_map) are PHL's recorded
 * fiber residual and live in the 002-integration twin pair. */
function iisRun(string $label, Closure $body): void
{
    $f = new Fiber($body);
    try {
        $f->start();
        if ($f->isSuspended()) {
            $f->resume('R');
        }
        echo $label, ": ", var_export($f->getReturn(), true), "\n";
    } catch (Throwable $e) {
        echo $label, ": ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

iisRun('direct', function () { return Fiber::suspend('a'); });

iisRun('closure', function () {
    $inner = function () { return Fiber::suspend('b'); };
    return $inner();
});

iisRun('first-class callable', function () {
    $inner = function () { return Fiber::suspend('c'); };
    $g = $inner(...);
    return $g();
});

class IisSuspend
{
    public function __invoke()
    {
        return Fiber::suspend('d');
    }
}
iisRun('__invoke', function () { $o = new IisSuspend(); return $o(); });

/* An __invoke reached from inside a fiber, one PHP call deeper still. */
class IisOuter
{
    public function __invoke()
    {
        $inner = new IisSuspend();
        return $inner();
    }
}
iisRun('__invoke in __invoke', function () { $o = new IisOuter(); return $o(); });

/* Not an invokable: the dispatch is unchanged, and still catchable. */
class IisPlain {}
try {
    $iisP = new IisPlain();
    $iisP(1, 2);
} catch (Error $e) {
    echo "not callable: ", $e->getMessage(), "\n";
}
try {
    (new IisPlain())(...[1, 2]);
} catch (Error $e) {
    echo "not callable (unpacked): ", $e->getMessage(), "\n";
}
?>
--EXPECT--
depth: 1000
depth via argument: 900
direct: 'R'
closure: 'R'
first-class callable: 'R'
__invoke: 'R'
__invoke in __invoke: 'R'
not callable: Object of type IisPlain is not callable
not callable (unpacked): Object of type IisPlain is not callable
--CLEAN--
<?php
