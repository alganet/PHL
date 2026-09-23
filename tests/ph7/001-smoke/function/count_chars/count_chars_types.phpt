--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count_chars() counts BYTES, and its $string is php's declared string parameter
--FILE--
<?php
// The embedded-PHP implementation cast its argument with (string), so an array
// warned "Array to string conversion" and counted the letters of "Array", and a
// plain object raised php's cast Error instead of the ZPP TypeError. php
// declares `string $string`, which refuses both and COERCES a Stringable.
foreach ([[[1]], [new stdClass]] as $ccCase) {
    try {
        count_chars($ccCase[0], 3);
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
class CountCharsSayer { public function __toString(): string { return "abc"; } }
var_dump(count_chars(new CountCharsSayer, 3));
var_dump(count_chars(111, 3), count_chars(true, 3));

// It is byte-based and binary safe: an embedded NUL is a byte value like any
// other, and a multibyte character is its individual bytes.
var_dump(bin2hex(count_chars("a\0b", 3)));
var_dump(bin2hex(count_chars("\xff\x80\x00", 3)));
var_dump(count_chars("héllo", 1));

// Mode 2 reports the count it stored for an unused byte, which is always 0.
$ccUnused = count_chars("a", 2);
var_dump(count($ccUnused), $ccUnused[98], array_key_exists(97, $ccUnused));

// Reflection sees php's declared signature now that this is a host builtin.
$ccR = new ReflectionFunction('count_chars');
foreach ($ccR->getParameters() as $ccP) {
    echo $ccP->getPosition(), " ", $ccP->getType(), " $", $ccP->getName(),
         $ccP->isOptional() ? " (optional)" : "", "\n";
}
echo $ccR->getReturnType(), "\n";
var_dump($ccR->isInternal(), $ccR->getFileName());
?>
--EXPECT--
count_chars(): Argument #1 ($string) must be of type string, array given
count_chars(): Argument #1 ($string) must be of type string, stdClass given
string(3) "abc"
string(1) "1"
string(1) "1"
string(6) "006162"
string(6) "0080ff"
array(5) {
  [104]=>
  int(1)
  [108]=>
  int(2)
  [111]=>
  int(1)
  [169]=>
  int(1)
  [195]=>
  int(1)
}
int(255)
int(0)
bool(false)
0 string $string
1 int $mode (optional)
array|string
bool(true)
bool(false)
--CLEAN--
<?php
