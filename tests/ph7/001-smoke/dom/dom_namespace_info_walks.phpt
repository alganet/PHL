--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Element::getInScopeNamespaces()/getDescendantNamespaces() and the Dom\NamespaceInfo they mint
--FILE--
<?php
$dom_nsi_show = static function (string $label, array $rows): void {
    echo '-- ', $label, ' (', count($rows), ")\n";
    foreach ($rows as $i => $r) {
        printf("  [%d] %s prefix=%s ns=%s elem=%s\n", $i, get_class($r),
            var_export($r->prefix, true), var_export($r->namespaceURI, true),
            $r->element->nodeName);
    }
};

/* The class itself: a readonly final record with a private constructor, so the
 * two walks are its only producers. */
$dom_nsi_r = new ReflectionClass('Dom\NamespaceInfo');
echo 'final=', var_export($dom_nsi_r->isFinal(), true),
     ' readonly=', var_export($dom_nsi_r->isReadOnly(), true),
     ' instantiable=', var_export($dom_nsi_r->isInstantiable(), true), "\n";
foreach ($dom_nsi_r->getProperties() as $dom_nsi_p) {
    echo '  prop ', $dom_nsi_p->getName(), ' ', (string) $dom_nsi_p->getType(),
         ' readonly=', var_export($dom_nsi_p->isReadOnly(), true), "\n";
}

/* A prefix re-declared on a descendant moves to the END of that descendant's
 * list -- even when it is re-bound to the very same URI, which is why `t`
 * below reports the default first and `p` last. */
$dom_nsi_doc = Dom\XMLDocument::createFromString(
    '<r xmlns="http://d" xmlns:a="http://a" xmlns:b="http://b">'
    . '<a:c xmlns:d="http://d2" xmlns:a="http://a2"><e xmlns=""><f/></e></a:c>'
    . '<g/></r>');
$dom_nsi_root = $dom_nsi_doc->documentElement;
$dom_nsi_c = $dom_nsi_root->firstElementChild;
$dom_nsi_e = $dom_nsi_c->firstElementChild;

$dom_nsi_show('r in-scope', $dom_nsi_root->getInScopeNamespaces());
$dom_nsi_show('a:c in-scope', $dom_nsi_c->getInScopeNamespaces());
/* `xmlns=""` un-declares the default: `e` reports no default binding at all,
 * not one bound to "". */
$dom_nsi_show('e in-scope', $dom_nsi_e->getInScopeNamespaces());
/* The descendant walk is the element's own list followed by every descendant
 * element's, in document order -- and `element` names the element each list was
 * taken for, never the ancestor the declaration was written on. */
$dom_nsi_show('r descendant', $dom_nsi_root->getDescendantNamespaces());

/* A re-declaration to the SAME uri still moves the prefix; and `xml:lang` is no
 * declaration, so the implicit `xml` prefix is on no list. */
$dom_nsi_doc2 = Dom\XMLDocument::createFromString(
    '<r xmlns:p="http://p" xmlns="http://d">'
    . '<s xmlns="http://d2" xml:lang="en" p:q="1"/><t xmlns:p="http://p"/></r>');
$dom_nsi_r2 = $dom_nsi_doc2->documentElement;
$dom_nsi_show('r2 in-scope', $dom_nsi_r2->getInScopeNamespaces());
$dom_nsi_show('s in-scope', $dom_nsi_r2->firstElementChild->getInScopeNamespaces());
$dom_nsi_show('t in-scope', $dom_nsi_r2->lastElementChild->getInScopeNamespaces());

/* Nothing to report is an empty list on both walks, connected or not. */
$dom_nsi_doc3 = Dom\XMLDocument::createFromString('<a><b/></a>');
$dom_nsi_show('plain in-scope', $dom_nsi_doc3->documentElement->getInScopeNamespaces());
$dom_nsi_show('plain descendant', $dom_nsi_doc3->documentElement->getDescendantNamespaces());
$dom_nsi_show('detached in-scope', $dom_nsi_doc3->createElement('z')->getInScopeNamespaces());

/* The record is unconstructible and unwritable, and `prefix` reads a real null
 * on the default binding rather than an uninitialized slot. */
