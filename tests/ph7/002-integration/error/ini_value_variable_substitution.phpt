--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A php.ini value's ${NAME} substitution reads the source, then the environment
--DESCRIPTION--
php answers `${NAME}` in a php.ini value in three steps: a directive ALREADY SET
in this source wins over everything, then the environment, then php 8.5's
`${NAME:-fallback}` text, then the empty string. A directive left at its built-in
default is not in that table, so `${memory_limit}` is empty even though the
directive has a value.

The name is any run of bytes that are not the value grammar's own delimiters,
braces or brackets, and not the `:` of a `:-`. Blanks are IN the run and trimmed
off both ends afterwards, so `${ NAME }` is NAME while `${NA ME}` is the
two-word name. A substitution happens inside a double-quoted run and never
inside a raw one.

Three shapes are refused, and a refused value keeps the directive at its default
AND drops every directive behind it. The middle one is php's scanner being
literal: its variable-name rule matches ONE byte and then looks ahead, and when
what follows is `:-` it jumps to the fallback state without ever handing back the
name -- so a two-byte name takes a fallback and a one-byte name is a syntax
error over a token the parser never received.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to run each ini source in a process of its own";
}
?>
--FILE--
<?php
$varExe = getenv('PHPT_TARGET_EXECUTABLE');
if ($varExe === false || $varExe === '') {
    $varExe = PHP_BINARY;
}
$varFile = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phlinivar' . getmypid() . '.ini';

/** Run one whole ini source through a fresh engine and answer what it made of it. */
function varRun($varSource)
{
    global $varExe, $varFile;
    file_put_contents($varFile, $varSource);
    $varSpec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $varProc = proc_open(
        array($varExe, '-c', $varFile, '-r', 'echo ini_get("error_log"), "/", ini_get("precision");'),
        $varSpec,
        $varPipes,
        null,
        array('XDEBUG_MODE' => 'off', 'PHLVAR' => 'hello')
    );
    if (!is_resource($varProc)) {
        return 'could not run';
    }
    fclose($varPipes[0]);
    $varOut = stream_get_contents($varPipes[1]);
    $varErr = stream_get_contents($varPipes[2]);
    fclose($varPipes[1]);
    fclose($varPipes[2]);
    proc_close($varProc);
    $varSaid = 'ok';
    foreach (preg_split('/\r?\n/', $varErr) as $varLine) {
        if (strpos($varLine, 'syntax error, unexpected ') !== false) {
            $varSaid = preg_replace('/^.*syntax error, unexpected (.*) in .* on line (\d+)$/', '$1 @$2', $varLine);
            break;
        }
    }
    return $varSaid . ' | ' . trim($varOut);
}

/** The common shape: the substitution under test, then a directive behind it. */
function varOne($varValue)
{
    return varRun("error_log = " . $varValue . "\nprecision = 7\n");
}

foreach (array(
    '${PHLVAR}', 'a${PHLVAR}b', '${PHLVAR}x${PHLVAR}', 'a${PHLNOPE}b',
    '[${PHLNOPE}]', '[${memory_limit}]',
    '${PHLNOPE:-dflt}', '${PHLVAR:-dflt}',
    'x${PHLNOPE:-}y', '[${PHLNOPE:- }]', '${PHLNOPE:-${PHLVAR}}',
    '${PHLNOPE:-${PHLMISS:-deep}}', '${PHLNOPE:-a:-b}', '${PHLNOPE:-"q w"}',
    '${PHLNOPE:-"a}b"}', '${PHLNOPE:-a!b}', '${PHLNOPE:-a&b}',
    '${PHLNOPE:-a\}b}', '${PHLNOPE:-a\${PHLVAR}}', '${PHLNOPE:-a}b',
    '${ PHLVAR }', '${PHL VAR}', '[${ }]', '[${P}]', '${PH:LVAR}',
    '"a${PHLVAR}b"', "'a\${PHLVAR}b'", '${PHLVAR}|1', 'a$b', 'a$\\{PHLVAR}',
    '${a}b}',
    '${PHLVAR', '${', '${}', '${&}', '${P:-x}', "\${PHLNOPE:-'q'}",
    '${PHLNOPE:-a;b}',
) as $varValue) {
    printf("%-24s => %s\n", $varValue, varOne($varValue));
}

/* A directive set earlier in the SAME source outranks the environment. */
echo "\n";
printf("%-24s => %s\n", 'source order', varRun("error_log = 77\nprecision = \${error_log}\n"));
printf("%-24s => %s\n", 'last one wins', varRun("error_log = 1\nerror_log = 2\nprecision = \${error_log}\n"));
printf("%-24s => %s\n", 'later line', varRun("error_log = \${PHLVAR\nprecision = 7\n"));
@unlink($varFile);
?>
--EXPECT--
${PHLVAR}                => ok | hello/7
a${PHLVAR}b              => ok | ahellob/7
${PHLVAR}x${PHLVAR}      => ok | helloxhello/7
a${PHLNOPE}b             => ok | ab/7
[${PHLNOPE}]             => ok | []/7
[${memory_limit}]        => ok | []/7
${PHLNOPE:-dflt}         => ok | dflt/7
${PHLVAR:-dflt}          => ok | hello/7
x${PHLNOPE:-}y           => ok | xy/7
[${PHLNOPE:- }]          => ok | [ ]/7
${PHLNOPE:-${PHLVAR}}    => ok | hello/7
${PHLNOPE:-${PHLMISS:-deep}} => ok | deep/7
${PHLNOPE:-a:-b}         => ok | a:-b/7
${PHLNOPE:-"q w"}        => ok | q w/7
${PHLNOPE:-"a}b"}        => ok | a}b/7
${PHLNOPE:-a!b}          => ok | a!b/7
${PHLNOPE:-a&b}          => ok | a&b/7
${PHLNOPE:-a\}b}         => ok | a\}b/7
${PHLNOPE:-a\${PHLVAR}}  => ok | a\${PHLVAR}/7
${PHLNOPE:-a}b           => ok | ab/7
${ PHLVAR }              => ok | hello/7
${PHL VAR}               => ok | /7
[${ }]                   => ok | []/7
[${P}]                   => ok | []/7
${PH:LVAR}               => ok | /7
"a${PHLVAR}b"            => ok | ahellob/7
'a${PHLVAR}b'            => ok | a${PHLVAR}b/7
${PHLVAR}|1              => ok | 1/7
a$b                      => ok | a$b/7
a$\{PHLVAR}              => ok | a$\{PHLVAR}/7
${a}b}                   => ok | b}/7
${PHLVAR                 => end of file, expecting TC_FALLBACK or '}' @1 | /14
${                       => end of file, expecting TC_VARNAME @1 | /14
${}                      => '}', expecting TC_VARNAME @1 | /14
${&}                     => end of file, expecting TC_VARNAME @1 | /14
${P:-x}                  => TC_FALLBACK, expecting TC_VARNAME @1 | /14
${PHLNOPE:-'q'}          => end of file, expecting '}' @1 | /14
${PHLNOPE:-a;b}          => end of file, expecting '}' @1 | /14

source order             => ok | 77/77
last one wins            => ok | 2/2
later line               => end of file, expecting TC_FALLBACK or '}' @1 | /14
