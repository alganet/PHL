--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
http_build_query nests, prefixes numeric keys and honours the RFC encoding
--FILE--
<?php
echo http_build_query(["a" => 1, "b" => ["c" => 2, "d" => 3], "e" => "x y&z"]), "\n";
echo http_build_query(["a" => true, "b" => false, "c" => null, "d" => 1.5]), "\n";
echo http_build_query(["a b" => "c"], "", null, PHP_QUERY_RFC3986), "\n";
echo http_build_query([1 => "a", 2 => "b"], "pre_"), "\n";
echo http_build_query(["foo" => ["a", "b"]]), "\n";
echo http_build_query(["a" => ["x" => ["y" => 1]]]), "\n";
echo http_build_query(["a" => [], "b" => 1]), "\n";
echo http_build_query(["a" => 1, "b" => 2], "", "&amp;"), "\n";
class Hbq21Obj { public $a = 1; public $b = 2; private $c = 3; }
echo http_build_query(new Hbq21Obj()), "\n";
echo http_build_query([["a", "b"]], "n"), "\n";
echo "[", http_build_query([]), "]\n";
?>
--EXPECT--
a=1&b%5Bc%5D=2&b%5Bd%5D=3&e=x+y%26z
a=1&b=0&d=1.5
a%20b=c
pre_1=a&pre_2=b
foo%5B0%5D=a&foo%5B1%5D=b
a%5Bx%5D%5By%5D=1
b=1
a=1&amp;b=2
a=1&b=2
n0%5B0%5D=a&n0%5B1%5D=b
[]
--CLEAN--
<?php