try { new Dom\NamespaceInfo(); } catch (\Throwable $dom_nsi_t) {
    echo get_class($dom_nsi_t), ': ', $dom_nsi_t->getMessage(), "\n";
}
$dom_nsi_first = $dom_nsi_root->getInScopeNamespaces()[0];
try { $dom_nsi_first->prefix = 'x'; } catch (\Throwable $dom_nsi_t) {
    echo get_class($dom_nsi_t), ': ', $dom_nsi_t->getMessage(), "\n";
}
var_dump($dom_nsi_first->prefix, $dom_nsi_first->namespaceURI);
var_dump($dom_nsi_first->element === $dom_nsi_root);
?>
--EXPECT--
final=true readonly=true instantiable=false
  prop prefix ?string readonly=true
  prop namespaceURI ?string readonly=true
  prop element Dom\Element readonly=true
-- r in-scope (3)
  [0] Dom\NamespaceInfo prefix=NULL ns='http://d' elem=r
  [1] Dom\NamespaceInfo prefix='a' ns='http://a' elem=r
  [2] Dom\NamespaceInfo prefix='b' ns='http://b' elem=r
-- a:c in-scope (4)
  [0] Dom\NamespaceInfo prefix=NULL ns='http://d' elem=a:c
  [1] Dom\NamespaceInfo prefix='b' ns='http://b' elem=a:c
  [2] Dom\NamespaceInfo prefix='d' ns='http://d2' elem=a:c
  [3] Dom\NamespaceInfo prefix='a' ns='http://a2' elem=a:c
-- e in-scope (3)
  [0] Dom\NamespaceInfo prefix='b' ns='http://b' elem=e
  [1] Dom\NamespaceInfo prefix='d' ns='http://d2' elem=e
  [2] Dom\NamespaceInfo prefix='a' ns='http://a2' elem=e
-- r descendant (16)
  [0] Dom\NamespaceInfo prefix=NULL ns='http://d' elem=r
  [1] Dom\NamespaceInfo prefix='a' ns='http://a' elem=r
  [2] Dom\NamespaceInfo prefix='b' ns='http://b' elem=r
  [3] Dom\NamespaceInfo prefix=NULL ns='http://d' elem=a:c
  [4] Dom\NamespaceInfo prefix='b' ns='http://b' elem=a:c
  [5] Dom\NamespaceInfo prefix='d' ns='http://d2' elem=a:c
  [6] Dom\NamespaceInfo prefix='a' ns='http://a2' elem=a:c
  [7] Dom\NamespaceInfo prefix='b' ns='http://b' elem=e
  [8] Dom\NamespaceInfo prefix='d' ns='http://d2' elem=e
  [9] Dom\NamespaceInfo prefix='a' ns='http://a2' elem=e
  [10] Dom\NamespaceInfo prefix='b' ns='http://b' elem=f
  [11] Dom\NamespaceInfo prefix='d' ns='http://d2' elem=f
  [12] Dom\NamespaceInfo prefix='a' ns='http://a2' elem=f
  [13] Dom\NamespaceInfo prefix=NULL ns='http://d' elem=g
  [14] Dom\NamespaceInfo prefix='a' ns='http://a' elem=g
  [15] Dom\NamespaceInfo prefix='b' ns='http://b' elem=g
-- r2 in-scope (2)
  [0] Dom\NamespaceInfo prefix='p' ns='http://p' elem=r
  [1] Dom\NamespaceInfo prefix=NULL ns='http://d' elem=r
-- s in-scope (2)
  [0] Dom\NamespaceInfo prefix='p' ns='http://p' elem=s
  [1] Dom\NamespaceInfo prefix=NULL ns='http://d2' elem=s
-- t in-scope (2)
  [0] Dom\NamespaceInfo prefix=NULL ns='http://d' elem=t
  [1] Dom\NamespaceInfo prefix='p' ns='http://p' elem=t
-- plain in-scope (0)
-- plain descendant (0)
-- detached in-scope (0)
Error: Call to private Dom\NamespaceInfo::__construct() from global scope
Error: Cannot modify readonly property Dom\NamespaceInfo::$prefix
NULL
string(8) "http://d"
bool(true)
