--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced DOM document could not build a single node
--FILE--
<?php
/* php 8.4's `Dom\Document` declares its own factory, and until it existed the
 * namespaced tree could only be READ: every insertion method the class carries
 * had nothing to hand it. The nodes come out family-aware on their own, so the
 * bodies are the 2004 ones -- what differs is the DECLARATIONS and four
 * measured rules:
 *
 *   * `createElement` has no `$value` second parameter at all.
 *   * `createProcessingInstruction`'s `$data` is REQUIRED, and an empty one is
 *     not an absent one: `nodeValue` reads "" and the serializer writes the
 *     separator space.
 *   * A name the grammar refuses is the Invalid Character Error here where the
 *     2004 factory answers Namespace Error.
 *   * An EMPTY-STRING namespace is simply no namespace, where the 2004
 *     `createElementNS` keeps '' apart from null.
 *   * `createCDATASection` screens `]]>` and names the sequence.
 */
function sh($v) {
    if ($v === null) { return 'null'; }
    if (is_object($v)) { return get_class($v) . ' ' . $v->nodeName; }
    return var_export($v, true);
}
function t($label, callable $f) {
    printf("%-34s ", $label);
    try { $v = $f(); } catch (Throwable $e) {
        printf("%s(%d) %s\n", get_class($e), $e->getCode(), $e->getMessage()); return;
    }
    echo sh($v), "\n";
}
$d = Dom\XMLDocument::createFromString('<r/>');

echo "== the nine, and what each answers\n";
t('createElement',        fn() => $d->createElement('x'));
t('createElementNS',      fn() => $d->createElementNS('urn:u', 'p:x'));
t('createDocumentFragment', fn() => $d->createDocumentFragment());
t('createTextNode',       fn() => $d->createTextNode('t'));
t('createCDATASection',   fn() => $d->createCDATASection('c'));
t('createComment',        fn() => $d->createComment('c'));
t('createProcessingInstruction', fn() => $d->createProcessingInstruction('t', 'd'));
t('createAttribute',      fn() => $d->createAttribute('a'));
t('createEntityReference', fn() => $d->createEntityReference('amp'));

echo "\n== the grammar's refusals are Invalid Character, not Namespace\n";
t('element bad name',     fn() => $d->createElement('1x'));
t('element empty name',   fn() => $d->createElement(''));
t('elementNS bad qname',  fn() => $d->createElementNS('urn:u', '1:x'));
t('elementNS space qname', fn() => $d->createElementNS('urn:u', 'a b'));
t('elementNS empty qname', fn() => $d->createElementNS('urn:u', ''));
t('attribute bad name',   fn() => $d->createAttribute('1a'));
t('PI bad target',        fn() => $d->createProcessingInstruction('a b', 'd'));
t('entity ref bad name',  fn() => $d->createEntityReference('1x'));

echo "\n== but a prefix with no namespace is still a Namespace Error\n";
t('elementNS null uri, prefix', fn() => $d->createElementNS(null, 'p:x'));

echo "\n== an empty-string namespace is no namespace\n";
$e = $d->createElementNS('', 'x');
printf("ns=%s prefix=%s\n", sh($e->namespaceURI), sh($e->prefix));
$e = $d->createElementNS(null, 'x');
printf("ns=%s prefix=%s\n", sh($e->namespaceURI), sh($e->prefix));

echo "\n== createCDATASection names the one sequence it refuses\n";
t('cdata with ]]>',       fn() => $d->createCDATASection('c]]>d'));

/* the PI factory carries the same screen over `?>`, the sequence that ends an
 * instruction. The TARGET is screened first, so a bad one is still the bare
 * sentence; a lone '?' or '>' is fine; and nothing screens a WRITE. */
echo "\n== createProcessingInstruction names the one sequence it refuses\n";
t('pi with ?>',           fn() => $d->createProcessingInstruction('t', 'a?>b'));
t('pi data is only ?>',   fn() => $d->createProcessingInstruction('t', '?>'));
t('pi target wins',       fn() => $d->createProcessingInstruction('a b', '?>'));
t('pi lone ?',            fn() => $d->createProcessingInstruction('t', 'a?')->data);
t('pi lone >',            fn() => $d->createProcessingInstruction('t', 'a>b')->data);
t('pi split by newline',  fn() => str_replace("\n", '\\n', $d->createProcessingInstruction('t', "a?\n>b")->data));
t('pi write unscreened',  function() use ($d) { $p = $d->createProcessingInstruction('t', 'ok'); $p->data = 'a?>b'; return $p->data; });
t('2004 takes ?>',        fn() => (new DOMDocument())->createProcessingInstruction('t', 'a?>b')->data);

