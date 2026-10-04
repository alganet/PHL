--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: php 8.4 walks an element's namespace declarations and its attributes as one sibling chain
--FILE--
<?php
/* php 8.4's attribute map lists an element's namespace DECLARATIONS ahead of
 * its attributes, and the sibling chain runs through BOTH as a single list:
 * the last declaration's `nextSibling` is the first attribute, and that
 * attribute's `previousSibling` is the last declaration. libxml keeps a
 * declaration off the element's attribute chain entirely, so a walk that reads
 * that chain raw stops at the join in both directions and reports every
 * declaration as an only child.
 *
 * The 2004 tree lists no declaration on the map at all -- it answers one
 * through DOMNameSpaceNode, which declares no sibling property -- so its chain
 * is the attributes alone and must stay that way. */
$nm = static fn ($n) => $n === null ? 'null' : $n->nodeName;
$chain = static function ($el) use ($nm) {
    $c = $el->attributes;
    for ($i = 0; $i < $c->length; $i++) {
        $it = $c->item($i);
        printf("  %-8s prev=%-8s next=%s isset(prev)=%d isset(next)=%d\n",
            $it->nodeName, $nm($it->previousSibling), $nm($it->nextSibling),
            (int) isset($it->previousSibling), (int) isset($it->nextSibling));
    }
    $walk = static function ($from, $step) {
        $out = [];
        for ($n = $from; $n !== null; $n = $n->{$step}) { $out[] = $n->nodeName; }
        return $out === [] ? '-' : implode(' ', $out);
    };
    printf("  forward  %s\n", $walk($c->length ? $c->item(0) : null, 'nextSibling'));
    printf("  backward %s\n", $walk($c->length ? $c->item($c->length - 1) : null, 'previousSibling'));
};

$cases = [
    'two declarations, two attributes' => '<r xmlns:p="urn:p" xmlns="urn:d" a="1" p:b="2"/>',
    'declarations only'                => '<r xmlns:p="urn:p" xmlns="urn:d"/>',
    'attributes only'                  => '<r a="1" b="2"/>',
    'one declaration, one attribute'   => '<r xmlns="urn:d" a="1"/>',
    'a lone declaration'               => '<r xmlns:p="urn:p"/>',
    'a lone attribute'                 => '<r a="1"/>',
    'neither'                          => '<r/>',
];
foreach ($cases as $label => $xml) {
    echo $label, "\n";
    $chain(Dom\XMLDocument::createFromString($xml)->documentElement);
}

/* The join is the same one object the map hands out, reached from either end. */
echo "identity across the join\n";
$el = Dom\XMLDocument::createFromString('<r xmlns:p="urn:p" a="1"/>')->documentElement;
var_dump($el->attributes->item(1)->previousSibling === $el->attributes->item(0));
var_dump($el->attributes->item(0)->nextSibling === $el->attributes->item(1));

/* A declaration reached by NAME rather than by position walks the same chain. */
echo "reached by name\n";
$el = Dom\XMLDocument::createFromString('<r xmlns:p="urn:p" xmlns:q="urn:q" a="1"/>')->documentElement;
foreach (['xmlns:p', 'xmlns:q'] as $name) {
    $g = $el->getAttributeNode($name);
    printf("  %-8s prev=%-8s next=%s\n", $name, $nm($g->previousSibling), $nm($g->nextSibling));
}

/* A declaration a parent makes is not on the CHILD's chain, and a declaration
 * the engine minted to name something it was asked to write is on nobody's:
 * the chain lists exactly what the map lists. */
echo "scope\n";
$doc = Dom\XMLDocument::createFromString('<r xmlns:p="urn:p"><c xmlns:q="urn:q" a="1" p:b="2"/></r>');
$chain($doc->documentElement->firstElementChild);
$el = Dom\XMLDocument::createFromString('<r a="1"/>')->documentElement;
$el->setAttributeNS('urn:z', 'zz:k', 'v');
$chain($el);

/* An attribute off any tree has neither neighbour. */
echo "detached\n";
$at = Dom\XMLDocument::createFromString('<r/>')->createAttribute('z');
var_dump($at->previousSibling, $at->nextSibling);

/* The 2004 tree: the attributes alone, and DOMNameSpaceNode has no sibling. */
echo "the 2004 tree\n";
$doc = new DOMDocument();
$doc->loadXML('<r xmlns:p="urn:p" a="1" b="2"/>');
$chain($doc->documentElement);
$ns = $doc->documentElement->getAttributeNode('xmlns:p');
printf("  %s declares previousSibling: %s\n", get_class($ns),
    var_export(property_exists($ns, 'previousSibling'), true));
--EXPECT--
two declarations, two attributes
  xmlns:p  prev=null     next=xmlns isset(prev)=0 isset(next)=1
  xmlns    prev=xmlns:p  next=a isset(prev)=1 isset(next)=1
  a        prev=xmlns    next=p:b isset(prev)=1 isset(next)=1
  p:b      prev=a        next=null isset(prev)=1 isset(next)=0
  forward  xmlns:p xmlns a p:b
  backward p:b a xmlns xmlns:p
declarations only
  xmlns:p  prev=null     next=xmlns isset(prev)=0 isset(next)=1
  xmlns    prev=xmlns:p  next=null isset(prev)=1 isset(next)=0
  forward  xmlns:p xmlns
  backward xmlns xmlns:p
attributes only
  a        prev=null     next=b isset(prev)=0 isset(next)=1
  b        prev=a        next=null isset(prev)=1 isset(next)=0
  forward  a b
  backward b a
one declaration, one attribute
  xmlns    prev=null     next=a isset(prev)=0 isset(next)=1
  a        prev=xmlns    next=null isset(prev)=1 isset(next)=0
  forward  xmlns a
  backward a xmlns
a lone declaration
  xmlns:p  prev=null     next=null isset(prev)=0 isset(next)=0
  forward  xmlns:p
  backward xmlns:p
a lone attribute
  a        prev=null     next=null isset(prev)=0 isset(next)=0
  forward  a
  backward a
neither
  forward  -
  backward -
identity across the join
bool(true)
bool(true)
reached by name
  xmlns:p  prev=null     next=xmlns:q
  xmlns:q  prev=xmlns:p  next=a
scope
  xmlns:q  prev=null     next=a isset(prev)=0 isset(next)=1
  a        prev=xmlns:q  next=p:b isset(prev)=1 isset(next)=1
  p:b      prev=a        next=null isset(prev)=1 isset(next)=0
  forward  xmlns:q a p:b
  backward p:b a xmlns:q
  a        prev=null     next=zz:k isset(prev)=0 isset(next)=1
  zz:k     prev=a        next=null isset(prev)=1 isset(next)=0
  forward  a zz:k
  backward zz:k a
detached
NULL
NULL
the 2004 tree
  a        prev=null     next=b isset(prev)=0 isset(next)=1
  b        prev=a        next=null isset(prev)=1 isset(next)=0
  forward  a b
  backward b a
  DOMNameSpaceNode declares previousSibling: false
