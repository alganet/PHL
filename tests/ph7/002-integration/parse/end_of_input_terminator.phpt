--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The end of the file is not a statement terminator, and does not close a lexeme
--DESCRIPTION--
`<?php echo "a"` is `syntax error, unexpected end of file, expecting "," or ";"`
in php, and PHL RAN it and exited 0 -- the single largest row of the
lint-acceptance sweep this session ran. A `?>` IS a terminator, which is why
`<?php echo "a" ?>` is legal in both engines, so the refusal belongs only to a
chunk that met the end of the FILE.

php names what its parser was still open to, which the statement's own keyword
decides: the comma-list statements may take another element, `return`, `break`,
`continue`, `goto`, `unset` and a do-while may not, `namespace` still wants its
block, and an expression statement is open to too much to name. A block, a
declaration and a LABEL close themselves; the `}` of a CLOSURE does not, because
the statement around it is an expression.

The lexemes end of input can leave open are refused too -- an unterminated quote,
heredoc or block comment was consumed up to EOF here and the program RAN
(`<?php echo 'a` printed `a`). php reports what its scanner was waiting for, which
is a different sentence per shape: the single-quoted scanner hands the parser the
CONTENT it had read, an empty double-quoted body may still take string content
where one with bytes in it may not, an INTERPOLATING body leaves nothing to
expect, and a block comment names the line it began on.

Each row is LINTED in a subprocess: the diagnostic is a whole-file refusal.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to lint each row in a process of its own (PHL has none on Windows)";
}
?>
--FILE--
<?php
$eofExe = getenv('PHPT_TARGET_EXECUTABLE');
if ($eofExe === false || $eofExe === '') {
    $eofExe = PHP_BINARY;
}
$eofFile = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phleof' . getmypid() . '.php';

function eofLint($eofSrc)
{
    global $eofExe, $eofFile;
    file_put_contents($eofFile, "<?php\n" . $eofSrc);
    $eofSpec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $eofProc = proc_open(array($eofExe, '-l', $eofFile), $eofSpec, $eofPipes);
    if (!is_resource($eofProc)) {
        return 'could not lint';
    }
    fclose($eofPipes[0]);
    $eofOut = stream_get_contents($eofPipes[1]) . stream_get_contents($eofPipes[2]);
    fclose($eofPipes[1]);
    fclose($eofPipes[2]);
    proc_close($eofProc);
    foreach (preg_split('/\r?\n/', $eofOut) as $eofLine) {
        if (preg_match('/(Parse|Fatal) error:/', $eofLine)) {
            $eofLine = preg_replace('/ in .* on line \d+$/', '', $eofLine);
            return preg_replace('/^PHP /', '', $eofLine);
        }
    }
    return strpos($eofOut, 'No syntax errors') !== false ? 'accepted' : 'no diagnostic';
}

