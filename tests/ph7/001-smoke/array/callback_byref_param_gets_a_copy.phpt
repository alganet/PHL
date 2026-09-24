--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a builtin hands its callback arguments BY VALUE: a by-reference parameter warns and gets a copy, and array_walk's ELEMENT is the one exception
--FILE--
<?php
// php builds each callback argument itself and passes it as a VALUE, so a
// callback declaring `&$p` gets a warning and a copy -- it never reaches the
// live array element the builtin is walking, and it never fails either. Both
// halves were wrong here: a live element was ALIASED (silent data corruption)
// and an engine-built argument (the key, array_reduce's carry, preg's matches)
// raised "could not be passed by reference" on a program php runs.
// Warnings are collected DISTINCT: php raises one per invocation, and how many
// times a comparator is invoked is a property of the sort, not of this rule.
function cbr_warn(callable $cbr_fn): void
{
    $cbr_seen = [];
    set_error_handler(static function ($cbr_no, $cbr_msg) use (&$cbr_seen) {
        // A closure's php display name carries the FILE it was written in, which
        // the corpus must not depend on; the nesting is what makes it a regex.
        $cbr_seen[preg_replace('/^\{closure.*\}\(\): /', '{closure}(): ', $cbr_msg)] = true;
        return true;
    });
    try {
        $cbr_fn();
    } catch (Throwable $cbr_e) {
        echo '  THREW ', get_class($cbr_e), ': ', $cbr_e->getMessage(), "\n";
    }
    restore_error_handler();
    foreach (array_keys($cbr_seen) as $cbr_msg) {
        echo '  W: ', $cbr_msg, "\n";
    }
}

echo "array_filter value\n";
cbr_warn(static function () {
    $cbr_a = ['k' => 1];
    array_filter($cbr_a, static function (&$cbr_v) { $cbr_v = 99; return true; });
    var_dump($cbr_a);
});

echo "array_filter key (USE_KEY)\n";
cbr_warn(static function () {
    $cbr_a = ['k' => 1];
    var_dump(array_filter($cbr_a, static function (&$cbr_k) { $cbr_k = 'z'; return true; },
        ARRAY_FILTER_USE_KEY));
});

echo "array_filter both (USE_BOTH)\n";
cbr_warn(static function () {
    $cbr_a = ['k' => 1];
    var_dump(array_filter($cbr_a, static function ($cbr_v, &$cbr_k) { $cbr_k = 'z'; return true; },
        ARRAY_FILTER_USE_BOTH));
});

echo "array_map\n";
cbr_warn(static function () {
    $cbr_a = [1];
    var_dump(array_map(static function (&$cbr_v) { $cbr_v = 99; return $cbr_v; }, $cbr_a), $cbr_a);
});

echo "array_map zip\n";
cbr_warn(static function () {
    $cbr_a = [1];
    $cbr_b = [2];
    var_dump(array_map(static function ($cbr_x, &$cbr_y) { $cbr_y = 99; return 0; }, $cbr_a, $cbr_b), $cbr_b);
});

echo "array_reduce carry\n";
cbr_warn(static function () {
    var_dump(array_reduce([1, 2], static function (&$cbr_c, $cbr_v) { return ($cbr_c ?? 0) + $cbr_v; }));
});

echo "usort operands\n";
cbr_warn(static function () {
    $cbr_a = [3, 1];
    usort($cbr_a, static function (&$cbr_x, $cbr_y) { $cbr_x = 99; return $cbr_x <=> $cbr_y; });
    var_dump($cbr_a);
});

echo "uksort operands\n";
cbr_warn(static function () {
    $cbr_a = ['b' => 1, 'a' => 2];
    uksort($cbr_a, static function (&$cbr_x, $cbr_y) { $cbr_x = 'z'; return 0; });
    var_dump(array_keys($cbr_a));
});

echo "array_udiff comparator\n";
cbr_warn(static function () {
    $cbr_a = [1, 2];
    var_dump(array_udiff($cbr_a, [3], static function (&$cbr_x, $cbr_y) { $cbr_x = 99; return $cbr_x <=> $cbr_y; }), $cbr_a);
});

