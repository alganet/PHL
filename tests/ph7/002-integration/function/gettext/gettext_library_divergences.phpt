--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/gettext: the two places PHL answers the RULE where glibc answers its implementation
--DESCRIPTION--
Both rows here are PLAN.md §7.4 records, and neither can carry a `_zend` twin.

(1) A `Plural-Forms` expression that divides or takes a modulo by ZERO. glibc's
`plural_eval` raises SIGFPE for it on purpose, so php does not answer at all --
it dies with a Floating point exception and a core dump, on nothing worse than
a typo in a translation catalog. PHL answers 0 for the division, which lands on
form 0. A php half of this test would kill the runner.

(2) glibc REMEMBERS a translation it has already found, keyed by domain + msgid
+ category, and drops that memory only when `_nl_msg_cat_cntr` moves --
`setlocale()`, `textdomain()` and a CHANGED binding move it, `putenv()` does
not. So under php a bare `putenv("LANGUAGE=...")` between two lookups of ONE
msgid is answered out of the cache while a msgid never asked for before sees the
new list. PHL re-resolves whenever the category value changes, so both see it.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL half of a divergence pair; php raises SIGFPE for (1) and caches (2)";
    return;
}
if (PHP_OS_FAMILY === 'Windows') { return; }
if (setlocale(LC_MESSAGES, 'en_US.UTF-8', 'en_US.utf8', 'en_GB.UTF-8', 'en_GB.utf8', 'en_US') === false) {
    die("skip this box has no non-C LC_MESSAGES locale, so gettext reads no catalog\n");
}
--FILE--
<?php
function mo_key(string $k): string {
    $p = strpos($k, "\0");
    return $p === false ? $k : substr($k, 0, $p);
}
function mo(array $pairs): string {
    $keys = array_map('strval', array_keys($pairs));
    usort($keys, fn($a, $b) => strcmp(mo_key($a), mo_key($b)));
    $n = count($keys);
    $orig = 28; $trans = $orig + 8 * $n; $strings = $trans + 8 * $n;
    $ot = ''; $tt = ''; $blob = ''; $at = $strings;
    foreach ($keys as $k) {
        $ot .= pack('VV', strlen($k), $at); $blob .= $k . "\0"; $at += strlen($k) + 1;
    }
    foreach ($keys as $k) {
        $v = $pairs[$k];
        $tt .= pack('VV', strlen($v), $at); $blob .= $v . "\0"; $at += strlen($v) + 1;
    }
    return pack('V7', 0x950412de, 0, $n, $orig, $trans, 0, $strings) . $ot . $tt . $blob;
}
$root = sys_get_temp_dir() . '/phl_gettext_divergences';
@mkdir("$root/xx_YY/LC_MESSAGES", 0777, true);
@mkdir("$root/xx_ZZ/LC_MESSAGES", 0777, true);

$bad = "Content-Type: text/plain; charset=UTF-8\nPlural-Forms: nplurals=3; plural=(n%0);\n";
file_put_contents("$root/xx_YY/LC_MESSAGES/badrule.mo",
    mo(['' => $bad, "s\0p" => "zero\0one\0two"]));
$ok = "Content-Type: text/plain; charset=UTF-8\nPlural-Forms: nplurals=2; plural=(n != 1);\n";
file_put_contents("$root/xx_YY/LC_MESSAGES/cache.mo", mo(['' => $ok, 'Hi' => 'in xx_YY']));
file_put_contents("$root/xx_ZZ/LC_MESSAGES/cache.mo", mo(['' => $ok, 'Hi' => 'in xx_ZZ']));

putenv('LC_MESSAGES=xx_YY');
setlocale(LC_MESSAGES, 'en_US.UTF-8', 'en_US.utf8', 'en_GB.UTF-8', 'en_GB.utf8', 'en_US');
putenv('LANGUAGE=xx_YY');
bindtextdomain('badrule', $root);
bindtextdomain('cache', $root);

echo "-- (1) a plural rule that takes a modulo by zero\n";
foreach ([0, 1, 2, 7] as $n) {
    printf("%d %s\n", $n, dngettext('badrule', 's', 'p', $n));
}

echo "-- (2) a bare putenv() moves the answer\n";
echo dgettext('cache', 'Hi'), "\n";
putenv('LANGUAGE=xx_ZZ');
echo dgettext('cache', 'Hi'), "\n";
putenv('LANGUAGE=xx_YY');
echo dgettext('cache', 'Hi'), "\n";
?>
--CLEAN--
<?php
$root = sys_get_temp_dir() . '/phl_gettext_divergences';
foreach (['xx_YY', 'xx_ZZ'] as $d) {
    foreach (['badrule', 'cache'] as $f) {
        @unlink("$root/$d/LC_MESSAGES/$f.mo");
    }
    @rmdir("$root/$d/LC_MESSAGES");
    @rmdir("$root/$d");
}
@rmdir($root);
--EXPECT--
-- (1) a plural rule that takes a modulo by zero
0 zero
1 zero
2 zero
7 zero
-- (2) a bare putenv() moves the answer
in xx_YY
in xx_ZZ
in xx_YY
