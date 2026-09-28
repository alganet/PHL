--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What break/continue accept as a level, and what ends the statement
--DESCRIPTION--
php's grammar is `break optional_expr ";"`, and the level is then screened as a
compile-time VALUE rather than parsed as a number. An operand that is not a
literal node -- a constant, a variable, `-1`, `(1+0)` -- is
`'break' operator with non-integer operand is no longer supported`; one that IS a
literal but not a positive integer -- `1.5`, `"1"`, `0` -- is
`'break' operator accepts only positive integers`. Parentheses leave no node of
their own, so `((1))` is 1 while the arithmetic inside them is NOT folded away
first. A token where the semicolon was due is the ordinary parse error.

PHL read a NUMBER token and ignored anything else, so `break 1.5;` broke one level
in SILENCE, `break $x;` and `break foo;` compiled to a plain break behind a warning
about the missing semicolon, and the program then RAN. `goto LABEL x;` had a
sentence of its own for the same shape.

Each row is LINTED in a subprocess: php's compile fatals here are uncatchable
even inside eval(), so the diagnostic has to be read off a process of its own.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to lint each row in a process of its own (PHL has none on Windows)";
}
?>
--FILE--
<?php
$jumpExe = getenv('PHPT_TARGET_EXECUTABLE');
if ($jumpExe === false || $jumpExe === '') {
    $jumpExe = PHP_BINARY;
}
$jumpFile = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phljump' . getmypid() . '.php';

/** Lint one snippet in a subprocess and answer the diagnostic, path stripped. */
function jumpLint($jumpSrc)
{
    global $jumpExe, $jumpFile;
    file_put_contents($jumpFile, "<?php\n" . $jumpSrc . "\n");
    $jumpSpec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $jumpProc = proc_open(array($jumpExe, '-l', $jumpFile), $jumpSpec, $jumpPipes);
    if (!is_resource($jumpProc)) {
        return 'could not lint';
    }
    fclose($jumpPipes[0]);
    $jumpOut = stream_get_contents($jumpPipes[1]) . stream_get_contents($jumpPipes[2]);
    fclose($jumpPipes[1]);
    fclose($jumpPipes[2]);
    proc_close($jumpProc);
    /* The two engines split lint output between the pipes differently; take the
     * first DIAGNOSTIC line wherever it landed. */
    foreach (preg_split('/\r?\n/', $jumpOut) as $jumpLine) {
        if (preg_match('/(Parse|Fatal) error:/', $jumpLine)) {
            $jumpLine = preg_replace('/ in .* on line \\d+$/', '', $jumpLine);
            return preg_replace('/^PHP /', '', $jumpLine);
        }
    }
    return strpos($jumpOut, 'No syntax errors') !== false ? 'accepted' : 'no diagnostic';
}

$jumpRows = [
    'no operand'        => 'while (1) { break; }',
    'one'               => 'while (1) { break 1; }',
    'hex one'           => 'while (1) { break 0x1; }',
    'separator'         => str_repeat('while (1) { ', 10) . 'break 1_0;' . str_repeat('}', 10),
    'empty parentheses' => 'while (1) { break (); }',
    'parenthesised'     => 'while (1) { break (1); }',
    'twice parenthesised' => 'while (1) { break ((1)); }',
    'arithmetic'        => 'while (1) { break (1+0); }',
    'negative'          => 'while (1) { break -1; }',
    'signed'            => 'while (1) { break +1; }',
    'zero'              => 'while (1) { break 0; }',
    'float'             => 'while (1) { break 1.5; }',
    'numeric string'    => 'while (1) { break "1"; }',
    'constant'          => 'while (1) { break PHP_INT_MAX; }',
    'variable'          => 'while (1) { break $x; }',
    'expression'        => 'while (1) { break $a + 1; }',
    'trailing token'    => 'while (1) { break 1 foo; }',
    'trailing comma'    => 'while (1) { break 1, 2; }',
    'two words'         => 'while (1) { break foo bar; }',
    'bad separator'     => 'while (1) { break 1_; }',
    'too many levels'   => 'while (1) { break 2; }',
    'continue, float'   => 'while (1) { continue 1.5; }',
    'continue, name'    => 'while (1) { continue foo; }',
    'continue, trailing token' => 'while (1) { continue 1 foo; }',
    'goto, trailing token' => 'goto jumpEnd foo; jumpEnd: ;',
];
foreach ($jumpRows as $jumpLabel => $jumpSrc) {
    printf("%-24s %s\n", $jumpLabel, jumpLint($jumpSrc));
}
@unlink($jumpFile);
?>
--EXPECT--
no operand               accepted
one                      accepted
hex one                  accepted
separator                accepted
empty parentheses        Parse error:  syntax error, unexpected token ")"
parenthesised            accepted
twice parenthesised      accepted
arithmetic               Fatal error:  'break' operator with non-integer operand is no longer supported
negative                 Fatal error:  'break' operator with non-integer operand is no longer supported
signed                   Fatal error:  'break' operator with non-integer operand is no longer supported
zero                     Fatal error:  'break' operator accepts only positive integers
float                    Fatal error:  'break' operator accepts only positive integers
numeric string           Fatal error:  'break' operator accepts only positive integers
constant                 Fatal error:  'break' operator with non-integer operand is no longer supported
variable                 Fatal error:  'break' operator with non-integer operand is no longer supported
expression               Fatal error:  'break' operator with non-integer operand is no longer supported
trailing token           Parse error:  syntax error, unexpected identifier "foo", expecting ";"
trailing comma           Parse error:  syntax error, unexpected token ",", expecting ";"
two words                Parse error:  syntax error, unexpected identifier "bar", expecting ";"
bad separator            Parse error:  syntax error, unexpected identifier "_", expecting ";"
too many levels          Fatal error:  Cannot 'break' 2 levels
continue, float          Fatal error:  'continue' operator accepts only positive integers
continue, name           Fatal error:  'continue' operator with non-integer operand is no longer supported
continue, trailing token Parse error:  syntax error, unexpected identifier "foo", expecting ";"
goto, trailing token     Parse error:  syntax error, unexpected identifier "foo", expecting ";"