echo "array_find\n";
cbr_warn(static function () {
    $cbr_a = ['k' => 1];
    var_dump(array_find($cbr_a, static function (&$cbr_v, $cbr_k) { $cbr_v = 99; return true; }), $cbr_a);
});

echo "preg_replace_callback matches\n";
cbr_warn(static function () {
    var_dump(preg_replace_callback('/a/', static function (&$cbr_m) { $cbr_m = []; return 'X'; }, 'a'));
});

echo "iterator_apply\n";
cbr_warn(static function () {
    var_dump(iterator_apply(new ArrayIterator([1, 2]), static function (&$cbr_v) { return false; }, [1]));
});

// array_walk is php's ONE by-reference callback argument: the ELEMENT really is
// aliased and writing through it is the point of the function. Its key and its
// $arg are by value like everyone else's.
echo "array_walk element (by REFERENCE, no warning)\n";
cbr_warn(static function () {
    $cbr_a = ['a' => 1, 'b' => 2];
    array_walk($cbr_a, static function (&$cbr_v, $cbr_k) { $cbr_v = $cbr_v * 10; });
    var_dump($cbr_a);
});

echo "array_walk key\n";
cbr_warn(static function () {
    $cbr_a = ['a' => 1];
    array_walk($cbr_a, static function ($cbr_v, &$cbr_k) { $cbr_k = 'z'; });
    var_dump($cbr_a);
});

echo "array_walk extra arg\n";
cbr_warn(static function () {
    $cbr_a = ['a' => 1];
    $cbr_d = 'Y';
    array_walk($cbr_a, static function ($cbr_v, $cbr_k, &$cbr_d) { $cbr_d = 'Z'; }, $cbr_d);
    var_dump($cbr_d);
});

echo "array_walk_recursive element\n";
cbr_warn(static function () {
    $cbr_a = [[1, 2], [3]];
    array_walk_recursive($cbr_a, static function (&$cbr_v) { $cbr_v = $cbr_v * 10; });
    var_dump($cbr_a);
});

// The warning is decided from the formals the call will ACTUALLY bind to. A
// callable naming a method the scope cannot reach routes to __call, whose own
// `($name, $args)` has no by-ref parameter -- so reading the private method's
// signature would diagnose a method php never enters.
echo "inaccessible method routes to __call\n";
class CbrefPriv
{
    private function cmp(&$cbr_x, $cbr_y) { return 0; }
    public function __call($cbr_n, $cbr_a) { echo '  CALL(', $cbr_n, ")\n"; return 0; }
}
cbr_warn(static function () {
    $cbr_a = [3, 1];
    usort($cbr_a, [new CbrefPriv, 'cmp']);
    var_dump($cbr_a);
});

// An argument list longer than the 31-bit by-ref mask must still arrive WHOLE.
echo "long argument list\n";
cbr_warn(static function () {
    $cbr_arrs = [];
    for ($cbr_i = 0; $cbr_i < 35; $cbr_i++) {
        $cbr_arrs[] = [$cbr_i];
    }
    var_dump(array_map(static function (&$cbr_a, ...$cbr_rest) { return count($cbr_rest) + 1; }, ...$cbr_arrs));
});

// A set_error_handler() that does not return stops the call: php runs nothing
// after it, so the callback body is never entered.
echo "throwing error handler\n";
set_error_handler(static function ($cbr_no, $cbr_msg) { throw new RuntimeException('EH'); });
try {
    array_map(static function (&$cbr_v) { echo "  RAN\n"; return 1; }, [1, 2]);
} catch (Throwable $cbr_e) {
    echo '  caught: ', $cbr_e->getMessage(), "\n";
}
restore_error_handler();

