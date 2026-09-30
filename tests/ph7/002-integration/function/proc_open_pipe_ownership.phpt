--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A proc_open() handle owns the pipes it made, and the next child does not inherit them
--DESCRIPTION--
php's process resource OWNS the parent ends it created: its destructor closes every
one of them BEFORE it waits, with the comment "Close all pipes first, so that the
child may exit" -- and `proc_close()` IS that destructor, so afterwards the script's
own `$pipes` entries are closed resources and so is the handle. PHL waited first and
never closed anything, which deadlocks with any child that reads its stdin.

The second half is php's `fcntl(parentend, F_SETFD, FD_CLOEXEC)`. Without it the NEXT
proc_open()'s child inherits a copy of the FIRST child's stdin, so that child never
sees EOF however carefully the script closes its own end, and the first proc_close()
waits forever. monolog's ProcessHandler suite has two such handlers alive at once: it
ran to its last assertion and then hung the interpreter, every time.

Three smaller answers came with them: the three verbs name a bad argument the way php
names it (a TypeError, one sentence for a resource that is not a live process and
php's ordinary argument sentence for something that is not a resource at all), and a
read from a NON-BLOCKING pipe with nothing on it is an empty read rather than a
failure -- EAGAIN means "not now", and php raises no diagnostic for it, where PHL
latched it and reported `Read of 8192 bytes failed with errno=11` on every poll.
--SKIPIF--
<?php
if (PHP_OS_FAMILY === 'Windows') { echo 'skip proc_open() pipes are a POSIX shape here'; }
?>
--FILE--
<?php
function t(string $label, callable $fn): void {
    try { $r = $fn(); echo $label, ' => '; var_dump($r); }
    catch (Throwable $e) { echo $label, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
$spec = [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']];

echo "== proc_close() owns the pipes it made ==\n";
$p = proc_open('cat', $spec, $pipes);
var_dump(is_resource($p), is_resource($pipes[0]), is_resource($pipes[1]), is_resource($pipes[2]));
var_dump(proc_close($p));
echo "after close: ";
var_dump(is_resource($p), is_resource($pipes[0]), is_resource($pipes[1]), is_resource($pipes[2]));
t('a second close', fn() => proc_close($p));
t('get_status after close', fn() => proc_get_status($p));
t('terminate after close', fn() => proc_terminate($p));
t('fclose a pipe it closed', fn() => fclose($pipes[0]));

echo "== and it still works when the script closed them first ==\n";
$q = proc_open('cat', $spec, $qpipes);
foreach ($qpipes as $pipe) { fclose($pipe); }
var_dump(proc_close($q));

echo "== the wrong resource, and no resource at all ==\n";
$fh = fopen('php://memory', 'w');
t('proc_close(file handle)', fn() => proc_close($fh));
t('proc_get_status(file handle)', fn() => proc_get_status($fh));
t('proc_terminate(file handle)', fn() => proc_terminate($fh));
t('proc_close(int)', fn() => proc_close(5));
fclose($fh);

echo "== a second child does not inherit the first one's pipes ==\n";
/* Without close-on-exec on the parent ends, the SECOND child holds a copy of the
 * first child's stdin, so the first never sees EOF and its proc_close() waits
 * forever. Both are closed here in creation order, which is the order that hangs. */
$a = proc_open('cat', $spec, $ap);
$b = proc_open('cat', $spec, $bp);
fwrite($ap[0], "one\n");
fwrite($bp[0], "two\n");
fclose($ap[0]);
echo 'first child said: ', trim((string) stream_get_contents($ap[1])), "\n";
var_dump(proc_close($a));
fclose($bp[0]);
echo 'second child said: ', trim((string) stream_get_contents($bp[1])), "\n";
var_dump(proc_close($b));

echo "== a non-blocking pipe with nothing on it reads empty, quietly ==\n";
$c = proc_open('cat', $spec, $cp);
foreach ($cp as $pipe) { stream_set_blocking($pipe, false); }
var_dump(stream_get_contents($cp[2]));
var_dump(fread($cp[1], 8192));
var_dump(proc_close($c));
echo "done\n";
?>
--EXPECT--
== proc_close() owns the pipes it made ==
bool(true)
bool(true)
bool(true)
bool(true)
int(0)
after close: bool(false)
bool(false)
bool(false)
bool(false)
a second close => TypeError: proc_close(): supplied resource is not a valid process resource
get_status after close => TypeError: proc_get_status(): supplied resource is not a valid process resource
terminate after close => TypeError: proc_terminate(): supplied resource is not a valid process resource
fclose a pipe it closed => TypeError: fclose(): Argument #1 ($stream) must be an open stream resource
== and it still works when the script closed them first ==
int(0)
== the wrong resource, and no resource at all ==
proc_close(file handle) => TypeError: proc_close(): supplied resource is not a valid process resource
proc_get_status(file handle) => TypeError: proc_get_status(): supplied resource is not a valid process resource
proc_terminate(file handle) => TypeError: proc_terminate(): supplied resource is not a valid process resource
proc_close(int) => TypeError: proc_close(): Argument #1 ($process) must be of type resource, int given
== a second child does not inherit the first one's pipes ==
first child said: one
int(0)
second child said: two
int(0)
== a non-blocking pipe with nothing on it reads empty, quietly ==
string(0) ""
string(0) ""
int(0)
done
