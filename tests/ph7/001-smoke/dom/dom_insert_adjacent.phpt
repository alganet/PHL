--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMElement::insertAdjacentElement/insertAdjacentText: positions, adoption, refusals
--FILE--
<?php
set_error_handler(function($no,$str){ echo "  [err $no] $str\n"; return true; });
$dom_ia_try = function ($l, $f) { echo "== $l\n"; try { $r = $f(); echo "  ret: ", var_export($r, true), "\n"; } catch (Throwable $e) { echo "  ", get_class($e), "(", $e->getCode(), "): ", $e->getMessage(), "\n"; } };
$dom_ia_try('afterend own NEXT sibling', function(){
  $d=new DOMDocument; $d->loadXML('<r><a/><b/><c/></r>');
  $a=$d->documentElement->firstChild;
  $ret = $a->insertAdjacentElement('afterend', $a->nextSibling);
  return [$ret->tagName, $d->saveXML($d->documentElement)];
});
$dom_ia_try('beforebegin own PREV sibling', function(){
  $d=new DOMDocument; $d->loadXML('<r><a/><b/><c/></r>');
  $b=$d->documentElement->firstChild->nextSibling;
  $ret = $b->insertAdjacentElement('beforebegin', $d->documentElement->firstChild);
  return [$ret->tagName, $d->saveXML($d->documentElement)];
});
$dom_ia_try('afterbegin own first child', function(){
  $d=new DOMDocument; $d->loadXML('<r><host><x/><y/></host></r>');
  $h=$d->documentElement->firstChild;
  $h->insertAdjacentElement('afterbegin', $h->firstChild);
  return $d->saveXML($d->documentElement);
});
$dom_ia_try('adopt: wrapper follows into new doc', function(){
  $d=new DOMDocument; $d->loadXML('<r><host/></r>');
  $d2=new DOMDocument; $d2->loadXML('<o><z><zz/></z></o>');
  $z=$d2->documentElement->firstChild; $zz=$z->firstChild;
  $r=$d->documentElement->firstChild->insertAdjacentElement('afterbegin', $z);
  return [$r === $z, $z->ownerDocument === $d, $zz->ownerDocument === $d, $d->saveXML($d->documentElement), $d2->saveXML($d2->documentElement)];
});
$dom_ia_try('adopt then CYCLE fail: state', function(){
  $d=new DOMDocument; $d->loadXML('<r><host><in/></host></r>');
  $h=$d->documentElement->firstChild;
  try { $h->firstChild->insertAdjacentElement('afterbegin', $h); } catch (Throwable $e) { echo "  ",get_class($e),"(",$e->getCode(),")\n"; }
  return $d->saveXML();
});
$dom_ia_try('STRICT-ANCESTOR beforebegin: subtree lost', function(){
  $d=new DOMDocument; $d->loadXML('<r><host><mid><in/></mid></host></r>');
  $in=$d->documentElement->firstChild->firstChild->firstChild;
  $host=$d->documentElement->firstChild;
  try { $in->insertAdjacentElement('beforebegin', $host); } catch (Throwable $e) { echo "  ",get_class($e),"(",$e->getCode(),")\n"; }
  return $d->saveXML();
});
$dom_ia_try('text empty where', function(){
  $d=new DOMDocument; $d->loadXML('<r><a/></r>');
  $d->documentElement->firstChild->insertAdjacentText('', 'T');
});
$dom_ia_try('adopt cross-doc namespaced', function(){
  $d=new DOMDocument; $d->loadXML('<r><host/></r>');
  $d2=new DOMDocument; $d2->loadXML('<o xmlns:p="urn:p"><p:z k="1"/></o>');
  $z=$d2->documentElement->firstChild;
  $d->documentElement->firstChild->insertAdjacentElement('beforeend', $z);
  return $d->saveXML($d->documentElement);
});
$dom_ia_try('strict=false cycle', function(){
  $d=new DOMDocument; $d->loadXML('<r><host><in/></host></r>');
  $d->strictErrorChecking=false;
  $h=$d->documentElement->firstChild;
  $ret=$h->firstChild->insertAdjacentElement('afterbegin', $h);
  return [$ret, $d->saveXML()];
});
$dom_ia_try('all four + case + returns', function(){
  $d=new DOMDocument; $d->loadXML('<r><host><in/></host></r>');
  $h=$d->documentElement->firstChild;
  $r1=$h->insertAdjacentElement('BeforeBegin', $d->createElement('bb'));
  $r2=$h->insertAdjacentElement('AFTERBEGIN', $d->createElement('ab'));
  $h->insertAdjacentText('beforeend', 'BE');
  $h->insertAdjacentText('afterend', 'AE');
  return [$r1->tagName, $r2->tagName, $d->saveXML($d->documentElement)];
});
$dom_ia_try('invalid where beats parentless', function(){
  $d=new DOMDocument; $o=$d->createElement('o');
  return $o->insertAdjacentElement('bogus', $d->createElement('x'));
});
$dom_ia_try('parentless beforebegin/afterend null', function(){
  $d=new DOMDocument; $o=$d->createElement('o');
  $a=$o->insertAdjacentElement('beforebegin', $d->createElement('x'));
  $b=$o->insertAdjacentElement('afterend', $d->createElement('y'));
  $o->insertAdjacentText('beforebegin', 'T');
  $c=$o->insertAdjacentElement('afterbegin', $d->createElement('z'));
  return [$a, $b, $c->tagName, $d->saveXML($o)];
});
$dom_ia_try('text empty string still a node', function(){
  $d=new DOMDocument; $d->loadXML('<r><host/></r>');
  $h=$d->documentElement->firstChild;
  $h->insertAdjacentText('afterbegin', '');
  return [$h->childNodes->length, $d->saveXML($d->documentElement)];
});
$dom_ia_try('type errors from the declared row', function(){
  $d=new DOMDocument; $d->loadXML('<r><host/></r>');
  $h=$d->documentElement->firstChild;
  try { $h->insertAdjacentElement('beforeend', $d->createTextNode('t')); }
  catch (TypeError $e) { echo "  TypeError: ", $e->getMessage(), "\n"; }
  $h->insertAdjacentText('afterbegin', 42);
  return $d->saveXML($d->documentElement);
});
echo "done\n";
--EXPECT--
== afterend own NEXT sibling
  ret: array (
  0 => 'b',
  1 => '<r><a/><b/><c/></r>',
)
== beforebegin own PREV sibling
  ret: array (
  0 => 'a',
  1 => '<r><a/><b/><c/></r>',
)
== afterbegin own first child
  ret: '<r><host><x/><y/></host></r>'
