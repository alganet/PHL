--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
filter_var FILTER_VALIDATE_IP: IPv4/IPv6 and family flags
--FILE--
<?php
if(!function_exists("fv_show")){function fv_show($v){if($v===true)echo "T";elseif($v===false)echo "F";elseif($v===null)echo "N";elseif(is_int($v))echo "i:$v";elseif(is_float($v))echo "f:".(0+$v);else echo "s:$v";echo "
";}}

foreach (["192.168.0.1","256.0.0.1","01.0.0.0","1.2.3","::1","::ffff:192.0.2.1","2001:db8::1","::1::","fe80::"] as $x) fv_show(filter_var($x, FILTER_VALIDATE_IP));
fv_show(filter_var("::1", FILTER_VALIDATE_IP, FILTER_FLAG_IPV4));
fv_show(filter_var("1.2.3.4", FILTER_VALIDATE_IP, FILTER_FLAG_IPV6));
fv_show(filter_var("1.2.3.4", FILTER_VALIDATE_IP, FILTER_FLAG_IPV4));
?>
--EXPECT--
s:192.168.0.1
F
F
F
s:::1
s:::ffff:192.0.2.1
s:2001:db8::1
F
s:fe80::
F
F
s:1.2.3.4
