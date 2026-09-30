--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/gettext: the MO reader, the candidate expansion and the plural expression
--SKIPIF--
<?php
/* The catalog half needs an LC_MESSAGES category that is not "C": gettext reads
 * no catalog in the C locale and ignores LANGUAGE there too. On Windows there is
 * no LC_MESSAGES in the C library at all, so the ENVIRONMENT answers for it and
 * putenv() is always enough; everywhere else a real locale has to exist. */
if (!extension_loaded('gettext')) {
    die("skip this php has no ext/gettext\n");
}
if (PHP_OS_FAMILY === 'Darwin') {
    die("skip php on macOS links GNU libintl, whose catalog answers are not glibc's\n");
}
if (PHP_OS_FAMILY === 'Windows') { return; }
if (setlocale(LC_MESSAGES, 'en_US.UTF-8', 'en_US.utf8', 'en_GB.UTF-8', 'en_GB.utf8', 'en_US') === false) {
    die("skip this box has no non-C LC_MESSAGES locale, so gettext reads no catalog\n");
}
--FILE--
<?php
/* The GNU MO format and the four rules that pick the file, with the catalogs
 * written out by hand so the test needs no msgfmt. Every candidate case below
 * uses a msgid of its OWN: glibc caches a translation it has already found,
 * keyed by domain+msgid and invalidated only by setlocale()/textdomain()/a
 * changed binding, so re-asking for one msgid under a different LANGUAGE would
 * be answered out of that cache rather than measured. */
function mo_key(string $k): string {
    $p = strpos($k, "\0");
    return $p === false ? $k : substr($k, 0, $p);
}
/* One catalog. $pairs is msgid => msgstr, a plural entry spelling both halves
 * with a NUL between them; $be writes the big-endian flavour of the format. */
function mo(array $pairs, bool $be = false): string {
    $keys = array_map('strval', array_keys($pairs));
    /* The msgid table is sorted for BINARY SEARCH and the comparison is a
     * C-string one -- which is what makes `singular\0plural` findable by its
     * singular alone. */
    usort($keys, fn($a, $b) => strcmp(mo_key($a), mo_key($b)));
    $n = count($keys);
    $orig = 28;
    $trans = $orig + 8 * $n;
    $strings = $trans + 8 * $n;
    $w = fn(int ...$v) => pack($be ? 'N*' : 'V*', ...$v);
    $ot = ''; $tt = ''; $blob = ''; $at = $strings;
    foreach ($keys as $k) {
        $ot .= $w(strlen($k), $at); $blob .= $k . "\0"; $at += strlen($k) + 1;
    }
    foreach ($keys as $k) {
        $v = $pairs[$k];
        $tt .= $w(strlen($v), $at); $blob .= $v . "\0"; $at += strlen($v) + 1;
    }
    return $w(0x950412de, 0, $n, $orig, $trans, 0, $strings) . $ot . $tt . $blob;
}
function put(string $dir, string $name, string $bytes, string $domain = 'phlt'): void {
    @mkdir("$dir/$name/LC_MESSAGES", 0777, true);
    file_put_contents("$dir/$name/LC_MESSAGES/$domain.mo", $bytes);
}
function t(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-40s %s\n", $l, var_export($r, true));
}

$root = sys_get_temp_dir() . '/phl_gettext_catalog';
$hdr = "Project-Id-Version: phlt\nMIME-Version: 1.0\n"
     . "Content-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: 8bit\n"
     . "Plural-Forms: nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4"
     . " && (n%100<10 || n%100>=20) ? 1 : 2);\n";
$labels = ['k1', 'k2', 'k3', 'k4', 'k5', 'k6', 'k7'];
/* The catalog the lookups read, plus one per candidate spelling that says only
 * where it is. */
$main = ['' => $hdr, 'Hello' => "Ol\u{e1}", 'Yes' => 'Sim',
         "file\0files" => "um\0poucos\0muitos", 'over' => "a\0b"];
