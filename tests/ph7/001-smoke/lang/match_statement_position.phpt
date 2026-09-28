--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
match is an expression a statement may be made of
--DESCRIPTION--
`match (true) { ... };` is how a dispatch table is written when the answer is not wanted, and
php takes it as the expression statement it is. The statement dispatcher refused the keyword
outright (`Syntax error: Unexpected keyword 'match'`), so Doctrine's DQL parser -- which
dispatches its tree walkers exactly that way -- did not compile.
--FILE--
<?php
$mspX = 1;
match ($mspX) { 1 => print("one\n"), default => null };
match (true) { 1 > 2 => print("a\n"), default => print("b\n") };
foreach ([1, 2] as $mspV) {
    match ($mspV) { 1 => print("v-one\n"), 2 => print("v-two\n") };
}
if (true) { match (1) { 1 => print("nested\n") }; }
try { match (9) { 1 => 1 }; }
catch (UnhandledMatchError $e) { echo $e->getMessage(), "\n"; }
var_dump(match ($mspX) { 1 => 'still-an-expression', default => 'd' });
class MspHolder {
    const match = 'const-name';
    public function match() { return 'method-name'; }
    public static function smatch() { return 'static-name'; }
}
echo MspHolder::match, "\n", (new MspHolder)->match(), "\n", MspHolder::smatch(), "\n";
?>
--EXPECT--
one
b
v-one
v-two
nested
Unhandled match case of type int
string(19) "still-an-expression"
const-name
method-name
static-name
--CLEAN--
<?php
