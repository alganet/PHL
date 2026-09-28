--TEST--
Reflection: DateTimeInterface declares the nine methods it contracts for
--FILE--
<?php
// The interface had CONSTANTS and nothing else here, so the nine methods php
// contracts for reported no `prototype` on either implementor (18 export rows),
// `getMethods()` answered the empty array, and the interface itself was not
// abstract for want of a member.
$reflDTI = new ReflectionClass('DateTimeInterface');
echo implode(', ', array_map(fn($reflDTM) => $reflDTM->name, $reflDTI->getMethods())), "\n";
var_dump($reflDTI->isAbstract(), $reflDTI->hasMethod('diff'));
echo (string)$reflDTI;
foreach ([['DateTime', 'diff'], ['DateTime', '__wakeup'], ['DateTimeImmutable', 'format'],
          ['DateTimeImmutable', '__serialize'], ['DateTime', 'modify']] as [$reflDTC, $reflDTM]) {
    echo trim(strtok((string)new ReflectionMethod($reflDTC, $reflDTM), "\n")), "\n";
}
// The contract is a real one now: an implementor must satisfy it.
var_dump((new DateTime('@0')) instanceof DateTimeInterface);
--EXPECT--
format, getTimezone, getOffset, getTimestamp, getMicrosecond, diff, __wakeup, __serialize, __unserialize
bool(true)
bool(true)
Interface [ <internal:date> interface DateTimeInterface ] {

  - Constants [14] {
    Constant [ public string ATOM ] { Y-m-d\TH:i:sP }
    Constant [ public string COOKIE ] { l, d-M-Y H:i:s T }
    Constant [ public string ISO8601 ] { Y-m-d\TH:i:sO }
    Constant [ public string ISO8601_EXPANDED ] { X-m-d\TH:i:sP }
    Constant [ public string RFC822 ] { D, d M y H:i:s O }
    Constant [ public string RFC850 ] { l, d-M-y H:i:s T }
    Constant [ public string RFC1036 ] { D, d M y H:i:s O }
    Constant [ public string RFC1123 ] { D, d M Y H:i:s O }
    Constant [ public string RFC7231 ] { D, d M Y H:i:s \G\M\T }
    Constant [ public string RFC2822 ] { D, d M Y H:i:s O }
    Constant [ public string RFC3339 ] { Y-m-d\TH:i:sP }
    Constant [ public string RFC3339_EXTENDED ] { Y-m-d\TH:i:s.vP }
    Constant [ public string RSS ] { D, d M Y H:i:s O }
    Constant [ public string W3C ] { Y-m-d\TH:i:sP }
  }

  - Static properties [0] {
  }

  - Static methods [0] {
  }

  - Properties [0] {
  }

  - Methods [9] {
    Method [ <internal:date> abstract public method format ] {

      - Parameters [1] {
        Parameter #0 [ <required> string $format ]
      }
      - Tentative return [ string ]
    }

    Method [ <internal:date> abstract public method getTimezone ] {

      - Parameters [0] {
      }
      - Tentative return [ DateTimeZone|false ]
    }

    Method [ <internal:date> abstract public method getOffset ] {

      - Parameters [0] {
      }
      - Tentative return [ int ]
    }

    Method [ <internal:date> abstract public method getTimestamp ] {

      - Parameters [0] {
      }
      - Tentative return [ int ]
    }

    Method [ <internal:date> abstract public method getMicrosecond ] {

      - Parameters [0] {
      }
      - Return [ int ]
    }

    Method [ <internal:date> abstract public method diff ] {

      - Parameters [2] {
        Parameter #0 [ <required> DateTimeInterface $targetObject ]
        Parameter #1 [ <optional> bool $absolute = false ]
      }
      - Tentative return [ DateInterval ]
    }

    Method [ <internal, deprecated:date> abstract public method __wakeup ] {

      - Parameters [0] {
      }
      - Tentative return [ void ]
    }

    Method [ <internal:date> abstract public method __serialize ] {

      - Parameters [0] {
      }
      - Return [ array ]
    }

    Method [ <internal:date> abstract public method __unserialize ] {

      - Parameters [1] {
        Parameter #0 [ <required> array $data ]
      }
      - Return [ void ]
    }
  }
}
Method [ <internal:date, prototype DateTimeInterface> public method diff ] {
Method [ <internal, deprecated:date, prototype DateTimeInterface> public method __wakeup ] {
Method [ <internal:date, prototype DateTimeInterface> public method format ] {
Method [ <internal:date, prototype DateTimeInterface> public method __serialize ] {
Method [ <internal:date> public method modify ] {
bool(true)
