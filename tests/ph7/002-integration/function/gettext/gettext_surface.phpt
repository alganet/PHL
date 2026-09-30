--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ext/gettext: the bindings, the queries, php's screens and the C-locale rule
--SKIPIF--
<?php
/* php on macOS links GNU libintl, whose compiled-in catalog directory is its
 * own install prefix rather than glibc's /usr/share/locale. */
if (!extension_loaded('gettext')) {
    die("skip this php has no ext/gettext\n");
}
if (PHP_OS_FAMILY === 'Darwin') {
    die("skip php on macOS links GNU libintl, not glibc's\n");
}
--FILE--
<?php
/* Everything ext/gettext answers WITHOUT reading a catalog. The C locale is the
 * one every program starts in, and gettext reads no catalog there at all, so
 * this half is the same on every box: the ten names, the domain and directory
 * bindings with their query forms, and the five argument screens php's own
 * layer adds on top of libintl. The catalog reader is the test beside this one. */
function t(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-44s %s\n", $l, var_export($r, true));
}
$tmp = sys_get_temp_dir();

echo "-- the extension, with php's ten names in php's own order\n";
var_dump(extension_loaded('gettext'));
echo implode(', ', get_extension_funcs('gettext')), "\n";

echo "-- textdomain\n";
t('with nothing, the default domain',       fn() => textdomain());
t('null only QUERIES',                      fn() => textdomain(null));
t('setting one answers it back',            fn() => textdomain('phlt'));
t('...and the query agrees',                fn() => textdomain(null));
t('a NUL ends the name libintl sees',       fn() => textdomain("phlt\0ignored"));
t('a name that is ONLY a NUL queries',      fn() => textdomain("\0"));

echo "-- bindtextdomain: php makes the directory absolute first\n";
t('a domain nobody bound reads the default',fn() => bindtextdomain('never-bound', null));
t('a relative name resolving nowhere',      fn() => bindtextdomain('phlt', 'no/such/place'));
t('the EMPTY directory is the cwd',         fn() => bindtextdomain('phlt', '') === getcwd());
t('...and so is the string "0"',            fn() => bindtextdomain('phlt', '0') === getcwd());
t('a real one comes back canonical',        fn() => bindtextdomain('phlt', $tmp . '/.') === realpath($tmp));
t('the query answers it',                   fn() => bindtextdomain('phlt', null) === realpath($tmp));

echo "-- bind_textdomain_codeset\n";
t('nothing bound yet',                      fn() => bind_textdomain_codeset('phlt', null));
t('binding answers the name',               fn() => bind_textdomain_codeset('phlt', 'ISO-8859-1'));
t('the query answers it',                   fn() => bind_textdomain_codeset('phlt'));
t('a domain with no directory takes one',   fn() => bind_textdomain_codeset('other', 'UTF-8'));
t('an empty C string is no domain at all',  fn() => bind_textdomain_codeset("\0", 'UTF-8'));

echo "-- the C locale reads no catalog, so every door answers its own argument\n";
setlocale(LC_MESSAGES, 'C');
t('gettext',                                fn() => gettext('Hello'));
t('_ is the same function',                 fn() => _('Hello'));
t('dgettext',                               fn() => dgettext('phlt', 'Hello'));
t('dcgettext',                              fn() => dcgettext('phlt', 'Hello', LC_MESSAGES));
t('ngettext, one',                          fn() => ngettext('one', 'many', 1));
t('ngettext, two',                          fn() => ngettext('one', 'many', 2));
t('...zero is PLURAL',                      fn() => ngettext('one', 'many', 0));
t('...and the count is UNSIGNED',           fn() => ngettext('one', 'many', -1));
t('dngettext',                              fn() => dngettext('phlt', 'one', 'many', 3));
t('dcngettext',                             fn() => dcngettext('phlt', 'one', 'many', 1, LC_MESSAGES));
t('a NUL inside the message survives',      fn() => gettext("a\0b"));

