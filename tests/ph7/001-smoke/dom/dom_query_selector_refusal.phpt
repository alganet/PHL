--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Element: closest/matches, the selector scope, and the refusal vocabulary
--FILE--
<?php
$csr_doc = Dom\XMLDocument::createFromString(
    '<root><section id="s1" class="box"><div id="d1"><p id="p1">x</p><p id="p2">y</p></div>'
    . '</section><p id="p3"/></root>');
$csr_id = static fn($n) => $n === null ? 'null' : ($n->getAttribute('id') ?: '<' . $n->nodeName . '>');
$csr_ids = static function ($l) use ($csr_id) {
    $a = [];
    foreach ($l as $n) {
        $a[] = $csr_id($n);
    }
    return '[' . implode(' ', $a) . ']';
};
$csr_d1 = $csr_doc->getElementsByTagName('div')->item(0);
$csr_p1 = $csr_doc->getElementsByTagName('p')->item(0);

// The candidate set is the context node's strict DESCENDANTS -- but a match
// still reads the whole tree ABOVE them, so a combinator may reach outside
// the scope entirely while `:root` reaches nothing from inside it.
echo 'd1 p              ', $csr_ids($csr_d1->querySelectorAll('p')), "\n";
echo 'd1 section p      ', $csr_ids($csr_d1->querySelectorAll('section p')), "\n";
echo 'd1 div            ', $csr_id($csr_d1->querySelector('div')), "\n";
echo 'd1 *              ', $csr_ids($csr_d1->querySelectorAll('*')), "\n";
echo 'd1 :root          ', $csr_ids($csr_d1->querySelectorAll(':root')), "\n";
echo 'p1 p              ', $csr_ids($csr_p1->querySelectorAll('p')), "\n";

// matches() asks about the context node itself, ancestors still in play;
// closest() walks up from it and answers the FIRST match, itself included.
foreach (['p', 'div p', '#p2', 'p:first-child', 'section > p'] as $csr_s) {
    printf("matches %-14s %s\n", $csr_s, var_export($csr_p1->matches($csr_s), true));
}
foreach (['p', '.box', 'nope', ':root', 'div'] as $csr_s) {
    printf("closest %-14s %s\n", $csr_s, $csr_id($csr_p1->closest($csr_s)));
}

// A detached subtree and a fragment are both askable.
$csr_frag = $csr_doc->createDocumentFragment();
$csr_frag->appendChild($csr_q = $csr_doc->createElement('q'));
$csr_q->setAttribute('id', 'q1');
echo 'fragment          ', $csr_ids($csr_frag->querySelectorAll('q')), "\n";

// The refusal vocabulary is THREE sentences, and which one a name takes is
// not the standard's supported/unsupported split: `:hover` parses and matches
// nothing, `:visited` refuses, `:foo` is not a name at all, and `:blank` is
// its own sentence under the not-supported code rather than the syntax one.
foreach (['', ' ', '>', 'p >', 'p,', ',p', '#', '.', '[', '[id', '[id=', 'p:', 'p::', 'p:foo',
          'p::before', 'p::first-line', 'p:nth-child()', 'p:nth-child(z)', 'p:not()', ':scope',
          'p:visited', '|', '*|', '**', '[id="x"', 'p:has()', ':is()', 'p{', '@media',
          'p:lang(en)', 'p:first-line', 'div:dir(ltr)', '[id i]', ':blank', 'p..x', 'p.,x',
          'p.*'] as $csr_s) {
    try {
        $csr_doc->querySelectorAll($csr_s);
        printf("%-18s OK\n", var_export($csr_s, true));
    } catch (Throwable $e) {
        printf("%-18s %s(%d) %s\n", var_export($csr_s, true), get_class($e), $e->getCode(),
            $e->getMessage());
    }
}
// Every door refuses through the same parser.
foreach ([[$csr_p1, 'matches'], [$csr_p1, 'closest'], [$csr_doc, 'querySelector']] as [$csr_o, $csr_m]) {
    try {
        $csr_o->$csr_m(':scope');
    } catch (Throwable $e) {
        printf("%-16s %s: %s\n", $csr_m, get_class($e), $e->getMessage());
    }
}

