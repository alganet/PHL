--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/pcntl: the inventory, the status macros, the remembered errno and the screens
--SKIPIF--
<?php
/* php builds no ext/pcntl on Windows, and neither does this. */
if (!extension_loaded('pcntl')) {
    die("skip this build has no ext/pcntl (php has none on Windows either)\n");
}
if (PHP_OS_FAMILY !== 'Linux') {
    die("skip the inventory pinned here is Linux's; php builds fewer names elsewhere\n");
}
--FILE--
<?php
/* The VALUES here are the platform's -- SIGUSR1 is 10 on Linux and 30 on macOS,
 * a cpu count is the box's -- so what is pinned is the SHAPE: which names exist
 * in which order, which screen each argument gets, which call remembers an
 * errno, and the relations between two answers. */
function t(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-42s %s\n", $l, str_replace("\n", '', var_export($r, true)));
}
set_error_handler(function (int $no, string $msg): bool {
    printf("  [%d] %s\n", $no, $msg);
    return true;
});

echo "-- the extension, with php's names in php's own order\n";
var_dump(extension_loaded('pcntl'));
echo implode("\n", get_extension_funcs('pcntl')), "\n";
echo "-- php registers this enum on every platform, even where nothing reads it\n";
var_dump(enum_exists('Pcntl\QosClass'));
echo implode(',', array_column(Pcntl\QosClass::cases(), 'name')), "\n";

echo "-- the status macros are the C macros, with no screen in front of them\n";
$exited = 7 << 8;                       /* what a child that returned 7 leaves */
t('an exit status IS an exit',    fn() => pcntl_wifexited($exited));
t('...and carries the code',      fn() => pcntl_wexitstatus($exited));
t('...it was not signalled',      fn() => pcntl_wifsignaled($exited));
t('...nor stopped',               fn() => pcntl_wifstopped($exited));
t('...nor continued',             fn() => pcntl_wifcontinued($exited));
t('wstopsig reads the SAME byte', fn() => pcntl_wstopsig($exited));
t('a killed status is signalled', fn() => pcntl_wifsignaled(SIGTERM));
t('...and names the signal',      fn() => pcntl_wtermsig(SIGTERM) === SIGTERM);
t('a nonsense status still answers', fn() => [pcntl_wexitstatus(-1), pcntl_wtermsig(-1),
    pcntl_wstopsig(-1), pcntl_wifexited(-1), pcntl_wifsignaled(-1)]);

echo "-- pcntl_signal's two bounds are worded SEPARATELY; get_handler's are one\n";
t('signal 0',                     fn() => pcntl_signal(0, SIG_IGN));
t('signal -1',                    fn() => pcntl_signal(-1, SIG_IGN));
t('one past the top',             fn() => pcntl_signal(PHP_INT_MAX, SIG_IGN));
t('get_handler 0',                fn() => pcntl_signal_get_handler(0));
t('get_handler past the top',     fn() => pcntl_signal_get_handler(PHP_INT_MAX));
t('an int that is neither',       fn() => pcntl_signal(SIGURG, 42));
t('a name nothing declares',      fn() => pcntl_signal(SIGURG, 'no-such-function-8a1f9b'));
t('null is not a callable',       fn() => pcntl_signal(SIGURG, null));
t('nor is true',                  fn() => pcntl_signal(SIGURG, true));
t('nor a float',                  fn() => pcntl_signal(SIGURG, 1.5));
t('nor a broken array callable',  fn() => pcntl_signal(SIGURG, [new stdClass, 'x']));
t('a plain function name IS one', fn() => pcntl_signal(SIGURG, 'strlen'));
t('...and answers itself back',   fn() => pcntl_signal_get_handler(SIGURG));
t('SIG_IGN reads back as 1',      function () { pcntl_signal(SIGURG, SIG_IGN);
    return pcntl_signal_get_handler(SIGURG) === SIG_IGN; });
t('SIG_DFL reads back as 0',      function () { pcntl_signal(SIGURG, SIG_DFL);
    return pcntl_signal_get_handler(SIGURG) === SIG_DFL; });
t('a signal nobody spoke for',    fn() => pcntl_signal_get_handler(SIGWINCH) === SIG_DFL);

echo "-- sigprocmask screens the MODE, then every element of the array\n";
t('a mode that is none of three', fn() => pcntl_sigprocmask(99, [SIGURG]));
t('an out-of-range signal',       fn() => pcntl_sigprocmask(SIG_BLOCK, [9999]));
t('a signal that is a string',    fn() => pcntl_sigprocmask(SIG_BLOCK, ['x']));
t('an empty set is a valid mask', function () { $r = pcntl_sigprocmask(SIG_SETMASK, [], $old);
    return [$r, $old]; });

echo "-- sigtimedwait screens its two halves and their SUM\n";
t('negative seconds',             fn() => pcntl_sigtimedwait([SIGURG], $i, -1, 0));
t('negative nanoseconds',         fn() => pcntl_sigtimedwait([SIGURG], $i, 0, -1));
t('a whole second of them',       fn() => pcntl_sigtimedwait([SIGURG], $i, 0, 2000000000));
t('no time at all',               fn() => pcntl_sigtimedwait([SIGURG], $i, 0, 0));
t('nothing arrives in a moment',  fn() => pcntl_sigtimedwait([SIGURG], $i, 0, 1000));
t('...and that is not a FAILURE', fn() => pcntl_get_last_error() === 0);

