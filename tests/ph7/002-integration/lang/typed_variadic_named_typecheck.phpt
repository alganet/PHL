--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named-argument variadic elements are type-checked and coerced like positional ones
--FILE--
<?php
// The named-argument binding path must run each variadic-collected element
// through the same type check + weak coercion as the positional path.
// php's numbering quirk: a failing NAMED element always reports
// max(total positional args, declared non-variadic formals) + 1, whichever
// named element fails; a failing POSITIONAL element in a mixed call keeps
// its own 1-based call position. The ` ($name)` clause is omitted either way.
function shortMsg(TypeError $e) {
    $msg = $e->getMessage();
    $pos = strpos($msg, ", called in");
    if ($pos !== false) $msg = substr($msg, 0, $pos);
    return $msg;
}

function nvInt(int ...$a) { var_dump($a); }
function nvLead(int $p, int ...$a) {}
function nvUnion(int|string ...$a) { var_dump($a); }
function nvObject(object ...$a) {}
function nvNullable(?int ...$a) { var_dump($a); }
function nvString(string ...$a) { var_dump($a); }
class NvSvc { function take(int ...$a) {} }
class NvStr { function __toString() { return "s"; } }

// Named element failures: always (positional count) + 1.
try { nvInt(x: 1, y: "abc");    } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { nvInt(x: 1, y: 2, z: "z");} catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { nvInt(1, y: "x");         } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { nvLead(0, x: "x");        } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// Declared non-variadic formals count even when filled by NAME: the named
// element reports max(positional, declared formals) + 1.
function nvLead2(int $p, int $q = 0, int ...$a) {}
try { nvLead(x: "s", p: 1);     } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { nvLead2(p: 1, x: "s");    } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { nvLead2(1, q: 2, x: "s"); } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { nvLead(1, 2, 3, x: "s");  } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// Positional element failures in a mixed call keep their own position.
try { nvInt("bad", x: 1);       } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { nvInt(1, "b", x: 1);      } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// Union and object hints reject named elements too.
try { nvUnion(x: [1]);          } catch (TypeError $e) { echo shortMsg($e), "\n"; }
try { nvObject(x: 1);           } catch (TypeError $e) { echo shortMsg($e), "\n"; }

// A method reports its class-qualified name.
try { (new NvSvc)->take(x: "s");} catch (TypeError $e) { echo shortMsg($e), "\n"; }

// Weak-mode coercion applies per element, keys preserved.
nvInt(x: "5");
nvInt(1, 2, x: 3);

// A nullable hint lets null through and still coerces the rest.
nvNullable(x: null, y: "7");

// A Stringable object coerces into a string variadic (weak mode).
nvString(x: new NvStr);
echo "ok\n";
?>
--EXPECT--
nvInt(): Argument #1 must be of type int, string given
nvInt(): Argument #1 must be of type int, string given
nvInt(): Argument #2 must be of type int, string given
nvLead(): Argument #2 must be of type int, string given
nvLead(): Argument #2 must be of type int, string given
nvLead2(): Argument #3 must be of type int, string given
nvLead2(): Argument #3 must be of type int, string given
nvLead(): Argument #4 must be of type int, string given
nvInt(): Argument #1 must be of type int, string given
nvInt(): Argument #2 must be of type int, string given
nvUnion(): Argument #1 must be of type string|int, array given
nvObject(): Argument #1 must be of type object, int given
NvSvc::take(): Argument #1 must be of type int, string given
array(1) {
  ["x"]=>
  int(5)
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  ["x"]=>
  int(3)
}
array(2) {
  ["x"]=>
  NULL
  ["y"]=>
  int(7)
}
array(1) {
  ["x"]=>
  string(1) "s"
}
ok
--CLEAN--
<?php
