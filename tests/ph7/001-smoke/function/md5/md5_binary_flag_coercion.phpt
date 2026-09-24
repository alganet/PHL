--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
md5()/sha1()'s $binary flag is COERCED like every other declared bool, not read only when it already is one
--DESCRIPTION--
php parses this parameter with Z_PARAM_BOOL, so weak mode converts whatever
arrives: md5($s, 1) and md5($s, "1") ask for the raw digest exactly as true
does. md5() and sha1() read the argument only when it was ALREADY a bool and
answered the 32-character hex digest for every other spelling — a different
string, of a different length, in silence. Their _file twins and the whole
hash() family were already correct, which is what makes this two functions
rather than a family.
--FILE--
<?php
foreach ([true, 1, "1", "yes", 1.5, PHP_INT_MAX] as $truthy) {
    echo strlen(md5("a", $truthy)), " ", strlen(sha1("a", $truthy)), " ";
}
echo "\n";
// (null is §10's null-to-a-non-nullable-scalar refusal, not this rule)
foreach ([false, 0, "", "0", 0.0] as $falsy) {
    echo strlen(md5("a", $falsy)), " ", strlen(sha1("a", $falsy)), " ";
}
echo "\n";
// the raw digest is the hex one's bytes, whichever spelling asked for it
echo var_export(bin2hex(md5("a", 1)) === md5("a"), true), " ",
     var_export(bin2hex(sha1("a", "yes")) === sha1("a"), true), "\n";
// an argument no bool can be made of is still a TypeError
try { md5("a", [1]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { sha1("a", new stdClass); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
16 20 16 20 16 20 16 20 16 20 16 20 
32 40 32 40 32 40 32 40 32 40 
true true
md5(): Argument #2 ($binary) must be of type bool, array given
sha1(): Argument #2 ($binary) must be of type bool, stdClass given
--CLEAN--
<?php
