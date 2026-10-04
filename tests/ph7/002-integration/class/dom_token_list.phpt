--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An element's class attribute reads as an ordered set of unique tokens
--FILE--
<?php
/* php 8.4's `Dom\Element::$classList` is one attribute seen through the DOM
 * standard's "ordered set" grammar, and four of its rules are not the ones a
 * `explode(' ', ...)` would give:
 *
 *  - The split is on ASCII whitespace, which is space, tab, LF, FF and CR and
 *    nothing else. A vertical tab is an ordinary token byte and so is the
 *    non-breaking space, so `"\x0bx"` and `"\xc2\xa0x"` are each ONE token
 *    carrying that byte.
 *  - The set is unique: a class attribute naming `x` twice has it once, at the
 *    position of its FIRST appearance.
 *  - `$value` is the attribute's raw bytes and not the serialization, so it
 *    reads back every space the document wrote while `$length` counts the set.
 *  - The list is the element's: `$e->classList === $e->classList`, where
 *    `$e->attributes` mints a new map every read.
 *
 * The class is final, holds a PRIVATE constructor, and refuses to be cloned or
 * serialized. `supports()` is the standard's "does this attribute define
 * supported tokens" question, and `class` defines none, so its only answer is
 * the refusal -- a TypeError naming the attribute, not a DOMException.
 */
$doc = Dom\XMLDocument::createFromString('<r><a class=" x  y x z "/><b/></r>');
$e = $doc->documentElement->firstElementChild;
$b = $doc->documentElement->lastElementChild;
$cl = $e->classList;

/* The dump shape is the derived fact -- the two virtual names, length before
 * value, and nothing else -- so the object HANDLE is masked out: how many
 * objects a document mints on the way is an engine's own business. */
ob_start();
var_dump($cl);
echo preg_replace('/#\d+/', '#N', ob_get_clean());
echo 'value=', var_export($cl->value, true), ' length=', $cl->length,
     ' count=', count($cl), "\n";
foreach ($cl as $k => $v) {
    echo "  $k => $v\n";
}
echo 'item(0)=', var_export($cl->item(0), true),
     ' item(3)=', var_export($cl->item(3), true),
     ' item(-1)=', var_export($cl->item(-1), true), "\n";
echo 'contains(x)=', var_export($cl->contains('x'), true),
     ' contains(q)=', var_export($cl->contains('q'), true), "\n";
/* Neither of these is a token, and neither is refused: they are simply not
 * members, where the same two strings ARE refused by every mutator. */
echo 'contains("")=', var_export($cl->contains(''), true),
     ' contains("a b")=', var_export($cl->contains('a b'), true), "\n";

echo "-- identity\n";
var_dump($e->classList === $cl, $e->attributes === $e->attributes);
var_dump($cl->getIterator() instanceof Iterator, $cl->getIterator() === $cl->getIterator());

echo "-- an element with no class attribute\n";
$c2 = $b->classList;
echo 'value=', var_export($c2->value, true), ' length=', $c2->length, "\n";

echo "-- what counts as whitespace\n";
foreach ([" a\tb\nc\rd\x0ce ", "a\x0bb", "\xc2\xa0x", '  ', ''] as $raw) {
    $e->setAttribute('class', $raw);
    $out = [];
    foreach ($cl as $t) {
        $out[] = $t;
    }
    /* json_encode both sides: a raw CR in a pinned expectation is a phantom
     * diff even where both engines emit the same bytes. */
    echo str_pad(json_encode($raw), 22), ' => ', $cl->length, ' ',
         json_encode($out), "\n";
}

echo "-- the declaration\n";
$r = new ReflectionClass('Dom\TokenList');
echo 'final=', var_export($r->isFinal(), true),
     ' implements=', implode(',', $r->getInterfaceNames()), "\n";
try { new Dom\TokenList(); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
try { clone $cl; } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
try { serialize($cl); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
try { $cl->supports('x'); } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
echo json_encode([json_encode($cl), get_object_vars($cl)]), "\n";
echo 'isset(length)=', var_export(isset($cl->length), true),
     ' isset(nope)=', var_export(isset($cl->nope), true), "\n";
try { $cl->length = 1; } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
try { $e->classList = $c2; } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
--EXPECT--
object(Dom\TokenList)#N (2) {
  ["length"]=>
  int(3)
  ["value"]=>
  string(10) " x  y x z "
}
value=' x  y x z ' length=3 count=3
  0 => x
  1 => y
  2 => z
item(0)='x' item(3)=NULL item(-1)=NULL
contains(x)=true contains(q)=false
contains("")=false contains("a b")=false
-- identity
bool(true)
bool(false)
bool(true)
bool(false)
-- an element with no class attribute
value='' length=0
-- what counts as whitespace
" a\tb\nc\rd\fe "      => 5 ["a","b","c","d","e"]
"a\u000bb"             => 1 ["a\u000bb"]
"\u00a0x"              => 1 ["\u00a0x"]
"  "                   => 0 []
""                     => 0 []
-- the declaration
final=true implements=IteratorAggregate,Traversable,Countable
Error: Call to private Dom\TokenList::__construct() from global scope
Error: Trying to clone an uncloneable object of class Dom\TokenList
Exception: Serialization of 'Dom\TokenList' is not allowed
TypeError: Attribute "class" does not define any supported tokens
["{}",[]]
isset(length)=true isset(nope)=false
Error: Cannot modify readonly property Dom\TokenList::$length
Error: Cannot modify readonly property Dom\Element::$classList
