--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/posix: the inventory, the array shapes, the remembered errno and the screens
--SKIPIF--
<?php
/* php builds no ext/posix on Windows, and neither does this. */
if (!extension_loaded('posix')) {
    die("skip this build has no ext/posix (php has none on Windows either)\n");
}
/* uname()'s domainname, strerror(0)'s wording and the rlimit/pathconf answers
 * pinned here are glibc's; another libc answers its own. */
if (PHP_OS_FAMILY !== 'Linux') {
    die("skip the answers pinned here are Linux's\n");
}
--FILE--
<?php
/* Everything here is either a RELATION between two answers or a SHAPE, because
 * the values themselves are the box's: a uid, a group name and a resource limit
 * are all different on the next machine. What is contract is php's key ORDER,
 * which errno each call remembers, and the four argument screens. */
function t(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-42s %s\n", $l, str_replace("\n", '', var_export($r, true)));
}
/* The descriptor screens raise WARNINGS, and a warning names the file it was
 * raised in -- which is the runner's scratch copy. The handler keeps the
 * sentence and drops the location. */
set_error_handler(function (int $no, string $msg): bool {
    printf("  [%d] %s\n", $no, $msg);
    return true;
});

echo "-- the extension, with php's 41 names in php's own order\n";
var_dump(extension_loaded('posix'));
echo implode("\n", get_extension_funcs('posix')), "\n";

echo "-- the identity calls answer each other\n";
t('getpid is the pid',            fn() => posix_getpid() === getmypid());
t('a parent exists',              fn() => posix_getppid() > 0);
t('the four ids are ints',        fn() => array_map('is_int',
    [posix_getuid(), posix_geteuid(), posix_getgid(), posix_getegid()]) === [true, true, true, true]);
t('getpwuid(getuid()) is us',     fn() => posix_getpwuid(posix_getuid())['uid'] === posix_getuid());
t('...and names itself back',     fn() => posix_getpwnam(posix_getpwuid(posix_getuid())['name'])['uid'] === posix_getuid());
t('getgrgid(getgid()) is ours',   fn() => posix_getgrgid(posix_getgid())['gid'] === posix_getgid());
t('getgroups is a list of ints',  function () {
    $g = posix_getgroups();
    return $g !== false && $g === array_values($g) && $g === array_filter($g, 'is_int');
});
t('getcwd agrees with getcwd()',  fn() => posix_getcwd() === getcwd());
t('getpgid(0) is the group',      fn() => posix_getpgid(0) === posix_getpgrp());
t('getsid(0) is a session',       fn() => posix_getsid(0) > 0);
t('signal 0 finds us',            fn() => posix_kill(posix_getpid(), 0));
t('getlogin is a string or false',fn() => is_string(@posix_getlogin()) || @posix_getlogin() === false);
t('ctermid is a path',            fn() => str_starts_with(posix_ctermid(), '/'));

echo "-- php's key ORDER, which is the contract\n";
t('uname',                        fn() => array_keys(posix_uname()));
t('times',                        fn() => array_keys(posix_times()));
t('...all integers',              fn() => array_unique(array_map('gettype', posix_times())));
t('getpwnam',                     fn() => array_keys(posix_getpwuid(posix_getuid())));
t('getgrnam, members THIRD',      fn() => array_keys(posix_getgrgid(posix_getgid())));
t('...members is a list',         fn() => is_array(posix_getgrgid(posix_getgid())['members']));
t('getrlimit, ten pairs',         fn() => array_keys(posix_getrlimit()));
t('one resource is a LIST',       fn() => array_keys(posix_getrlimit(POSIX_RLIMIT_CORE)));
t('...int or the string',         function () {
    foreach (posix_getrlimit() as $v) { if (!is_int($v) && $v !== 'unlimited') { return false; } }
    return true;
});
t('an unknown resource',          fn() => posix_getrlimit(999999));

