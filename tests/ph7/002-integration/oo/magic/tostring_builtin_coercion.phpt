--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The BUILTINS that render a mixed value for the user throw php's coercion Error for an object with no __toString() (implode, the printf family, strval, array_fill_keys/array_combine keys, include/require/eval)
--DESCRIPTION--
The engine's coercion sites throw php's
`Error: Object of class X could not be converted to string`, but the builtins
that render a `mixed` argument or element went through ph7_value_to_string() —
the SILENT embedder API — and answered the literal "Object" instead. php splits
these two ways and both have to be right: a builtin whose php signature says
`string` raises the ZPP TypeError (`strlen(): Argument #1 ($string) must be of
type string, Bare given`, which PHL already did), while one taking `mixed`
raises the coercion Error.

PH7_ValueToStringUV is the builtin-side twin of PH7_MemObjToStringUV: same
warning for an array, same Error for a not-stringable object, and the status
recorded on the call context so OP_CALL cannot mistake the call for a normal
return. ph7_value_to_string() itself stays silent — it is the embedder API, and
the internal coercions behind it (array keys, sort comparisons, print_r) must not
throw, exactly as php's do not.

The printf family is the delicate one: php does NOT let the Error interrupt the
format. The failing conversion substitutes NOTHING, the format runs to the end,
the output is written, and only then does the Error surface — so
`printf("A[%s]B", new Bare())` prints "A[]B" first. The throw is therefore
deferred to the end of the format instead of raised at the conversion.

array_fill_keys() and array_combine() also get php's own key rule, which is not
the ordinary offset canonicalisation: an INT is an index and everything else
takes the (string) cast, so 1.5 keys under "1.5" (not the index 1), null under
"" (not 0), an array warns, and an object throws.
--FILE--
<?php
class Bare {}
class Str { public function __toString(): string { return "S"; } }

function t(string $label, callable $f): void {
    try {
        $r = $f();
        echo $label, " => ";
        var_dump($r);
    } catch (Throwable $e) {
        echo $label, " => ", get_class($e), ": ", $e->getMessage(), "\n";
    }
}

// --- implode / join: the VALUE side.
t('implode',        fn () => implode(",", [new Bare()]));
t('implode-2nd',    fn () => implode(",", ["a", new Bare()]));
t('implode-str',    fn () => implode(",", [new Str(), "x"]));
t('implode-nested', fn () => implode(",", [[1], "x"]));
t('join',           fn () => join(",", [new Bare()]));
// ...and the signature php enforces around it: a string separator REQUIRES the
// array, which `implode("x")` / `implode("x", null)` used to answer "" for.
t('implode-noarr',  fn () => implode("x"));
t('implode-null',   fn () => implode("x", null));
t('implode-int2nd', fn () => implode("x", 5));
t('implode-swapped',fn () => implode(["a"], ["b"]));
t('implode-onearr', fn () => implode(["a", "b"]));

// --- the printf family. php finishes the format and writes it, THEN throws.
t('sprintf',      fn () => sprintf("A[%s]B", new Bare()));
t('sprintf-str',  fn () => sprintf("A[%s]B", new Str()));
t('sprintf-pos',  fn () => sprintf("%1\$s", new Bare()));
t('sprintf-pad',  fn () => sprintf("[%10s]", new Bare()));
t('sprintf-2',    fn () => sprintf("%s/%s", new Bare(), new Bare()));
t('printf',       function () { printf("A[%s]B%sC", new Bare(), "T"); return 'ok'; });
t('vsprintf',     fn () => vsprintf("A[%s]B", [new Bare()]));
t('vprintf',      function () { vprintf("A[%s]B", [new Bare()]); return 'ok'; });
t('fprintf',      function () {
    $h = fopen("php://memory", "w+");
    try { fprintf($h, "A[%s]B", new Bare()); } finally { fclose($h); }
    return 'ok';
});
t('vfprintf',     function () {
    $h = fopen("php://memory", "w+");
    try { vfprintf($h, "A[%s]B", [new Bare()]); } finally { fclose($h); }
    return 'ok';
});
// an ARRAY still only warns and renders as "Array"
t('printf-array', fn () => sprintf("A[%s]B", [1]));

// --- strval() is the (string) cast spelled as a function.
t('strval',     fn () => strval(new Bare()));
t('strval-str', fn () => strval(new Str()));

// --- array_fill_keys / array_combine keys.
t('fill_keys',        fn () => array_fill_keys([new Bare()], 'v'));
t('fill_keys-str',    fn () => array_fill_keys([new Str()], 'v'));
t('fill_keys-float',  fn () => array_fill_keys([1.5, 2.0], 'v'));
t('fill_keys-null',   fn () => array_fill_keys([null], 'v'));
t('fill_keys-bool',   fn () => array_fill_keys([true, false], 'v'));
t('fill_keys-array',  fn () => array_fill_keys([[1]], 'v'));
t('combine',          fn () => array_combine([new Bare()], ['a']));
t('combine-str',      fn () => array_combine([new Str()], ['a']));
t('combine-float',    fn () => array_combine([1.5, 2.0], ['a', 'b']));
t('combine-null',     fn () => array_combine([null], ['a']));
t('combine-array',    fn () => array_combine([[1]], ['a']));

