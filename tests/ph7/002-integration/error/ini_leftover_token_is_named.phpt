--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An ini value's leftover token is reported under the name php's grammar gives it
--DESCRIPTION--
php's ini grammar reduces a value as soon as the next byte cannot extend it, and
only then discovers that byte starts nothing it can take either -- so the value
COMMITS and a syntax error is separately reported over what was left standing.
Naming that leftover is the whole of the report, and php names it three ways: a
symbol its grammar declares (TC_CONSTANT, TC_NUMBER, TC_STRING, TC_RAW,
TC_DOLLAR_CURLY), the byte itself in quotes for the tokens that are one
character, and END_OF_LINE when the value simply ran out.

Its value scanner picks between the first three by longest match and, on a tie,
by the order the rules are written: every byte a constant or a number can take is
also one the catch-all takes, so the tie IS the test. `x1` is a constant, `1x` is
a string, `-1` is a number and `-1x` is not.

Only a boolean word (which is a whole-value shape, not an operand) or a closing
parenthesis can leave a named token standing: an operand run swallows letters,
digits and quoted pieces itself. A leftover is also a syntax error for the ini
SOURCE, so every directive behind it is dropped -- `precision` stays at its
default on each row that reports.
--SKIPIF--
<?php
if (!function_exists('proc_open') || stripos(PHP_OS, 'WIN') === 0) {
    echo "skip needs proc_open to run each ini source in a process of its own";
}
?>
--FILE--
<?php
$leftExe = getenv('PHPT_TARGET_EXECUTABLE');
if ($leftExe === false || $leftExe === '') {
    $leftExe = PHP_BINARY;
}
$leftFile = sys_get_temp_dir() . DIRECTORY_SEPARATOR . 'phlleftover' . getmypid() . '.ini';

/** Run one ini source through a fresh engine and answer what it made of it. */
function leftRun($leftValue)
{
    global $leftExe, $leftFile;
    file_put_contents($leftFile, "error_reporting = " . $leftValue . "\nprecision = 7\n");
    $leftSpec = array(0 => array('pipe', 'r'), 1 => array('pipe', 'w'), 2 => array('pipe', 'w'));
    $leftProc = proc_open(
        array($leftExe, '-c', $leftFile, '-r', 'echo ini_get("precision");'),
        $leftSpec,
        $leftPipes,
        null,
        array('XDEBUG_MODE' => 'off')
    );
    if (!is_resource($leftProc)) {
        return 'could not run';
    }
    fclose($leftPipes[0]);
    $leftOut = stream_get_contents($leftPipes[1]);
    $leftErr = stream_get_contents($leftPipes[2]);
    fclose($leftPipes[1]);
    fclose($leftPipes[2]);
    proc_close($leftProc);
    $leftSaid = 'no error';
    foreach (preg_split('/\r?\n/', $leftErr) as $leftLine) {
        if (strpos($leftLine, 'syntax error, unexpected ') !== false) {
            $leftSaid = preg_replace('/^.*syntax error, unexpected (.*) in .* on line (\d+)$/', '$1 @$2', $leftLine);
            break;
        }
    }
    return $leftSaid . ' | precision=' . trim($leftOut);
}

foreach (array(
    'On X', 'On 1', 'On "a"', "On 'a'", 'On $x', 'On ${A}', 'On @', 'On -',
    'On|E_NOTICE', 'On ;x',
    '(1)x', '(1)x1', '(1)_1', '(1)1x', '(1)9z', '(1)0', '(1)-1', '(1)1.5',
    '(1).5', '(1)1.', '(1)1e3', '(1)-x', '(1)--1', '(1)@', '(1)=', '(1))',
    '(1)~', '(1)"a"', "(1)'a'", '(1)${', '(1)$', '(1) X', '(1);x',
    'E_ALL & ~E_NOTICE',
) as $leftValue) {
    printf("%-18s => %s\n", $leftValue, leftRun($leftValue));
}
@unlink($leftFile);
?>
--EXPECT--
On X               => TC_CONSTANT @1 | precision=14
On 1               => TC_NUMBER @1 | precision=14
On "a"             => '"' @1 | precision=14
On 'a'             => TC_RAW @1 | precision=14
On $x              => TC_STRING @1 | precision=14
On ${A}            => TC_DOLLAR_CURLY @1 | precision=14
On @               => TC_STRING @1 | precision=14
On -               => TC_STRING @1 | precision=14
On|E_NOTICE        => '|' @1 | precision=14
On ;x              => no error | precision=7
(1)x               => TC_CONSTANT @1 | precision=14
(1)x1              => TC_CONSTANT @1 | precision=14
(1)_1              => TC_CONSTANT @1 | precision=14
(1)1x              => TC_STRING @1 | precision=14
(1)9z              => TC_STRING @1 | precision=14
(1)0               => TC_NUMBER @1 | precision=14
(1)-1              => TC_NUMBER @1 | precision=14
(1)1.5             => TC_NUMBER @1 | precision=14
(1).5              => TC_NUMBER @1 | precision=14
(1)1.              => TC_NUMBER @1 | precision=14
(1)1e3             => TC_STRING @1 | precision=14
(1)-x              => TC_STRING @1 | precision=14
(1)--1             => TC_STRING @1 | precision=14
(1)@               => TC_STRING @1 | precision=14
(1)=               => '=' @1 | precision=14
(1))               => ')' @1 | precision=14
(1)~               => '~' @1 | precision=14
(1)"a"             => '"' @1 | precision=14
(1)'a'             => TC_RAW @1 | precision=14
(1)${              => TC_DOLLAR_CURLY @1 | precision=14
(1)$               => TC_STRING @1 | precision=14
(1) X              => TC_CONSTANT @1 | precision=14
(1);x              => no error | precision=7
E_ALL & ~E_NOTICE  => no error | precision=7
