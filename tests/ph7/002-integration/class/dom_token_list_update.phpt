--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Writing through the class list re-serializes the whole set, and never invents the attribute
--FILE--
<?php
/* Every mutator on `Dom\TokenList` ends in the standard's "update steps", and
 * those are what a caller actually sees:
 *
 *  - The whole set is written back, so a `remove()` of a token that was never
 *    there still rewrites `" a  a  b "` as `"a b"`. Reading does not: the
 *    getters leave the bytes alone.
 *  - The rewrite does NOT create the attribute. An element with no `class`
 *    whose set ends up empty still has none -- so `add()` with no arguments,
 *    and a `remove()` of anything, are both invisible there.
 *  - Two calls return early and write nothing at all: `toggle($t, true)` on a
 *    token already in the set, and a `replace()` whose token is not in it.
 *  - `$value` is the OTHER door and it writes bytes: it takes them verbatim,
 *    it creates the attribute, and `$list->value = ''` leaves `class=""`.
 *
 * The two refusals are DOMExceptions carrying their level-2 codes -- 12 for
 * the empty token, 5 for one holding ASCII whitespace -- and the whole
 * argument list is screened before any of it is applied.
 */
$doc = Dom\XMLDocument::createFromString('<r><a/></r>');
$e = $doc->documentElement->firstElementChild;
$cl = $e->classList;

function at(Dom\Element $e): string {
    return $e->hasAttribute('class') ? var_export($e->getAttribute('class'), true) : 'ABSENT';
}
function seed(Dom\Element $e, ?string $raw): void {
    if ($raw === null) {
        $e->removeAttribute('class');
    } else {
        $e->setAttribute('class', $raw);
    }
}
function step(string $label, Dom\Element $e, ?string $raw, callable $f): void {
    seed($e, $raw);
    $r = $f();
    echo str_pad($label, 26), str_pad($raw === null ? 'ABSENT' : var_export($raw, true), 14),
         ' -> ', at($e), ' returned ', var_export($r, true), "\n";
}

$cl = $e->classList;
echo "-- add\n";
step('add(r, s)',    $e, 'p q',       fn() => $cl->add('r', 's'));
step('add(p)',       $e, 'p q',       fn() => $cl->add('p'));
step('add()',        $e, ' a  a  b ', fn() => $cl->add());
step('add() absent', $e, null,        fn() => $cl->add());
step('add(q) absent',$e, null,        fn() => $cl->add('q'));
echo "-- remove\n";
step('remove(q, zz)',   $e, 'p q r s',   fn() => $cl->remove('q', 'zz'));
step('remove(zz)',      $e, ' a  a  b ', fn() => $cl->remove('zz'));
step('remove()',        $e, ' a  a  b ', fn() => $cl->remove());
step('remove() absent', $e, null,        fn() => $cl->remove('q'));
step('remove() empty',  $e, '',          fn() => $cl->remove());
echo "-- toggle\n";
step('toggle(p)',          $e, 'p q',       fn() => $cl->toggle('p'));
step('toggle(z)',          $e, 'p q',       fn() => $cl->toggle('z'));
step('toggle(a, true)',    $e, ' a  a  b ', fn() => $cl->toggle('a', true));
step('toggle(z, true)',    $e, ' a  a  b ', fn() => $cl->toggle('z', true));
step('toggle(a, false)',   $e, ' a  a  b ', fn() => $cl->toggle('a', false));
step('toggle(z, false)',   $e, ' a  a  b ', fn() => $cl->toggle('z', false));
step('toggle(z, null)',    $e, 'a',         fn() => $cl->toggle('z', null));
step('toggle(z, false) ab', $e, null,       fn() => $cl->toggle('z', false));
echo "-- replace\n";
step('replace(b, c)',   $e, 'a b c',     fn() => $cl->replace('b', 'c'));
step('replace(b, b)',   $e, 'a b c',     fn() => $cl->replace('b', 'b'));
step('replace(b, B)',   $e, ' a  a  b ', fn() => $cl->replace('b', 'B'));
step('replace(zz, q)',  $e, ' a  a  b ', fn() => $cl->replace('zz', 'q'));
echo "-- value writes bytes\n";
step('value = "  a  b  "', $e, 'x',  function () use ($cl) { $cl->value = '  a  b  '; return $cl->length; });
step('value = ""',         $e, 'x',  function () use ($cl) { $cl->value = ''; return $cl->length; });
step('value = "q" absent',  $e, null, function () use ($cl) { $cl->value = 'q'; return $cl->length; });

