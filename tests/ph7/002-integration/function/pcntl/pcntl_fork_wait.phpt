--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/pcntl: fork, the wait family, the resource-usage shape and SA_RESTART
--SKIPIF--
<?php
if (!extension_loaded('pcntl') || !extension_loaded('posix')) {
    die("skip needs ext/pcntl and ext/posix (php has neither on Windows)\n");
}
--FILE--
<?php
/* Nothing here prints a pid, a duration or a usage NUMBER -- those are the
 * box's. What is contract is which call answers which pid, php's key order for
 * the usage array, and whether a wait interrupted by a signal resumes. */
function line(string $l, $v): void {
    printf("%-42s %s\n", $l, str_replace("\n", '', var_export($v, true)));
}

echo "-- fork answers 0 in the child and the child's pid in the parent\n";
$pid = pcntl_fork();
if ($pid === 0) {
    /* A forked child of an interpreter is a whole interpreter: it can still
     * allocate, still run PHP, and still exit cleanly. */
    $x = array_sum(range(1, 100));
    exit($x === 5050 ? 7 : 1);
}
line('the parent got a real pid', $pid > 0);
$got = pcntl_waitpid($pid, $status);
line('waitpid answers that pid', $got === $pid);
line('it exited', pcntl_wifexited($status));
line('...with the code it chose', pcntl_wexitstatus($status));
line('...and was not signalled', pcntl_wifsignaled($status));

echo "-- a child that is KILLED reports the signal instead\n";
$pid = pcntl_fork();
if ($pid === 0) { usleep(2000000); exit(0); }
usleep(50000);
posix_kill($pid, SIGKILL);
pcntl_waitpid($pid, $status);
line('it did not exit', pcntl_wifexited($status));
line('it was signalled', pcntl_wifsignaled($status));
line('...by the one we sent', pcntl_wtermsig($status) === SIGKILL);

echo "-- pcntl_wait is pcntl_waitpid(-1)\n";
$pid = pcntl_fork();
if ($pid === 0) { exit(3); }
$got = pcntl_wait($status);
line('wait finds the only child', $got === $pid);
line('...and its code', pcntl_wexitstatus($status));

echo "-- WNOHANG answers 0 while the child is still running\n";
$pid = pcntl_fork();
if ($pid === 0) { usleep(400000); exit(0); }
line('nothing to reap yet', pcntl_waitpid($pid, $status, WNOHANG));
pcntl_waitpid($pid, $status);
line('...and then there is', pcntl_wifexited($status));

echo "-- the fourth argument is php's seventeen usage keys, in php's order\n";
$pid = pcntl_fork();
if ($pid === 0) { exit(0); }
pcntl_waitpid($pid, $status, 0, $usage);
line('the key order', array_keys($usage));
line('...all integers', array_unique(array_map('gettype', $usage)));

echo "-- pcntl_waitid answers a bool and fills a SIGINFO array\n";
$pid = pcntl_fork();
if ($pid === 0) { exit(4); }
line('it succeeded', pcntl_waitid(P_PID, $pid, $info, WEXITED, $usage2));
/* utime/stime exist only where siginfo_t carries them (glibc does, macOS not) */
line('the siginfo keys for a child', array_values(array_diff(array_keys($info), ['utime', 'stime'])));
/* php on macOS defines no CLD_* names at all */
line('it was a child that EXITED', [$info['signo'] === SIGCHLD, !defined('CLD_EXITED') || $info['code'] === CLD_EXITED]);
line('...carrying its exit code', $info['status']);
/* php fills it only through Linux's waitid syscall; elsewhere it stays null */
line('the usage keys again', $usage2 === null || array_keys($usage2) === array_keys($usage));
line('nothing left to wait for', pcntl_waitid(P_ALL, 0, $i2, WEXITED | WNOHANG));

