--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
copy() resolves a name's wrapper every time it touches the name, and a failed stat is not a refusal
--DESCRIPTION--
php's copy() looks a wrapper up for the source three times (its open_basedir
screen, the stat, the open) and for the destination twice (the stat, the open),
and a scheme nobody is registered under warns on every lookup: five lines for two
unknown schemes, three for the source alone, two for the destination alone. This
engine resolved once per name and warned once per name.

Under that count sat two more of php's rules. A stat that FAILS is a
'non-statable stream' php walks past, straight to the open -- so a missing
source is reported by the open, and a directory destination behind it is never
looked at, where this engine refused the destination first. And the stat goes
through the wrapper that owns the name: a userland wrapper with no url_stat says
so, once for each end, before the open fails.
--SKIPIF--
<?php
if (DIRECTORY_SEPARATOR === '\\') {
    die("skip a directory named 'zzz:' cannot exist on Windows");
}
?>
--FILE--
<?php
set_error_handler(function ($no, $msg) {
    if (!(error_reporting() & $no)) { return false; }
    echo "W: $msg\n"; return true;
});
class NoStat { public $context; function stream_open($p, $m, $o, &$op) { return false; } }
stream_wrapper_register('nostat', 'NoStat');

$cwd = getcwd();
$dir = sys_get_temp_dir() . '/phl_copy_wrappers_' . getmypid();
@mkdir($dir . '/zzz:/sub', 0777, true);
@mkdir($dir . '/plaindir');
file_put_contents($dir . '/zzz:/hit.txt', "hit\n");
file_put_contents($dir . '/plain.txt', "plain\n");
chdir($dir);

echo "--- unknown source, unknown destination, source missing\n";
var_dump(copy('zzz://nope', 'zzz://out.txt'));
echo "--- unknown source, unknown destination, source present\n";
var_dump(copy('zzz://hit.txt', 'zzz://out.txt'));
var_dump(file_get_contents('zzz:/out.txt'));
echo "--- one file under two unknown-scheme names\n";
var_dump(copy('zzz://hit.txt', 'zzz://hit.txt'));
echo "--- plain source, unknown destination that is a directory\n";
var_dump(copy('plain.txt', 'zzz://sub'));
echo "--- missing plain source, plain destination that is a directory\n";
var_dump(copy('nope.txt', 'plaindir'));
echo "--- a wrapper with no url_stat\n";
var_dump(copy('nostat://x', 'out2.txt'));
var_dump(copy('plain.txt', 'nostat://y'));

chdir($cwd);
unlink($dir . '/zzz:/out.txt');
unlink($dir . '/zzz:/hit.txt');
rmdir($dir . '/zzz:/sub');
rmdir($dir . '/zzz:');
rmdir($dir . '/plaindir');
unlink($dir . '/plain.txt');
rmdir($dir);
echo "done\n";
--EXPECT--
--- unknown source, unknown destination, source missing
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(zzz://nope): Failed to open stream: No such file or directory
bool(false)
--- unknown source, unknown destination, source present
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(true)
string(4) "hit
"
--- one file under two unknown-scheme names
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
bool(false)
--- plain source, unknown destination that is a directory
W: copy(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
W: copy(): The second argument to copy() function cannot be a directory
bool(false)
--- missing plain source, plain destination that is a directory
W: copy(nope.txt): Failed to open stream: No such file or directory
bool(false)
--- a wrapper with no url_stat
W: copy(): NoStat::url_stat is not implemented!
W: copy(nostat://x): Failed to open stream: "NoStat::stream_open" call failed
bool(false)
W: copy(): NoStat::url_stat is not implemented!
W: copy(nostat://y): Failed to open stream: "NoStat::stream_open" call failed
bool(false)
done
