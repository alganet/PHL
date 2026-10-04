--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The empty namespace means "no namespace" on php 8.4's DOM tree and a URI on the 2004 one
--FILE--
<?php
$dom_ens_show = static function (string $lbl, callable $fn): void {
    try {
        var_dump($lbl, $fn());
    } catch (\Throwable $e) {
        echo $lbl, ' => ', get_class($e), ': ', $e->getMessage(), "\n";
    }
};
$dom_ens_src = '<r xmlns:p="urn:u" a="V" p:b="W"/>';
$dom_ens_old = static function () use ($dom_ens_src) {
    $d = new DOMDocument();
    $d->loadXML($dom_ens_src);
    return $d->documentElement;
};
$dom_ens_new = static fn() => Dom\XMLDocument::createFromString($dom_ens_src)->documentElement;

/* The four attribute doors that LOOK a namespace up. On the modern tree the
   empty string is the null one, the same normalization its factories make. */
foreach (['old' => $dom_ens_old, 'new' => $dom_ens_new] as $dom_ens_tree => $dom_ens_mk) {
    foreach (['' => "''", 'urn:u' => 'urn:u'] as $dom_ens_u => $dom_ens_lbl) {
        $dom_ens_e = $dom_ens_mk();
        $dom_ens_show("$dom_ens_tree get($dom_ens_lbl,a)", fn() => $dom_ens_e->getAttributeNS($dom_ens_u, 'a'));
        $dom_ens_show("$dom_ens_tree has($dom_ens_lbl,a)", fn() => $dom_ens_e->hasAttributeNS($dom_ens_u, 'a'));
        $dom_ens_show("$dom_ens_tree node($dom_ens_lbl,a)", fn() => $dom_ens_e->getAttributeNodeNS($dom_ens_u, 'a')?->value);
        $dom_ens_e->removeAttributeNS($dom_ens_u, 'a');
        $dom_ens_show("$dom_ens_tree gone($dom_ens_lbl,a)", fn() => $dom_ens_e->hasAttributeNS('urn:u', 'b') && !$dom_ens_e->getAttributeNode('a'));
    }
}

/* setIdAttributeNS keeps the literal argument: Not Found on both trees. */
$dom_ens_show('new setId(\'\',a)', function () use ($dom_ens_new) {
    $dom_ens_new()->setIdAttributeNS('', 'a', true);
    return 'no refusal';
});

/* Asking the SCOPE instead of a URI: "is nothing the default here?" is a
   question only the modern tree takes, and an xmlns="" answers it. */
