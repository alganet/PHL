--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An enum's cases()/from()/tryFrom() are engine methods, not source php declares
--FILE--
<?php
// php declares these three on the UnitEnum/BackedEnum prototypes, so they are
// INTERNAL wherever the enum itself is written: no file, no line, and
// from(string|int $value) rather than the enum's own backing type.
enum EnumNatInt: int { case A = 1; case B = 2; }
enum EnumNatStr: string { case X = 'x'; case N = '7'; }
enum EnumNatPure { case P; }

foreach (['EnumNatInt', 'EnumNatStr', 'EnumNatPure'] as $e) {
    foreach ((new ReflectionClass($e))->getMethods() as $m) {
        printf("%s::%s internal=%d file=%s line=%d proto=%s ret=%s", $e, $m->getName(),
            (int)$m->isInternal(), var_export($m->getFileName(), true),
            (int)$m->getStartLine(), $m->hasPrototype() ? $m->getPrototype()->class : '-',
            var_export((string)$m->getReturnType(), true));
        foreach ($m->getParameters() as $p) {
            printf(" param=%s:%s", $p->getName(), (string)$p->getType());
        }
        echo "\n";
    }
}
// The prototypes themselves carry php's declared types.
echo new ReflectionClass('BackedEnum'), "\n";

// Behaviour does not move: weak coercion still applies, an int-backed enum
// takes "02" as 2, and a string-backed one takes an int through the int arm
// first, so from(1.0) looks for "1" and from(false) for "0".
$vals = [1, 2, 99, '1', '02', ' 2', '2 ', '7', 7, 1.0, 2.0, true, false, 'x', 'X', '', '1e0'];
foreach ($vals as $v) {
    foreach ([['EnumNatInt', 'from'], ['EnumNatInt', 'tryFrom'],
              ['EnumNatStr', 'from'], ['EnumNatStr', 'tryFrom']] as [$e, $m]) {
        try {
            $r = $e::$m($v);
            echo $e, '::', $m, '(', var_export($v, true), ')=', $r === null ? 'NULL' : $r->name, "\n";
        } catch (Throwable $t) {
            echo $e, '::', $m, '(', var_export($v, true), ')! ', get_class($t), ': ', $t->getMessage(), "\n";
        }
    }
}
var_dump(EnumNatInt::cases(), EnumNatPure::cases());
var_dump(EnumNatInt::from(1) === EnumNatInt::A, EnumNatInt::A instanceof BackedEnum,
    EnumNatPure::P instanceof BackedEnum, EnumNatPure::P instanceof UnitEnum);
$fcc = EnumNatInt::tryFrom(...);
var_dump($fcc(2) === EnumNatInt::B, is_callable(['EnumNatStr', 'from']));

// Arity is enforced from the declared signature, php's wording for an internal.
foreach ([['EnumNatInt', 'from', []], ['EnumNatInt', 'from', [1, 2]],
          ['EnumNatInt', 'cases', [1]], ['EnumNatPure', 'cases', [1]]] as [$e, $m, $a]) {
    try {
        $e::$m(...$a);
    } catch (ArgumentCountError $t) {
        echo $t->getMessage(), "\n";
    }
}

// A user class may override a NATIVE method: its parameters live in a signature
// STRING rather than compiled records, and counting those as "no parameters"
// used to make every such override incompatible.
class EnumNatChildDate extends DateTime {
    public function format(string $format): string { return 'overridden'; }
}
echo (new EnumNatChildDate('2026-01-01'))->format('Y'), "\n";
--EXPECT--
EnumNatInt::cases internal=1 file=false line=0 proto=UnitEnum ret='array'
EnumNatInt::from internal=1 file=false line=0 proto=BackedEnum ret='static' param=value:string|int
EnumNatInt::tryFrom internal=1 file=false line=0 proto=BackedEnum ret='?static' param=value:string|int
EnumNatStr::cases internal=1 file=false line=0 proto=UnitEnum ret='array'
EnumNatStr::from internal=1 file=false line=0 proto=BackedEnum ret='static' param=value:string|int
EnumNatStr::tryFrom internal=1 file=false line=0 proto=BackedEnum ret='?static' param=value:string|int
EnumNatPure::cases internal=1 file=false line=0 proto=UnitEnum ret='array'
Interface [ <internal:Core> interface BackedEnum extends UnitEnum ] {

  - Constants [0] {
  }

  - Static properties [0] {
  }

  - Static methods [3] {
    Method [ <internal:Core> abstract static public method from ] {

      - Parameters [1] {
        Parameter #0 [ <required> string|int $value ]
      }
      - Return [ static ]
    }

    Method [ <internal:Core> abstract static public method tryFrom ] {

      - Parameters [1] {
        Parameter #0 [ <required> string|int $value ]
      }
      - Return [ ?static ]
    }

    Method [ <internal:Core, inherits UnitEnum> abstract static public method cases ] {

      - Parameters [0] {
      }
      - Return [ array ]
    }
  }

  - Properties [0] {
  }

  - Methods [0] {
  }
}

