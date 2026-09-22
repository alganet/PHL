--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throw inside a match arm/condition or a switch case expression ABANDONS the statement
--DESCRIPTION--
A match condition, a match arm BODY and a switch `case` expression are each compiled into
their own bytecode container and run by VmLocalExec, which shares the caller's VM frame. An
enclosing try therefore finds the throw and runs its catch body IN PLACE, and the nested exec
then returns PH7_EXCEPTION -- a status the three host sites used to DISCARD. The statement
carried on after the catch had already run: `match` moved to the next condition and finally to
its DEFAULT arm (returning a value php never produces), `switch` scanned the remaining cases
and jumped to `default:`, and the assignment completed. Every one of those branches is code php
does not reach: the throw abandons the whole statement. Both hosts route the status through
PH7_THROW_ROUTE_MIDEXPR now, so the catch is the last thing that runs.
--FILE--
<?php
function boom() { throw new RuntimeException("boom"); }

echo "== match ARM throws ==\n";
$r = 'untouched';
try {
    $r = match (1) { 1 => boom(), 2 => 'two', default => 'DEFAULT' };
    echo "resumed with "; var_dump($r);
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
var_dump($r);

echo "== match CONDITION throws ==\n";
try {
    $r = match (1) { boom() => 'a', default => 'DEFAULT' };
    echo "resumed with "; var_dump($r);
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== match arm throwing a throw-EXPRESSION ==\n";
try {
    $r = match (1) { 1 => throw new LogicException("expr"), default => 'DEFAULT' };
    echo "resumed with "; var_dump($r);
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== switch CASE expression throws ==\n";
try {
    switch (1) {
        case boom():
            echo "matched\n";
            break;
        default:
            echo "DEFAULT\n";
    }
    echo "resumed after switch\n";
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== a LATER case throws after an earlier one missed ==\n";
try {
    switch (9) {
        case 1:
            echo "one\n";
            break;
        case boom():
            echo "matched\n";
            break;
        default:
            echo "DEFAULT\n";
    }
} catch (Throwable $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

echo "== the loop around it keeps working ==\n";
foreach ([1, 2, 1] as $v) {
    try {
        $r = match ($v) { 1 => boom(), 2 => 'two' };
        echo "r=$r\n";
    } catch (Throwable $e) {
        echo "c:", $e->getMessage(), "\n";
    }
}

echo "== finally still runs, the outer catch still catches ==\n";
try {
    try {
        $r = match (1) { 1 => boom() };
        echo "inner resumed\n";
    } finally {
        echo "finally\n";
    }
} catch (Throwable $e) {
    echo "outer: ", $e->getMessage(), "\n";
}

echo "== mid-expression: the surrounding operator is abandoned too ==\n";
try {
    $x = 5 + match (1) { 1 => boom() };
    echo "x=$x\n";
} catch (Throwable $e) {
    echo "c:", $e->getMessage(), "\n";
}

echo "== inside a generator ==\n";
function gen() {
    try {
        yield match (1) { 1 => boom() };
    } catch (Throwable $e) {
        yield "g:" . $e->getMessage();
    }
    yield "tail";
}
foreach (gen() as $y) {
    var_dump($y);
}

echo "== a throw the match does NOT reach is not raised ==\n";
try {
    var_dump(match (2) { 1 => boom(), 2 => 'two' });
} catch (Throwable $e) {
    echo "c:", $e->getMessage(), "\n";
}

echo "== unhandled match still throws UnhandledMatchError ==\n";
try {
    var_dump(match (99) { 1 => 'one' });
} catch (Throwable $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
?>
--EXPECT--
== match ARM throws ==
caught: boom
string(9) "untouched"
== match CONDITION throws ==
caught: boom
== match arm throwing a throw-EXPRESSION ==
caught: expr
== switch CASE expression throws ==
caught: boom
== a LATER case throws after an earlier one missed ==
caught: boom
== the loop around it keeps working ==
c:boom
r=two
c:boom
== finally still runs, the outer catch still catches ==
finally
outer: boom
== mid-expression: the surrounding operator is abandoned too ==
c:boom
== inside a generator ==
string(6) "g:boom"
string(4) "tail"
== a throw the match does NOT reach is not raised ==
string(3) "two"
== unhandled match still throws UnhandledMatchError ==
UnhandledMatchError: Unhandled match case of type int