echo "-- refusals\n";
foreach ([
    'add("")'          => fn() => $cl->add(''),
    'add("a b")'       => fn() => $cl->add('a b'),
    'add("a\tb")'      => fn() => $cl->add("a\tb"),
    'add("a\x0bb")'    => fn() => $cl->add("a\x0bb"),
    'add("ok", "")'    => fn() => $cl->add('ok', ''),
    'remove("")'       => fn() => $cl->remove(''),
    'remove("a b")'    => fn() => $cl->remove('a b'),
    'toggle("")'       => fn() => $cl->toggle(''),
    'toggle("a b")'    => fn() => $cl->toggle('a b'),
    'replace("", "a")' => fn() => $cl->replace('', 'a'),
    'replace("a", "")' => fn() => $cl->replace('a', ''),
    'replace("a", "b c")' => fn() => $cl->replace('a', 'b c'),
] as $label => $f) {
    seed($e, 'a');
    try {
        $f();
        echo str_pad($label, 22), ' -> no refusal, ', at($e), "\n";
    } catch (Throwable $t) {
        echo str_pad($label, 22), ' -> ', get_class($t), '(', $t->getCode(), '): ',
             $t->getMessage(), ' | ', at($e), "\n";
    }
}

echo "-- the list is live, inside a walk too\n";
seed($e, 'a b c');
foreach ($cl as $k => $v) {
    echo "  $k => $v\n";
    if ($v === 'a') {
        $cl->remove('b');
    }
}
echo at($e), "\n";
$e->className = 'q1 q2';
echo 'after className write: ', var_export($cl->value, true), ' length=', $cl->length, "\n";
/* Another element's attribute in another namespace is not this list's. */
$doc2 = Dom\XMLDocument::createFromString('<r xmlns:n="urn:n"><a n:class="p q" class="k"/></r>');
echo 'n:class ignored: ', var_export($doc2->documentElement->firstElementChild->classList->value, true), "\n";
--EXPECT--
-- add
add(r, s)                 'p q'          -> 'p q r s' returned NULL
add(p)                    'p q'          -> 'p q' returned NULL
add()                     ' a  a  b '    -> 'a b' returned NULL
add() absent              ABSENT         -> ABSENT returned NULL
add(q) absent             ABSENT         -> 'q' returned NULL
-- remove
remove(q, zz)             'p q r s'      -> 'p r s' returned NULL
remove(zz)                ' a  a  b '    -> 'a b' returned NULL
remove()                  ' a  a  b '    -> 'a b' returned NULL
remove() absent           ABSENT         -> ABSENT returned NULL
remove() empty            ''             -> '' returned NULL
-- toggle
toggle(p)                 'p q'          -> 'q' returned false
toggle(z)                 'p q'          -> 'p q z' returned true
toggle(a, true)           ' a  a  b '    -> ' a  a  b ' returned true
toggle(z, true)           ' a  a  b '    -> 'a b z' returned true
toggle(a, false)          ' a  a  b '    -> 'b' returned false
toggle(z, false)          ' a  a  b '    -> ' a  a  b ' returned false
toggle(z, null)           'a'            -> 'a z' returned true
toggle(z, false) ab       ABSENT         -> ABSENT returned false
-- replace
replace(b, c)             'a b c'        -> 'a c' returned true
replace(b, b)             'a b c'        -> 'a b c' returned true
replace(b, B)             ' a  a  b '    -> 'a B' returned true
replace(zz, q)            ' a  a  b '    -> ' a  a  b ' returned false
-- value writes bytes
value = "  a  b  "        'x'            -> '  a  b  ' returned 2
value = ""                'x'            -> '' returned 0
value = "q" absent        ABSENT         -> 'q' returned 1
-- refusals
add("")                -> DOMException(12): The empty string is not a valid token | 'a'
add("a b")             -> DOMException(5): The token must not contain any ASCII whitespace | 'a'
add("a\tb")            -> DOMException(5): The token must not contain any ASCII whitespace | 'a'
add("a\x0bb")          -> no refusal, 'a ab'
add("ok", "")          -> DOMException(12): The empty string is not a valid token | 'a'
remove("")             -> DOMException(12): The empty string is not a valid token | 'a'
remove("a b")          -> DOMException(5): The token must not contain any ASCII whitespace | 'a'
toggle("")             -> DOMException(12): The empty string is not a valid token | 'a'
toggle("a b")          -> DOMException(5): The token must not contain any ASCII whitespace | 'a'
replace("", "a")       -> DOMException(12): The empty string is not a valid token | 'a'
replace("a", "")       -> DOMException(12): The empty string is not a valid token | 'a'
replace("a", "b c")    -> DOMException(5): The token must not contain any ASCII whitespace | 'a'
-- the list is live, inside a walk too
  0 => a
  1 => c
'a c'
after className write: 'q1 q2' length=2
n:class ignored: 'k'