EnumNatInt::from(1)=A
EnumNatInt::tryFrom(1)=A
EnumNatStr::from(1)! ValueError: "1" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom(1)=NULL
EnumNatInt::from(2)=B
EnumNatInt::tryFrom(2)=B
EnumNatStr::from(2)! ValueError: "2" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom(2)=NULL
EnumNatInt::from(99)! ValueError: 99 is not a valid backing value for enum EnumNatInt
EnumNatInt::tryFrom(99)=NULL
EnumNatStr::from(99)! ValueError: "99" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom(99)=NULL
EnumNatInt::from('1')=A
EnumNatInt::tryFrom('1')=A
EnumNatStr::from('1')! ValueError: "1" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom('1')=NULL
EnumNatInt::from('02')=B
EnumNatInt::tryFrom('02')=B
EnumNatStr::from('02')! ValueError: "02" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom('02')=NULL
EnumNatInt::from(' 2')=B
EnumNatInt::tryFrom(' 2')=B
EnumNatStr::from(' 2')! ValueError: " 2" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom(' 2')=NULL
EnumNatInt::from('2 ')=B
EnumNatInt::tryFrom('2 ')=B
EnumNatStr::from('2 ')! ValueError: "2 " is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom('2 ')=NULL
EnumNatInt::from('7')! ValueError: 7 is not a valid backing value for enum EnumNatInt
EnumNatInt::tryFrom('7')=NULL
EnumNatStr::from('7')=N
EnumNatStr::tryFrom('7')=N
EnumNatInt::from(7)! ValueError: 7 is not a valid backing value for enum EnumNatInt
EnumNatInt::tryFrom(7)=NULL
EnumNatStr::from(7)=N
EnumNatStr::tryFrom(7)=N
EnumNatInt::from(1.0)=A
EnumNatInt::tryFrom(1.0)=A
EnumNatStr::from(1.0)! ValueError: "1" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom(1.0)=NULL
EnumNatInt::from(2.0)=B
EnumNatInt::tryFrom(2.0)=B
EnumNatStr::from(2.0)! ValueError: "2" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom(2.0)=NULL
EnumNatInt::from(true)=A
EnumNatInt::tryFrom(true)=A
EnumNatStr::from(true)! ValueError: "1" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom(true)=NULL
EnumNatInt::from(false)! ValueError: 0 is not a valid backing value for enum EnumNatInt
EnumNatInt::tryFrom(false)=NULL
EnumNatStr::from(false)! ValueError: "0" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom(false)=NULL
EnumNatInt::from('x')! TypeError: EnumNatInt::from(): Argument #1 ($value) must be of type int, string given
EnumNatInt::tryFrom('x')! TypeError: EnumNatInt::tryFrom(): Argument #1 ($value) must be of type int, string given
EnumNatStr::from('x')=X
EnumNatStr::tryFrom('x')=X
EnumNatInt::from('X')! TypeError: EnumNatInt::from(): Argument #1 ($value) must be of type int, string given
EnumNatInt::tryFrom('X')! TypeError: EnumNatInt::tryFrom(): Argument #1 ($value) must be of type int, string given
EnumNatStr::from('X')! ValueError: "X" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom('X')=NULL
EnumNatInt::from('')! TypeError: EnumNatInt::from(): Argument #1 ($value) must be of type int, string given
EnumNatInt::tryFrom('')! TypeError: EnumNatInt::tryFrom(): Argument #1 ($value) must be of type int, string given
EnumNatStr::from('')! ValueError: "" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom('')=NULL
EnumNatInt::from('1e0')=A
EnumNatInt::tryFrom('1e0')=A
EnumNatStr::from('1e0')! ValueError: "1e0" is not a valid backing value for enum EnumNatStr
EnumNatStr::tryFrom('1e0')=NULL
array(2) {
  [0]=>
  enum(EnumNatInt::A)
  [1]=>
  enum(EnumNatInt::B)
}
array(1) {
  [0]=>
  enum(EnumNatPure::P)
}
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
EnumNatInt::from() expects exactly 1 argument, 0 given
EnumNatInt::from() expects exactly 1 argument, 2 given
EnumNatInt::cases() expects exactly 0 arguments, 1 given
EnumNatPure::cases() expects exactly 0 arguments, 1 given
overridden
--CLEAN--
<?php
