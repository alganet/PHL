--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
printf family: a dangling '%' counts as a required argument, and the argument-count check runs BEFORE specifier validation
--FILE--
<?php
$cases = [
    ['%y'],        // unknown specifier, but arg count is short -> count wins
    ['%y', 1],
    ['%'],         // dangling percent still needs a value
    ['%', 1],
    ['%d%', 1],
    ['%d%', 1, 2],
    ['x%', 1],
    ['%1$', 1],
    ['%05', 1],
    ['100%%', 1],  // %% consumes nothing and is not dangling
];
foreach ($cases as $a) {
    echo str_pad(json_encode($a), 16);
    try {
        $out = var_export(sprintf(...$a), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo '-> ', $out, "\n";
}
foreach ([[], [1]] as $vals) {
    echo str_pad('vsprintf ' . json_encode($vals), 16);
    try {
        $out = var_export(vsprintf('%', $vals), true);
    } catch (Throwable $e) {
        $out = get_class($e) . ': ' . $e->getMessage();
    }
    echo '-> ', $out, "\n";
}
?>
--EXPECT--
["%y"]          -> ArgumentCountError: 2 arguments are required, 1 given
["%y",1]        -> ValueError: Unknown format specifier "y"
["%"]           -> ArgumentCountError: 2 arguments are required, 1 given
["%",1]         -> ValueError: Missing format specifier at end of string
["%d%",1]       -> ArgumentCountError: 3 arguments are required, 2 given
["%d%",1,2]     -> ValueError: Missing format specifier at end of string
["x%",1]        -> ValueError: Missing format specifier at end of string
["%1$",1]       -> ValueError: Missing format specifier at end of string
["%05",1]       -> ValueError: Missing format specifier at end of string
["100%%",1]     -> '100%'
vsprintf []     -> ValueError: The arguments array must contain 1 items, 0 given
vsprintf [1]    -> ValueError: Missing format specifier at end of string
--CLEAN--
<?php
unset($cases, $e, $out);
