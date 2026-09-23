--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
exec()/system()/passthru(): what each one answers and where the output goes
--FILE--
<?php
/* `echo` is the one command both shells have, so this half of the family's
 * coverage runs on cmd.exe as well; the line-by-line rules need printf and are
 * POSIX-only in exec_lines_posix.phpt. */
$r = exec('echo hi', $out, $code);
var_dump($r, $out, $code);

/* system() WRITES the output as it arrives and answers the last line */
$r = system('echo sys', $code2);
var_dump($r, $code2);

/* passthru() writes and answers NULL -- false is reserved for a failed fork */
$r = passthru('echo pass', $code3);
var_dump($r, $code3);

/* An empty command is refused by all four runners rather than handed to a
 * shell that would answer success for it. */
foreach (['exec', 'system', 'passthru', 'shell_exec'] as $f) {
    try { $f(''); } catch (ValueError $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}
?>
--EXPECT--
string(2) "hi"
array(1) {
  [0]=>
  string(2) "hi"
}
int(0)
sys
string(3) "sys"
int(0)
pass
NULL
int(0)
ValueError: exec(): Argument #1 ($command) must not be empty
ValueError: system(): Argument #1 ($command) must not be empty
ValueError: passthru(): Argument #1 ($command) must not be empty
ValueError: shell_exec(): Argument #1 ($command) must not be empty
