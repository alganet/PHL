--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A live collection is keyed by id, and by name only inside the HTML namespace
--FILE--
<?php
/* `Dom\HTMLCollection::namedItem()` reads a collection the way an HTML page's
 * `document.forms['x']` does, and the phrase "id, falling back to name" hides
 * two rules that separate it from every other DOM lookup:
 *
 *   - the fallback is on a MISMATCH, not on an absence, so an element that
 *     carries an `id` which is not the key is still asked for its `name`, and
 *     one element answers to both of its keys;
 *   - `name` keys only an element in the HTML namespace, which in an XML
 *     document is the prefixed one. `id` keys either.
 *
 * The first matching element in collection order wins, so an `id` later in the
 * document loses to a `name` earlier in it only when the earlier one is in the
 * HTML namespace to begin with. All three doors -- the method, `$col[$key]`
 * and `isset($col[$key])` -- read through the same rule, and the empty key
 * answers nothing before the walk starts. */
$xml = '<?xml version="1.0"?><r xmlns:h="http://www.w3.org/1999/xhtml">'
     . '<a id="i1" name="n1"/><h:b id="i2" name="n2"/><h:c name="n3"/>'
     . '<d name="n4"/><h:e id="n5" name="zz"/><h:f name="n5"/></r>';
$doc = Dom\XMLDocument::createFromString($xml);
$col = $doc->getElementsByTagName('*');

foreach (['i1', 'n1', 'i2', 'n2', 'n3', 'n4', 'n5', '', ' ', 'zz'] as $key) {
    $hit = $col->namedItem($key);
    printf("namedItem(%-4s) = %s\n", var_export($key, true),
        $hit === null ? 'null' : $hit->nodeName);
}

echo "~~\n";
foreach (['i1', 'n1', 'n3', 'n4', 'zz', 'nope'] as $key) {
    $hit = $col[$key];
    printf("[%-4s] = %-4s isset=%s\n", $key,
        $hit === null ? 'null' : $hit->nodeName,
        var_export(isset($col[$key]), true));
}

/* The same reader serves a collection filtered by class name. */
echo "~~\n";
$doc2 = Dom\XMLDocument::createFromString('<?xml version="1.0"?>'
    . '<r xmlns:h="http://www.w3.org/1999/xhtml"><h:p class="k" name="q"/>'
    . '<s class="k" name="w"/><h:t class="k" id="u" name="v"/></r>');
$byClass = $doc2->getElementsByClassName('k');
foreach (['q', 'w', 'u', 'v'] as $key) {
    $hit = $byClass->namedItem($key);
    printf("byClass(%s) = %s\n", $key, $hit === null ? 'null' : $hit->nodeName);
}
?>
--EXPECT--
namedItem('i1') = a
namedItem('n1') = null
namedItem('i2') = h:b
namedItem('n2') = h:b
namedItem('n3') = h:c
namedItem('n4') = null
namedItem('n5') = h:e
namedItem(''  ) = null
namedItem(' ' ) = null
namedItem('zz') = h:e
~~
[i1  ] = a    isset=true
[n1  ] = null isset=false
[n3  ] = h:c  isset=true
[n4  ] = null isset=false
[zz  ] = h:e  isset=true
[nope] = null isset=false
~~
byClass(q) = h:p
byClass(w) = null
byClass(u) = h:t
byClass(v) = h:t
