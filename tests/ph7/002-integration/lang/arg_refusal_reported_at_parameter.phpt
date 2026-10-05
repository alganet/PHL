--TEST--
An argument refusal is reported at the refused parameter's declaration, not at the call
--FILE--
<?php
// php raises a user callee's argument TypeError/ArgumentCountError from inside the
// callee, on the line of the parameter it refuses: that parameter's TYPE (a `?`
// prefix does not count), else its `$name` -- never a modifier or an attribute.
// "Too few arguments" belongs to the first parameter nobody passed. The door the call
// came through does not matter, and neither does whether the callee's frame exists
// yet (a fiber body, a generator), so the FILE is the callee's too.
#[Attribute] class At {}
function typed(int $a,
    #[At]
    string
    $b) {}
function nullable(?
    int $x) {}
function untyped(
    &
    $r, ...
    $rest) {}
function few(int $a,
    int $b,
    int $c = 3) {}
function tail(int $a, string ...$v
) {}
class P {
    public function __construct(
        public
        readonly
        int $p,
    ) {}
}
function gen(
    int $g) { yield 1; }
eval("function ev(\n\n int \$e, int \$f) {}");
$c = function(
    int $q) {};
$doors = [
    'direct'   => fn() => typed(1, []),
    'cuf'      => fn() => call_user_func('typed', 1, []),
    'map'      => fn() => array_map('typed', [1], [[]]),
    'fiber'    => fn() => (new Fiber('typed'))->start(1, []),
    'reflect'  => fn() => (new ReflectionFunction('typed'))->invoke(1, []),
    'nullable' => fn() => nullable("x"),
    'variadic' => fn() => tail(1, "ok", []),
    'few'      => fn() => few(1),
    'fewmap'   => fn() => array_map('few', [1]),
    'untyped'  => fn() => untyped(),
    'hole'     => fn() => few(b: 2),
    'fhole'    => fn() => (new Fiber('few'))->start(b: 2),
    'ctor'     => fn() => new P("x"),
    'newinst'  => fn() => (new ReflectionClass('P'))->newInstance("x"),
    'gen'      => fn() => gen("x"),
    'closure'  => fn() => $c("x"),
    'eval'     => fn() => ev("x", 1),
    'evalhole' => fn() => (new Fiber('ev'))->start(f: 1),
];
foreach ($doors as $k => $d) {
    try {
        $d();
    } catch (Throwable $e) {
        echo str_pad($k, 9), get_class($e), " ",
            str_replace(__FILE__, 'FILE', $e->getFile()), ":", $e->getLine(), "\n";
    }
}
?>
--EXPECT--
direct   TypeError FILE:11
cuf      TypeError FILE:11
map      TypeError FILE:11
fiber    TypeError FILE:11
reflect  TypeError FILE:11
nullable TypeError FILE:14
variadic TypeError FILE:22
few      ArgumentCountError FILE:20
fewmap   ArgumentCountError FILE:20
untyped  ArgumentCountError FILE:17
hole     ArgumentCountError FILE:19
fhole    ArgumentCountError FILE:19
ctor     TypeError FILE:28
newinst  TypeError FILE:28
gen      TypeError FILE:32
closure  TypeError FILE:35
eval     TypeError FILE(33) : eval()'d code:3
evalhole ArgumentCountError FILE(33) : eval()'d code:3
