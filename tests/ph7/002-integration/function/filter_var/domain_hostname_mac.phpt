--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_DOMAIN: the lenient default, FILTER_FLAG_HOSTNAME, and the MAC dotted form
--FILE--
<?php
/* FILTER_VALIDATE_DOMAIN without a flag is a LENGTH check in php — 253 bytes,
 * labels of 63, no empty label — and PHL was applying a syntax rule it does not
 * have, so a name with a space or an '@' in it was refused where php answers it
 * back. FILTER_FLAG_HOSTNAME, which is the rule PHL was applying to everything,
 * did not exist as a constant at all. */
function dm($d)
{
    printf("%-26s len=%-3d none=%-3s host=%s\n", '[' . $d . ']', strlen($d),
        filter_var($d, FILTER_VALIDATE_DOMAIN) === false ? 'F' : 'ok',
        filter_var($d, FILTER_VALIDATE_DOMAIN, FILTER_FLAG_HOSTNAME) === false ? 'F' : 'ok');
}
foreach (['example.com', 'example', 'a', '', ' spaced ', 'a b', 'ex@mple.com', "a\tb",
          '1.2.3.4', 'A.COM', 'xn--d1acufc.xn--p1ai', 'a_b.com', '-a.com', 'a-.com',
          'a-b.com', 'a--b.com', '*.example.com', '.example.com', 'example..com',
          '.', '..', 'a.', 'a..', '0-', '0-.', 'x.0-.', '-.', '_.'] as $d) {
    dm($d);
}
// the label and total length boundaries, and the trailing dot that moves them
dm(str_repeat('a', 63));
dm(str_repeat('a', 64));
dm(str_repeat('a', 63) . '.com');
dm(str_repeat('a', 64) . '.com');
echo strlen($x = str_repeat('a.', 126) . 'a'), ' ', filter_var($x, FILTER_VALIDATE_DOMAIN) === false ? 'F' : 'ok', "\n";
echo strlen($x = str_repeat('a.', 127)), ' ', filter_var($x, FILTER_VALIDATE_DOMAIN) === false ? 'F' : 'ok', "\n";
echo strlen($x = str_repeat('a.', 127) . 'a'), ' ', filter_var($x, FILTER_VALIDATE_DOMAIN) === false ? 'F' : 'ok', "\n";

/* FILTER_VALIDATE_MAC has THREE spellings in php and PHL knew two: the dotted
 * one is how a Cisco device writes its own address. */
foreach (['00:11:22:33:44:55', '00-11-22-33-44-55', '0011.2233.4455', 'AABB.CCDD.EEFF',
          '001122334455', '00:11-22:33:44:55', '0011.2233.445', '0011.2233.4455.6677',
          '00112233.4455', '0011:2233.4455', '0011.2233.44 5'] as $m) {
    printf("%-24s %s\n", '[' . $m . ']', var_export(filter_var($m, FILTER_VALIDATE_MAC), true));
}
?>
--EXPECT--
[example.com]              len=11  none=ok  host=ok
[example]                  len=7   none=ok  host=ok
[a]                        len=1   none=ok  host=ok
[]                         len=0   none=ok  host=F
[ spaced ]                 len=8   none=ok  host=F
[a b]                      len=3   none=ok  host=F
[ex@mple.com]              len=11  none=ok  host=F
[a	b]                      len=3   none=ok  host=F
[1.2.3.4]                  len=7   none=ok  host=ok
[A.COM]                    len=5   none=ok  host=ok
[xn--d1acufc.xn--p1ai]     len=20  none=ok  host=ok
[a_b.com]                  len=7   none=ok  host=F
[-a.com]                   len=6   none=ok  host=F
[a-.com]                   len=6   none=ok  host=F
[a-b.com]                  len=7   none=ok  host=ok
[a--b.com]                 len=8   none=ok  host=ok
[*.example.com]            len=13  none=ok  host=F
[.example.com]             len=12  none=F   host=F
[example..com]             len=12  none=F   host=F
[.]                        len=1   none=F   host=F
[..]                       len=2   none=F   host=F
[a.]                       len=2   none=ok  host=ok
[a..]                      len=3   none=F   host=F
[0-]                       len=2   none=ok  host=F
[0-.]                      len=3   none=ok  host=ok
[x.0-.]                    len=5   none=ok  host=ok
[-.]                       len=2   none=ok  host=F
[_.]                       len=2   none=ok  host=F
[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa] len=63  none=ok  host=ok
[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa] len=64  none=F   host=F
[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com] len=67  none=ok  host=ok
[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.com] len=68  none=F   host=F
253 ok
254 ok
255 F
[00:11:22:33:44:55]      '00:11:22:33:44:55'
[00-11-22-33-44-55]      '00-11-22-33-44-55'
[0011.2233.4455]         '0011.2233.4455'
[AABB.CCDD.EEFF]         'AABB.CCDD.EEFF'
[001122334455]           false
[00:11-22:33:44:55]      false
[0011.2233.445]          false
[0011.2233.4455.6677]    false
[00112233.4455]          false
[0011:2233.4455]         false
[0011.2233.44 5]         false
--CLEAN--
<?php
unset($x);
