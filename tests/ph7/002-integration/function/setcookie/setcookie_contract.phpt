--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: setcookie()/setrawcookie() — what a name and a raw value may be, and the options array
--FILE--
<?php
$out = [];
$t = function ($fn) use (&$out) {
    try {
        $out[] = var_export($fn(), true);
    } catch (Throwable $e) {
        $out[] = get_class($e) . ": " . $e->getMessage();
    }
};
// The NAME goes on the wire unchanged, so php refuses the bytes that would make
// the header parse as something else.
$t(fn() => setcookie("a;b", "v"));
$t(fn() => setcookie("a=b", "v"));
$t(fn() => setcookie("", "v"));
// A setcookie() VALUE is url-encoded and so may hold anything; the RAW one may not.
$t(fn() => setcookie("a", "v w"));
$t(fn() => setrawcookie("a", "v w"));
$t(fn() => setrawcookie("a", "v,w"));
// The options ARRAY: the key match is case-insensitive and an unknown key is a
// ValueError, so a typo cannot silently drop the attribute it was meant to set.
$t(fn() => setcookie("a", "v", ["SameSite" => "Lax"]));
$t(fn() => setcookie("a", "v", ["bogus" => 1]));
$t(fn() => setcookie("a", "v", ["expires" => 1, "BOGUS" => 1]));
$t(fn() => setcookie("a", "v", []));
$t(fn() => setcookie("a", "v", ["partitioned" => true, "secure" => true]));
// php's CLI SAPI takes the header and answers true; nothing ever prints it.
$t(fn() => setcookie("plain", "v1"));
$out[] = count(headers_list());
echo implode("\n", $out), "\n";
?>
--EXPECT--
ValueError: setcookie(): Argument #1 ($name) cannot contain "=", ",", ";", " ", "\t", "\r", "\n", "\013", or "\014"
ValueError: setcookie(): Argument #1 ($name) cannot contain "=", ",", ";", " ", "\t", "\r", "\n", "\013", or "\014"
ValueError: setcookie(): Argument #1 ($name) must not be empty
true
ValueError: setrawcookie(): Argument #2 ($value) cannot contain ",", ";", " ", "\t", "\r", "\n", "\013", or "\014"
ValueError: setrawcookie(): Argument #2 ($value) cannot contain ",", ";", " ", "\t", "\r", "\n", "\013", or "\014"
true
ValueError: setcookie(): option "bogus" is invalid
ValueError: setcookie(): option "BOGUS" is invalid
true
true
true
0
--CLEAN--
<?php
unset($out, $t);
