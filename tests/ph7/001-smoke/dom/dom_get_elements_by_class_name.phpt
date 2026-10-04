--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getElementsByClassName asks for a SET of tokens, not a name
--FILE--
<?php
$dom_gcn_xml = '<r class="a b">'
    . '<p class="a">1</p>'
    . '<p class="B">2</p>'
    . '<p class="  a   b  ">3</p>'
    . '<p class="ab">4</p>'
    . '<p class="a b c">5</p>'
    . '<p>6</p>'
    . '<p class="">7</p>'
    . '<q class="a"><s class="a b">8</s></q>'
    . '<p CLASS="a">9</p>'
    . '</r>';
$dom_gcn_d = Dom\XMLDocument::createFromString($dom_gcn_xml);
$dom_gcn_show = static function (Dom\HTMLCollection $l): string {
    $o = [];
    foreach ($l as $n) $o[] = $n->localName . trim($n->textContent);
    return $l->length . ' [' . implode(' ', $o) . ']';
};

// The query is an ordered SET: every token must be on the element, in any
// order and however many times either side repeats one. A query holding NO
// token -- "" or nothing but whitespace -- matches nothing, not everything.
// Matching is byte-exact, and the attribute is the no-namespace `class`, so
// neither `A` nor a `CLASS=` spelling is this question.
foreach (['a', 'b', 'A', 'a b', 'b a', '', '   ', 'a  b', 'c', 'ab', 'a a',
          "a\tb", "a\nb", "a\x0cb", "a\x0bb", 'zzz'] as $dom_gcn_q) {
    printf("doc(%-12s) %s\n", json_encode($dom_gcn_q), $dom_gcn_show($dom_gcn_d->getElementsByClassName($dom_gcn_q)));
}

// Rooted at an ELEMENT the receiver itself is never in the list, so the root's
// own `class="a b"` is in the document's answer and not in its own.
$dom_gcn_root = $dom_gcn_d->documentElement;
foreach (['a', 'a b'] as $dom_gcn_q) {
    printf("root(%-5s) %s\n", json_encode($dom_gcn_q), $dom_gcn_show($dom_gcn_root->getElementsByClassName($dom_gcn_q)));
}
printf("q(%s) %s\n", '"a"', $dom_gcn_show($dom_gcn_d->getElementsByTagName('q')->item(0)->getElementsByClassName('a')));

// A namespaced ELEMENT carrying a plain `class` is found; an `x:class` bound
// somewhere else is not this query's attribute at all.
$dom_gcn_ns = Dom\XMLDocument::createFromString(
    '<r xmlns:x="urn:x" xmlns:h="urn:h"><a class="k"/><b x:class="k"/><h:c class="k"/></r>');
printf("ns(k) %s\n", $dom_gcn_show($dom_gcn_ns->getElementsByClassName('k')));

// The list is LIVE: it re-walks the tree on every question.
$dom_gcn_live = $dom_gcn_d->getElementsByClassName('zz');
echo 'live ', $dom_gcn_live->length;
$dom_gcn_new = $dom_gcn_d->createElement('p');
$dom_gcn_new->setAttribute('class', 'zz');
$dom_gcn_root->appendChild($dom_gcn_new);
echo ' -> ', $dom_gcn_live->length, "\n";

// The attribute is read RESOLVED, so an entity standing for two tokens is two.
$dom_gcn_ent = Dom\XMLDocument::createFromString(
    '<!DOCTYPE r [<!ENTITY e "a b">]><r><p class="&e;"/></r>');
printf("ent(a)=%d ent(a b)=%d\n", $dom_gcn_ent->getElementsByClassName('a')->length,
    $dom_gcn_ent->getElementsByClassName('a b')->length);

// An empty document has no children to walk.
var_dump(Dom\XMLDocument::createEmpty()->getElementsByClassName('a')->length);

// Both faces of the argument screen.
foreach ([[], [1], ['a', 'b'], [[]]] as $dom_gcn_args) {
    try {
        $dom_gcn_d->getElementsByClassName(...$dom_gcn_args);
        echo "ok\n";
    } catch (Throwable $dom_gcn_e) {
        echo get_class($dom_gcn_e), ': ', $dom_gcn_e->getMessage(), "\n";
    }
}
--EXPECT--
doc("a"         ) 6 [r123456789 p1 p3 p5 q8 s8]
doc("b"         ) 4 [r123456789 p3 p5 s8]
doc("A"         ) 0 []
doc("a b"       ) 4 [r123456789 p3 p5 s8]
doc("b a"       ) 4 [r123456789 p3 p5 s8]
doc(""          ) 0 []
doc("   "       ) 0 []
doc("a  b"      ) 4 [r123456789 p3 p5 s8]
doc("c"         ) 1 [p5]
doc("ab"        ) 1 [p4]
doc("a a"       ) 6 [r123456789 p1 p3 p5 q8 s8]
doc("a\tb"      ) 4 [r123456789 p3 p5 s8]
doc("a\nb"      ) 4 [r123456789 p3 p5 s8]
doc("a\fb"      ) 4 [r123456789 p3 p5 s8]
doc("a\u000bb"  ) 0 []
doc("zzz"       ) 0 []
root("a"  ) 5 [p1 p3 p5 q8 s8]
root("a b") 3 [p3 p5 s8]
q("a") 1 [s8]
ns(k) 2 [a c]
live 0 -> 1
ent(a)=1 ent(a b)=1
int(0)
ArgumentCountError: Dom\Document::getElementsByClassName() expects exactly 1 argument, 0 given
ok
ArgumentCountError: Dom\Document::getElementsByClassName() expects exactly 1 argument, 2 given
TypeError: Dom\Document::getElementsByClassName(): Argument #1 ($classNames) must be of type string, array given