echo "-- php's own screens\n";
t('an empty domain',                        fn() => dgettext('', 'x'));
t('...at every door that takes one',        fn() => bindtextdomain('', '.'));
t('...including the codeset one',           fn() => bind_textdomain_codeset('', 'UTF-8'));
t('...and textdomain',                      fn() => textdomain(''));
t('...and the plural doors',                fn() => dngettext('', 's', 'p', 1));
t('a domain of 1024 bytes is fine',         fn() => dgettext(str_repeat('d', 1024), 'x'));
t('one of 1025 is not',                     fn() => dgettext(str_repeat('d', 1025), 'x'));
t('a message of 4096 is fine',              fn() => strlen(gettext(str_repeat('m', 4096))));
t('one of 4097 is not',                     fn() => gettext(str_repeat('m', 4097)));
t('the singular is argument #1',            fn() => ngettext(str_repeat('m', 4097), 'p', 1));
t('the plural is argument #2',              fn() => ngettext('s', str_repeat('m', 4097), 1));
t('LC_ALL is refused by name',              fn() => dcgettext('phlt', 'x', LC_ALL));
t('...for the plural door too',             fn() => dcngettext('phlt', 's', 'p', 1, LC_ALL));
t('another category is not',                fn() => dcgettext('phlt', 'x', LC_CTYPE));
t('nor is one php has no name for',         fn() => dcgettext('phlt', 'x', 99));
t('bindtextdomain refuses a NUL domain',    fn() => bindtextdomain("a\0b", '.'));
t('...and a NUL directory',                 fn() => bindtextdomain('phlt', "a\0b"));
?>
--EXPECT--
-- the extension, with php's ten names in php's own order
bool(true)
textdomain, gettext, _, dgettext, dcgettext, bindtextdomain, ngettext, dngettext, dcngettext, bind_textdomain_codeset
-- textdomain
with nothing, the default domain             'messages'
null only QUERIES                            'messages'
setting one answers it back                  'phlt'
...and the query agrees                      'phlt'
a NUL ends the name libintl sees             'phlt'
a name that is ONLY a NUL queries            'messages'
-- bindtextdomain: php makes the directory absolute first
a domain nobody bound reads the default      '/usr/share/locale'
a relative name resolving nowhere            false
the EMPTY directory is the cwd               true
...and so is the string "0"                  true
a real one comes back canonical              true
the query answers it                         true
-- bind_textdomain_codeset
nothing bound yet                            false
binding answers the name                     'ISO-8859-1'
the query answers it                         'ISO-8859-1'
a domain with no directory takes one         'UTF-8'
an empty C string is no domain at all        false
-- the C locale reads no catalog, so every door answers its own argument
gettext                                      'Hello'
_ is the same function                       'Hello'
dgettext                                     'Hello'
dcgettext                                    'Hello'
ngettext, one                                'one'
ngettext, two                                'many'
...zero is PLURAL                            'many'
...and the count is UNSIGNED                 'many'
dngettext                                    'many'
dcngettext                                   'one'
a NUL inside the message survives            'a' . "\0" . 'b'
-- php's own screens
an empty domain                              'ValueError: dgettext(): Argument #1 ($domain) must not be empty'
...at every door that takes one              'ValueError: bindtextdomain(): Argument #1 ($domain) must not be empty'
...including the codeset one                 'ValueError: bind_textdomain_codeset(): Argument #1 ($domain) must not be empty'
...and textdomain                            'ValueError: textdomain(): Argument #1 ($domain) must not be empty'
...and the plural doors                      'ValueError: dngettext(): Argument #1 ($domain) must not be empty'
a domain of 1024 bytes is fine               'x'
one of 1025 is not                           'ValueError: dgettext(): Argument #1 ($domain) is too long'
a message of 4096 is fine                    4096
one of 4097 is not                           'ValueError: gettext(): Argument #1 ($message) is too long'
the singular is argument #1                  'ValueError: ngettext(): Argument #1 ($singular) is too long'
the plural is argument #2                    'ValueError: ngettext(): Argument #2 ($plural) is too long'
LC_ALL is refused by name                    'ValueError: dcgettext(): Argument #3 ($category) cannot be LC_ALL'
...for the plural door too                   'ValueError: dcngettext(): Argument #5 ($category) cannot be LC_ALL'
another category is not                      'x'
nor is one php has no name for               'x'
bindtextdomain refuses a NUL domain          'ValueError: bindtextdomain(): Argument #1 ($domain) must not contain any null bytes'
...and a NUL directory                       'ValueError: bindtextdomain(): Argument #2 ($directory) must not contain any null bytes'