== adopt: wrapper follows into new doc
  ret: array (
  0 => true,
  1 => true,
  2 => true,
  3 => '<r><host><z><zz/></z></host></r>',
  4 => '<o/>',
)
== adopt then CYCLE fail: state
  DOMException(3)
  ret: '<?xml version="1.0"?>
<r/>
'
== STRICT-ANCESTOR beforebegin: subtree lost
  DOMException(3)
  ret: '<?xml version="1.0"?>
<r/>
'
== text empty where
  DOMException(12): Syntax Error
== adopt cross-doc namespaced
  ret: '<r><host><p:z xmlns:p="urn:p" k="1"/></host></r>'
== strict=false cycle
  [err 2] DOMElement::insertAdjacentElement(): Hierarchy Request Error
  ret: array (
  0 => NULL,
  1 => '<?xml version="1.0"?>
<r/>
',
)
== all four + case + returns
  ret: array (
  0 => 'bb',
  1 => 'ab',
  2 => '<r><bb/><host><ab/><in/>BE</host>AE</r>',
)
== invalid where beats parentless
  DOMException(12): Syntax Error
== parentless beforebegin/afterend null
  ret: array (
  0 => NULL,
  1 => NULL,
  2 => 'z',
  3 => '<o><z/></o>',
)
== text empty string still a node
  ret: array (
  0 => 1,
  1 => '<r><host></host></r>',
)
== type errors from the declared row
  TypeError: DOMElement::insertAdjacentElement(): Argument #2 ($element) must be of type DOMElement, DOMText given
  ret: '<r><host>42</host></r>'
done
