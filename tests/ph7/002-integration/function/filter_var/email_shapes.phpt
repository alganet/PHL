--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_EMAIL: quoted local parts, address literals, the length limits and FILTER_FLAG_EMAIL_UNICODE
--FILE--
<?php
/* php's email filter is a REGEX (Michael Rushton's, cut down to routable
 * addresses). PHL screened by hand, which got five kinds of address wrong: a
 * quoted local part and an address-literal domain were refused, the 64-byte
 * local part and 254-byte total were unbounded, a bare IPv4 domain and a
 * digit-leading TLD were accepted, and a non-ASCII local part was accepted
 * unconditionally — which is the FILTER_FLAG_EMAIL_UNICODE answer given to a
 * program that did not ask for it. The flag itself was not defined. */
function em($e)
{
    printf("%-34s n=%-4s u=%s\n", '[' . addcslashes($e, "\0..\37") . ']',
        filter_var($e, FILTER_VALIDATE_EMAIL) === false ? 'F' : 'ok',
        filter_var($e, FILTER_VALIDATE_EMAIL, FILTER_FLAG_EMAIL_UNICODE) === false ? 'F' : 'ok');
}
foreach (['a@b.com', 'a.b@c.co.uk', "a!#$%&'*+-/=?^_`{|}~@b.com", 'A@B.COM',
          'a@b', 'a@b.c', 'a..b@c.com', '.a@b.com', 'a.@b.com', '@b.com', 'a@', 'a',
          '"a@b"@c.com', '"a\\"b"@c.com', 'a"b@c.com', '"a b"@c.com',
          'a@[127.0.0.1]', 'a@[IPv6:::1]', 'a@[1.2.3]', 'a@[300.1.1.1]', 'a@[]',
          'a@1.2.3.4', 'a@b.4', 'a@b.1a', 'a@b.a1', 'a@4.b',
          'a@-b.com', 'a@b-.com', 'a@b_c.com', 'a@b..com', 'a@b.com.',
          'a@xn--tda.com', "\xc3\xbc@b.com", "a@\xc3\xbc.com",
          'a b@c.com', "a\nb@c.com", 'a(comment)@b.com', 'a@@b.com'] as $e) {
    em($e);
}
// the length limits the hand-rolled screen never had
em(str_repeat('a', 64) . '@b.com');
em(str_repeat('a', 65) . '@b.com');
em('a@' . str_repeat('b', 63) . '.com');
em('a@' . str_repeat('b', 64) . '.com');
echo strlen($x = str_repeat('a', 64) . '@' . str_repeat('b.', 100) . 'com'), ' ',
     filter_var($x, FILTER_VALIDATE_EMAIL) === false ? 'F' : 'ok', "\n";
?>
--EXPECT--
[a@b.com]                          n=ok   u=ok
[a.b@c.co.uk]                      n=ok   u=ok
[a!#$%&'*+-/=?^_`{|}~@b.com]       n=ok   u=ok
[A@B.COM]                          n=ok   u=ok
[a@b]                              n=F    u=F
[a@b.c]                            n=ok   u=ok
[a..b@c.com]                       n=F    u=F
[.a@b.com]                         n=F    u=F
[a.@b.com]                         n=F    u=F
[@b.com]                           n=F    u=F
[a@]                               n=F    u=F
[a]                                n=F    u=F
["a@b"@c.com]                      n=ok   u=ok
["a\"b"@c.com]                     n=ok   u=ok
[a"b@c.com]                        n=F    u=F
["a b"@c.com]                      n=F    u=F
[a@[127.0.0.1]]                    n=ok   u=ok
[a@[IPv6:::1]]                     n=ok   u=ok
[a@[1.2.3]]                        n=F    u=F
[a@[300.1.1.1]]                    n=F    u=F
[a@[]]                             n=F    u=F
[a@1.2.3.4]                        n=F    u=F
[a@b.4]                            n=F    u=F
[a@b.1a]                           n=F    u=F
[a@b.a1]                           n=ok   u=ok
[a@4.b]                            n=ok   u=ok
[a@-b.com]                         n=F    u=F
[a@b-.com]                         n=F    u=F
[a@b_c.com]                        n=F    u=F
[a@b..com]                         n=F    u=F
[a@b.com.]                         n=F    u=F
[a@xn--tda.com]                    n=ok   u=ok
[ü@b.com]                         n=F    u=ok
[a@ü.com]                         n=F    u=F
[a b@c.com]                        n=F    u=F
[a\nb@c.com]                       n=F    u=F
[a(comment)@b.com]                 n=F    u=F
[a@@b.com]                         n=F    u=F
[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@b.com] n=ok   u=ok
[aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa@b.com] n=F    u=F
[a@bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.com] n=ok   u=ok
[a@bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.com] n=F    u=F
268 F
--CLEAN--
<?php
unset($x);
