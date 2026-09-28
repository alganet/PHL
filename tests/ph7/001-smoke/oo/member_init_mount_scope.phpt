--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class MOUNTED inside a method, and an enum case first read there, still resolve self:: to their own class
--DESCRIPTION--
The two initializer evaluations that do not happen at `new` have the same
question to answer. A class declared where execution reaches it is MOUNTED at
that point, so its static property defaults and its typed constants run with
whatever method frame is current; and a backed enum's case materializes at the
first ACCESS, which is likewise wherever the program happened to be. Both set
the const-eval class and neither marked the frame, so `self::` walked past them
to the enclosing method and read that class's member — silently, when the two
classes both have one, as `MntCaller::OFF` and `MntTop::OFF` do here.
--FILE--
<?php
class MntCaller {
    const K   = 'WRONG';
    const OFF = 999;
    public function declareIt() {
        eval('class MntQ { const K = 5; public static $s = self::K; const CC = self::K * 3; }');
        eval('enum MntE: int { const OFF = 10; case A = self::OFF + 1; }');
        return [MntQ::$s, MntQ::CC, MntE::A->value];
    }
    public function readEnum() { return MntE::A->value; }
}
$c = new MntCaller;
var_dump($c->declareIt());

enum MntTop: int { const OFF = 10; case A = self::OFF + 1; case B = self::OFF + 2; }
class MntEnumCaller {
    const OFF = 999;
    public function first() { return MntTop::B->value; }
}
var_dump((new MntEnumCaller)->first(), MntTop::A->value);
?>
--EXPECT--
array(3) {
  [0]=>
  int(5)
  [1]=>
  int(15)
  [2]=>
  int(11)
}
int(12)
int(11)
--CLEAN--
<?php
