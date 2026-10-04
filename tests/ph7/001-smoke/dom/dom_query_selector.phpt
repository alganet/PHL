--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\ParentNode: querySelector/querySelectorAll over the CSS selector grammar
--FILE--
<?php
$css_doc = Dom\XMLDocument::createFromString(<<<'X'
<?xml version="1.0"?>
<root id="r" class="a b">
  <section id="s1" class="box wide" data-k="v1">
    <p id="p1" class="lead">one</p>
    <p id="p2">two</p>
    <span id="sp1" class="lead hot">three</span>
  </section>
  <section id="s2" class="box">
    <p id="p3" class="lead">four</p>
    <div id="d1"><p id="p4">five</p></div>
  </section>
  <Mixed id="mx"/>
  <empty id="e1"/>
</root>
X);
$css_id = static fn($n) => $n === null ? 'null' : ($n->getAttribute('id') ?: '<' . $n->nodeName . '>');

// Every shape of the grammar, asked through BOTH finders: the singular one
// answers the first match in document order, the plural one the whole set.
foreach ([
    '*', 'p', 'section p', 'section > p', '#p1', '.lead', 'p.lead', 'section.box > p.lead',
    'p, span', '*|p', '[id]', '[data-k]', '[data-k="v1"]', '[data-k=v1]', '[class~="box"]',
    '[id^="p"]', '[id$="1"]', '[id*="p"]', '[class|="box"]', 'p + p', 'p ~ span',
    'section:first-child', 'p:last-child', 'p:nth-child(2)', 'p:nth-child(odd)', ':root',
    'p:first-of-type', 'p:last-of-type', 'p:nth-of-type(2)', 'p:only-child', 'empty:empty',
    'p:not(.lead)', 'p:is(.lead)', 'p:where(.lead)', 'section:has(> div)', 'Mixed', 'mixed',
    'MIXED', 'p:empty', 'p:nth-last-child(1)', 'p:nth-last-of-type(1)', 'div p',
    'root>section>p', '[ID]', '[Data-K]', 'p:nth-child( 2n + 1 )', ':not(p, section)',
    '[id="P1" i]', '[id="P1" s]', 'section:has(p + span)', 'p:is(#p1, #p4)',
] as $css_s) {
    $css_all = [];
    foreach ($css_doc->querySelectorAll($css_s) as $css_n) {
        $css_all[] = $css_id($css_n);
    }
    printf("%-24s one=%-8s all=[%s]\n", $css_s, $css_id($css_doc->querySelector($css_s)),
        implode(' ', $css_all));
}

// A name lexbor knows and declines is a refusal; one it merely cannot ask of
// an XML element is not. `:enabled` and `:read-only` answer every element,
// their negatives none, and none of the four raises.
foreach (['*:enabled', '*:read-only', '*:disabled', '*:read-write', '*:hover', '*:link',
          '*:checked', '*:any-link', '*:active', '*:focus', '*:required', '*:optional',
          '*:placeholder-shown'] as $css_s) {
    printf("%-22s %d\n", $css_s, count($css_doc->querySelectorAll($css_s)));
}

// The list is a SNAPSHOT: php's querySelectorAll is not one of the live views.
$css_list = $css_doc->querySelectorAll('p');
$css_doc->querySelector('#s2')->appendChild($css_doc->createElement('p'));
printf("snapshot: %s %d\n", get_class($css_list), count($css_list));
?>
--EXPECT--
*                        one=r        all=[r s1 p1 p2 sp1 s2 p3 d1 p4 mx e1]
p                        one=p1       all=[p1 p2 p3 p4]
section p                one=p1       all=[p1 p2 p3 p4]
section > p              one=p1       all=[p1 p2 p3]
#p1                      one=p1       all=[p1]
.lead                    one=p1       all=[p1 sp1 p3]
p.lead                   one=p1       all=[p1 p3]
section.box > p.lead     one=p1       all=[p1 p3]
p, span                  one=p1       all=[p1 p2 sp1 p3 p4]
*|p                      one=p1       all=[p1 p2 p3 p4]
[id]                     one=r        all=[r s1 p1 p2 sp1 s2 p3 d1 p4 mx e1]
[data-k]                 one=s1       all=[s1]
[data-k="v1"]            one=s1       all=[s1]
[data-k=v1]              one=s1       all=[s1]
[class~="box"]           one=s1       all=[s1 s2]
[id^="p"]                one=p1       all=[p1 p2 p3 p4]
[id$="1"]                one=s1       all=[s1 p1 sp1 d1 e1]
[id*="p"]                one=p1       all=[p1 p2 sp1 p3 p4]
[class|="box"]           one=s2       all=[s2]
p + p                    one=p2       all=[p2]
p ~ span                 one=sp1      all=[sp1]
section:first-child      one=s1       all=[s1]
p:last-child             one=p4       all=[p4]
p:nth-child(2)           one=p2       all=[p2]
p:nth-child(odd)         one=p1       all=[p1 p3 p4]
:root                    one=r        all=[r]
p:first-of-type          one=p1       all=[p1 p3 p4]
p:last-of-type           one=p2       all=[p2 p3 p4]
p:nth-of-type(2)         one=p2       all=[p2]
p:only-child             one=p4       all=[p4]
empty:empty              one=e1       all=[e1]
p:not(.lead)             one=p2       all=[p2 p4]
p:is(.lead)              one=p1       all=[p1 p3]
p:where(.lead)           one=p1       all=[p1 p3]
section:has(> div)       one=s2       all=[s2]
Mixed                    one=mx       all=[mx]
mixed                    one=null     all=[]
MIXED                    one=null     all=[]
p:empty                  one=null     all=[]
p:nth-last-child(1)      one=p4       all=[p4]
p:nth-last-of-type(1)    one=p2       all=[p2 p3 p4]
div p                    one=p4       all=[p4]
root>section>p           one=p1       all=[p1 p2 p3]
[ID]                     one=null     all=[]
[Data-K]                 one=null     all=[]
p:nth-child( 2n + 1 )    one=p1       all=[p1 p3 p4]
:not(p, section)         one=r        all=[r sp1 d1 mx e1]
[id="P1" i]              one=p1       all=[p1]
[id="P1" s]              one=null     all=[]
section:has(p + span)    one=s1       all=[s1]
p:is(#p1, #p4)           one=p1       all=[p1 p4]
*:enabled              11
*:read-only            11
*:disabled             0
*:read-write           0
*:hover                0
*:link                 0
*:checked              0
*:any-link             0
*:active               0
*:focus                0
*:required             0
*:optional             0
*:placeholder-shown    0
snapshot: Dom\NodeList 4