foreach ($labels as $k) { $main[$k] = 'xx_YY.UTF-8'; }
put($root, 'xx_YY.UTF-8', mo($main));
foreach (['xx_YY', 'xx', 'xx@mod'] as $where) {
    $rows = ['' => $hdr];
    foreach ($labels as $k) { $rows[$k] = $where; }
    put($root, $where, mo($rows));
}
/* A big-endian catalog, and a file that is not a catalog at all. */
put($root, 'be_BE', mo(['' => $hdr, 'be' => 'grande'], true));
put($root, 'ju_NK', 'not a catalog at all, not even the right length');
/* A second domain in TWO of the candidate directories: the specific one says
 * nothing about most of the msgids, and the search falls through to the one
 * behind it -- taking that catalog's plural rule with it. */
$hdr2 = "Content-Type: text/plain; charset=UTF-8\nPlural-Forms: nplurals=2; plural=(n != 1);\n";
put($root, 'xx_YY.UTF-8', mo(['' => $hdr2, 'A' => 'A in xx_YY.UTF-8']), 'fall');
put($root, 'xx', mo(['' => $hdr, 'A' => 'A in xx', 'B' => 'B in xx',
                     "s\0p" => "um\0poucos\0muitos"]), 'fall');

/* On Windows the category IS the environment; everywhere else it is the C
 * library's, so both have to be set for LANGUAGE to be consulted at all. */
putenv('LC_MESSAGES=xx_YY');
setlocale(LC_MESSAGES, 'en_US.UTF-8', 'en_US.utf8', 'en_GB.UTF-8', 'en_GB.utf8', 'en_US');
bindtextdomain('phlt', $root);
textdomain('phlt');
putenv('LANGUAGE=xx_YY.UTF-8');

echo "-- the catalog answers (nothing bound: these msgids are ASCII either way)\n";
t('gettext',                       fn() => gettext('Yes'));
t('_ is the same function',        fn() => _('Yes'));
t('dgettext',                      fn() => dgettext('phlt', 'Yes'));
t('dcgettext, LC_MESSAGES',        fn() => dcgettext('phlt', 'Yes', LC_MESSAGES));
t('a msgid it does not carry',     fn() => gettext('Absent'));
t('the empty msgid is the HEADER', fn() => strlen(gettext('')) === strlen($hdr));
t('another category, no such dir', fn() => dcgettext('phlt', 'Yes', LC_MONETARY));
t('a domain nobody bound',         fn() => dgettext('nowhere', 'Yes'));

echo "-- the plural expression out of the header\n";
foreach ([0, 1, 2, 5, 11, 21, 22, 25, 101, 111] as $n) {
    printf("%4d %s\n", $n, ngettext('file', 'files', $n));
}
t('a count of -1 is UNSIGNED',      fn() => ngettext('file', 'files', -1));
t('the singular door takes form 0', fn() => gettext('file'));
t('a msgid with too few forms',     fn() => ngettext('over', 'overs', 5));
t('a msgid the catalog lacks',      fn() => ngettext('none', 'nones', 3));
t('...and its singular',            fn() => ngettext('none', 'nones', 1));

echo "-- the output encoding\n";
bind_textdomain_codeset('phlt', 'UTF-8');
t('the catalog charset itself',     fn() => bin2hex(gettext('Hello')));
bind_textdomain_codeset('phlt', 'ISO-8859-1');
t('converted to ISO-8859-1',        fn() => bin2hex(gettext('Hello')));
bind_textdomain_codeset('phlt', 'no-such-charset');
t('a charset nothing can open',     fn() => gettext('Hello'));
bind_textdomain_codeset('phlt', 'UTF-8');
t('and back',                       fn() => bin2hex(gettext('Hello')));

