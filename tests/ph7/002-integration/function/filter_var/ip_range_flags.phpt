--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_IP: NO_PRIV_RANGE, NO_RES_RANGE and GLOBAL_RANGE
--FILE--
<?php
/* The three flags that ask what an address is FOR rather than how it is spelled
 * were undefined constants here, and the filter under them read only the two
 * family flags — so the SSRF screen every "fetch this user-supplied URL" guard
 * is written with (`FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE`) was
 * a fatal if it was spelled, and passed 127.0.0.1 through if the flags reached
 * the filter as bare integers. */
function ipf($ip)
{
    $f = [
        'none'   => 0,
        'priv'   => FILTER_FLAG_NO_PRIV_RANGE,
        'res'    => FILTER_FLAG_NO_RES_RANGE,
        'global' => FILTER_FLAG_GLOBAL_RANGE,
        'both'   => FILTER_FLAG_NO_PRIV_RANGE | FILTER_FLAG_NO_RES_RANGE,
    ];
    $out = [];
    foreach ($f as $name => $flags) {
        $out[] = $name . '=' . (filter_var($ip, FILTER_VALIDATE_IP, $flags) === false ? 'F' : 'ok');
    }
    echo str_pad($ip, 20), ' ', implode(' ', $out), "\n";
}
// IPv4: the private blocks, the reserved blocks, and the extra GLOBAL ones
foreach (['8.8.8.8', '10.1.2.3', '9.255.255.255', '11.0.0.1', '172.15.0.1', '172.16.0.1',
          '172.31.255.255', '172.32.0.1', '192.168.0.1', '192.169.0.1', '0.0.0.0',
          '127.0.0.1', '169.254.1.1', '169.253.1.1', '224.0.0.1', '239.255.255.255',
          '240.0.0.1', '255.255.255.255', '100.63.0.1', '100.64.0.1', '100.127.255.1',
          '100.128.0.1', '192.0.0.1', '192.0.1.1', '192.0.2.1', '198.18.0.1', '198.19.0.1',
          '198.20.0.1', '198.51.100.1', '198.51.101.1', '203.0.113.1', '203.0.114.1'] as $ip) {
    ipf($ip);
}
// IPv6: the ULA block, the link-local block, the two IPv4-in-IPv6 prefixes and
// the four GLOBAL-only blocks
foreach (['2a00:1450::1', '::', '::1', '::2', 'fe80::1', 'febf::1', 'fec0::1',
          'fc00::1', 'fdff::1', 'fe00::1', '::ffff:127.0.0.1', '::ffff:0:1.2.3.4',
          '2001::1', '2001:1ff::1', '2001:200::1', '2001:db8::1', '2001:db9::1',
          '2002::1', '2003::1', '100::1', '100:0:0:1::1'] as $ip) {
    ipf($ip);
}
// the family flags still decide which spelling is read at all
var_dump(filter_var('10.0.0.1', FILTER_VALIDATE_IP, FILTER_FLAG_IPV6 | FILTER_FLAG_NO_PRIV_RANGE));
var_dump(filter_var('fc00::1', FILTER_VALIDATE_IP, FILTER_FLAG_IPV4 | FILTER_FLAG_NO_PRIV_RANGE));
var_dump(filter_var('8.8.8.8', FILTER_VALIDATE_IP, FILTER_FLAG_IPV4 | FILTER_FLAG_GLOBAL_RANGE));
?>
--EXPECT--
8.8.8.8              none=ok priv=ok res=ok global=ok both=ok
10.1.2.3             none=ok priv=F res=ok global=F both=F
9.255.255.255        none=ok priv=ok res=ok global=ok both=ok
11.0.0.1             none=ok priv=ok res=ok global=ok both=ok
172.15.0.1           none=ok priv=ok res=ok global=ok both=ok
172.16.0.1           none=ok priv=F res=ok global=F both=F
172.31.255.255       none=ok priv=F res=ok global=F both=F
172.32.0.1           none=ok priv=ok res=ok global=ok both=ok
192.168.0.1          none=ok priv=F res=ok global=F both=F
192.169.0.1          none=ok priv=ok res=ok global=ok both=ok
0.0.0.0              none=ok priv=ok res=F global=F both=F
127.0.0.1            none=ok priv=ok res=F global=F both=F
169.254.1.1          none=ok priv=ok res=F global=F both=F
169.253.1.1          none=ok priv=ok res=ok global=ok both=ok
224.0.0.1            none=ok priv=ok res=ok global=ok both=ok
239.255.255.255      none=ok priv=ok res=ok global=ok both=ok
240.0.0.1            none=ok priv=ok res=F global=F both=F
255.255.255.255      none=ok priv=ok res=F global=F both=F
100.63.0.1           none=ok priv=ok res=ok global=ok both=ok
100.64.0.1           none=ok priv=ok res=ok global=F both=ok
100.127.255.1        none=ok priv=ok res=ok global=F both=ok
100.128.0.1          none=ok priv=ok res=ok global=ok both=ok
192.0.0.1            none=ok priv=ok res=ok global=F both=ok
192.0.1.1            none=ok priv=ok res=ok global=ok both=ok
192.0.2.1            none=ok priv=ok res=ok global=F both=ok
198.18.0.1           none=ok priv=ok res=ok global=F both=ok
198.19.0.1           none=ok priv=ok res=ok global=F both=ok
198.20.0.1           none=ok priv=ok res=ok global=ok both=ok
198.51.100.1         none=ok priv=ok res=ok global=F both=ok
198.51.101.1         none=ok priv=ok res=ok global=ok both=ok
203.0.113.1          none=ok priv=ok res=ok global=F both=ok
203.0.114.1          none=ok priv=ok res=ok global=ok both=ok
2a00:1450::1         none=ok priv=ok res=ok global=ok both=ok
::                   none=ok priv=ok res=F global=F both=F
::1                  none=ok priv=ok res=F global=F both=F
::2                  none=ok priv=ok res=ok global=ok both=ok
fe80::1              none=ok priv=ok res=F global=F both=F
febf::1              none=ok priv=ok res=F global=F both=F
fec0::1              none=ok priv=ok res=ok global=ok both=ok
fc00::1              none=ok priv=F res=ok global=F both=F
fdff::1              none=ok priv=F res=ok global=F both=F
fe00::1              none=ok priv=ok res=ok global=ok both=ok
::ffff:127.0.0.1     none=ok priv=ok res=F global=F both=F
::ffff:0:1.2.3.4     none=ok priv=ok res=F global=F both=F
2001::1              none=ok priv=ok res=ok global=F both=ok
2001:1ff::1          none=ok priv=ok res=ok global=F both=ok
2001:200::1          none=ok priv=ok res=ok global=ok both=ok
2001:db8::1          none=ok priv=ok res=ok global=F both=ok
2001:db9::1          none=ok priv=ok res=ok global=ok both=ok
2002::1              none=ok priv=ok res=ok global=F both=ok
2003::1              none=ok priv=ok res=ok global=ok both=ok
100::1               none=ok priv=ok res=ok global=F both=ok
100:0:0:1::1         none=ok priv=ok res=ok global=ok both=ok
bool(false)
bool(false)
string(7) "8.8.8.8"
--CLEAN--
<?php
