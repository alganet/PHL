--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The constant-expression forms php accepts in a parameter default, an enum case value and an attribute argument keep compiling (false-positive guard for the newly screened positions)
--FILE--
<?php
// The forms php accepts in the three positions the screens newly reach:
// parameter defaults, enum case values and attribute arguments.
#[Attribute]
class CeoAttr {
    public function __construct(public $a = 0, public $b = '', public $c = null) {}
}
class CeoBox { public function __construct(public int $v = 1) {} }
class CeoK { const K = 3; }

enum CeoEnum: int {
    const OFF = 10;
    case A = 1 + 1;
    case B = self::OFF * 2;
    case C = CeoK::K;
}
enum CeoStr: string {
    case A = 'a' . 'b';
    case B = CeoK::K . '';
}

#[CeoAttr(1 + 1, 'x' . 'y', [CeoK::K, self::class])]
class CeoTarget {
    public function withDefaults(
        $a = CeoK::K,
        $b = [1, 2, CeoK::K],
        $c = new CeoBox(9),                       // `new` IS allowed here
        $d = strlen(...),                         // first-class callable
        $e = static function () { return strlen('deferred'); },
        $f = self::class,
        $g = PHP_INT_SIZE >= 4
    ) {
        return [$a, $b[2], $c->v, $d('abcd'), $e(), $f, $g];
    }
}
var_dump(CeoEnum::A->value, CeoEnum::B->value, CeoEnum::C->value,
         CeoStr::A->value, CeoStr::B->value);
var_dump((new CeoTarget)->withDefaults());
$args = (new ReflectionClass('CeoTarget'))->getAttributes()[0]->getArguments();
var_dump($args);
?>
--EXPECT--
int(2)
int(20)
int(3)
string(2) "ab"
string(1) "3"
array(7) {
  [0]=>
  int(3)
  [1]=>
  int(3)
  [2]=>
  int(9)
  [3]=>
  int(4)
  [4]=>
  int(8)
  [5]=>
  string(9) "CeoTarget"
  [6]=>
  bool(true)
}
array(3) {
  [0]=>
  int(2)
  [1]=>
  string(2) "xy"
  [2]=>
  array(2) {
    [0]=>
    int(3)
    [1]=>
    string(9) "CeoTarget"
  }
}
--CLEAN--
<?php
