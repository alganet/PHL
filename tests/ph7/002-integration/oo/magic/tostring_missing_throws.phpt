--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An object with no __toString() throws php's catchable Error at every user-visible string coercion (it used to render as the literal "Object"), and a __toString() that THREW no longer leaves that placeholder behind
--DESCRIPTION--
PH7 expanded a six-byte "Object" wherever an object reached a string, so
`echo $o`, `"$o"`, `(string)$o` and `"x".$o` all produced a value where php
throws `Error: Object of class P could not be converted to string` — a silent
wrong answer no arity or type check catches. The int and float casts already
diagnosed php's way.

The throw is raised at the coercion, which is never a call boundary, so it is
routed mid-expression: a catch in the same body resumes AFTER the try, and the
lvalue that reached a `.=` still holds its object. Only settype() ends up
blanked, matching php's convert_to_string(), which empties the zval before
letting the Error out.

The SILENT internal coercions must stay silent, exactly as php never throws for
them: an array key of type object is its own TypeError, print_r renders the
object, and a sort comparison just orders it.
--FILE--
<?php
class P { public $x = 1; }
class S { public function __toString(): string { return "s"; } }

function show(string $label, callable $f): void {
    try {
        $r = $f();
        echo $label, " => ";
        var_dump($r);
    } catch (Throwable $e) {
        echo $label, " => ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

// Every user-visible coercion site.
show('echo',       function () { echo new P(); return 'no-throw'; });
show('print',      function () { print new P(); return 'no-throw'; });
show('cast',       function () { return (string)new P(); });
show('interp',     function () { $o = new P(); return "x$o"; });
show('interp-cplx',function () { $o = new P(); return "x{$o}y"; });
show('heredoc',    function () { $o = new P(); return <<<EOT
    v$o
    EOT; });
show('concat-r',   function () { return "x" . new P(); });
show('concat-l',   function () { return new P() . "x"; });
show('concat-3',   function () { return "a" . "b" . new P() . "c"; });
show('cat-store',  function () { $s = "x"; $s .= new P(); return $s; });
show('cat-store-i',function () { $s = 5;   $s .= new P(); return $s; });
show('cat-elem',   function () { $a = ["k" => "v"]; $a["k"] .= new P(); return $a["k"]; });
show('cat-prop',   function () { $p = new stdClass(); $p->v = "v"; $p->v .= new P(); return $p->v; });
show('varvar-read',function () { $o = new P(); $y = $$o; return 'no-throw'; });
show('varvar-write',function () { $o = new P(); $$o = 1; return 'no-throw'; });
show('global-name',function () { $o = new P(); return (function () use ($o) { global $$o; return 'no-throw'; })(); });
show('str-offset', function () { $s = "abc"; $s[1] = new P(); return $s; });
show('str-key',    function () { $o = new P(); $a = []; $a["$o"] = 1; return 'no-throw'; });
show('settype',    function () { $o = new P(); settype($o, 'string'); return $o; });

// A class that CAN stringify is untouched, inherited/trait __toString included.
class Sub extends S {}
trait TS { public function __toString(): string { return "t"; } }
class Tr { use TS; }
show('with-tostring',      function () { return "x" . new S(); });
show('inherited-tostring', function () { return "x" . new Sub(); });
show('trait-tostring',     function () { return "x" . new Tr(); });

// The SILENT internal coercions keep working: php throws for none of these.
show('print_r',   function () { return print_r(new P(), true); });
show('var_export',function () { return var_export(new P(), true); });
show('compare',   function () { return (new P()) == "x"; });
show('sort',      function () { $a = [new P(), "b"]; sort($a); return count($a); });
show('array-key', function () { $a = []; $a[new P()] = 1; return 'no-throw'; });

// The throw lands where php lands it: the catch runs, then the code AFTER the
// try, and the enclosing scope keeps going.
function resumes(): string {
    try {
        echo "a", new P(), "z";
    } catch (Error $e) {
        echo "[caught]";
    }
    echo "[after try]";
    return "returned";
}
echo resumes(), "\n";

// An inner catch resumes its own try; the outer body carries on.
function nested(): string {
    try {
        try {
            $s = "x" . new P();
        } catch (Error $e) {
            echo "[inner]";
        }
        echo "[between]";
        throw new RuntimeException("outer");
    } catch (RuntimeException $e) {
        echo "[outer:", $e->getMessage(), "]";
    }
    return "end";
}
echo nested(), "\n";

// The abandoned coercion does not write the lvalue it was reading (php's
// concat leaves the variable alone; only settype() blanks it).
$keep = new P();
try { $keep .= "x"; } catch (Error $e) { echo "[cat-store caught]"; }
var_dump(is_object($keep));
$blank = new P();
try { settype($blank, 'string'); } catch (Error $e) { echo "[settype caught]"; }
var_dump($blank);
$target = "abc";
try { $target[1] = new P(); } catch (Error $e) { echo "[offset caught]"; }
var_dump($target);

// A __toString() that THREW must not fall back to the placeholder either: the
// value is abandoned, so echo prints nothing more, `.=` keeps its object and
// settype() lands on php's empty string.
class Boom { public function __toString(): string { throw new LogicException("boom"); } }
try { echo "A", new Boom(), "B"; } catch (Throwable $e) { echo "[", $e->getMessage(), "]"; }
echo "\n";
$b1 = new Boom();
try { $b1 .= "z"; } catch (Throwable $e) { echo "[boom cat-store]"; }
var_dump(is_object($b1));
$b2 = new Boom();
try { settype($b2, 'string'); } catch (Throwable $e) { echo "[boom settype]"; }
var_dump($b2);

// Thrown in a loop: each caught throw must unwind its operands (a leaked slot
// per iteration overflows the operand stack).
for ($i = 0; $i < 200; $i++) {
    try { $z = "x" . new P(); } catch (Error $e) { /* discard */ }
}
echo "loop survived\n";

// finally still runs, and the Error is an Error (code 0, no previous).
function withFinally(): string {
    try {
        return "x" . new P();
    } catch (Error $e) {
        return "from-catch:" . get_class($e) . ":" . $e->getCode() . ":" . var_export($e->getPrevious(), true);
    } finally {
        echo "[finally]";
    }
}
echo withFinally(), "\n";

// The multi-operand shapes: an OP_CAT operand list, an echo list and a `global`
// name list all coerce in a LOOP, and the throw has to leave that loop without
// finishing the op it was in the middle of.
show('cat-mid',   function () { $o = new P(); return "a" . "b" . $o . "c"; });
show('cat-last',  function () { $o = new P(); return "a" . "b" . "c" . $o; });
show('cat-two',   function () { return "a" . new P() . new P(); });
show('echo-list', function () { $o = new P(); echo "x", "y", $o, "z"; return 'no-throw'; });
show('global-2',  function () { $n = "gname"; $o = new P();
    return (function () use ($n, $o) { global $$n, $$o; return 'no-throw'; })(); });
show('global-2b', function () { $n = "gname"; $o = new P();
    return (function () use ($n, $o) { global $$o, $$n; return 'no-throw'; })(); });
echo "end\n";
?>
--EXPECT--
echo => Error: Object of class P could not be converted to string
print => Error: Object of class P could not be converted to string
cast => Error: Object of class P could not be converted to string
interp => Error: Object of class P could not be converted to string
interp-cplx => Error: Object of class P could not be converted to string
heredoc => Error: Object of class P could not be converted to string
concat-r => Error: Object of class P could not be converted to string
concat-l => Error: Object of class P could not be converted to string
concat-3 => Error: Object of class P could not be converted to string
cat-store => Error: Object of class P could not be converted to string
cat-store-i => Error: Object of class P could not be converted to string
cat-elem => Error: Object of class P could not be converted to string
cat-prop => Error: Object of class P could not be converted to string
varvar-read => Error: Object of class P could not be converted to string
varvar-write => Error: Object of class P could not be converted to string
global-name => Error: Object of class P could not be converted to string
str-offset => Error: Object of class P could not be converted to string
str-key => Error: Object of class P could not be converted to string
settype => Error: Object of class P could not be converted to string
with-tostring => string(2) "xs"
inherited-tostring => string(2) "xs"
trait-tostring => string(2) "xt"
print_r => string(26) "P Object
(
    [x] => 1
)
"
var_export => string(38) "\P::__set_state(array(
   'x' => 1,
))"
compare => bool(false)
sort => int(2)
array-key => TypeError: Cannot access offset of type P on array
a[caught][after try]returned
[inner][between][outer:outer]end
[cat-store caught]bool(true)
[settype caught]string(0) ""
[offset caught]string(3) "abc"
A[boom]
[boom cat-store]bool(true)
[boom settype]string(0) ""
loop survived
[finally]from-catch:Error:0:NULL
cat-mid => Error: Object of class P could not be converted to string
cat-last => Error: Object of class P could not be converted to string
cat-two => Error: Object of class P could not be converted to string
xyecho-list => Error: Object of class P could not be converted to string
global-2 => Error: Object of class P could not be converted to string
global-2b => Error: Object of class P could not be converted to string
end
