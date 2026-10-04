--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Dom\Element::$innerHTML/$outerHTML in an HTML document: the HTML5 walk, and no well-formed screen
--FILE--
<?php
$dom_hioh_shw = function ($l, $f) {
  echo "== $l\n";
  try { echo "  ", str_replace(["\r", "\n"], ['<CR>', '<LF>'], $f()), "\n"; }
  catch (Throwable $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
};
$dom_hioh_doc = function ($x) { return Dom\HTMLDocument::createFromString($x, LIBXML_NOERROR); };

/* A void element is `<br>` and never `<br/>`, and no element carries an
 * `xmlns` its context already binds -- neither the HTML root's nor an `<svg>`
 * subtree's. */
$dom_hioh_d = $dom_hioh_doc('<html><body><div id=q><p>a<br>b</p><svg><path/></svg></div></body></html>');
$dom_hioh_q = $dom_hioh_d->getElementsByTagName('div')->item(0);
$dom_hioh_shw('outer, void child and a foreign subtree', fn () => $dom_hioh_q->outerHTML);
$dom_hioh_shw('inner, the same span one level down', fn () => $dom_hioh_q->innerHTML);
$dom_hioh_shw('saveHtml agrees with outer', fn () => $dom_hioh_d->saveHtml($dom_hioh_q));

/* An element authored in a namespace of the program's own still writes under
 * the HTML rules, because the DOCUMENT decides and not the node. */
$dom_hioh_n = $dom_hioh_d->createElementNS('urn:custom', 'x:thing');
$dom_hioh_n->setAttribute('a', '1');
$dom_hioh_n->appendChild($dom_hioh_d->createTextNode('t'));
$dom_hioh_q->appendChild($dom_hioh_n);
$dom_hioh_shw('outer, a non-HTML namespace mints no declaration', fn () => $dom_hioh_n->outerHTML);

/* The HTML escapes, which are not the XML ones: U+00A0 is a named entity, a
 * quote leaves text alone, and `<script>`/`<style>` hold RAW text. */
$dom_hioh_p = $dom_hioh_d->getElementsByTagName('p')->item(0);
$dom_hioh_p->textContent = "<&>\u{00a0}\"'";
$dom_hioh_p->setAttribute('t', "<&>\u{00a0}\"'");
$dom_hioh_shw('outer, text and attribute escapes', fn () => $dom_hioh_p->outerHTML);
$dom_hioh_s = $dom_hioh_d->createElement('script');
$dom_hioh_s->appendChild($dom_hioh_d->createTextNode('if (a<b && c) x("&");'));
$dom_hioh_q->appendChild($dom_hioh_s);
$dom_hioh_shw('outer, script holds raw text', fn () => $dom_hioh_s->outerHTML);

/* A carriage return goes out RAW in both positions, where the XML serializer
 * spells it `&#13;` in an attribute. */
$dom_hioh_p->textContent = "a\rb";
$dom_hioh_p->setAttribute('t', "u\rv");
$dom_hioh_shw('outer, a carriage return by position', fn () => $dom_hioh_p->outerHTML);

/* The well-formed screen belongs to the XML serializer alone: nothing here is
 * going to be read back as XML, so php raises nothing. */
$dom_hioh_q->replaceChildren($dom_hioh_d->createComment('a--b'));
$dom_hioh_shw('outer, a comment holding two hyphens', fn () => $dom_hioh_q->outerHTML);
$dom_hioh_q->replaceChildren($dom_hioh_d->createProcessingInstruction('x:y', 'd'));
$dom_hioh_shw('outer, a processing instruction with a colon', fn () => $dom_hioh_q->outerHTML);

/* A template reads its CONTENT and the walk never mints a fragment. */
$dom_hioh_t = $dom_hioh_doc('<html><body><template><i>a</i><br></template></body></html>')
  ->getElementsByTagName('template')->item(0);
$dom_hioh_shw('inner, a template', fn () => $dom_hioh_t->innerHTML);
$dom_hioh_shw('outer, a template', fn () => $dom_hioh_t->outerHTML);

/* Adopted across, the node answers under its NEW document's rules. */
$dom_hioh_x = Dom\XMLDocument::createFromString('<r><c/></r>');
$dom_hioh_b = $dom_hioh_doc('<html><body><div><i>x</i></div></body></html>');
$dom_hioh_out = $dom_hioh_x->importNode($dom_hioh_b->getElementsByTagName('div')->item(0), true);
$dom_hioh_x->documentElement->appendChild($dom_hioh_out);
$dom_hioh_shw('outer, moved into an XML document', fn () => $dom_hioh_out->outerHTML);
$dom_hioh_in = $dom_hioh_b->importNode($dom_hioh_x->getElementsByTagName('c')->item(0), true);
$dom_hioh_in->appendChild($dom_hioh_b->createComment('a--b'));
$dom_hioh_b->getElementsByTagName('body')->item(0)->appendChild($dom_hioh_in);
$dom_hioh_shw('outer, moved into an HTML document', fn () => $dom_hioh_in->outerHTML);
?>
--EXPECT--
== outer, void child and a foreign subtree
  <div id="q"><p>a<br>b</p><svg><path></path></svg></div>
== inner, the same span one level down
  <p>a<br>b</p><svg><path></path></svg>
== saveHtml agrees with outer
  <div id="q"><p>a<br>b</p><svg><path></path></svg></div>
== outer, a non-HTML namespace mints no declaration
  <x:thing a="1">t</x:thing>
== outer, text and attribute escapes
  <p t="<&amp;>&nbsp;&quot;'">&lt;&amp;&gt;&nbsp;"'</p>
== outer, script holds raw text
  <script>if (a<b && c) x("&");</script>
== outer, a carriage return by position
  <p t="u<CR>v">a<CR>b</p>
== outer, a comment holding two hyphens
  <div id="q"><!--a--b--></div>
== outer, a processing instruction with a colon
  <div id="q"><?x:y d></div>
== inner, a template
  <i>a</i><br>
== outer, a template
  <template><i>a</i><br></template>
== outer, moved into an XML document
  <div xmlns="http://www.w3.org/1999/xhtml"><i>x</i></div>
== outer, moved into an HTML document
  <c><!--a--b--></c>
