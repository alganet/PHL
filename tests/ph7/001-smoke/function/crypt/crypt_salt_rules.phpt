--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
crypt() setting-string rules: failure tokens, rounds= parsing, salt truncation and alphabets
--DESCRIPTION--
What each scheme accepts is as load-bearing as what it computes. A malformed
setting is the "*0" token — "*1" when the setting itself begins with "*0", so
the token never verifies against itself. "rounds=N" is a spec only when its
digit run ends at a '$' (otherwise the whole field is an ordinary salt, as in
"rounds=1000x"), leading zeros are canonicalised, and an out-of-range count is
refused, not clamped. DES wants two ascii64 characters and re-encodes them;
MD5 stops its salt at '$' or 8 bytes; SHA at '$' or 16; bcrypt wants exactly
22 salt characters and ignores the rest; ext-DES refuses a zero count.
--FILE--
<?php
foreach (['', 'a', '!@', '*0', '*1', '*', '$3$x$y', '_11', '_1111san', '_1111sa!t', '_....abcd',
          '$2y$03$0123456789012345678901', '$2y$32$0123456789012345678901',
          '$2y$4$01234567890123456789012', '$2y$04$012345678901234567890',
          '$5$rounds=999$mysalt', '$5$rounds=1000000000$mysalt', '$5$rounds=$mysalt'] as $salt) {
    echo str_replace("\n", '', var_export($salt, true)), ' => ', crypt('p', $salt), "\n";
}
echo crypt('p', 'ab$junk'), "\n";           // DES: chars past 2 ignored
echo crypt('p', '$1$123456789$'), "\n";     // MD5: salt truncated to 8
echo crypt('p', '$1$sa$lt$'), "\n";         // MD5: salt ends at first $
echo crypt('p', '$1$salt'), "\n";           // MD5: unterminated salt is fine
echo crypt('p', '$5$rounds=01000$mysalt'), "\n";   // leading zeros canonicalised
echo crypt('p', '$5$rounds=1000x$mysalt'), "\n";   // not a spec: salt "rounds=1000x"
echo crypt('p', '$6$rounds=5000$ab$'), "\n";       // explicit default is still echoed
echo crypt('p', '$6$0123456789abcdef0$'), "\n";    // SHA salt truncated to 16
echo crypt('p', '$2y$04$0123456789012345678901extra'), "\n";  // bcrypt: extra ignored
var_dump(crypt("a\0b", 'ab') === crypt('a', 'ab'));           // NUL ends the password
try { crypt('x'); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
'' => *0
'a' => *0
'!@' => *0
'*0' => *1
'*1' => *0
'*' => *0
'$3$x$y' => *0
'_11' => *0
'_1111san' => *0
'_1111sa!t' => *0
'_....abcd' => *0
'$2y$03$0123456789012345678901' => *0
'$2y$32$0123456789012345678901' => *0
'$2y$4$01234567890123456789012' => *0
'$2y$04$012345678901234567890' => *0
'$5$rounds=999$mysalt' => *0
'$5$rounds=1000000000$mysalt' => *0
'$5$rounds=$mysalt' => *0
ab8Smhzf5D4wA
$1$12345678$nYXUJE.mBJyqCGS2l1xPQ.
$1$sa$nh747.LyA7PUoGlB0ux8n1
$1$salt$NS8.kqcTYt.fmSL.V8Lvz/
$5$rounds=1000$mysalt$1rXuEs2J7pRbAoD0PD5nE.K9X00ByWgp/i9VGpoEjM.
$5$rounds=1000x$pEnfvMhT9Eo7uFj09kcaWB/NdlLXf3YDG6Kaz2irE15
$6$rounds=5000$ab$53oaJstz.dN9TxWNvOp1k.ptOameFLjtwzhrAT.A4i5Nh4FRb1uOnAq9pDLyQwXNLNGKGS8kcJ8LZw8QB3RYk.
$6$0123456789abcdef$pUoilKX2fNNXQM/D9bilo9FJQBKgG.7it1plknW1DYjJWacS2l/V1y6NNjjKYm3ccWsx6QlzdrJtbng1tFPwb/
$2y$04$012345678901234567890ul5i3BMaI0IIZDGeKtPbaJqIYlvtJkd6
bool(true)
crypt() expects exactly 2 arguments, 1 given
--CLEAN--
<?php
