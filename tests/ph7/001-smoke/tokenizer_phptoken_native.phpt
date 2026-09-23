--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PhpToken is a base class: tokenize() builds the CALLED class
--DESCRIPTION--
php's PhpToken is not final and `tokenize()` returns `static[]` — subclassing is
the documented way to attach behaviour to a token stream. The embedded PHP
declared the class `final` (so `extends PhpToken` was a fatal here and works in
php) and left the CONSTRUCTOR non-final, which is the one php does mark final:
php never calls it from tokenize(), writing the four slots directly so a
subclass keeps its own declared defaults. `is()` also screens its untyped
argument the way php does, with one refusal for the argument and another for an
element of an array argument; the PHP answered false for all of them.
--FILE--
<?php
function tokShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}
class TokKid extends PhpToken { public $extra = 'E'; public function shout() { return strtoupper($this->text); } }
abstract class TokAbstract extends PhpToken {}

$tokSrc = "<?php \$a = 1;\n";

echo "-- tokenize() builds the called class, and its own defaults survive\n";
tokShow('class', fn() => get_class(TokKid::tokenize($tokSrc)[0]));
tokShow('subclass default kept', fn() => TokKid::tokenize($tokSrc)[0]->extra);
tokShow('subclass method', fn() => TokKid::tokenize($tokSrc)[1]->shout());
tokShow('base class unchanged', fn() => get_class(PhpToken::tokenize($tokSrc)[0]));
tokShow('abstract refused', fn() => TokAbstract::tokenize($tokSrc));

echo "-- the stream: id, text, line and the byte offset\n";
foreach (PhpToken::tokenize($tokSrc) as $t) {
    printf("%-14s|%d|%d|%s\n", $t->getTokenName() ?? 'NULL', $t->line, $t->pos,
        str_replace("\n", '\n', $t->text));
}
tokShow('text round-trips', fn() => implode('', array_map(fn($t) => $t->text,
    PhpToken::tokenize($tokSrc))) === $tokSrc);
tokShow('pos indexes the source', function () use ($tokSrc) {
    foreach (PhpToken::tokenize($tokSrc) as $t) {
        if (substr($tokSrc, $t->pos, strlen($t->text)) !== $t->text) { return false; }
    }
    return true;
});

echo "-- is() screens its untyped argument\n";
$tokV = PhpToken::tokenize($tokSrc)[1];
tokShow('by id', fn() => $tokV->is(T_VARIABLE));
tokShow('by text', fn() => $tokV->is('$a'));
tokShow('by list', fn() => $tokV->is([T_STRING, '$a']));
tokShow('by list, no match', fn() => $tokV->is([T_STRING, ';']));
tokShow('empty list', fn() => $tokV->is([]));
tokShow('float', fn() => $tokV->is(1.5));
tokShow('null', fn() => $tokV->is(null));
tokShow('bool', fn() => $tokV->is(true));
tokShow('array element', fn() => $tokV->is([T_VARIABLE, 1.5]));
tokShow('array element, after a match', fn() => $tokV->is(['$a', 1.5]));

echo "-- the declaration\n";
$tokR = new ReflectionClass('PhpToken');
tokShow('final class', fn() => $tokR->isFinal());
tokShow('final ctor', fn() => $tokR->getMethod('__construct')->isFinal());
tokShow('interfaces', fn() => $tokR->getInterfaceNames());
tokShow('properties', fn() => array_map(fn($p) => $p->getType() . ' $' . $p->getName()
    . ' default=' . var_export($p->hasDefaultValue(), true), $tokR->getProperties()));
tokShow('ctor defaults', fn() => [($t = new PhpToken(1, 'x'))->line, $t->pos]);

echo "-- an unconstructed token has no slots at all\n";
$tokBare = $tokR->newInstanceWithoutConstructor();
tokShow('read', fn() => $tokBare->id);
tokShow('is', fn() => $tokBare->is(1));
tokShow('isIgnorable', fn() => $tokBare->isIgnorable());
tokShow('getTokenName', fn() => $tokBare->getTokenName());
tokShow('__toString', fn() => (string)$tokBare);
?>
--EXPECT--
-- tokenize() builds the called class, and its own defaults survive
class => 'TokKid'
subclass default kept => 'E'
subclass method => '$A'
base class unchanged => 'PhpToken'
abstract refused => Error: Cannot instantiate abstract class TokAbstract
-- the stream: id, text, line and the byte offset
T_OPEN_TAG    |1|0|<?php 
T_VARIABLE    |1|6|$a
T_WHITESPACE  |1|8| 
=             |1|9|=
T_WHITESPACE  |1|10| 
T_LNUMBER     |1|11|1
;             |1|12|;
T_WHITESPACE  |1|13|\n
text round-trips => true
pos indexes the source => true
-- is() screens its untyped argument
by id => true
by text => true
by list => true
by list, no match => false
empty list => false
float => TypeError: PhpToken::is(): Argument #1 ($kind) must be of type string|int|array, float given
null => TypeError: PhpToken::is(): Argument #1 ($kind) must be of type string|int|array, null given
bool => TypeError: PhpToken::is(): Argument #1 ($kind) must be of type string|int|array, true given
array element => true
array element, after a match => true
-- the declaration
final class => false
final ctor => true
interfaces => array (  0 => 'Stringable',)
properties => array (  0 => 'int $id default=false',  1 => 'string $text default=false',  2 => 'int $line default=false',  3 => 'int $pos default=false',)
ctor defaults => array (  0 => -1,  1 => -1,)
-- an unconstructed token has no slots at all
read => Error: Typed property PhpToken::$id must not be accessed before initialization
is => Error: Typed property PhpToken::$id must not be accessed before initialization
isIgnorable => Error: Typed property PhpToken::$id must not be accessed before initialization
getTokenName => Error: Typed property PhpToken::$id must not be accessed before initialization
__toString => Error: Typed property PhpToken::$text must not be accessed before initialization
