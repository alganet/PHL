--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mbstring's `auto` is an encoding LIST, the detect ORDER is state, and Windows-1252 is modelled
--FILE--
<?php
/* php's `auto` is an encoding LIST, never a single encoding: accepted where a list
 * is wanted (mb_detect_encoding's $encodings, mb_convert_encoding's $from_encoding,
 * mb_detect_order's argument) and refused everywhere else. It expands to the
 * LANGUAGE's default order -- ASCII then UTF-8 -- and NOT to whatever
 * mb_detect_order() currently holds, which only shows once a script sets one.
 * Windows-1252 comes with it: egulias/email-validator converts every character it
 * lexes from that code page. */
function mbd(string $l, callable $f): void {
    try { $r = $f(); } catch (\Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    printf("%-40s %s\n", $l, str_replace("\n", '', var_export($r, true)));
}
echo "-- auto is a LIST\n";
mbd('detect with auto (ascii)',      fn() => mb_detect_encoding('abc', 'auto', true));
mbd('detect with auto (utf-8)',      fn() => mb_detect_encoding("\xc2\xbf", 'auto', true));
mbd('detect with auto (latin-1)',    fn() => mb_detect_encoding("\xbf", 'auto', true));
mbd('detect with ["auto"]',          fn() => mb_detect_encoding('abc', ['auto'], true));
mbd('detect with "auto,UTF-8"',      fn() => mb_detect_encoding('abc', 'auto,UTF-8', true));
mbd('convert FROM auto',             fn() => bin2hex(mb_convert_encoding("\xc2\xbf", 'UTF-8', 'auto')));
mbd('...and it is not an encoding',  fn() => mb_strlen('abc', 'auto'));
mbd('...nor a target',               fn() => mb_convert_encoding('a', 'auto', 'UTF-8'));
mbd('...nor checkable',              fn() => mb_check_encoding('a', 'auto'));

echo "-- the detect ORDER\n";
mbd('default order',                 fn() => mb_detect_order());
mbd('set an order',                  fn() => mb_detect_order(['UTF-8']));
mbd('...and read it back',           fn() => mb_detect_order());
mbd('a bare detect follows it',      fn() => mb_detect_encoding('abc'));
mbd('...but auto does NOT',          fn() => mb_detect_encoding('abc', 'auto'));
mbd('aliases canonicalise',          function () { mb_detect_order('UTF8,latin1,US-ASCII,binary'); return mb_detect_order(); });
mbd('auto RESETS it',                function () { $r = mb_detect_order('auto'); return [$r, mb_detect_order()]; });
mbd('an empty list is refused',      fn() => mb_detect_order([]));
mbd('an unknown name is refused',    fn() => mb_detect_order('nosuch'));

echo "-- Windows-1252\n";
mbd('the euro sign decodes',         fn() => bin2hex(mb_convert_encoding("\x80", 'UTF-8', 'Windows-1252')));
mbd('...and encodes back',           fn() => bin2hex(mb_convert_encoding("\xe2\x82\xac", 'Windows-1252', 'UTF-8')));
mbd('a curly quote',                 fn() => bin2hex(mb_convert_encoding("\x92", 'UTF-8', 'CP1252')));
mbd('an UNDEFINED byte keeps latin1',fn() => bin2hex(mb_convert_encoding("\x81", 'UTF-8', 'Windows-1252')));
mbd('every byte is a character',     fn() => [mb_check_encoding("\x80\x81\xff", 'Windows-1252'), mb_strlen("\x80\x81\xff", 'Windows-1252')]);
mbd('a code point it cannot hold',   fn() => bin2hex(mb_convert_encoding("\xe4\xb8\x80", 'Windows-1252', 'UTF-8')));
mbd('mb_ord / mb_chr',               fn() => [mb_ord("\x92", 'Windows-1252'), bin2hex(mb_chr(0x20AC, 'Windows-1252')), mb_chr(0x4e00, 'Windows-1252')]);
mbd('the canonical spelling',        function () { $s = mb_internal_encoding(); mb_internal_encoding('CP1252'); $r = mb_internal_encoding(); mb_internal_encoding($s); return $r; });
mbd('a name php does not know',      fn() => mb_convert_encoding('a', 'UTF-8', '1252'));

echo "-- a SINGLE name is used, not detected\n";
mbd('8bit decodes as itself',        fn() => bin2hex(mb_convert_encoding("\xe9", 'UTF-8', '8bit')));
mbd('...but never WINS a detection', fn() => bin2hex(mb_convert_encoding("\xe9", 'UTF-8', '8bit,ASCII')));
mb_detect_order('auto');
--EXPECT--
-- auto is a LIST
detect with auto (ascii)                 'ASCII'
detect with auto (utf-8)                 'UTF-8'
detect with auto (latin-1)               false
detect with ["auto"]                     'ASCII'
detect with "auto,UTF-8"                 'ASCII'
convert FROM auto                        'c2bf'
...and it is not an encoding             'ValueError: mb_strlen(): Argument #2 ($encoding) must be a valid encoding, "auto" given'
...nor a target                          'ValueError: mb_convert_encoding(): Argument #2 ($to_encoding) must be a valid encoding, "auto" given'
...nor checkable                         'ValueError: mb_check_encoding(): Argument #2 ($encoding) must be a valid encoding, "auto" given'
-- the detect ORDER
default order                            array (  0 => 'ASCII',  1 => 'UTF-8',)
set an order                             true
...and read it back                      array (  0 => 'UTF-8',)
a bare detect follows it                 'UTF-8'
...but auto does NOT                     'ASCII'
aliases canonicalise                     array (  0 => 'UTF-8',  1 => 'ISO-8859-1',  2 => 'ASCII',  3 => '8bit',)
auto RESETS it                           array (  0 => true,  1 =>   array (    0 => 'ASCII',    1 => 'UTF-8',  ),)
an empty list is refused                 'ValueError: mb_detect_order(): Argument #1 ($encoding) must specify at least one encoding'
an unknown name is refused               'ValueError: mb_detect_order(): Argument #1 ($encoding) contains invalid encoding "nosuch"'
-- Windows-1252
the euro sign decodes                    'e282ac'
...and encodes back                      '80'
a curly quote                            'e28099'
an UNDEFINED byte keeps latin1           'c281'
every byte is a character                array (  0 => true,  1 => 3,)
a code point it cannot hold              '3f'
mb_ord / mb_chr                          array (  0 => 8217,  1 => '80',  2 => false,)
the canonical spelling                   'Windows-1252'
a name php does not know                 'ValueError: mb_convert_encoding(): Argument #3 ($from_encoding) contains invalid encoding "1252"'
-- a SINGLE name is used, not detected
8bit decodes as itself                   'c3a9'
...but never WINS a detection            '3f'
