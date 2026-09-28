--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
What a destructuring source may BE when the list binds by reference
--DESCRIPTION--
A by-reference entry has to bind to something with a slot behind it, and php asks
at COMPILE time -- `zend_is_variable_or_call`, which takes a variable, a property,
a static property, a subscript and a CALL, and refuses everything else with
`Cannot assign reference to non referenceable value`. A list with no by-ref entry
never asks: `[$a] = [7];` is ordinary source.

`[, ,] = $src` is php's `Cannot use empty list` beside them: a destructure with no
position to fill at all.

Each row is LINTED in a subprocess -- php's compile fatals here are uncatchable
even inside eval() -- so the diagnostic has to be read off a process of its own.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to lint each row in a process of its own (PHL has none on Windows)";
}
?>
--FILE--
<?php
$refExe = getenv('PHPT_TARGET_EXECUTABLE');
if ($refExe === false || $refExe === '') {
    $refExe = PHP_BINARY;
}
$refFile = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phlreflist' . getmypid() . '.php';

function refLint($refSrc)
{
    global $refExe, $refFile;
    $refPrelude = 'function f(){ return [7]; } class C { public static $s = [1]; public $p = [1]; }'
        . "\n" . '$v = [1]; $o = new C();' . "\n";
    file_put_contents($refFile, "<?php\n" . $refPrelude . $refSrc . "\n");
    $refSpec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $refProc = proc_open(array($refExe, '-l', $refFile), $refSpec, $refPipes);
    if (!is_resource($refProc)) {
        return 'could not lint';
    }
    fclose($refPipes[0]);
    $refOut = stream_get_contents($refPipes[1]) . stream_get_contents($refPipes[2]);
    fclose($refPipes[1]);
    fclose($refPipes[2]);
    proc_close($refProc);
    foreach (preg_split('/\r?\n/', $refOut) as $refLine) {
        if (preg_match('/(Parse|Fatal) error:/', $refLine)) {
            $refLine = preg_replace('/ in .* on line \d+$/', '', $refLine);
            return preg_replace('/^PHP /', '', $refLine);
        }
    }
    return strpos($refOut, 'No syntax errors') !== false ? 'accepted' : 'no diagnostic';
}

$refRows = [
    'variable'          => '[&$r] = $v;',
    'subscript'         => '[&$r] = $v[0];',
    'property'          => '[&$r] = $o->p;',
    'static property'   => '[&$r] = C::$s;',
    'call'              => '[&$r] = f();',
    'parenthesised var' => '[&$r] = ($v);',
    'array literal'     => '[&$r] = [7];',
    'string literal'    => '[&$r] = "str";',
    'new'               => '[&$r] = new C();',
    'computed'          => '[&$r] = $v + [];',
    'elvis'             => '[&$r] = $v ?: [];',
    'nested by-ref'     => '[[&$r]] = [7];',
    'keyed by-ref'      => '["k" => &$r] = [7];',
    'no by-ref entry'   => '[$r] = [7];',
    'empty list'        => '[, ,] = $v;',
    'empty list, list()' => 'list(,) = $v;',
];
foreach ($refRows as $refLabel => $refSrc) {
    printf("%-20s %s\n", $refLabel, refLint($refSrc));
}
@unlink($refFile);
?>
--EXPECT--
variable             accepted
subscript            accepted
property             accepted
static property      accepted
call                 accepted
parenthesised var    accepted
array literal        Fatal error:  Cannot assign reference to non referenceable value
string literal       Fatal error:  Cannot assign reference to non referenceable value
new                  Fatal error:  Cannot assign reference to non referenceable value
computed             Fatal error:  Cannot assign reference to non referenceable value
elvis                Fatal error:  Cannot assign reference to non referenceable value
nested by-ref        Fatal error:  Cannot assign reference to non referenceable value
keyed by-ref         Fatal error:  Cannot assign reference to non referenceable value
no by-ref entry      accepted
empty list           Fatal error:  Cannot use empty list
empty list, list()   Fatal error:  Cannot use empty list