echo "-- what is NOT there answers false\n";
t('getpwnam of nobody',           fn() => posix_getpwnam('no-such-user-8a1f9b'));
t('getpwuid of nobody',           fn() => posix_getpwuid(999999));
t('getgrnam of no group',         fn() => posix_getgrnam('no-such-group-8a1f9b'));
t('getgrgid of no group',         fn() => posix_getgrgid(999999));
t('getpgid of no process',        fn() => posix_getpgid(999999));
t('getsid of no process',         fn() => posix_getsid(999999));
t('kill of no process',           fn() => posix_kill(999999, 0));
t('access of no file',            fn() => posix_access('/no/such/thing/8a1f9b'));
t('pathconf of no file',          fn() => posix_pathconf('/no/such/thing/8a1f9b', POSIX_PC_NAME_MAX));
t('mkfifo into no directory',     fn() => posix_mkfifo('/no/such/dir/8a1f9b/f', 0644));

echo "-- the remembered errno: which calls store one, and which leave it\n";
$reset = function (): void { posix_kill(999999, 0); };   /* leaves ESRCH */
$esrch = posix_get_last_error();
t('a failure stores its reason',  fn() => posix_strerror($esrch));
t('...and the alias agrees',      fn() => posix_errno() === $esrch);
t('a SUCCESS leaves it standing', function () use ($esrch) { posix_getpid(); posix_access('.');
    return posix_get_last_error() === $esrch; });
t('a name that is not there ZEROES it', function () { posix_getpwnam('no-such-user-8a1f9b');
    return posix_get_last_error(); });
t('sysconf stores nothing',       function () use ($reset, $esrch) { $reset();
    posix_sysconf(999999); return posix_get_last_error() === $esrch; });
t('times stores nothing',         function () use ($reset, $esrch) { $reset();
    posix_times(); return posix_get_last_error() === $esrch; });
t('uname stores nothing',         function () use ($reset, $esrch) { $reset();
    posix_uname(); return posix_get_last_error() === $esrch; });
t('a getcwd stores nothing',      function () use ($reset, $esrch) { $reset();
    posix_getcwd(); return posix_get_last_error() === $esrch; });
t('initgroups stores nothing',    function () use ($reset, $esrch) { $reset();
    @posix_initgroups('no-such-user-8a1f9b', 0); return posix_get_last_error() === $esrch; });
t('an OK pathconf stores nothing',function () use ($reset, $esrch) { $reset();
    posix_pathconf('.', POSIX_PC_NAME_MAX); return posix_get_last_error() === $esrch; });
t('a BAD pathconf stores EINVAL', function () use ($reset) { $reset();
    posix_pathconf('.', 999999); return posix_strerror(posix_get_last_error()); });
t('strerror of no error at all',  fn() => posix_strerror(0));

echo "-- descriptors\n";
t('a pipe is no terminal',        fn() => posix_isatty(STDOUT));
t('...and says why',              fn() => posix_strerror(posix_get_last_error()));
t('a descriptor nothing opened',  fn() => posix_isatty(999));
t('...says why too',              fn() => posix_strerror(posix_get_last_error()));
t('a numeric STRING is a number', fn() => posix_isatty("1"));
t('a bool is one too',            fn() => posix_isatty(true));
t('a negative one is refused',    fn() => posix_isatty(-1));
t('a memory stream has none',     function () { $f = fopen('php://memory', 'r');
    $r = posix_isatty($f); fclose($f); return $r; });
t('a real file HAS one',          function () { $f = fopen(__FILE__, 'r');
    $r = posix_isatty($f); fclose($f); return $r; });
t('a closed one is a TypeError',  function () { $f = fopen('php://memory', 'r'); fclose($f);
    return posix_isatty($f); });
t('a string that is no number',   fn() => posix_isatty('abc'));
t('an array is not a descriptor', fn() => posix_isatty([]));
t('nor is an object',             fn() => posix_isatty(new stdClass));
t('ttyname of a pipe',            fn() => posix_ttyname(STDOUT));
t('ttyname of a memory stream',   function () { $f = fopen('php://memory', 'r');
    $r = posix_ttyname($f); fclose($f); return $r; });
t('fpathconf takes a stream',     function () { $f = fopen(__FILE__, 'r');
    $r = posix_fpathconf($f, POSIX_PC_NAME_MAX); fclose($f); return is_int($r) && $r > 0; });
