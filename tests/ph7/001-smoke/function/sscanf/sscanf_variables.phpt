--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sscanf() with variables answers a COUNT, and creates what it writes through
--FILE--
<?php
/* Passing variables changes the answer entirely: an int count of the
 * conversions PERFORMED, -1 when the input ran out before any of them, and
 * every variable written through by reference -- so an undefined one is
 * CREATED rather than read. The two refusals here are the validation pass
 * comparing the format's substitutions against the variables it was handed. */
function sscanf_vars(string $label, callable $fn): void {
    $seen = [];
    set_error_handler(function ($no, $msg) use (&$seen) { $seen[] = "W: $msg"; return true; });
    try {
        $out = $fn();
    } catch (\Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    restore_error_handler();
    foreach ($seen as $line) {
        echo '  ', $line, "\n";
    }
    printf("%-34s -> %s\n", $label, str_replace("\n", '', var_export($out, true)));
}
sscanf_vars('two of two', function () {
    $r = sscanf('25 30', '%d %d', $a, $b);
    return [$r, $a, $b];
});
sscanf_vars('one of two, second untouched', function () {
    $b = 'kept';
    $r = sscanf('25', '%d %d', $a, $b);
    return [$r, $a, $b];
});
sscanf_vars('nothing at all', function () {
    $a = 'kept';
    return [sscanf('', '%d', $a), $a];
});
sscanf_vars('mixed types', function () {
    $r = sscanf('bob 42 1.5 ff', '%s %d %f %x', $s, $i, $f, $x);
    return [$r, $s, $i, $f, $x];
});
sscanf_vars('positional order', function () {
    $r = sscanf('5 6', '%2$d %1$d', $a, $b);
    return [$r, $a, $b];
});
sscanf_vars('written into an array element', function () {
    $out = [];
    $r = sscanf('7 8', '%d %d', $out['a'], $out['b']);
    return [$r, $out];
});
sscanf_vars('more variables than specifiers', fn() => sscanf('25 30', '%d', $a, $b));
sscanf_vars('more specifiers than variables', fn() => sscanf('25 30', '%d %d', $a));
sscanf_vars('a non-variable in a by-ref slot', fn() => sscanf('1', '%d', 5));
--EXPECT--
two of two                         -> array (  0 => 2,  1 => 25,  2 => 30,)
one of two, second untouched       -> array (  0 => 1,  1 => 25,  2 => 'kept',)
nothing at all                     -> array (  0 => -1,  1 => 'kept',)
mixed types                        -> array (  0 => 4,  1 => 'bob',  2 => 42,  3 => 1.5,  4 => 255,)
positional order                   -> array (  0 => 2,  1 => 6,  2 => 5,)
written into an array element      -> array (  0 => 2,  1 =>   array (    'a' => 7,    'b' => 8,  ),)
more variables than specifiers     -> 'ValueError: Variable is not assigned by any conversion specifiers'
more specifiers than variables     -> 'ValueError: Different numbers of variable names and field specifiers'
a non-variable in a by-ref slot    -> 'Error: sscanf(): Argument #3 could not be passed by reference'
