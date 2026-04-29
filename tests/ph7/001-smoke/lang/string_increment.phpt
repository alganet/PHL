--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Perl-style string increment ($a = "a"; $a++; -> "b")
--FILE--
<?php
function s_inc_silence_dep() { return true; }

// PHP 8.3+ raises E_DEPRECATED for non-numeric string increment.  Install
// a swallow-only handler for that level so the runner's default error hook
// doesn't echo deprecation lines and break our EXPECT match.  Other error
// levels fall through to whatever handler was previously installed.
if (defined('E_DEPRECATED')) {
    set_error_handler('s_inc_silence_dep', E_DEPRECATED);
}

function s_inc_check(string $label, $expected, $actual): void {
    if ($actual === $expected) {
        echo "$label OK\n";
    } else {
        echo "$label FAIL: expected ";
        var_export($expected);
        echo " got ";
        var_export($actual);
        echo "\n";
    }
}

// Empty string increments to "1".
$a = ""; $a++;
s_inc_check("empty", "1", $a);

// Single letters and uppercase chains.
$a = "a"; $a++; s_inc_check("a->b", "b", $a);
$a = "z"; $a++; s_inc_check("z->aa", "aa", $a);
$a = "Z"; $a++; s_inc_check("Z->AA", "AA", $a);
$a = "Zz"; $a++; s_inc_check("Zz->AAa", "AAa", $a);

// Long carry chains exercise the prepend path and the right-shift loop.
$a = "zzzz"; $a++; s_inc_check("zzzz->aaaaa", "aaaaa", $a);
$a = "ZZZZ"; $a++; s_inc_check("ZZZZ->AAAAA", "AAAAA", $a);

// Mixed-case carry: the interior carry walks across class boundaries until
// it hits an incrementable char.
$a = "ZzZ"; $a++; s_inc_check("ZzZ->AAaA", "AAaA", $a);

// Carry stops at a non-edge letter without prepending.
$a = "yz"; $a++; s_inc_check("yz->za", "za", $a);
$a = "az"; $a++; s_inc_check("az->ba", "ba", $a);

// Carry through digits and across letter classes.
$a = "a9"; $a++; s_inc_check("a9->b0", "b0", $a);
$a = "Z9"; $a++; s_inc_check("Z9->AA0", "AA0", $a);
$a = "Az"; $a++; s_inc_check("Az->Ba", "Ba", $a);
$a = "abc"; $a++; s_inc_check("abc->abd", "abd", $a);

// Numeric strings still go through numeric increment.
$a = "5"; $a++; s_inc_check("\"5\"->6", 6, $a);
$a = "1.5"; $a++; s_inc_check("\"1.5\"->2.5", 2.5, $a);

// Non-alphanumeric stops the carry without prepending.
$a = "!"; $a++; s_inc_check("!->!", "!", $a);
$a = "a-z"; $a++; s_inc_check("a-z->a-a", "a-a", $a);

// UTF-8 / high-bit bytes (>= 0x80) are non-alphanumeric to the byte-wise
// walker.  An ASCII char following them still increments normally; a 'z'
// preceded by them carries to 'a' but the carry stops at the high byte
// without prepending (matches PHP's byte-level increment semantics).
$a = "\xc3\xa1";  $a++; s_inc_check("á->á",   "\xc3\xa1",     $a);
$a = "\xc3\xa1b"; $a++; s_inc_check("áb->ác", "\xc3\xa1c",    $a);
$a = "\xc3\xa1z"; $a++; s_inc_check("áz->áa", "\xc3\xa1\x61", $a);

// Result of carry through letters can look numeric ("e0") but must stay a string.
$a = "d9"; $a++;
s_inc_check("d9->e0", "e0", $a);
s_inc_check("d9 stays string", true, is_string($a));

// Post-increment must preserve the OLD value (regression test for COW guard).
$a = "z"; $b = $a++;
s_inc_check("post: a", "aa", $a);
s_inc_check("post: b", "z", $b);

// Pre-increment yields the NEW value.
$a = "z"; $b = ++$a;
s_inc_check("pre:  a", "aa", $a);
s_inc_check("pre:  b", "aa", $b);

// Pre-increment of an empty string also yields "1" (helper's empty branch
// is reached whether or not the iP1 deep-copy fires).
$a = ""; $b = ++$a;
s_inc_check("pre-empty: a", "1", $a);
s_inc_check("pre-empty: b", "1", $b);

// Numeric-string post-increment must preserve the OLD string value.
// Regression test for the COW guard in the numeric-coercion branch:
// without it pTos still aliased pObj's blob and read freed memory.
$a = "5"; $b = $a++;
s_inc_check("post-num int: a", 6, $a);
s_inc_check("post-num int: b", "5", $b);

$a = "1.5"; $b = $a++;
s_inc_check("post-num float: a", 2.5, $a);
s_inc_check("post-num float: b", "1.5", $b);

// Leading-numeric junk must take the Perl path (matches PHP's is_numeric).
$a = "5foo"; $b = $a++;
s_inc_check("post-prefix: a", "5fop", $a);
s_inc_check("post-prefix: b", "5foo", $b);

// Whitespace-padded numeric strings still take the numeric path.
$a = "  5  "; $b = $a++;
s_inc_check("post-pad: a", 6, $a);
s_inc_check("post-pad: b", "  5  ", $b);
?>
--EXPECT--
empty OK
a->b OK
z->aa OK
Z->AA OK
Zz->AAa OK
zzzz->aaaaa OK
ZZZZ->AAAAA OK
ZzZ->AAaA OK
yz->za OK
az->ba OK
a9->b0 OK
Z9->AA0 OK
Az->Ba OK
abc->abd OK
"5"->6 OK
"1.5"->2.5 OK
!->! OK
a-z->a-a OK
á->á OK
áb->ác OK
áz->áa OK
d9->e0 OK
d9 stays string OK
post: a OK
post: b OK
pre:  a OK
pre:  b OK
pre-empty: a OK
pre-empty: b OK
post-num int: a OK
post-num int: b OK
post-num float: a OK
post-num float: b OK
post-prefix: a OK
post-prefix: b OK
post-pad: a OK
post-pad: b OK
--CLEAN--
<?php
unset($a, $b);
