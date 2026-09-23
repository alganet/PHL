--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reflection __toString(): php's export format, straight from the engine
--FILE--
<?php
/* The `@@` line names the file the declaration came from — this test's own
 * path — so it is normalized away; everything else is byte-exact php. */
function rExShow($s) { echo preg_replace('/^(\s*@@ ).*?([0-9]+ ?-+ ?[0-9]+)$/m', '${1}FILE $2', (string)$s); }
abstract class RExBase {
    const IMPL = 1;
    final const FIN = 2;
    protected const PROT = 'p';
    private const PRIV = [1, 2];
    public static $sp = 'x';
    protected readonly int $ro;
    private ?string $opt = null;
    public array $mix = [1, 'k' => 2];
    public array $list = [1, 2];
    public function __construct(public int $promoted = 5) {}
    abstract public function absM(): void;
    final public static function statM(int $a, string ...$rest): ?array { return null; }
    public function over(&$byref, $d = "x\ny", $e = 1.0, $f = M_PI, $g = null) {}
    private function priv() {}
}
final class RExKid extends RExBase {
    public function absM(): void {}
    public function over(&$byref, $d = "x\ny", $e = 1.0, $f = M_PI, $g = null) {}
}
readonly class RExRo { public function __construct(public int $x = 1) {} }
enum RExPure { case P; }
enum RExBacked: string { case Q = 'q'; case R = 'r'; }
class RExSet { public private(set) string $ps = 'a'; }
class RExHook { public string $h = 'v' { get => $this->h; set => $this->h = $value; } }
function rExFn(int $a, ?RExKid $b = null, int|string $c = 3, mixed ...$v): RExKid|false { return false; }

