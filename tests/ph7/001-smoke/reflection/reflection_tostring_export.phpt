--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reflection __toString export format
--FILE--
<?php
class ReflExpBase { public function inh() {} }
class ReflExp extends ReflExpBase implements Countable {
    const CPUB = 5;
    protected const CPROT = 'x';
    public static $st = 1;
    private $priv = array(1);
    public ?int $typed = 3;
    public $plain;
    public function __construct(int $a, $b = 'q', ...$rest) {}
    public function count(): int { return 0; }
    protected static function ps() {}
}
function reflExpFn(string $s, ?int &$x = null) {}

function reflExpNorm($s) { return str_replace(__FILE__, '%FILE%', $s); }
echo reflExpNorm((string)new ReflectionFunction('reflExpFn')), "===\n";
echo reflExpNorm((string)new ReflectionMethod('ReflExp', 'count')), "===\n";
echo (string)new ReflectionParameter('reflExpFn', 1), "\n===\n";
echo (string)new ReflectionProperty('ReflExp', 'typed'), "===\n";
echo (string)new ReflectionClassConstant('ReflExp', 'CPROT'), "===\n";
echo reflExpNorm((string)new ReflectionClass('ReflExp')), "===\n";
echo reflExpNorm((string)new ReflectionFunction('strlen')), "===\n";
--EXPECT--
Function [ <user> function reflExpFn ] {
  @@ %FILE% 14 - 14

  - Parameters [2] {
    Parameter #0 [ <required> string $s ]
    Parameter #1 [ <optional> ?int &$x = NULL ]
  }
}
===
Method [ <user, prototype Countable> public method count ] {
  @@ %FILE% 11 - 11

  - Parameters [0] {
  }
  - Return [ int ]
}
===
Parameter #1 [ <optional> ?int &$x = NULL ]
===
Property [ public ?int $typed = 3 ]
===
Constant [ protected string CPROT ] { x }
===
Class [ <user> class ReflExp extends ReflExpBase implements Countable ] {
  @@ %FILE% 3-13

  - Constants [2] {
    Constant [ public int CPUB ] { 5 }
    Constant [ protected string CPROT ] { x }
  }

  - Static properties [1] {
    Property [ public static $st = 1 ]
  }

  - Static methods [1] {
    Method [ <user> static protected method ps ] {
      @@ %FILE% 12 - 12
    }
  }

  - Properties [3] {
    Property [ private $priv = [1] ]
    Property [ public ?int $typed = 3 ]
    Property [ public $plain = NULL ]
  }

  - Methods [3] {
    Method [ <user, ctor> public method __construct ] {
      @@ %FILE% 10 - 10

      - Parameters [3] {
        Parameter #0 [ <required> int $a ]
        Parameter #1 [ <optional> $b = 'q' ]
        Parameter #2 [ <optional> ...$rest ]
      }
    }

    Method [ <user, prototype Countable> public method count ] {
      @@ %FILE% 11 - 11

      - Parameters [0] {
      }
      - Return [ int ]
    }

    Method [ <user, inherits ReflExpBase> public method inh ] {
      @@ %FILE% 2 - 2
    }
  }
}
===
Function [ <internal:Core> function strlen ] {

  - Parameters [1] {
    Parameter #0 [ <required> string $string ]
  }
  - Return [ int ]
}
===
