--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
openlog/syslog/closelog: the constants, the four syslog.filter modes and the split
--SKIPIF--
<?php
/* LOG_PERROR is what makes a record visible to a test at all: it copies every
 * one to stderr. Windows has the three functions but no such option -- php's
 * own header does define LOG_PERROR there, but the event log is the only sink
 * and nothing a test can read comes back. */
if (!defined('LOG_LOCAL0')) {
    die("skip needs a POSIX syslog (LOG_LOCAL0 does not exist on Windows)\n");
}
/* LOG_PERROR's stderr copy is the C library's own format: glibc writes
 * `ident: message`, macOS a dated `ident[pid] <Level>:` line. */
if (PHP_OS_FAMILY !== 'Linux') {
    die("skip the LOG_PERROR line pinned here is glibc's\n");
}
--FILE--
<?php
/* The RECORDS go to stderr through LOG_PERROR, which is the only sink a test can
 * read; the prefix is the test's own so nothing here depends on the box. */
function t(string $l, $v): void {
    printf("%-38s %s\n", $l, str_replace("\n", '', var_export($v, true)));
}

echo "-- the three names are ext/standard's, on every platform php has them\n";
t('all three exist', array_map('function_exists', ['openlog', 'syslog', 'closelog']));
t('and they are standard\'s', (new ReflectionFunction('openlog'))->getExtensionName());
t('each answers the literal true', [
    (string) (new ReflectionFunction('openlog'))->getReturnType(),
    (string) (new ReflectionFunction('syslog'))->getReturnType(),
    (string) (new ReflectionFunction('closelog'))->getReturnType(),
]);

echo "-- the thirty-three constants, in php's own registration order\n";
$r = new ReflectionExtension('standard');
$log = [];
foreach ($r->getConstants() as $k => $v) {
    if (str_starts_with($k, 'LOG_')) { $log[$k] = $v; }
}
echo implode("\n", array_keys($log)), "\n";
t('how many', count($log));
t('a priority is 0..7',       array_slice($log, 0, 8) === ['LOG_EMERG' => 0, 'LOG_ALERT' => 1,
    'LOG_CRIT' => 2, 'LOG_ERR' => 3, 'LOG_WARNING' => 4, 'LOG_NOTICE' => 5,
    'LOG_INFO' => 6, 'LOG_DEBUG' => 7]);
t('a facility is a multiple of 8', LOG_USER === 8 && LOG_LOCAL0 === 128 && LOG_LOCAL7 === 184);

echo "-- the three ini directives are php's, and they are CORE's\n";
$core = array_values(array_filter(array_keys((new ReflectionExtension('Core'))->getINIEntries()),
    fn ($x) => str_starts_with($x, 'syslog')));
t('which extension owns them', $core);
$all = ini_get_all();
foreach (['syslog.facility', 'syslog.filter', 'syslog.ident'] as $n) {
    t($n, [$all[$n]['local_value'], $all[$n]['access']]);
}

echo "-- syslog.filter names four modes, CASE-SENSITIVELY, and refuses the rest\n";
t('the default',                  ini_get('syslog.filter'));
t('raw is taken',                 [ini_set('syslog.filter', 'raw'), ini_get('syslog.filter')]);
t('a name it does not know',      [ini_set('syslog.filter', 'bogus'), ini_get('syslog.filter')]);
t('...the empty string either',   [ini_set('syslog.filter', ''), ini_get('syslog.filter')]);
t('...nor the wrong CASE',        [ini_set('syslog.filter', 'ASCII'), ini_get('syslog.filter')]);
t('ascii is taken',               [ini_set('syslog.filter', 'ascii'), ini_get('syslog.filter')]);
t('all is taken',                 [ini_set('syslog.filter', 'all'), ini_get('syslog.filter')]);
t('and back to the default',      [ini_set('syslog.filter', 'no-ctrl'), ini_get('syslog.filter')]);
t('the two others are SYSTEM',    [ini_set('syslog.ident', 'x'), ini_set('syslog.facility', 'LOG_MAIL')]);

echo "-- every call answers the literal true, opened or not\n";
t('a syslog with nothing open',   syslog(LOG_INFO, 'ignored'));
t('closelog with nothing open',   closelog());
t('openlog',                      openlog('phlt', LOG_PERROR, LOG_USER));
t('syslog',                       syslog(LOG_INFO, 'ignored'));
t('closelog',                     closelog());
t('...and closelog again',        closelog());

echo "-- what each filter mode does to a byte, derived from php by sweeping all 256\n";
/* One record per case, with markers, so the transform is unambiguous. The
 * records land on STDERR; STDOUT carries the labels, and the runner reads both. */
/* The high-byte cases are spelled as UTF-8 rather than lone 0x80/0xff so this
 * FILE stays UTF-8 -- what is being pinned is "a byte >= 0x80", and `\u{00e9}`
 * is two of them. The full 256-value sweep that derived the table lives in the
 * session record, not here. */
$cases = ['nul' => "\0", 'tab' => "\t", 'esc' => "\x1b", 'del' => "\x7f",
          'utf8' => "\u{00e9}", 'letter' => 'A', 'space' => ' '];
foreach (['no-ctrl', 'ascii', 'all', 'raw'] as $mode) {
    ini_set('syslog.filter', $mode);
    openlog("f-$mode", LOG_PERROR, LOG_USER);
    foreach ($cases as $name => $byte) {
        fwrite(STDERR, "  $mode/$name ");
        syslog(LOG_INFO, "<{$byte}>");
    }
    closelog();
}

echo "-- a newline SPLITS the message into one record each; raw does not\n";
$split = ['plain' => 'abc', 'inner' => "a\nb", 'trailing' => "abc\n",
          'leading' => "\nabc", 'double' => "a\n\nb", 'only' => "\n", 'empty' => ''];
