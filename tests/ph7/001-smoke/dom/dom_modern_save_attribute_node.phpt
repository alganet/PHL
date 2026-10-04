--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An attribute or a doctype saved on its own out of the namespaced tree carries the 2004 tree's separating space
--FILE--
<?php
$xml = '<?xml version="1.0"?>' . "\n"
     . '<!DOCTYPE r [<!ENTITY e "EV">]>' . "\n"
     . '<r xmlns="urn:d" xmlns:a="urn:a" xml:lang="en" a:x="1"'
     . ' y="&e;pre" z="" w="a&#13;b&#9;c&#10;d" g="1&gt;0 &quot;q&quot; &amp; &lt;t&gt;"/>';

$names = ['xmlns', 'xmlns:a', 'xml:lang', 'a:x', 'y', 'z', 'w', 'g'];

echo "-- the namespaced tree writes an attribute from its name\n";
$d = Dom\XMLDocument::createFromString($xml);
$r = $d->documentElement;
foreach ($names as $n) {
    printf("%-9s [%s]\n", $n, $d->saveXml($r->getAttributeNode($n)));
}

echo "-- an attribute whose prefix only the document parks declares nothing\n";
$c = $d->createElement('c');
$r->appendChild($c);
$m = $d->createAttributeNS('urn:z', 'p:q');
$m->value = 'v';
$c->setAttributeNodeNS($m);
printf("%-9s [%s]\n", 'p:q', $d->saveXml($m));

echo "-- the 2004 tree writes it from inside the start tag it belongs to\n";
$o = new DOMDocument;
$o->loadXML($xml);
$or = $o->documentElement;
foreach ($names as $n) {
    $at = $or->getAttributeNode($n);
    if (!($at instanceof DOMAttr)) {
        printf("%-9s (%s)\n", $n, get_class($at));
        continue;
    }
    printf("%-9s [%s]\n", $n, $o->saveXML($at));
}

/* A doctype's own bytes are libxml's and differ between its releases, so the
   subset above is left out of this one: what is being asked is only whether a
   newline is joined to it. */
echo "-- a doctype is libxml's, and the namespaced tree joins a newline to it\n";
$bare = '<!DOCTYPE r><r/>';
$dn = Dom\XMLDocument::createFromString($bare);
$on = new DOMDocument;
$on->loadXML($bare);
printf("%-9s [%s]\n", 'new', $dn->saveXml($dn->doctype));
printf("%-9s [%s]\n", 'old', $on->saveXML($on->doctype));
?>
--EXPECT--
-- the namespaced tree writes an attribute from its name
xmlns     [xmlns="urn:d"]
xmlns:a   [xmlns:a="urn:a"]
xml:lang  [xml:lang="en"]
a:x       [a:x="1"]
y         [y="&e;pre"]
z         [z=""]
w         [w="a&#13;b&#9;c&#10;d"]
g         [g="1&gt;0 &quot;q&quot; &amp; &lt;t&gt;"]
-- an attribute whose prefix only the document parks declares nothing
p:q       [p:q="v"]
-- the 2004 tree writes it from inside the start tag it belongs to
xmlns     (DOMNameSpaceNode)
xmlns:a   (DOMNameSpaceNode)
xml:lang  [ xml:lang="en"]
a:x       [ a:x="1"]
y         [ y="&e;pre"]
z         [ z=""]
w         [ w="a&#13;b&#9;c&#10;d"]
g         [ g="1&gt;0 &quot;q&quot; &amp; &lt;t&gt;"]
-- a doctype is libxml's, and the namespaced tree joins a newline to it
new       [<!DOCTYPE r>
]
old       [<!DOCTYPE r>]
