--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator called with named arguments that skip a middle parameter: the skipped one keeps its default instead of taking the next argument's value
--DESCRIPTION--
`g(a: 1, c: 9)` names the first and third parameters and says nothing about the
second. php binds each actual to the parameter it names and leaves the ones
between them on their declared defaults, so `function g($a, $b = 2, $c = 3)`
receives `1/2/9` — and `func_get_args()` reports all three, defaults included.

A generator call resolved its named arguments into a POSITIONAL list and then
compacted the holes away, so every later actual slid down one parameter: `$b`
took the 9 meant for `$c`, and `$c` kept its default. A silent wrong answer, and
only on this path — the ordinary call binds by name and was always right.

The reordered list carries one entry per parameter now, with a null for a
parameter nothing named, which the frame setup reads as "not passed" and answers
with the declared default. Trailing holes are simply not passed, so a variadic
tail still collects what belongs to it, and a hole below the required-argument
watermark is still php's ArgumentCountError, raised before the generator object
exists.
--FILE--
<?php
/* A named argument binds the formal it NAMES; the formals between two named ones
 * keep their declared defaults, and count as passed. */
function g($a, $b = 2, $c = 3)
{
    echo func_num_args(), ':', json_encode(func_get_args()), " -> $a/$b/$c\n";
    yield 1;
}
foreach (g(a: 1, c: 9) as $ignored);
foreach (g(1, c: 9) as $ignored);
foreach (g(a: 1) as $ignored);
foreach (g(c: 9, a: 1) as $ignored);

/* Two formals wide, and the hole keeps a TYPED default's own shape. */
function wide($a, string $b = 'B', int $c = 3, $d = 4)
{
    echo json_encode([$a, $b, $c, $d]), "\n";
    yield 1;
}
foreach (wide(a: 1, d: 9) as $ignored);

/* A variadic tail collects nothing when every actual named a formal. */
function tail($a, $b = 2, ...$rest)
{
    echo "$a/$b/", json_encode($rest), "\n";
    yield 1;
}
foreach (tail(a: 1, b: 5) as $ignored);
foreach (tail(a: 1) as $ignored);

/* A hole below the required watermark is still php's ArgumentCountError, and it
 * is raised before the generator object exists. */
function req($a, $b, $c = 3)
{
    yield 1;
}
try {
    $gen = req(a: 1, c: 9);
    echo "no throw\n";
} catch (ArgumentCountError $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

/* Methods and static methods take the same binder. */
class Holder
{
    public function m($a, $b = 2, $c = 3)
    {
        echo "m $a/$b/$c\n";
        yield 1;
    }

    public static function s($a, $b = 2, $c = 3)
    {
        echo "s $a/$b/$c\n";
        yield 1;
    }
}
foreach ((new Holder())->m(a: 1, c: 9) as $ignored);
foreach (Holder::s(a: 1, c: 9) as $ignored);

/* A by-reference formal still aliases the caller's variable across a hole. */
function byref($a, $b = 2, &$c = null)
{
    $c = 'written';
    yield 1;
}
$sink = 'before';
foreach (byref(a: 1, c: $sink) as $ignored);
var_dump($sink);
?>
--EXPECT--
3:[1,2,9] -> 1/2/9
3:[1,2,9] -> 1/2/9
1:[1] -> 1/2/3
3:[1,2,9] -> 1/2/9
[1,"B",3,9]
1/5/[]
1/2/[]
ArgumentCountError: req(): Argument #2 ($b) not passed
m 1/2/9
s 1/2/9
string(7) "written"
--CLEAN--
<?php
