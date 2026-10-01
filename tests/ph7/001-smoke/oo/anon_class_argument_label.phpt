--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Anonymous class: the Argument #N family loses the method name with the NUL
--FILE--
<?php
/* php assembles `Class::method` into ONE string for this family and prints it with
 * `%s`, so an anonymous owner's NUL takes the `::method` with it: every method of
 * the class reports the same `class@anonymous()`. The diagnostics that print the
 * class and the method as separate `%s` keep both halves. */
$say = static function (callable $f): void {
    try { $f(); } catch (\Throwable $e) { echo get_class($e), ': ',
        preg_replace(['/, called in .*$/', '/ in \S+ on line \d+/'], '', $e->getMessage()), "\n"; }
};

$o = new class {
    public function need(int $i) {}
    public static function sneed(int $i) {}
    public function byref(array &$a) {}
    public function ret(): int { return "x"; }
    public function mk() { return static function (int $q) {}; }
};

// assembled — the method name is behind the NUL
$say(static fn() => $o->need('zz'));
$say(static fn() => $o::sneed('zz'));
$say(static fn() => $o->byref([1]));
$say(static fn() => ($o->mk())('zz'));

// separate — both halves survive
$say(static fn() => $o->need());
$say(static fn() => $o->ret());
$say(static fn() => $o->nosuch());

// __invoke is a method like any other
$u = new class { public function __invoke(int $i) {} };
$say(static fn() => $u('zz'));
?>
--EXPECT--
TypeError: class@anonymous(): Argument #1 ($i) must be of type int, string given
TypeError: class@anonymous(): Argument #1 ($i) must be of type int, string given
Error: class@anonymous(): Argument #1 ($a) could not be passed by reference
TypeError: class@anonymous(): Argument #1 ($q) must be of type int, string given
ArgumentCountError: Too few arguments to function class@anonymous::need(), 0 passed and exactly 1 expected
TypeError: class@anonymous::ret(): Return value must be of type int, string returned
Error: Call to undefined method class@anonymous::nosuch()
TypeError: class@anonymous(): Argument #1 ($i) must be of type int, string given
--CLEAN--
<?php
unset($say, $o, $u);
