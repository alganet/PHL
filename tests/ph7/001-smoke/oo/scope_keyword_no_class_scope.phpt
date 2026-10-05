--TEST--
self, static and parent with no class scope behind them are refused by scope, not looked up
--FILE--
<?php
// A written self/static/parent with no class behind it is php's scope refusal, not a
// class lookup: a class fetch says "Cannot access", `X::class` says "Cannot use", and
// `parent` in a class without a base is told apart. A NAME held in a variable is still
// looked up as a class.
$t = [
    fn() => self::f(),
    fn() => SELF::f(),
    fn() => Static::$x,
    fn() => self::$x = 1,
    fn() => new PARENT,
    fn() => new self,
    fn() => new static,
    fn() => self::K,
    fn() => parent::$p,
    fn() => static::K,
    fn() => Self::CLASS,
    fn() => parent::class,
    fn() => static::class,
    fn() => [self::class, 'x'],
    function () { $c = 'self'; return $c::f(); },
    function () { $c = 'self'; return new $c; },
    Closure::bind(function () { return new self; }, null, null),
];
foreach ($t as $i => $f) {
    try { $f(); } catch (Throwable $e) { echo $i, ' ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
class ScopeKeywordNoBase {
    function g() {
        return [fn() => parent::class, fn() => new parent, fn() => parent::K, fn() => parent::g()];
    }
}
foreach ((new ScopeKeywordNoBase)->g() as $i => $f) {
    try { $f(); } catch (Throwable $e) { echo $i, ' ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
--EXPECT--
0 Error: Cannot access "self" when no class scope is active
1 Error: Cannot access "self" when no class scope is active
2 Error: Cannot access "static" when no class scope is active
3 Error: Cannot access "self" when no class scope is active
4 Error: Cannot access "parent" when no class scope is active
5 Error: Cannot access "self" when no class scope is active
6 Error: Cannot access "static" when no class scope is active
7 Error: Cannot access "self" when no class scope is active
8 Error: Cannot access "parent" when no class scope is active
9 Error: Cannot access "static" when no class scope is active
10 Error: Cannot use "self" in the global scope
11 Error: Cannot use "parent" in the global scope
12 Error: Cannot use "static" in the global scope
13 Error: Cannot use "self" in the global scope
14 Error: Class "self" not found
15 Error: Class "self" not found
16 Error: Cannot access "self" when no class scope is active
0 Error: Cannot use "parent" when current class scope has no parent
1 Error: Cannot access "parent" when current class scope has no parent
2 Error: Cannot access "parent" when current class scope has no parent
3 Error: Cannot access "parent" when current class scope has no parent