$eofRows = [
    'expression'        => '$x = 1',
    'call'              => 'f()',
    'echo'              => 'echo "a"',
    'echo, two'         => 'echo 1, 2',
    'print'             => 'print 1',
    'return'            => 'return',
    'return a value'    => 'return 1',
    'global'            => 'global $a',
    'static'            => 'static $a = 1',
    'const'             => 'const A = 1',
    'use'               => 'use Foo\Bar',
    'unset'             => 'unset($a)',
    'goto'              => 'goto x',
    'namespace'         => 'namespace N',
    'do-while'          => 'do { } while (1)',
    'throw'             => 'throw new Exception()',
    'include'           => 'include "a"',
    'array literal'     => '$a = [1, 2,]',
    'closure'           => '$a = function () { }',
    'match'             => '$a = match (1) { default => 1 }',
    'if with one stmt'  => 'if (1) echo 1',
    'for with one stmt' => 'for (;;) break',
    'alternative if'    => 'if (1): echo 1; endif',
    'alternative while' => 'while (1): endwhile',
    'label'             => 'x:',
    'label then stmt'   => 'x: echo 1',
    'block'             => '{ echo 1; }',
    'braced if'         => 'if (1) { echo 1; }',
    'function'          => 'function f() {}',
    'class'             => 'class C {}',
    'terminated'        => 'echo 1;',
    'closed by a tag'   => 'echo "a" ?>',
    'closed by a tag, expr' => '$x = 1 ?>',
    'single quote'      => "echo 'a",
    'single quote, empty' => "echo '",
    'double quote'      => 'echo "a',
    'double quote, empty' => 'echo "',
    'double quote, interpolating' => 'echo "a$b c',
    'double quote, braced' => 'echo "a{$b} z',
    'heredoc'           => "echo <<<EOT\na",
    'heredoc, empty'    => "echo <<<EOT\n",
    'heredoc, interpolating' => "echo <<<EOT\na\$b",
    'nowdoc'            => "echo <<<'EOT'\na",
    'block comment'     => '/* unterminated',
    'block comment, mid file' => '$a = 1; /* c',
    'closed comment'    => '/* ok */ echo 1;',
];
foreach ($eofRows as $eofLabel => $eofSrc) {
    printf("%-28s %s\n", $eofLabel, eofLint($eofSrc));
}
@unlink($eofFile);
?>
--EXPECT--
expression                   Parse error:  syntax error, unexpected end of file
call                         Parse error:  syntax error, unexpected end of file
echo                         Parse error:  syntax error, unexpected end of file, expecting "," or ";"
echo, two                    Parse error:  syntax error, unexpected end of file, expecting "," or ";"
print                        Parse error:  syntax error, unexpected end of file
return                       Parse error:  syntax error, unexpected end of file, expecting ";"
return a value               Parse error:  syntax error, unexpected end of file, expecting ";"
global                       Parse error:  syntax error, unexpected end of file, expecting "," or ";"
static                       Parse error:  syntax error, unexpected end of file, expecting "," or ";"
const                        Parse error:  syntax error, unexpected end of file, expecting "," or ";"
use                          Parse error:  syntax error, unexpected end of file, expecting "," or ";"
unset                        Parse error:  syntax error, unexpected end of file, expecting ";"
goto                         Parse error:  syntax error, unexpected end of file, expecting ";"
namespace                    Parse error:  syntax error, unexpected end of file, expecting "{"
do-while                     Parse error:  syntax error, unexpected end of file, expecting ";"
throw                        Parse error:  syntax error, unexpected end of file
include                      Parse error:  syntax error, unexpected end of file
array literal                Parse error:  syntax error, unexpected end of file
closure                      Parse error:  syntax error, unexpected end of file
match                        Parse error:  syntax error, unexpected end of file
if with one stmt             Parse error:  syntax error, unexpected end of file, expecting "," or ";"
for with one stmt            Parse error:  syntax error, unexpected end of file, expecting ";"
alternative if               Parse error:  syntax error, unexpected end of file, expecting ";"
alternative while            Parse error:  syntax error, unexpected end of file, expecting ";"
label                        accepted
label then stmt              Parse error:  syntax error, unexpected end of file, expecting "," or ";"
block                        accepted
braced if                    accepted
function                     accepted
class                        accepted
terminated                   accepted
closed by a tag              accepted
closed by a tag, expr        accepted
single quote                 Parse error:  syntax error, unexpected string content "a"
single quote, empty          Parse error:  syntax error, unexpected string content ""
double quote                 Parse error:  syntax error, unexpected end of file, expecting variable or "${" or "{$"
double quote, empty          Parse error:  syntax error, unexpected end of file, expecting variable or string content or "${" or "{$"
double quote, interpolating  Parse error:  syntax error, unexpected end of file
double quote, braced         Parse error:  syntax error, unexpected end of file
heredoc                      Parse error:  syntax error, unexpected end of file, expecting variable or heredoc end or "${" or "{$"
heredoc, empty               Parse error:  syntax error, unexpected end of file
heredoc, interpolating       Parse error:  syntax error, unexpected end of file
nowdoc                       Parse error:  syntax error, unexpected end of file, expecting variable or heredoc end or "${" or "{$"
block comment                Parse error:  Unterminated comment starting line 2
block comment, mid file      Parse error:  Unterminated comment starting line 2
closed comment               accepted
