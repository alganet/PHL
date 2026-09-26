--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMProcessingInstruction and DOMEntityReference, and the kinds with no nodeValue
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<r>t<?target some data?>&amp;<!--c--></r>');
$r = $d->documentElement;
$pi = $r->childNodes->item(1);
// (`&amp;` parses to TEXT -- a predefined entity is substituted, so the only
// way to hold an entity REFERENCE node is to create one.)
$er = $r->childNodes->item(2);
printf("pi   class=%s type=%d name=%s target=%s data=%s value=%s text=%s\n", get_class($pi), $pi->nodeType,
    $pi->nodeName, $pi->target, $pi->data, var_export($pi->nodeValue, true), var_export($pi->textContent, true));
printf("amp  class=%s type=%d name=%s value=%s\n", get_class($er), $er->nodeType, $er->nodeName,
    var_export($er->nodeValue, true));
// A PI's data is writable; its target is not, and it is NOT a DOMCharacterData.
$pi->data = 'x y';
echo $d->saveXML($pi), "\n";
var_dump($pi instanceof DOMCharacterData, $pi instanceof DOMNode, method_exists($pi, 'appendData'));
try { $pi->target = 'z'; } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }
// The factories, and the names they refuse.
$made = $d->createProcessingInstruction('php', 'echo 1;');
$ref = $d->createEntityReference('amp');
$r->appendChild($made);
$r->appendChild($ref);
printf("made=%s(%s|%s) ref=%s(%s) %s\n", get_class($made), $made->target, $made->data,
    get_class($ref), $ref->nodeName, str_replace("\n", '', $d->saveXML($r)));
foreach ([['createProcessingInstruction', '1bad'], ['createProcessingInstruction', 'a b'],
          ['createProcessingInstruction', ''], ['createEntityReference', '1bad'],
          ['createEntityReference', '']] as [$fn, $name]) {
    try { $d->$fn($name); printf("%-28s %s ok\n", $fn, var_export($name, true)); }
    catch (Throwable $t) { printf("%-28s %-8s %s: %s\n", $fn, var_export($name, true), get_class($t), $t->getMessage()); }
}
// A PI with no data at all, and the empty-data form.
$p2 = $d->createProcessingInstruction('bare');
echo $d->saveXML($p2), "|", var_export($p2->data, true), "\n";
// nodeValue is null for every kind that has no value of its own; textContent
// still walks the children. (Measured on a document with no entity REFERENCE
// in it: what one contributes to an ancestor's text is libxml-version
// dependent -- PLAN 7.4.)
$d2 = new DOMDocument;
$d2->loadXML('<r>t<?target d?></r>');
$f = $d2->createDocumentFragment();
$f->appendChild($d2->createTextNode('in-frag'));
$rows = ['doc' => $d2, 'frag' => $f, 'elem' => $d2->documentElement,
         'pi' => $d2->documentElement->lastChild];
foreach ($rows as $k => $n) {
    printf("%-5s nodeValue=%-12s textContent=%s\n", $k, var_export($n->nodeValue, true),
        var_export(substr($n->textContent, 0, 12), true));
}
// An entity reference's nodeValue is null too; what it CONTRIBUTES as text is
// the version-dependent half and is left out.
var_dump($d2->createEntityReference('amp')->nodeValue);
--EXPECT--
pi   class=DOMProcessingInstruction type=7 name=target target=target data=some data value='some data' text='some data'
amp  class=DOMText type=3 name=#text value='&'
<?target x y?>
bool(false)
bool(true)
bool(false)
Error: Cannot modify readonly property DOMProcessingInstruction::$target
made=DOMProcessingInstruction(php|echo 1;) ref=DOMEntityReference(amp) <r>t<?target x y?>&amp;<!--c--><?php echo 1;?>&amp;</r>
createProcessingInstruction  '1bad'   DOMException: Invalid Character Error
createProcessingInstruction  'a b'    DOMException: Invalid Character Error
createProcessingInstruction  ''       DOMException: Invalid Character Error
createEntityReference        '1bad'   DOMException: Invalid Character Error
createEntityReference        ''       DOMException: Invalid Character Error
<?bare?>|''
doc   nodeValue=NULL         textContent='t'
frag  nodeValue=NULL         textContent='in-frag'
elem  nodeValue='t'          textContent='t'
pi    nodeValue='d'          textContent='d'
NULL