echo "-- priority: the mode is screened, the process is not\n";
t('a mode php does not name',     fn() => pcntl_getpriority(null, 99));
t('...the setter says #3',        fn() => pcntl_setpriority(0, null, 99));
t('our own priority is an int',   fn() => is_int(pcntl_getpriority()));
t('...and null means us',         fn() => pcntl_getpriority(null) === pcntl_getpriority(0));
t('a process that is not there',  fn() => pcntl_getpriority(999999));
t('...and remembers ESRCH',       fn() => pcntl_strerror(pcntl_get_last_error()));
t('setting our own to what it is',fn() => pcntl_setpriority(pcntl_getpriority()));

echo "-- the remembered errno is pcntl's OWN, separate from ext/posix's\n";
t('a wait with no children',      fn() => pcntl_wait($s, WNOHANG));
t('...stores ECHILD',             fn() => pcntl_get_last_error() === PCNTL_ECHILD);
t('...and the alias agrees',      fn() => pcntl_errno() === pcntl_get_last_error());
t('a SUCCESS leaves it standing', function () { $was = pcntl_get_last_error();
    pcntl_alarm(0); return pcntl_get_last_error() === $was; });
t('posix keeps a different one',  function () { posix_kill(999999, 0);
    return pcntl_get_last_error() !== posix_get_last_error(); });
t('strerror of no error at all',  fn() => pcntl_strerror(0));

echo "-- pcntl_exec: three different sentences for a NUL, and php's own casts\n";
t('a NUL in the path',            fn() => pcntl_exec("a\0b"));
t('a NUL in an argument',         fn() => pcntl_exec('/bin/true', ["a\0b"]));
t('a NUL in a variable NAME',     fn() => pcntl_exec('/bin/true', [], ["K\0" => 'V']));
t('a NUL in its VALUE',           fn() => pcntl_exec('/bin/true', [], ['K' => "V\0"]));
t('an array argument is `Array`', fn() => pcntl_exec('/no/such/thing/8a1f9b', [[]]));
t('...once for each of them',     fn() => pcntl_exec('/no/such/thing/8a1f9b', [[], []]));
t('an object with no __toString', fn() => pcntl_exec('/no/such/thing/8a1f9b', [new stdClass]));
t('...in the environment too',    fn() => pcntl_exec('/no/such/thing/8a1f9b', [], ['K' => new stdClass]));
t('a path nothing can run',       fn() => pcntl_exec('/no/such/thing/8a1f9b'));
t('...and remembers ENOENT',      fn() => pcntl_get_last_error() === PCNTL_ENOENT);

echo "-- alarm answers what was LEFT on the previous one\n";
t('nothing was pending',          fn() => pcntl_alarm(30));
t('...now thirty seconds are',    fn() => pcntl_alarm(0));
t('...and cancelling twice',      fn() => pcntl_alarm(0));