t('...and an int',                fn() => is_int(posix_fpathconf(1, POSIX_PC_PATH_MAX)));
t('...but not an array',          fn() => posix_fpathconf([], POSIX_PC_PATH_MAX));

echo "-- php's own screens\n";
t('a NUL in a path',              fn() => posix_access("a\0b"));
t('...at every path door',        fn() => posix_mkfifo("a\0b", 0644));
t('...including mknod',           fn() => posix_mknod("a\0b", POSIX_S_IFIFO));
t('...and pathconf',              fn() => posix_pathconf("a\0b", POSIX_PC_NAME_MAX));
t('an EMPTY path, pathconf only', fn() => posix_pathconf('', POSIX_PC_NAME_MAX));
t('...access takes one',          fn() => posix_access(''));
t('a device node with no major',  fn() => posix_mknod('/tmp/phl_posix_none', POSIX_S_IFCHR));
t('...S_IFBLK the same',          fn() => posix_mknod('/tmp/phl_posix_none', POSIX_S_IFBLK, 0, 0));
t('...and S_IFSOCK, which php',   fn() => posix_mknod('/tmp/phl_posix_none', POSIX_S_IFSOCK));
t('a FIFO needs no major',        fn() => is_bool(@posix_mknod('/no/such/dir/8a1f9b/n', POSIX_S_IFIFO)));

echo "-- sysconf and pathconf answer numbers\n";
t('a page is a positive size',    fn() => posix_sysconf(POSIX_SC_PAGESIZE) > 0);
t('the clock ticks',              fn() => posix_sysconf(POSIX_SC_CLK_TCK) > 0);
t('an unknown id is -1',          fn() => posix_sysconf(999999));
t('a name is bounded',            fn() => posix_pathconf('.', POSIX_PC_NAME_MAX) > 0);
?>
--EXPECT--
-- the extension, with php's 41 names in php's own order
bool(true)
posix_kill
posix_getpid
posix_getppid
posix_getuid
posix_setuid
posix_geteuid
posix_seteuid
posix_getgid
posix_setgid
posix_getegid
posix_setegid
posix_getgroups
posix_getlogin
posix_getpgrp
posix_setsid
posix_setpgid
posix_getpgid
posix_getsid
posix_uname
posix_times
posix_ctermid
posix_ttyname
posix_isatty
posix_getcwd
posix_mkfifo
posix_mknod
posix_access
posix_eaccess
posix_getgrnam
posix_getgrgid
posix_getpwnam
posix_getpwuid
posix_getrlimit
posix_setrlimit
posix_get_last_error
posix_errno
posix_strerror
posix_initgroups
posix_sysconf
posix_pathconf
posix_fpathconf
-- the identity calls answer each other
getpid is the pid                          true
a parent exists                            true
the four ids are ints                      true
getpwuid(getuid()) is us                   true
...and names itself back                   true
getgrgid(getgid()) is ours                 true
getgroups is a list of ints                true
getcwd agrees with getcwd()                true
getpgid(0) is the group                    true
getsid(0) is a session                     true
signal 0 finds us                          true
getlogin is a string or false              true
ctermid is a path                          true
-- php's key ORDER, which is the contract
uname                                      array (  0 => 'sysname',  1 => 'nodename',  2 => 'release',  3 => 'version',  4 => 'machine',  5 => 'domainname',)
times                                      array (  0 => 'ticks',  1 => 'utime',  2 => 'stime',  3 => 'cutime',  4 => 'cstime',)
...all integers                            array (  'ticks' => 'integer',)
getpwnam                                   array (  0 => 'name',  1 => 'passwd',  2 => 'uid',  3 => 'gid',  4 => 'gecos',  5 => 'dir',  6 => 'shell',)
getgrnam, members THIRD                    array (  0 => 'name',  1 => 'passwd',  2 => 'members',  3 => 'gid',)
...members is a list                       true
getrlimit, ten pairs                       array (  0 => 'soft core',  1 => 'hard core',  2 => 'soft data',  3 => 'hard data',  4 => 'soft stack',  5 => 'hard stack',  6 => 'soft totalmem',  7 => 'hard totalmem',  8 => 'soft rss',  9 => 'hard rss',  10 => 'soft maxproc',  11 => 'hard maxproc',  12 => 'soft memlock',  13 => 'hard memlock',  14 => 'soft cpu',  15 => 'hard cpu',  16 => 'soft filesize',  17 => 'hard filesize',  18 => 'soft openfiles',  19 => 'hard openfiles',)
one resource is a LIST                     array (  0 => 0,  1 => 1,)
...int or the string                       true
an unknown resource                        false
-- what is NOT there answers false
getpwnam of nobody                         false
getpwuid of nobody                         false
getgrnam of no group                       false
getgrgid of no group                       false
getpgid of no process                      false
getsid of no process                       false
kill of no process                         false
access of no file                          false
pathconf of no file                        false
mkfifo into no directory                   false
-- the remembered errno: which calls store one, and which leave it
a failure stores its reason                'No such file or directory'
...and the alias agrees                    true
a SUCCESS leaves it standing               true
a name that is not there ZEROES it         0
sysconf stores nothing                     false
times stores nothing                       false
uname stores nothing                       false
a getcwd stores nothing                    false
initgroups stores nothing                  false
an OK pathconf stores nothing              false
a BAD pathconf stores EINVAL               'Invalid argument'
strerror of no error at all                'Success'
-- descriptors
a pipe is no terminal                      false
...and says why                            'Inappropriate ioctl for device'
a descriptor nothing opened                false
...says why too                            'Bad file descriptor'
a numeric STRING is a number               false
a bool is one too                          false
  [2] posix_isatty(): Argument #1 ($file_descriptor) must be between 0 and 2147483647
