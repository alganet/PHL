--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base64_decode $strict rejects invalid characters and bad padding
--FILE--
<?php
/* Print result as text; binary/false rendered unambiguously. */
function b64dump($s, $strict) {
    $r = base64_decode($s, $strict);
    if ($r === false) { echo "false\n"; return; }
    /* hex-encode so non-ASCII bytes stay stable across the --EXPECT-- match */
    echo bin2hex($r), "\n";
}
/* Strict: a byte outside the base64 alphabet fails with false. */
b64dump('!!!!', true);        // false
b64dump('SG*k=', true);       // false (star is invalid)
/* Strict: whitespace (\t \n \r space) is skipped, not rejected. */
b64dump('SG k=', true);       // "Hi"
b64dump("SGVs\tbG8=", true);  // "Hello"
b64dump(' SGk=', true);       // "Hi"
/* Strict: valid input round-trips. */
b64dump('SGk=', true);        // "Hi"
b64dump('YQ==', true);        // "a"
b64dump('++//', true);        // fbefff
/* Strict: padding/length validation. */
b64dump('a', true);           // false (lone char)
b64dump('YQ=', true);         // false (bad padding)
b64dump('YQ', true);          // "a"  (no padding, valid)
b64dump('ab', true);          // "i"
/* Non-strict skips invalid bytes and whitespace (best effort), never false. */
b64dump('!!!!', false);       // "" -> empty hex line
b64dump('S G k =', false);    // "Hi"
b64dump('SGVsbG8=', false);   // "Hello"
?>
--EXPECT--
false
false
4869
48656c6c6f
4869
4869
61
fbefff
false
false
61
69

4869
48656c6c6f