echo "-- a failed wait answers -1, stores ECHILD and still writes the status\n";
$status = 'untouched';
line('no such child', pcntl_waitpid(999999, $status));
line('...the status came back 0', $status);
line('...and ECHILD is remembered', pcntl_get_last_error() === PCNTL_ECHILD);

echo "-- restart_syscalls decides whether an interrupted wait resumes\n";
/* The child signals its parent halfway through and exits at the end; the
 * parent is inside pcntl_waitpid the whole time. With SA_RESTART the wait
 * resumes and reaps the child, without it the wait answers -1/EINTR. */
foreach ([true, false] as $restart) {
    pcntl_signal(SIGURG, function (): void {}, $restart);
    $pid = pcntl_fork();
    if ($pid === 0) { usleep(100000); posix_kill(posix_getppid(), SIGURG); usleep(100000); exit(0); }
    $got = pcntl_waitpid($pid, $status);
    line($restart ? 'SA_RESTART resumes the wait' : 'without it the wait is cut short',
        $restart ? $got === $pid : $got === -1);
    if ($got === -1) {
        line('...and EINTR is what it saw', pcntl_get_last_error() === PCNTL_EINTR);
        pcntl_waitpid($pid, $status);
    }
    pcntl_signal_dispatch();
}
pcntl_signal(SIGURG, SIG_DFL);

echo "-- a handler installed before the fork is still installed after it\n";
$ran = 0;
pcntl_signal(SIGURG, function () use (&$ran): void { $ran++; });
$pid = pcntl_fork();
if ($pid === 0) {
    posix_kill(posix_getpid(), SIGURG);
    pcntl_signal_dispatch();
    exit($ran === 1 ? 11 : 12);
}
pcntl_waitpid($pid, $status);
line('the child ran its own copy', pcntl_wexitstatus($status));
line('...and the parent ran none', $ran);
pcntl_signal(SIGURG, SIG_DFL);
?>
--EXPECT--
-- fork answers 0 in the child and the child's pid in the parent
the parent got a real pid                  true
waitpid answers that pid                   true
it exited                                  true
...with the code it chose                  7
...and was not signalled                   false
-- a child that is KILLED reports the signal instead
it did not exit                            false
it was signalled                           true
...by the one we sent                      true
-- pcntl_wait is pcntl_waitpid(-1)
wait finds the only child                  true
...and its code                            3
-- WNOHANG answers 0 while the child is still running
nothing to reap yet                        0
...and then there is                       true
-- the fourth argument is php's seventeen usage keys, in php's order
the key order                              array (  0 => 'ru_oublock',  1 => 'ru_inblock',  2 => 'ru_msgsnd',  3 => 'ru_msgrcv',  4 => 'ru_maxrss',  5 => 'ru_ixrss',  6 => 'ru_idrss',  7 => 'ru_minflt',  8 => 'ru_majflt',  9 => 'ru_nsignals',  10 => 'ru_nvcsw',  11 => 'ru_nivcsw',  12 => 'ru_nswap',  13 => 'ru_utime.tv_usec',  14 => 'ru_utime.tv_sec',  15 => 'ru_stime.tv_usec',  16 => 'ru_stime.tv_sec',)
...all integers                            array (  'ru_oublock' => 'integer',)
-- pcntl_waitid answers a bool and fills a SIGINFO array
it succeeded                               true
the siginfo keys for a child               array (  0 => 'signo',  1 => 'errno',  2 => 'code',  3 => 'status',  4 => 'pid',  5 => 'uid',)
it was a child that EXITED                 array (  0 => true,  1 => true,)
...carrying its exit code                  4
the usage keys again                       true
nothing left to wait for                   false
-- a failed wait answers -1, stores ECHILD and still writes the status
no such child                              -1
...the status came back 0                  0
...and ECHILD is remembered                true
-- restart_syscalls decides whether an interrupted wait resumes
SA_RESTART resumes the wait                true
without it the wait is cut short           true
...and EINTR is what it saw                true
-- a handler installed before the fork is still installed after it
the child ran its own copy                 11
...and the parent ran none                 0