a negative one is refused                  false
  [2] posix_isatty(): Could not use stream of type 'MEMORY'
a memory stream has none                   false
a real file HAS one                        false
a closed one is a TypeError                'TypeError: posix_isatty(): supplied resource is not a valid stream resource'
  [2] posix_isatty(): Argument #1 ($file_descriptor) must be of type int|resource, string given
a string that is no number                 false
  [2] posix_isatty(): Argument #1 ($file_descriptor) must be of type int|resource, array given
an array is not a descriptor               false
  [2] posix_isatty(): Argument #1 ($file_descriptor) must be of type int|resource, stdClass given
nor is an object                           false
ttyname of a pipe                          false
  [2] posix_ttyname(): Could not use stream of type 'MEMORY'
ttyname of a memory stream                 false
fpathconf takes a stream                   true
...and an int                              true
...but not an array                        'TypeError: posix_fpathconf(): Argument #1 ($file_descriptor) must be of type int|resource, array given'
-- php's own screens
a NUL in a path                            'ValueError: posix_access(): Argument #1 ($filename) must not contain any null bytes'
...at every path door                      'ValueError: posix_mkfifo(): Argument #1 ($filename) must not contain any null bytes'
...including mknod                         'ValueError: posix_mknod(): Argument #1 ($filename) must not contain any null bytes'
...and pathconf                            'ValueError: posix_pathconf(): Argument #1 ($path) must not contain any null bytes'
an EMPTY path, pathconf only               'ValueError: posix_pathconf(): Argument #1 ($path) must not be empty'
...access takes one                        false
a device node with no major                'ValueError: posix_mknod(): Argument #3 ($major) cannot be 0 for the POSIX_S_IFCHR and POSIX_S_IFBLK modes'
...S_IFBLK the same                        'ValueError: posix_mknod(): Argument #3 ($major) cannot be 0 for the POSIX_S_IFCHR and POSIX_S_IFBLK modes'
...and S_IFSOCK, which php                 'ValueError: posix_mknod(): Argument #3 ($major) cannot be 0 for the POSIX_S_IFCHR and POSIX_S_IFBLK modes'
a FIFO needs no major                      true
-- sysconf and pathconf answer numbers
a page is a positive size                  true
the clock ticks                            true
an unknown id is -1                        -1
a name is bounded                          true