rExShow(new ReflectionClass('RExBase'));
rExShow(new ReflectionClass('RExKid'));
rExShow(new ReflectionClass('RExRo'));
foreach (['RExPure', 'RExBacked'] as $e) {
    $s = (string)new ReflectionClass($e);
    rExShow(substr($s, 0, strpos($s, '  - Static methods')));
}
rExShow(new ReflectionClass('RExSet'));
rExShow(new ReflectionClass('RExHook'));
rExShow(new ReflectionFunction('rExFn'));
rExShow(new ReflectionMethod('RExKid', 'over'));
rExShow(new ReflectionProperty('RExBase', 'mix'), "\n");
rExShow(new ReflectionClassConstant('RExBase', 'FIN'), "\n");
rExShow((new ReflectionFunction('rExFn'))->getParameters()[1]); echo "\n";
--EXPECT--
Class [ <user> abstract class RExBase ] {
  @@ FILE 5-20

  - Constants [4] {
    Constant [ public int IMPL ] { 1 }
    Constant [ final public int FIN ] { 2 }
    Constant [ protected string PROT ] { p }
    Constant [ private array PRIV ] { Array }
  }

  - Static properties [1] {
    Property [ public static $sp = 'x' ]
  }

  - Static methods [1] {
    Method [ <user> final static public method statM ] {
      @@ FILE 17 - 17

      - Parameters [2] {
        Parameter #0 [ <required> int $a ]
        Parameter #1 [ <optional> string ...$rest ]
      }
      - Return [ ?array ]
    }
  }

  - Properties [5] {
    Property [ protected readonly int $ro ]
    Property [ private ?string $opt = NULL ]
    Property [ public array $mix = [0 => 1, 'k' => 2] ]
    Property [ public array $list = [1, 2] ]
    Property [ public int $promoted ]
  }

  - Methods [4] {
    Method [ <user, ctor> public method __construct ] {
      @@ FILE 15 - 15

      - Parameters [1] {
        Parameter #0 [ <optional> int $promoted = 5 ]
      }
    }

    Method [ <user> abstract public method absM ] {
      @@ FILE 16 - 16

      - Parameters [0] {
      }
      - Return [ void ]
    }

    Method [ <user> public method over ] {
      @@ FILE 18 - 18

      - Parameters [5] {
        Parameter #0 [ <required> &$byref ]
        Parameter #1 [ <optional> $d = 'x\ny' ]
        Parameter #2 [ <optional> $e = 1.0 ]
        Parameter #3 [ <optional> $f = M_PI ]
        Parameter #4 [ <optional> $g = NULL ]
      }
    }

    Method [ <user> private method priv ] {
      @@ FILE 19 - 19
    }
  }
}
Class [ <user> final class RExKid extends RExBase ] {
  @@ FILE 21-24

  - Constants [3] {
    Constant [ public int IMPL ] { 1 }
    Constant [ final public int FIN ] { 2 }
    Constant [ protected string PROT ] { p }
  }

  - Static properties [1] {
    Property [ public static $sp = 'x' ]
  }

  - Static methods [1] {
    Method [ <user, inherits RExBase> final static public method statM ] {
      @@ FILE 17 - 17

      - Parameters [2] {
        Parameter #0 [ <required> int $a ]
        Parameter #1 [ <optional> string ...$rest ]
      }
      - Return [ ?array ]
    }
  }

  - Properties [4] {
    Property [ protected readonly int $ro ]
    Property [ public array $mix = [0 => 1, 'k' => 2] ]
    Property [ public array $list = [1, 2] ]
    Property [ public int $promoted ]
  }

  - Methods [3] {
    Method [ <user, overwrites RExBase, prototype RExBase> public method absM ] {
      @@ FILE 22 - 22

      - Parameters [0] {
      }
      - Return [ void ]
    }

    Method [ <user, overwrites RExBase, prototype RExBase> public method over ] {
      @@ FILE 23 - 23

      - Parameters [5] {
        Parameter #0 [ <required> &$byref ]
        Parameter #1 [ <optional> $d = 'x\ny' ]
        Parameter #2 [ <optional> $e = 1.0 ]
        Parameter #3 [ <optional> $f = M_PI ]
        Parameter #4 [ <optional> $g = NULL ]
      }
    }

    Method [ <user, inherits RExBase, ctor> public method __construct ] {
      @@ FILE 15 - 15

      - Parameters [1] {
        Parameter #0 [ <optional> int $promoted = 5 ]
      }
    }
  }
}
Class [ <user> readonly class RExRo ] {
  @@ FILE 25-25

  - Constants [0] {
  }

  - Static properties [0] {
  }

  - Static methods [0] {
  }

  - Properties [1] {
    Property [ public protected(set) readonly int $x ]
  }

  - Methods [1] {
    Method [ <user, ctor> public method __construct ] {
      @@ FILE 25 - 25

      - Parameters [1] {
        Parameter #0 [ <optional> int $x = 1 ]
      }
    }
  }
}
Enum [ <user> enum RExPure implements UnitEnum ] {
  @@ FILE 26-26

  - Enum cases [1] {
    Case P
  }

  - Constants [0] {
  }

  - Static properties [0] {
  }

Enum [ <user> enum RExBacked: string implements UnitEnum, BackedEnum ] {
  @@ FILE 27-27

  - Enum cases [2] {
    Case Q = q
    Case R = r
  }

  - Constants [0] {
  }

  - Static properties [0] {
  }

Class [ <user> class RExSet ] {
  @@ FILE 28-28

  - Constants [0] {
  }

  - Static properties [0] {
  }

  - Static methods [0] {
  }

  - Properties [1] {
    Property [ final public private(set) string $ps = 'a' ]
  }

  - Methods [0] {
  }
}
Class [ <user> <iterateable> class RExHook ] {
  @@ FILE 29-29

  - Constants [0] {
  }

  - Static properties [0] {
  }

  - Static methods [0] {
  }

  - Properties [1] {
    Property [ public string $h = 'v' { get; set; } ]
  }

  - Methods [0] {
  }
}
Function [ <user> function rExFn ] {
  @@ FILE 30 - 30

  - Parameters [4] {
    Parameter #0 [ <required> int $a ]
    Parameter #1 [ <optional> ?RExKid $b = NULL ]
    Parameter #2 [ <optional> string|int $c = 3 ]
    Parameter #3 [ <optional> mixed ...$v ]
  }
  - Return [ RExKid|false ]
}
Method [ <user, overwrites RExBase, prototype RExBase> public method over ] {
  @@ FILE 23 - 23

  - Parameters [5] {
    Parameter #0 [ <required> &$byref ]
    Parameter #1 [ <optional> $d = 'x\ny' ]
    Parameter #2 [ <optional> $e = 1.0 ]
    Parameter #3 [ <optional> $f = M_PI ]
    Parameter #4 [ <optional> $g = NULL ]
  }
}
Property [ public array $mix = [0 => 1, 'k' => 2] ]
Constant [ final public int FIN ] { 2 }
Parameter #1 [ <optional> ?RExKid $b = NULL ]
