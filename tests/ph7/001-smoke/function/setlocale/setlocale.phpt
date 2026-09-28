--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
setlocale(), getrusage() and the build-shape constants a real tool reads first
--FILE--
<?php
/* The platform surface a real tool asks for before it does anything else:
 * Composer's `bin/composer` opens with setlocale(LC_ALL,'C'), symfony/process
 * reads INFO_GENERAL and ZEND_THREAD_SAFE to shape a subprocess call, Composer's
 * platform repository reads PCRE_VERSION, and PHPUnit's telemetry calls
 * getrusage() for every event it emits. Missing, each one stopped the tool at its
 * first line. */
function plt(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-38s %s\n", $l, str_replace("\n", '', var_export($r, true)));
}
/* php registers the C library's own LC_* macros, so the NUMBERS are the
 * platform's (macOS and Windows put LC_ALL at 0) and Windows has no LC_MESSAGES
 * at all -- only their shape is pinned */
echo "-- the LC_* categories are the C library's numbering\n";
printf("six distinct ints: %s\n", var_export(count(array_unique(array_filter(array_map('constant',
    ['LC_CTYPE','LC_NUMERIC','LC_TIME','LC_COLLATE','LC_MONETARY','LC_ALL']), 'is_int'))) === 6, true));
echo "-- setlocale\n";
plt('the C locale',            fn() => setlocale(LC_ALL, 'C'));
plt('"0" only QUERIES',        fn() => setlocale(LC_ALL, '0'));
plt('...and so does no name',  fn() => setlocale(LC_ALL));
plt('an unavailable locale',   fn() => setlocale(LC_ALL, 'no_SUCH_locale'));
plt('the first that works',    fn() => setlocale(LC_ALL, 'no_SUCH_locale', 'C'));
plt('...from an array too',    fn() => setlocale(LC_ALL, ['no_SUCH_locale', 'C']));
plt('one category',            fn() => setlocale(LC_NUMERIC, 'C'));
plt('a category php has none of', fn() => setlocale(99, 'C'));
plt('formatting does not move',fn() => [sprintf('%.2f', 1234.5), number_format(1234.5, 2)]);

echo "-- getrusage\n";
$ru = getrusage();
/* php's Windows build has keys for only the six fields it fills */
$ruKeys = DIRECTORY_SEPARATOR === '\\'
    ? ['ru_majflt','ru_maxrss','ru_utime.tv_usec','ru_utime.tv_sec','ru_stime.tv_usec','ru_stime.tv_sec']
    : ['ru_oublock','ru_inblock','ru_msgsnd','ru_msgrcv','ru_maxrss','ru_ixrss','ru_idrss','ru_minflt',
       'ru_majflt','ru_nsignals','ru_nvcsw','ru_nivcsw','ru_nswap','ru_utime.tv_usec','ru_utime.tv_sec',
       'ru_stime.tv_usec','ru_stime.tv_sec'];
printf("the platform's keys: %s, all int: %s\n", var_export(array_keys($ru) === $ruKeys, true),
    var_export(count(array_filter($ru, 'is_int')) === count($ru), true));
plt('children mode',           fn() => array_keys(getrusage(1)) === $ruKeys);
plt('any other mode is SELF',  fn() => [gettype(getrusage(2)), gettype(getrusage(99))]);
plt('cpu time does not go back', function () { $a = getrusage(); usleep(1000); $b = getrusage();
    return $b['ru_utime.tv_sec'] >= $a['ru_utime.tv_sec']; });

echo "-- the build-shape constants\n";
foreach (['INFO_GENERAL','INFO_CREDITS','INFO_CONFIGURATION','INFO_MODULES',
          'INFO_ENVIRONMENT','INFO_VARIABLES','INFO_LICENSE','INFO_ALL'] as $c) {
    printf("%-20s %d\n", $c, constant($c));
}
foreach (['ZEND_THREAD_SAFE','ZEND_DEBUG_BUILD','PHP_ZTS','PHP_DEBUG'] as $c) {
    printf("%-20s %s %s\n", $c, gettype(constant($c)), var_export(constant($c), true));
}
echo "-- PCRE reports the LINKED library, so only its shape is pinned\n";
/* no relation between the string and the two ints is pinned either: php's ints
 * are the headers it was BUILT against, the string the library it RUNS with */
printf("version non-empty: %s\nmajor/minor ints:  %s\njit is a bool:     %s\n",
    var_export(is_string(PCRE_VERSION) && PCRE_VERSION !== '', true),
    var_export(is_int(PCRE_VERSION_MAJOR) && is_int(PCRE_VERSION_MINOR), true),
    var_export(is_bool(PCRE_JIT_SUPPORT), true));
--EXPECT--
-- the LC_* categories are the C library's numbering
six distinct ints: true
-- setlocale
the C locale                           'C'
"0" only QUERIES                       'C'
...and so does no name                 'ArgumentCountError: setlocale() expects at least 2 arguments, 1 given'
an unavailable locale                  false
the first that works                   'C'
...from an array too                   'C'
one category                           'C'
a category php has none of             false
formatting does not move               array (  0 => '1234.50',  1 => '1,234.50',)
-- getrusage
the platform's keys: true, all int: true
children mode                          true
any other mode is SELF                 array (  0 => 'array',  1 => 'array',)
cpu time does not go back              true
-- the build-shape constants
INFO_GENERAL         1
INFO_CREDITS         2
INFO_CONFIGURATION   4
INFO_MODULES         8
INFO_ENVIRONMENT     16
INFO_VARIABLES       32
INFO_LICENSE         64
INFO_ALL             4294967295
ZEND_THREAD_SAFE     boolean false
ZEND_DEBUG_BUILD     boolean false
PHP_ZTS              boolean false
PHP_DEBUG            boolean false
-- PCRE reports the LINKED library, so only its shape is pinned
version non-empty: true
major/minor ints:  true
jit is a bool:     true