echo "-- the candidate expansion, most specific first\n";
$cases = [
    'k1' => 'xx_YY.UTF-8',      /* the name as given */
    'k2' => 'xx_YY.ISO-8859-1', /* no such spelling: falls back to xx_YY */
    'k3' => 'xx_ZZ',            /* no such territory: falls back to xx */
    'k4' => 'xx@mod',           /* the modifier is the most significant part */
    'k5' => 'xx_ZZ@mod',        /* ...so xx@mod beats xx_ZZ */
    'k6' => 'zz:xx_ZZ',         /* LANGUAGE is a priority LIST */
    'k7' => 'zz:C:xx',          /* ...and an element spelled C ends it */
];
foreach ($cases as $key => $language) {
    putenv("LANGUAGE=$language");
    printf("%-18s %-4s %s\n", $language, $key, gettext($key));
}

echo "-- a candidate that does not carry the msgid falls through to the next\n";
putenv('LANGUAGE=xx_YY.UTF-8');   /* the list above left it somewhere else */
bindtextdomain('fall', $root);
t('the specific catalog has it',   fn() => dgettext('fall', 'A'));
t('this one only the fallback has',fn() => dgettext('fall', 'B'));
t('and this one neither',          fn() => dgettext('fall', 'C'));
foreach ([1, 2, 5] as $n) {
    printf("n=%d %s\n", $n, dngettext('fall', 's', 'p', $n));
}

echo "-- other byte orders, and a file that is not a catalog\n";
putenv('LANGUAGE=be_BE');
t('a big-endian catalog reads',   fn() => gettext('be'));
putenv('LANGUAGE=ju_NK');
t('junk is answered as absent',   fn() => gettext('junk1'));
putenv('LANGUAGE=zz_ZZ');
t('no catalog anywhere',          fn() => gettext('junk2'));

echo "-- back in the C locale nothing is read\n";
putenv('LANGUAGE=xx_YY.UTF-8');
putenv('LC_MESSAGES=C');
setlocale(LC_MESSAGES, 'C');
t('gettext',                      fn() => gettext('Hello'));
?>
--CLEAN--
<?php
$root = sys_get_temp_dir() . '/phl_gettext_catalog';
foreach (['xx_YY.UTF-8', 'xx_YY', 'xx', 'xx@mod', 'be_BE', 'ju_NK'] as $d) {
    @unlink("$root/$d/LC_MESSAGES/phlt.mo");
    @unlink("$root/$d/LC_MESSAGES/fall.mo");
    @rmdir("$root/$d/LC_MESSAGES");
    @rmdir("$root/$d");
}
@rmdir($root);
--EXPECT--
-- the catalog answers (nothing bound: these msgids are ASCII either way)
gettext                                  'Sim'
_ is the same function                   'Sim'
dgettext                                 'Sim'
dcgettext, LC_MESSAGES                   'Sim'
a msgid it does not carry                'Absent'
the empty msgid is the HEADER            true
another category, no such dir            'Yes'
a domain nobody bound                    'Yes'
-- the plural expression out of the header
   0 muitos
   1 um
   2 poucos
   5 muitos
  11 muitos
  21 um
  22 poucos
  25 muitos
 101 um
 111 muitos
a count of -1 is UNSIGNED                'muitos'
the singular door takes form 0           'um'
a msgid with too few forms               'a'
a msgid the catalog lacks                'nones'
...and its singular                      'none'
-- the output encoding
the catalog charset itself               '4f6cc3a1'
converted to ISO-8859-1                  '4f6ce1'
a charset nothing can open               'Hello'
and back                                 '4f6cc3a1'
-- the candidate expansion, most specific first
xx_YY.UTF-8        k1   xx_YY.UTF-8
xx_YY.ISO-8859-1   k2   xx_YY
xx_ZZ              k3   xx
xx@mod             k4   xx@mod
xx_ZZ@mod          k5   xx@mod
zz:xx_ZZ           k6   xx
zz:C:xx            k7   k7
-- a candidate that does not carry the msgid falls through to the next
the specific catalog has it              'A in xx_YY.UTF-8'
this one only the fallback has           'B in xx'
and this one neither                     'C'
n=1 um
n=2 poucos
n=5 muitos
-- other byte orders, and a file that is not a catalog
a big-endian catalog reads               'grande'
junk is answered as absent               'junk1'
no catalog anywhere                      'junk2'
-- back in the C locale nothing is read
gettext                                  'Hello'
