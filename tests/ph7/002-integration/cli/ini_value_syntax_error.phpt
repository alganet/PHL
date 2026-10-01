--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php.ini value: what the parser reports when it cannot finish the expression
--SKIPIF--
<?php
// Spawns the interpreter through popen() with single-quoted `-r '...'` arguments —
// POSIX shell quoting that cmd.exe does not honour (php fails this on Windows too).
if (DIRECTORY_SEPARATOR === '\\') { echo 'skip POSIX shell quoting in the subprocess harness'; }
?>
--FILE--
<?php
$phl = getenv('PHPT_TARGET_EXECUTABLE');
$ini = tempnam(sys_get_temp_dir(), 'phlini');
function ini_run($cmd, $ini) {
    $fp = popen($cmd . ' 2>&1', 'r');
    $out = '';
    while (!feof($fp)) { $out .= fgets($fp); }
    pclose($fp);
    // a diagnostic names the file with its symlinks resolved (macOS /private)
    return str_replace([realpath($ini), $ini], 'INI', trim($out));
}
// The value grammar stops in one of two places, and php names each of them.
// A byte it cannot take is quoted; a value that simply runs out is END_OF_LINE,
// dated to the line AFTER the directive because the newline is already eaten.
// Inside an unclosed '(' with a complete expression in hand, php also lists
// what could still come.
$cases = array(
    '1)', 'On|E_NOTICE', '1~2', '1 & )', '1 &&',
    'E_ALL &', '~', '!', '1 ^', '1 | ~', '(', '~(', '!!', '1 & ;x',
    '(1', '((1)', '(E_ALL', '( 1 | 2', '(1 2', '(1~2)',
);
foreach ($cases as $value) {
    file_put_contents($ini, "precision=$value\n");
    printf("%-10s %s\n", $value, ini_run("\"$phl\" -c \"$ini\" -r 'echo \"P=\", ini_get(\"precision\");'", $ini));
}
@unlink($ini);
?>
--EXPECT--
1)         PHP:  syntax error, unexpected ')' in INI on line 1
P=1
On|E_NOTICE PHP:  syntax error, unexpected '|' in INI on line 1
P=1
1~2        PHP:  syntax error, unexpected '~' in INI on line 1
P=1
1 & )      PHP:  syntax error, unexpected ')' in INI on line 1
P=14
1 &&       PHP:  syntax error, unexpected '&' in INI on line 1
P=14
E_ALL &    PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
~          PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
!          PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
1 ^        PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
1 | ~      PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
(          PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
~(         PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
!!         PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
1 & ;x     PHP:  syntax error, unexpected END_OF_LINE in INI on line 2
P=14
(1         PHP:  syntax error, unexpected END_OF_LINE, expecting '^' or '|' or '&' or ')' in INI on line 2
P=14
((1)       PHP:  syntax error, unexpected END_OF_LINE, expecting '^' or '|' or '&' or ')' in INI on line 2
P=14
(E_ALL     PHP:  syntax error, unexpected END_OF_LINE, expecting '^' or '|' or '&' or ')' in INI on line 2
P=14
( 1 | 2    PHP:  syntax error, unexpected END_OF_LINE, expecting '^' or '|' or '&' or ')' in INI on line 2
P=14
(1 2       PHP:  syntax error, unexpected END_OF_LINE, expecting '^' or '|' or '&' or ')' in INI on line 2
P=14
(1~2)      PHP:  syntax error, unexpected '~', expecting '^' or '|' or '&' or ')' in INI on line 1
P=14
--CLEAN--
<?php
unset($phl, $ini, $cases, $value);
