--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
List destructuring into typed/readonly/static properties enforces the property type
--DESCRIPTION--
`[$o->p] = [...]` used to write straight into the property slot, bypassing the
typed-slot table entirely: a string landed in an `int` property, readonly was not
enforced, and an uninitialized typed target threw the READ Error php reserves for
actual reads. Positional destructuring now routes through the same enforcement as
a plain store (coerce weak-mode, TypeError on mismatch — the property keeps its
old value), the target member is compiled as a pure WRITE (no uninitialized-read
Error, no __get consult, missing properties vivify), and a missing source key
assigns null (warning + TypeError for a non-nullable type, both like php). The
error handler matches warning BODIES on both engines regardless of prefix (§6).
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "  [$no] $str\n"; return true; });

echo "== TypeError, property keeps its value ==\n";
class TldT { public int $p = 3; }
$o = new TldT;
try { [$o->p] = ["xx"]; } catch (TypeError $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
var_dump($o->p);

echo "== weak-mode coercion ==\n";
[$o->p] = [1.0]; var_dump($o->p);
[$o->p] = ["5"]; var_dump($o->p);
class TldS { public string $s = "a"; }
$s = new TldS;
list($s->s) = [123]; var_dump($s->s);

echo "== missing key: warning, then null TypeError, value kept ==\n";
try { [$o->p] = []; } catch (TypeError $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
var_dump($o->p);

echo "== nullable accepts the missing-key null ==\n";
class TldN { public ?int $n = 7; }
$n = new TldN;
[$n->n] = [];
var_dump($n->n);

echo "== left-to-right: earlier targets assigned before the throw ==\n";
$x = 0;
try { [$x, $o->p] = [7, "xx"]; } catch (TypeError $e) { echo "caught\n"; }
var_dump($x, $o->p);

echo "== nested ==\n";
try { [[$o->p]] = [["xx"]]; } catch (TypeError $e) { echo "caught nested\n"; }
var_dump($o->p);

echo "== foreach destructuring ==\n";
try { foreach ([["xx"]] as [$o->p]) {} } catch (TypeError $e) { echo "caught foreach\n"; }
var_dump($o->p);

echo "== uninitialized typed target is a write, then initialized ==\n";
class TldU { public int $u; }
$u = new TldU;
try { [$u->u] = ["xx"]; } catch (TypeError $e) { echo "caught uninit\n"; }
[$u->u] = [8];
var_dump($u->u);

echo "== readonly: first write in scope, second throws ==\n";
class TldR { public readonly int $r; public function __construct() { [$this->r] = [5]; } }
$r = new TldR;
var_dump($r->r);
try { [$r->r] = [9]; } catch (Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }

echo "== typed static ==\n";
class TldSt { public static int $s; }
[TldSt::$s] = [5];
var_dump(TldSt::$s);
try { [TldSt::$s] = ["xx"]; } catch (TypeError $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
var_dump(TldSt::$s);

echo "== dynamic property target is created ==\n";
$d = new stdClass;
[$d->q] = [5];
var_dump($d->q);

echo "== keyed form stays enforced ==\n";
try { ["k" => $o->p] = ["k" => "xx"]; } catch (TypeError $e) { echo "caught keyed\n"; }
var_dump($o->p);

echo "== non-array source: warning (bool warns, null silent), null enforced ==\n";
try { [$o->p] = 5; } catch (TypeError $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
var_dump($o->p);
try { [$o->p] = true; } catch (TypeError $e) { echo "caught bool-source\n"; }
try { [$o->p] = null; } catch (TypeError $e) { echo "caught null-source\n"; }
[$n->n] = 5;
var_dump($n->n);
[$plain] = 5;
var_dump(isset($plain), $plain);
?>
--EXPECT--
== TypeError, property keeps its value ==
TypeError: Cannot assign string to property TldT::$p of type int
int(3)
== weak-mode coercion ==
int(1)
int(5)
string(3) "123"
== missing key: warning, then null TypeError, value kept ==
  [2] Undefined array key 0
TypeError: Cannot assign null to property TldT::$p of type int
int(5)
== nullable accepts the missing-key null ==
  [2] Undefined array key 0
NULL
== left-to-right: earlier targets assigned before the throw ==
caught
int(7)
int(5)
== nested ==
caught nested
int(5)
== foreach destructuring ==
caught foreach
int(5)
== uninitialized typed target is a write, then initialized ==
caught uninit
int(8)
== readonly: first write in scope, second throws ==
int(5)
Error: Cannot modify readonly property TldR::$r
== typed static ==
int(5)
TypeError: Cannot assign string to property TldSt::$s of type int
int(5)
== dynamic property target is created ==
int(5)
== keyed form stays enforced ==
caught keyed
int(5)
== non-array source: warning (bool warns, null silent), null enforced ==
  [2] Cannot use int as array
TypeError: Cannot assign null to property TldT::$p of type int
int(5)
  [2] Cannot use bool as array
caught bool-source
caught null-source
  [2] Cannot use int as array
NULL
  [2] Cannot use int as array
bool(false)
NULL
--CLEAN--
<?php
unset($o, $s, $n, $x, $u, $r, $d);