echo "\n== an empty PI data is not an absent one\n";
$p = $d->createProcessingInstruction('t', '');
printf("nodeValue=%s data=%s\n", sh($p->nodeValue), sh($p->data));
$l = new DOMDocument();
$l->loadXML('<r/>');
$q = $l->createProcessingInstruction('t');
printf("2004 absent:  nodeValue=%s\n", sh($q->nodeValue));
$q = $l->createProcessingInstruction('t', '');
printf("2004 empty:   nodeValue=%s\n", sh($q->nodeValue));

echo "\n== and what the factory builds goes into the tree\n";
$x = $d->createElement('x');
$x->appendChild($d->createTextNode('t&x'));
$x->appendChild($d->createComment('c'));
$x->appendChild($d->createProcessingInstruction('pi', 'd'));
$x->appendChild($d->createCDATASection('cd'));
$x->setAttributeNode($d->createAttribute('plain'));
$f = $d->createDocumentFragment();
$f->appendChild($d->createTextNode('frag'));
$x->appendChild($f);
$d->documentElement->appendChild($x);
echo $d->documentElement->C14N(), "\n";
/* The namespaced element is asked for its own answers rather than for a
 * serialization: WHERE a fresh declaration lands when the node is linked in is
 * the insert-side reconciliation's question, not the factory's, and the two
 * trees still disagree about it. */
$n = $d->createElementNS('urn:n', 'n:y');
printf("nodeName=%s localName=%s prefix=%s ns=%s\n",
    $n->nodeName, $n->localName, sh($n->prefix), sh($n->namespaceURI));
?>
--EXPECT--
== the nine, and what each answers
createElement                      Dom\Element x
createElementNS                    Dom\Element p:x
createDocumentFragment             Dom\DocumentFragment #document-fragment
createTextNode                     Dom\Text #text
createCDATASection                 Dom\CDATASection #cdata-section
createComment                      Dom\Comment #comment
createProcessingInstruction        Dom\ProcessingInstruction t
createAttribute                    Dom\Attr a
createEntityReference              Dom\EntityReference amp

== the grammar's refusals are Invalid Character, not Namespace
element bad name                   DOMException(5) Invalid Character Error
element empty name                 DOMException(5) Invalid Character Error
elementNS bad qname                DOMException(5) Invalid Character Error
elementNS space qname              DOMException(5) Invalid Character Error
elementNS empty qname              DOMException(5) Invalid Character Error
attribute bad name                 DOMException(5) Invalid Character Error
PI bad target                      DOMException(5) Invalid Character Error
entity ref bad name                DOMException(5) Invalid Character Error

== but a prefix with no namespace is still a Namespace Error
elementNS null uri, prefix         DOMException(14) Namespace Error

== an empty-string namespace is no namespace
ns=null prefix=null
ns=null prefix=null

== createCDATASection names the one sequence it refuses
cdata with ]]>                     DOMException(5) Invalid character sequence "]]>" in CDATA section

== createProcessingInstruction names the one sequence it refuses
pi with ?>                         DOMException(5) Invalid character sequence "?>" in processing instruction
pi data is only ?>                 DOMException(5) Invalid character sequence "?>" in processing instruction
pi target wins                     DOMException(5) Invalid Character Error
pi lone ?                          'a?'
pi lone >                          'a>b'
pi split by newline                'a?\\n>b'
pi write unscreened                'a?>b'
2004 takes ?>                      'a?>b'

== an empty PI data is not an absent one
nodeValue='' data=''
2004 absent:  nodeValue=null
2004 empty:   nodeValue=''

== and what the factory builds goes into the tree
<r><x plain="">t&amp;x<?pi d?>cdfrag</x></r>
nodeName=n:y localName=y prefix='n' ns='urn:n'