// Every callable SPELLING reaches the same rule, and the callee is named the way
// php names it.
echo "spellings\n";
function cbr_named(&$cbr_v) { $cbr_v = 99; return true; }
class CbrefHolder
{
    public function m(&$cbr_v) { $cbr_v = 99; return true; }
    public static function s(&$cbr_v) { $cbr_v = 99; return true; }
    public function __invoke(&$cbr_v) { $cbr_v = 99; return true; }
}
foreach ([
    'name'    => 'cbr_named',
    'pair'    => [new CbrefHolder, 'm'],
    'static'  => ['CbrefHolder', 's'],
    'string'  => 'CbrefHolder::s',
    'invoke'  => new CbrefHolder,
    'fcc'     => cbr_named(...),
] as $cbr_kind => $cbr_cb) {
    echo ' ', $cbr_kind, ":\n";
    cbr_warn(static function () use ($cbr_cb) {
        $cbr_a = ['k' => 1];
        array_filter($cbr_a, $cbr_cb);
        var_dump($cbr_a['k']);
    });
}
?>
--EXPECT--
array_filter value
array(1) {
  ["k"]=>
  int(1)
}
  W: {closure}(): Argument #1 ($cbr_v) must be passed by reference, value given
array_filter key (USE_KEY)
array(1) {
  ["k"]=>
  int(1)
}
  W: {closure}(): Argument #1 ($cbr_k) must be passed by reference, value given
array_filter both (USE_BOTH)
array(1) {
  ["k"]=>
  int(1)
}
  W: {closure}(): Argument #2 ($cbr_k) must be passed by reference, value given
array_map
array(1) {
  [0]=>
  int(99)
}
array(1) {
  [0]=>
  int(1)
}
  W: {closure}(): Argument #1 ($cbr_v) must be passed by reference, value given
array_map zip
array(1) {
  [0]=>
  int(0)
}
array(1) {
  [0]=>
  int(2)
}
  W: {closure}(): Argument #2 ($cbr_y) must be passed by reference, value given
array_reduce carry
int(3)
  W: {closure}(): Argument #1 ($cbr_c) must be passed by reference, value given
usort operands
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(3)
}
  W: {closure}(): Argument #1 ($cbr_x) must be passed by reference, value given
uksort operands
array(2) {
  [0]=>
  string(1) "b"
  [1]=>
  string(1) "a"
}
  W: {closure}(): Argument #1 ($cbr_x) must be passed by reference, value given
array_udiff comparator
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
  W: {closure}(): Argument #1 ($cbr_x) must be passed by reference, value given
array_find
int(1)
array(1) {
  ["k"]=>
  int(1)
}
  W: {closure}(): Argument #1 ($cbr_v) must be passed by reference, value given
preg_replace_callback matches
string(1) "X"
  W: {closure}(): Argument #1 ($cbr_m) must be passed by reference, value given
iterator_apply
int(1)
  W: {closure}(): Argument #1 ($cbr_v) must be passed by reference, value given
array_walk element (by REFERENCE, no warning)
array(2) {
  ["a"]=>
  int(10)
  ["b"]=>
  int(20)
}
array_walk key
array(1) {
  ["a"]=>
  int(1)
}
  W: {closure}(): Argument #2 ($cbr_k) must be passed by reference, value given
array_walk extra arg
string(1) "Y"
  W: {closure}(): Argument #3 ($cbr_d) must be passed by reference, value given
array_walk_recursive element
array(2) {
  [0]=>
  array(2) {
    [0]=>
    int(10)
    [1]=>
    int(20)
  }
  [1]=>
  array(1) {
    [0]=>
    int(30)
  }
}
inaccessible method routes to __call
  CALL(cmp)
array(2) {
  [0]=>
  int(3)
  [1]=>
  int(1)
}
long argument list
array(1) {
  [0]=>
  int(35)
}
  W: {closure}(): Argument #1 ($cbr_a) must be passed by reference, value given
throwing error handler
  caught: EH
spellings
 name:
int(1)
  W: cbr_named(): Argument #1 ($cbr_v) must be passed by reference, value given
 pair:
int(1)
  W: CbrefHolder::m(): Argument #1 ($cbr_v) must be passed by reference, value given
 static:
int(1)
  W: CbrefHolder::s(): Argument #1 ($cbr_v) must be passed by reference, value given
 string:
int(1)
  W: CbrefHolder::s(): Argument #1 ($cbr_v) must be passed by reference, value given
 invoke:
int(1)
  W: CbrefHolder::__invoke(): Argument #1 ($cbr_v) must be passed by reference, value given
 fcc:
int(1)
  W: cbr_named(): Argument #1 ($cbr_v) must be passed by reference, value given
--CLEAN--
<?php
unset($cbr_e);