foreach ([
    'none' => '<r><c/></r>',
    'dflt' => '<r xmlns="urn:d"><c/></r>',
    'undo' => '<r xmlns="urn:d"><c xmlns=""/></r>',
    'pfx'  => '<p:r xmlns:p="urn:u"><p:c/></p:r>',
] as $dom_ens_k => $dom_ens_s) {
    $dom_ens_o = new DOMDocument();
    $dom_ens_o->loadXML($dom_ens_s);
    $dom_ens_m = Dom\XMLDocument::createFromString($dom_ens_s);
    foreach (['old' => $dom_ens_o, 'new' => $dom_ens_m] as $dom_ens_t => $dom_ens_d) {
        $dom_ens_r = $dom_ens_d->documentElement;
        $dom_ens_c = $dom_ens_r->firstElementChild;
        $dom_ens_show("$dom_ens_k $dom_ens_t root isDefault('')", fn() => $dom_ens_r->isDefaultNamespace(''));
        $dom_ens_show("$dom_ens_k $dom_ens_t kid  isDefault('')", fn() => $dom_ens_c->isDefaultNamespace(''));
        $dom_ens_show("$dom_ens_k $dom_ens_t root isDefault(urn:d)", fn() => $dom_ens_r->isDefaultNamespace('urn:d'));
        $dom_ens_show("$dom_ens_k $dom_ens_t root lookupUri('')", fn() => $dom_ens_r->lookupNamespaceURI(''));
        $dom_ens_show("$dom_ens_k $dom_ens_t kid  lookupUri('')", fn() => $dom_ens_c->lookupNamespaceURI(''));
        $dom_ens_show("$dom_ens_k $dom_ens_t kid  lookupUri(null)", fn() => $dom_ens_c->lookupNamespaceURI(null));
        $dom_ens_show("$dom_ens_k $dom_ens_t root lookupPrefix('')", fn() => $dom_ens_r->lookupPrefix(''));
    }
}
--EXPECT--
string(13) "old get('',a)"
string(0) ""
string(13) "old has('',a)"
bool(false)
string(14) "old node('',a)"
NULL
string(14) "old gone('',a)"
bool(false)
string(16) "old get(urn:u,a)"
string(0) ""
string(16) "old has(urn:u,a)"
bool(false)
string(17) "old node(urn:u,a)"
NULL
string(17) "old gone(urn:u,a)"
bool(false)
string(13) "new get('',a)"
string(1) "V"
string(13) "new has('',a)"
bool(true)
string(14) "new node('',a)"
string(1) "V"
string(14) "new gone('',a)"
bool(true)
string(16) "new get(urn:u,a)"
NULL
string(16) "new has(urn:u,a)"
bool(false)
string(17) "new node(urn:u,a)"
NULL
string(17) "new gone(urn:u,a)"
bool(false)
new setId('',a) => DOMException: Not Found Error
string(27) "none old root isDefault('')"
bool(false)
string(27) "none old kid  isDefault('')"
bool(false)
string(30) "none old root isDefault(urn:d)"
bool(false)
string(27) "none old root lookupUri('')"
NULL
string(27) "none old kid  lookupUri('')"
NULL
string(29) "none old kid  lookupUri(null)"
NULL
string(30) "none old root lookupPrefix('')"
NULL
string(27) "none new root isDefault('')"
bool(true)
string(27) "none new kid  isDefault('')"
bool(true)
string(30) "none new root isDefault(urn:d)"
bool(false)
string(27) "none new root lookupUri('')"
NULL
string(27) "none new kid  lookupUri('')"
NULL
string(29) "none new kid  lookupUri(null)"
NULL
string(30) "none new root lookupPrefix('')"
NULL
string(27) "dflt old root isDefault('')"
bool(false)
string(27) "dflt old kid  isDefault('')"
bool(false)
string(30) "dflt old root isDefault(urn:d)"
bool(true)
string(27) "dflt old root lookupUri('')"
NULL
string(27) "dflt old kid  lookupUri('')"
NULL
string(29) "dflt old kid  lookupUri(null)"
string(5) "urn:d"
string(30) "dflt old root lookupPrefix('')"
NULL
string(27) "dflt new root isDefault('')"
bool(false)
string(27) "dflt new kid  isDefault('')"
bool(false)
string(30) "dflt new root isDefault(urn:d)"
bool(true)
string(27) "dflt new root lookupUri('')"
string(5) "urn:d"
string(27) "dflt new kid  lookupUri('')"
string(5) "urn:d"
string(29) "dflt new kid  lookupUri(null)"
string(5) "urn:d"
string(30) "dflt new root lookupPrefix('')"
NULL
string(27) "undo old root isDefault('')"
bool(false)
string(27) "undo old kid  isDefault('')"
bool(false)
string(30) "undo old root isDefault(urn:d)"
bool(true)
string(27) "undo old root lookupUri('')"
NULL
string(27) "undo old kid  lookupUri('')"
NULL
string(29) "undo old kid  lookupUri(null)"
string(0) ""
string(30) "undo old root lookupPrefix('')"
NULL
string(27) "undo new root isDefault('')"
bool(false)
string(27) "undo new kid  isDefault('')"
bool(true)
string(30) "undo new root isDefault(urn:d)"
bool(true)
string(27) "undo new root lookupUri('')"
string(5) "urn:d"
string(27) "undo new kid  lookupUri('')"
NULL
string(29) "undo new kid  lookupUri(null)"
NULL
string(30) "undo new root lookupPrefix('')"
NULL
string(26) "pfx old root isDefault('')"
bool(false)
string(26) "pfx old kid  isDefault('')"
bool(false)
string(29) "pfx old root isDefault(urn:d)"
bool(false)
string(26) "pfx old root lookupUri('')"
NULL
string(26) "pfx old kid  lookupUri('')"
NULL
string(28) "pfx old kid  lookupUri(null)"
NULL
string(29) "pfx old root lookupPrefix('')"
NULL
string(26) "pfx new root isDefault('')"
bool(true)
string(26) "pfx new kid  isDefault('')"
bool(true)
string(29) "pfx new root isDefault(urn:d)"
bool(false)
string(26) "pfx new root lookupUri('')"
NULL
string(26) "pfx new kid  lookupUri('')"
NULL
string(28) "pfx new kid  lookupUri(null)"
NULL
string(29) "pfx new root lookupPrefix('')"
NULL