// ...and with the two finders concrete, the three ParentNode classes can be
// subclassed at all: an interface's abstract method bills every USER subclass.
foreach (['Dom\Element', 'Dom\Document', 'Dom\DocumentFragment'] as $csr_base) {
    $csr_n = 'Csr' . str_replace('\\', '_', $csr_base);
    try {
        eval("class $csr_n extends \\$csr_base {}");
        printf("%-22s subclass OK, querySelector from %s\n", $csr_base,
            (new ReflectionClass($csr_n))->getMethod('querySelector')->getDeclaringClass()->getName());
    } catch (Throwable $e) {
        printf("%-22s %s: %s\n", $csr_base, get_class($e), $e->getMessage());
    }
}
?>
--EXPECT--
d1 p              [p1 p2]
d1 section p      [p1 p2]
d1 div            null
d1 *              [p1 p2]
d1 :root          []
p1 p              []
matches p              true
matches div p          true
matches #p2            false
matches p:first-child  true
matches section > p    false
closest p              p1
closest .box           s1
closest nope           null
closest :root          <root>
closest div            d1
fragment          [q1]
''                 DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
' '                DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'>'                DOMException(12) Invalid selector (Selectors. Unexpected token: >)
'p >'              DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'p,'               DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
',p'               DOMException(12) Invalid selector (Selectors. Unexpected token: ,)
'#'                DOMException(12) Invalid selector (Selectors. Unexpected token: #)
'.'                DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'['                DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'[id'              DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'[id='             DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'p:'               DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'p::'              DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'p:foo'            DOMException(12) Invalid selector (Selectors. Unexpected token: foo)
'p::before'        DOMException(12) Invalid selector (Selectors. Not supported: before)
'p::first-line'    DOMException(12) Invalid selector (Selectors. Not supported: first-line)
'p:nth-child()'    DOMException(12) Invalid selector (Selectors. Pseudo function can't be empty: nth-child())
'p:nth-child(z)'   DOMException(12) Invalid selector (Selectors. Unexpected token: z)
'p:not()'          DOMException(12) Invalid selector (Selectors. Pseudo function can't be empty: not())
':scope'           DOMException(12) Invalid selector (Selectors. Not supported: scope)
'p:visited'        DOMException(12) Invalid selector (Selectors. Not supported: visited)
'|'                DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'*|'               DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'**'               DOMException(12) Invalid selector (Selectors. Unexpected token: *)
'[id="x"'          DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'p:has()'          DOMException(12) Invalid selector (Selectors. Pseudo function can't be empty: has())
':is()'            DOMException(12) Invalid selector (Selectors. Pseudo function can't be empty: is())
'p{'               DOMException(12) Invalid selector (Selectors. Unexpected token: {)
'@media'           DOMException(12) Invalid selector (Selectors. Unexpected token: @media)
'p:lang(en)'       DOMException(12) Invalid selector (Selectors. Not supported: lang)
'p:first-line'     DOMException(12) Invalid selector (Selectors. Unexpected token: first-line)
'div:dir(ltr)'     DOMException(12) Invalid selector (Selectors. Not supported: dir)
'[id i]'           DOMException(12) Invalid selector (Selectors. Unexpected token: i)
':blank'           DOMException(9) :blank selector is not implemented because CSSWG has not yet decided its semantics (https://github.com/w3c/csswg-drafts/issues/1967)
'p..x'             DOMException(12) Invalid selector (Selectors. Unexpected token: END-OF-FILE)
'p.,x'             DOMException(12) Invalid selector (Selectors. Unexpected token: ,)
'p.*'              DOMException(12) Invalid selector (Selectors. Unexpected token: *)
matches          DOMException: Invalid selector (Selectors. Not supported: scope)
closest          DOMException: Invalid selector (Selectors. Not supported: scope)
querySelector    DOMException: Invalid selector (Selectors. Not supported: scope)
Dom\Element            subclass OK, querySelector from Dom\Element
Dom\Document           subclass OK, querySelector from Dom\Document
Dom\DocumentFragment   subclass OK, querySelector from Dom\DocumentFragment
