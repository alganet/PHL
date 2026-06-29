--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments bind by name when invoking a first-class-callable Closure
--FILE--
<?php
class FccNamed {
  public $tag = "T";
  function m($greeting, $name){ return "$greeting, $name"; }
  static function sm($greeting, $name){ return "s:$greeting, $name"; }
  function withDefault($a, $b = "def", $c = "C"){ return "$a/$b/$c"; }
}
class FccNamedInv { function __invoke($greeting, $name){ return "i:$greeting, $name"; } }

$o = new FccNamed();

/* instance-method FCC, names supplied out of order */
$cm = $o->m(...);
echo $cm(name: "Bob", greeting: "Hi"), "\n";

/* static-method FCC */
$cs = FccNamed::sm(...);
echo $cs(name: "Sue", greeting: "Yo"), "\n";

/* value array callable [obj, method] */
$cb = [$o, "m"]; $cv = $cb(...);
echo $cv(name: "Al", greeting: "Hey"), "\n";

/* value array callable [class, staticmethod] */
$cb2 = ["FccNamed", "sm"]; $cv2 = $cb2(...);
echo $cv2(name: "Di", greeting: "Ho"), "\n";

/* __invoke object FCC */
$iv = new FccNamedInv(); $ci = $iv(...);
echo $ci(name: "Zoe", greeting: "Hej"), "\n";

/* mixed positional + named */
$cm2 = $o->m(...);
echo $cm2("Hello", name: "Mo"), "\n";

/* a named argument that skips an optional parameter (b keeps its default) */
$cd = $o->withDefault(...);
echo $cd("X", c: "Z"), "\n";

/* positional-only invocation of an FCC still works (no regression) */
echo $cm("A", "B"), "\n";
?>
--EXPECT--
Hi, Bob
s:Yo, Sue
Hey, Al
s:Ho, Di
i:Hej, Zoe
Hello, Mo
X/def/Z
A, B
--CLEAN--
<?php