// --- include / require / eval coerce their argument the same way.
t('include',      fn () => @include new Bare());
t('include_once', fn () => @include_once new Bare());
t('require',      fn () => @require new Bare());
t('require_once', fn () => @require_once new Bare());
t('eval',         fn () => @eval(new Bare()));

// --- a `string` PARAMETER still answers the ZPP TypeError, which is php's other
// half of the same rule.
t('strlen',     fn () => strlen(new Bare()));
t('str_repeat', fn () => str_repeat(new Bare(), 2));
t('explode',    fn () => explode(new Bare(), "a"));

// --- the SILENT internal coercions must stay silent.
t('print_r',    fn () => strlen(print_r([new Bare()], true)) > 0);
t('var_export', fn () => strlen(var_export([new Bare()], true)) > 0);
t('serialize',  fn () => strlen(serialize([new Bare()])) > 0);
t('json',       fn () => json_encode([new Bare()]));

// A caught coercion Error inside a builtin resumes after the try, and the
// enclosing scope carries on.
function resumes(): string {
    try {
        echo implode(",", ["a", new Bare()]), "\n";
    } catch (Error $e) {
        echo "[caught]";
    }
    echo "[after try]";
    return "returned";
}
echo resumes(), "\n";
?>
--EXPECTF--
implode => Error: Object of class Bare could not be converted to string
implode-2nd => Error: Object of class Bare could not be converted to string
implode-str => string(3) "S,x"
PHP Warning:  Array to string conversion in %s on line %d
implode-nested => string(7) "Array,x"
join => Error: Object of class Bare could not be converted to string
implode-noarr => TypeError: implode(): If argument #1 ($separator) is of type string, argument #2 ($array) must be of type array, null given
implode-null => TypeError: implode(): If argument #1 ($separator) is of type string, argument #2 ($array) must be of type array, null given
implode-int2nd => TypeError: implode(): Argument #2 ($array) must be of type ?array, int given
implode-swapped => TypeError: implode(): Argument #1 ($separator) must be of type string, array given
implode-onearr => string(2) "ab"
sprintf => Error: Object of class Bare could not be converted to string
sprintf-str => string(5) "A[S]B"
sprintf-pos => Error: Object of class Bare could not be converted to string
sprintf-pad => Error: Object of class Bare could not be converted to string
sprintf-2 => Error: Object of class Bare could not be converted to string
A[]BTCprintf => Error: Object of class Bare could not be converted to string
vsprintf => Error: Object of class Bare could not be converted to string
A[]Bvprintf => Error: Object of class Bare could not be converted to string
fprintf => Error: Object of class Bare could not be converted to string
vfprintf => Error: Object of class Bare could not be converted to string
PHP Warning:  Array to string conversion in %s on line %d
printf-array => string(9) "A[Array]B"
strval => Error: Object of class Bare could not be converted to string
strval-str => string(1) "S"
fill_keys => Error: Object of class Bare could not be converted to string
fill_keys-str => array(1) {
  ["S"]=>
  string(1) "v"
}
fill_keys-float => array(2) {
  ["1.5"]=>
  string(1) "v"
  [2]=>
  string(1) "v"
}
fill_keys-null => array(1) {
  [""]=>
  string(1) "v"
}
fill_keys-bool => array(2) {
  [1]=>
  string(1) "v"
  [""]=>
  string(1) "v"
}
PHP Warning:  Array to string conversion in %s on line %d
fill_keys-array => array(1) {
  ["Array"]=>
  string(1) "v"
}
combine => Error: Object of class Bare could not be converted to string
combine-str => array(1) {
  ["S"]=>
  string(1) "a"
}
combine-float => array(2) {
  ["1.5"]=>
  string(1) "a"
  [2]=>
  string(1) "b"
}
combine-null => array(1) {
  [""]=>
  string(1) "a"
}
PHP Warning:  Array to string conversion in %s on line %d
combine-array => array(1) {
  ["Array"]=>
  string(1) "a"
}
include => Error: Object of class Bare could not be converted to string
include_once => Error: Object of class Bare could not be converted to string
require => Error: Object of class Bare could not be converted to string
require_once => Error: Object of class Bare could not be converted to string
eval => Error: Object of class Bare could not be converted to string
strlen => TypeError: strlen(): Argument #1 ($string) must be of type string, Bare given
str_repeat => TypeError: str_repeat(): Argument #1 ($string) must be of type string, Bare given
explode => TypeError: explode(): Argument #1 ($separator) must be of type string, Bare given
print_r => bool(true)
var_export => bool(true)
serialize => bool(true)
json => string(4) "[{}]"
[caught][after try]returned
