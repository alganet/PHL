--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A Stringable reaches a builtin's `string` parameter instead of being screened out
--FILE--
<?php
class Str {
    public function __construct(private string $s) {}
    public function __toString(): string { return $this->s; }
}
class Plain {}

// php coerces a Stringable into a `string` parameter; these ten screened the
// object out before ever asking for __toString().
var_dump(str_split(new Str('abc')));
var_dump(addslashes(new Str("a'b")));
var_dump(addcslashes(new Str('abc'), 'a'));
var_dump(addcslashes('abc', new Str('a')));
var_dump(bindec(new Str('101')));
var_dump(hexdec(new Str('1f')));
var_dump(octdec(new Str('17')));
var_dump(sprintf(new Str('<%s>'), 'x'));
var_dump(vsprintf(new Str('<%s>'), ['y']));
printf(new Str("<%s>\n"), 'z');
vprintf(new Str("<%s>\n"), ['w']);
$h = fopen('php://output', 'w');
fprintf($h, new Str("<%s>\n"), 'v');
vfprintf($h, new Str("<%s>\n"), ['u']);
fclose($h);

// An empty __toString() still reaches the builtin as the empty string.
var_dump(str_split(new Str('')));

// An object with no __toString() stays a TypeError, and it names the class.
foreach ([
    'str_split'   => fn() => str_split(new Plain),
    'addslashes'  => fn() => addslashes(new Plain),
    'addcslashes' => fn() => addcslashes('abc', new Plain),
    'bindec'      => fn() => bindec(new Plain),
    'hexdec'      => fn() => hexdec(new Plain),
    'octdec'      => fn() => octdec(new Plain),
    'sprintf'     => fn() => sprintf(new Plain),
    'vsprintf'    => fn() => vsprintf(new Plain, []),
] as $name => $fn) {
    try { $fn(); echo "$name: no throw\n"; }
    catch (TypeError $e) { echo $e->getMessage(), "\n"; }
}

// An array and a resource are refused the same way they always were.
try { str_split([]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
$r = fopen('php://memory', 'r');
try { str_split($r); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
fclose($r);

// hash_equals() declares `string` too, and php refuses a Stringable there.
try { hash_equals(new Str('abc'), 'abc'); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { hash_equals('abc', new Str('abc')); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
array(3) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
  [2]=>
  string(1) "c"
}
string(4) "a\'b"
string(4) "\abc"
string(4) "\abc"
int(5)
int(31)
int(15)
string(3) "<x>"
string(3) "<y>"
<z>
<w>
<v>
<u>
array(0) {
}
str_split(): Argument #1 ($string) must be of type string, Plain given
addslashes(): Argument #1 ($string) must be of type string, Plain given
addcslashes(): Argument #2 ($characters) must be of type string, Plain given
bindec(): Argument #1 ($binary_string) must be of type string, Plain given
hexdec(): Argument #1 ($hex_string) must be of type string, Plain given
octdec(): Argument #1 ($octal_string) must be of type string, Plain given
sprintf(): Argument #1 ($format) must be of type string, Plain given
vsprintf(): Argument #1 ($format) must be of type string, Plain given
str_split(): Argument #1 ($string) must be of type string, array given
str_split(): Argument #1 ($string) must be of type string, resource given
hash_equals(): Argument #1 ($known_string) must be of type string, Str given
hash_equals(): Argument #2 ($user_string) must be of type string, Str given