echo "-- async_signals answers the OLD setting, and null only asks\n";
t('it starts off',                fn() => pcntl_async_signals());
t('turning it on answers false',  fn() => pcntl_async_signals(true));
t('...and it is on now',          fn() => pcntl_async_signals(null));
t('turning it off answers true',  fn() => pcntl_async_signals(false));
t('...and it is off again',       fn() => pcntl_async_signals());
?>
--EXPECT--
-- the extension, with php's names in php's own order
bool(true)
pcntl_fork
pcntl_waitpid
pcntl_waitid
pcntl_wait
pcntl_signal
pcntl_signal_get_handler
pcntl_signal_dispatch
pcntl_sigprocmask
pcntl_sigwaitinfo
pcntl_sigtimedwait
pcntl_wifexited
pcntl_wifstopped
pcntl_wifcontinued
pcntl_wifsignaled
pcntl_wexitstatus
pcntl_wtermsig
pcntl_wstopsig
pcntl_exec
pcntl_alarm
pcntl_get_last_error
pcntl_errno
pcntl_getpriority
pcntl_setpriority
pcntl_strerror
pcntl_async_signals
pcntl_unshare
pcntl_getcpuaffinity
pcntl_setcpuaffinity
pcntl_getcpu
-- php registers this enum on every platform, even where nothing reads it
bool(true)
UserInteractive,UserInitiated,Default,Utility,Background
-- the status macros are the C macros, with no screen in front of them
an exit status IS an exit                  true
...and carries the code                    7
...it was not signalled                    false
...nor stopped                             false
...nor continued                           false
wstopsig reads the SAME byte               7
a killed status is signalled               true
...and names the signal                    true
a nonsense status still answers            array (  0 => 255,  1 => 127,  2 => 255,  3 => false,  4 => false,)
-- pcntl_signal's two bounds are worded SEPARATELY; get_handler's are one
signal 0                                   'ValueError: pcntl_signal(): Argument #1 ($signal) must be greater than or equal to 1'
signal -1                                  'ValueError: pcntl_signal(): Argument #1 ($signal) must be greater than or equal to 1'
one past the top                           'ValueError: pcntl_signal(): Argument #1 ($signal) must be less than 65'
get_handler 0                              'ValueError: pcntl_signal_get_handler(): Argument #1 ($signal) must be between 1 and 64'
get_handler past the top                   'ValueError: pcntl_signal_get_handler(): Argument #1 ($signal) must be between 1 and 64'
an int that is neither                     'ValueError: pcntl_signal(): Argument #2 ($handler) must be either SIG_DFL or SIG_IGN when an integer value is given'
a name nothing declares                    'TypeError: pcntl_signal(): Argument #2 ($handler) must be of type callable|int, string given'
null is not a callable                     'TypeError: pcntl_signal(): Argument #2 ($handler) must be of type callable|int, null given'
nor is true                                'TypeError: pcntl_signal(): Argument #2 ($handler) must be of type callable|int, true given'
nor a float                                'TypeError: pcntl_signal(): Argument #2 ($handler) must be of type callable|int, float given'
nor a broken array callable                'TypeError: pcntl_signal(): Argument #2 ($handler) must be of type callable|int, array given'
a plain function name IS one               true
...and answers itself back                 'strlen'
SIG_IGN reads back as 1                    true
SIG_DFL reads back as 0                    true
a signal nobody spoke for                  true
-- sigprocmask screens the MODE, then every element of the array
a mode that is none of three               'ValueError: pcntl_sigprocmask(): Argument #1 ($mode) must be one of SIG_BLOCK, SIG_UNBLOCK, or SIG_SETMASK'
an out-of-range signal                     'ValueError: pcntl_sigprocmask(): Argument #2 ($signals) signals must be between 1 and 64'
a signal that is a string                  'TypeError: pcntl_sigprocmask(): Argument #2 ($signals) signals must be of type int, string given'
an empty set is a valid mask               array (  0 => true,  1 =>   array (  ),)
-- sigtimedwait screens its two halves and their SUM
negative seconds                           'ValueError: pcntl_sigtimedwait(): Argument #3 ($seconds) must be greater than or equal to 0'
negative nanoseconds                       'ValueError: pcntl_sigtimedwait(): Argument #4 ($nanoseconds) must be between 0 and 1e9'
a whole second of them                     'ValueError: pcntl_sigtimedwait(): Argument #4 ($nanoseconds) must be between 0 and 1e9'
no time at all                             'ValueError: pcntl_sigtimedwait(): At least one of argument #3 ($seconds) or argument #4 ($nanoseconds) must be greater than 0'
nothing arrives in a moment                false
...and that is not a FAILURE               false
-- priority: the mode is screened, the process is not
a mode php does not name                   'ValueError: pcntl_getpriority(): Argument #2 ($mode) must be one of PRIO_PGRP, PRIO_USER, or PRIO_PROCESS'
...the setter says #3                      'ValueError: pcntl_setpriority(): Argument #3 ($mode) must be one of PRIO_PGRP, PRIO_USER, or PRIO_PROCESS'
our own priority is an int                 true
...and null means us                       true
  [2] pcntl_getpriority(): Error 3: No process was located using the given parameters
a process that is not there                false
...and remembers ESRCH                     'No such process'
setting our own to what it is              true
-- the remembered errno is pcntl's OWN, separate from ext/posix's
a wait with no children                    -1
...stores ECHILD                           true
...and the alias agrees                    true
a SUCCESS leaves it standing               true
posix keeps a different one                true
strerror of no error at all                'Success'
-- pcntl_exec: three different sentences for a NUL, and php's own casts
a NUL in the path                          'ValueError: pcntl_exec(): Argument #1 ($path) must not contain any null bytes'
a NUL in an argument                       'ValueError: pcntl_exec(): Argument #2 ($args) individual argument must not contain null bytes'
a NUL in a variable NAME                   'ValueError: pcntl_exec(): Argument #3 ($env_vars) name for environment variable must not contain null bytes'
a NUL in its VALUE                         'ValueError: pcntl_exec(): Argument #3 ($env_vars) value for environment variable must not contain null bytes'
  [2] Array to string conversion
  [2] pcntl_exec(): Error has occurred: (errno 2) No such file or directory
an array argument is `Array`               false
  [2] Array to string conversion
  [2] Array to string conversion
  [2] pcntl_exec(): Error has occurred: (errno 2) No such file or directory
...once for each of them                   false
an object with no __toString               'Error: Object of class stdClass could not be converted to string'
...in the environment too                  'Error: Object of class stdClass could not be converted to string'
  [2] pcntl_exec(): Error has occurred: (errno 2) No such file or directory
a path nothing can run                     false
...and remembers ENOENT                    true
-- alarm answers what was LEFT on the previous one
nothing was pending                        0
...now thirty seconds are                  30
...and cancelling twice                    0
-- async_signals answers the OLD setting, and null only asks
it starts off                              false
turning it on answers false                false
...and it is on now                        true
turning it off answers true                true
...and it is off again                     false
