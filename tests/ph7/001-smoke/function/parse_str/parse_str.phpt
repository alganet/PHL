--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
parse_str vivifies its out-param and parses bracket nesting and appends
--FILE--
<?php
parse_str("a=1&b[]=2&b[]=3&c[x]=9&d=x%20y", $r1);
echo json_encode($r1), "\n";
parse_str("a.b=1&c d=2&x[y][z]=3&flag", $r2);
echo json_encode($r2), "\n";
parse_str("user[name]=J+D&user[tags][]=php&user[tags][]=c&n=5", $r3);
echo json_encode($r3), "\n";
parse_str("a[b][c][d]=deep&list[]=1&list[]=2", $r4);
echo json_encode($r4), "\n";
parse_str("empty=&only", $r5);
echo json_encode($r5), "\n";
parse_str("", $r6);
echo json_encode($r6), "\n";
?>
--EXPECT--
{"a":"1","b":["2","3"],"c":{"x":"9"},"d":"x y"}
{"a_b":"1","c_d":"2","x":{"y":{"z":"3"}},"flag":""}
{"user":{"name":"J D","tags":["php","c"]},"n":"5"}
{"a":{"b":{"c":{"d":"deep"}}},"list":["1","2"]}
{"empty":"","only":""}
[]
--CLEAN--
<?php