foreach (['no-ctrl', 'raw'] as $mode) {
    ini_set('syslog.filter', $mode);
    openlog("s-$mode", LOG_PERROR, LOG_USER);
    /* `crlf` runs under no-ctrl only. Under raw the carriage return would reach
     * this file's expectation block as a REAL byte, and git normalises one away
     * (.gitattributes eol=lf) -- a checked-out copy would then expect bytes the
     * engine does not produce. What raw does to a newline is already pinned by
     * the six cases above. */
    $cases = $mode === 'raw' ? $split : $split + ['crlf' => "a\r\nb"];
    foreach ($cases as $name => $msg) {
        fwrite(STDERR, "  $mode/$name\n");
        syslog(LOG_INFO, $msg);
    }
    closelog();
}
ini_set('syslog.filter', 'no-ctrl');
?>
--EXPECT--
-- the three names are ext/standard's, on every platform php has them
all three exist                        array (  0 => true,  1 => true,  2 => true,)
and they are standard's                'standard'
each answers the literal true          array (  0 => 'true',  1 => 'true',  2 => 'true',)
-- the thirty-three constants, in php's own registration order
LOG_EMERG
LOG_ALERT
LOG_CRIT
LOG_ERR
LOG_WARNING
LOG_NOTICE
LOG_INFO
LOG_DEBUG
LOG_KERN
LOG_USER
LOG_MAIL
LOG_DAEMON
LOG_AUTH
LOG_SYSLOG
LOG_LPR
LOG_NEWS
LOG_UUCP
LOG_CRON
LOG_AUTHPRIV
LOG_LOCAL0
LOG_LOCAL1
LOG_LOCAL2
LOG_LOCAL3
LOG_LOCAL4
LOG_LOCAL5
LOG_LOCAL6
LOG_LOCAL7
LOG_PID
LOG_CONS
LOG_ODELAY
LOG_NDELAY
LOG_NOWAIT
LOG_PERROR
how many                               33
a priority is 0..7                     true
a facility is a multiple of 8          true
-- the three ini directives are php's, and they are CORE's
which extension owns them              array (  0 => 'syslog.facility',  1 => 'syslog.ident',  2 => 'syslog.filter',)
syslog.facility                        array (  0 => 'LOG_USER',  1 => 4,)
syslog.filter                          array (  0 => 'no-ctrl',  1 => 7,)
syslog.ident                           array (  0 => 'php',  1 => 4,)
-- syslog.filter names four modes, CASE-SENSITIVELY, and refuses the rest
the default                            'no-ctrl'
raw is taken                           array (  0 => 'no-ctrl',  1 => 'raw',)
a name it does not know                array (  0 => false,  1 => 'raw',)
...the empty string either             array (  0 => false,  1 => 'raw',)
...nor the wrong CASE                  array (  0 => false,  1 => 'raw',)
ascii is taken                         array (  0 => 'raw',  1 => 'ascii',)
all is taken                           array (  0 => 'ascii',  1 => 'all',)
and back to the default                array (  0 => 'all',  1 => 'no-ctrl',)
the two others are SYSTEM              array (  0 => false,  1 => false,)
-- every call answers the literal true, opened or not
a syslog with nothing open             true
closelog with nothing open             true
openlog                                true
phlt: ignored
syslog                                 true
closelog                               true
...and closelog again                  true
-- what each filter mode does to a byte, derived from php by sweeping all 256
  no-ctrl/nul f-no-ctrl: <\x00>
  no-ctrl/tab f-no-ctrl: <\x09>
  no-ctrl/esc f-no-ctrl: <\x1b>
  no-ctrl/del f-no-ctrl: <\x7f>
  no-ctrl/utf8 f-no-ctrl: <é>
  no-ctrl/letter f-no-ctrl: <A>
  no-ctrl/space f-no-ctrl: < >
  ascii/nul f-ascii: <\x00>
  ascii/tab f-ascii: <\x09>
  ascii/esc f-ascii: <\x1b>
  ascii/del f-ascii: <\x7f>
  ascii/utf8 f-ascii: <\xc3\xa9>
  ascii/letter f-ascii: <A>
  ascii/space f-ascii: < >
  all/nul f-all: <
  all/tab f-all: <	>
  all/esc f-all: <>
  all/del f-all: <\x7f>
  all/utf8 f-all: <é>
  all/letter f-all: <A>
  all/space f-all: < >
  raw/nul f-raw: <
  raw/tab f-raw: <	>
  raw/esc f-raw: <>
  raw/del f-raw: <>
  raw/utf8 f-raw: <é>
  raw/letter f-raw: <A>
  raw/space f-raw: < >
-- a newline SPLITS the message into one record each; raw does not
  no-ctrl/plain
s-no-ctrl: abc
  no-ctrl/inner
s-no-ctrl: a
s-no-ctrl: b
  no-ctrl/trailing
s-no-ctrl: abc
s-no-ctrl: 
  no-ctrl/leading
s-no-ctrl: 
s-no-ctrl: abc
  no-ctrl/double
s-no-ctrl: a
s-no-ctrl: 
s-no-ctrl: b
  no-ctrl/only
s-no-ctrl: 
s-no-ctrl: 
  no-ctrl/empty
s-no-ctrl: 
  no-ctrl/crlf
s-no-ctrl: a\x0d
s-no-ctrl: b
  raw/plain
s-raw: abc
  raw/inner
s-raw: a
b
  raw/trailing
s-raw: abc
  raw/leading
s-raw: 
abc
  raw/double
s-raw: a

b
  raw/only
s-raw: 
  raw/empty
s-raw: 
