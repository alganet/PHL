--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOMNode::C14N reads all four of its parameters, C14NFile writes them, and __sleep/__wakeup are the serialization refusal
--FILE--
<?php
$d = new DOMDocument;
$d->loadXML('<!--top--><r xmlns:p="urn:x" xmlns:u="urn:unused" b="2" a="1"><!--c-->'
    . '<p:e p:z="9"><in xmlns:w="urn:w"/></p:e>tx<![CDATA[cd]]><?pi d?></r><!--tail-->');
$r = $d->documentElement;

/* the two bools, on the document and on a node */
foreach (['doc' => $d, 'root' => $r] as $k => $n) {
    foreach ([[false,false],[false,true],[true,false],[true,true]] as $f) {
        echo $k, ' ', (int)$f[0], (int)$f[1], ' ', var_export($n->C14N($f[0], $f[1]), true), "\n";
    }
}
/* a node set is what php canonicalizes a NODE from, so an attribute and a
 * comment each answer only themselves */
foreach (['attr' => $r->attributes->item(0), 'comment' => $r->firstChild,
          'text' => $r->childNodes->item(2), 'pi' => $r->lastChild,
          'detached' => $d->createElement('z')] as $k => $n) {
    echo $k, ' ', var_export($n->C14N(), true), ' ', var_export($n->C14N(false, true), true), "\n";
}

/* $xpath replaces the node set */
echo var_export($d->C14N(false, false, ['query' => '//*']), true), "\n";
echo var_export($d->C14N(false, false, ['query' => '//@*']), true), "\n";
echo var_export($d->C14N(false, false, ['query' => '//pp:e', 'namespaces' => ['pp' => 'urn:x']]), true), "\n";
echo var_export($r->C14N(false, false, ['query' => './/in']), true), "\n";
set_error_handler(function ($n, $s) { echo "E$n: $s\n"; return true; });
try { $d->C14N(false, false, ['query' => '///']); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
restore_error_handler();
try { $d->C14N(false, false, ['q' => '//*']); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $d->C14N(false, false, ['query' => 5]); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* $nsPrefixes: exclusive only, string entries only */
foreach ([[], ['u'], ['p'], ['p','u'], ['u','p'], ['zz'], ['p',5]] as $l) {
    echo json_encode($l), ' ', var_export($d->C14N(true, false, null, $l), true), "\n";
}
set_error_handler(function ($n, $s) { echo "E$n: $s\n"; return true; });
var_dump($d->C14N(false, false, null, ['u']));
restore_error_handler();

/* C14NFile: the same bytes, and their count */
$f = tempnam(sys_get_temp_dir(), 'phlc14n');
var_dump($d->C14NFile($f), file_get_contents($f));
var_dump($r->C14NFile($f, true, true), file_get_contents($f));
var_dump($r->firstChild->C14NFile($f), file_get_contents($f));
unlink($f);
var_dump(@$r->C14NFile('/nonexistent-dir-xyz/f.xml'));
set_error_handler(function ($n, $s) { echo "E$n: $s\n"; return true; });
var_dump($r->C14NFile('/nonexistent-dir-xyz/f.xml'));
restore_error_handler();
try { $d->C14NFile("a\0b"); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $d->C14NFile(''); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $d->C14NFile($f, false, false, ['q' => 1]); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

/* __sleep / __wakeup ARE the refusal */
foreach (['doc' => $d, 'el' => $r, 'text' => $d->createTextNode('t')] as $k => $n) {
    try { serialize($n); } catch (Throwable $e) { echo $k, ' ', get_class($e), ': ', $e->getMessage(), "\n"; }
    try { $n->__sleep(); } catch (Throwable $e) { echo $k, ' ', get_class($e), ': ', $e->getMessage(), "\n"; }
    try { $n->__wakeup(); } catch (Throwable $e) { echo $k, ' ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
foreach (['O:11:"DOMDocument":0:{}', 'O:10:"DOMElement":0:{}', 'O:11:"DOMNodeList":0:{}'] as $s) {
    try { $v = unserialize($s); echo $s, ' => ', (is_object($v) ? get_class($v) : var_export($v, true)), "\n"; }
    catch (Throwable $e) { echo $s, ' => ', get_class($e), ': ', $e->getMessage(), "\n"; }
}
class DomC14nSubSleep extends DOMElement { public function __sleep(): array { return []; } }
class DomC14nSubWake extends DOMElement { public function __wakeup(): void { echo "woke\n"; } }
echo serialize(new DomC14nSubSleep('q')), "\n";
$woke = unserialize('O:14:"DomC14nSubWake":0:{}');
echo get_class($woke), "\n";
$woke = null;
--EXPECT--
doc 00 '<r xmlns:p="urn:x" xmlns:u="urn:unused" a="1" b="2"><p:e p:z="9"><in xmlns:w="urn:w"></in></p:e>txcd<?pi d?></r>'
doc 01 '<!--top-->
<r xmlns:p="urn:x" xmlns:u="urn:unused" a="1" b="2"><!--c--><p:e p:z="9"><in xmlns:w="urn:w"></in></p:e>txcd<?pi d?></r>
<!--tail-->'
doc 10 '<r a="1" b="2"><p:e xmlns:p="urn:x" p:z="9"><in></in></p:e>txcd<?pi d?></r>'
doc 11 '<!--top-->
<r a="1" b="2"><!--c--><p:e xmlns:p="urn:x" p:z="9"><in></in></p:e>txcd<?pi d?></r>
<!--tail-->'
root 00 '<r xmlns:p="urn:x" xmlns:u="urn:unused" a="1" b="2"><p:e p:z="9"><in xmlns:w="urn:w"></in></p:e>txcd<?pi d?></r>'
root 01 '<r xmlns:p="urn:x" xmlns:u="urn:unused" a="1" b="2"><!--c--><p:e p:z="9"><in xmlns:w="urn:w"></in></p:e>txcd<?pi d?></r>'
root 10 '<r a="1" b="2"><p:e xmlns:p="urn:x" p:z="9"><in></in></p:e>txcd<?pi d?></r>'
root 11 '<r a="1" b="2"><!--c--><p:e xmlns:p="urn:x" p:z="9"><in></in></p:e>txcd<?pi d?></r>'
attr ' b="2"' ' b="2"'
comment '' '<!--c-->
'
text 'tx' 'tx'
pi '<?pi d?>
' '<?pi d?>
'
detached '' ''
'<r><p:e><in></in></p:e></r>'
' a="1" b="2" p:z="9"'
'<p:e></p:e>'
'<in></in>'
E2: DOMNode::C14N(): Invalid expression
Error: XPath query did not return a nodeset
ValueError: DOMNode::C14N(): Argument #3 ($xpath) must have a "query" key
TypeError: DOMNode::C14N(): Argument #3 ($xpath) "query" option must be a string, int given
[] '<r a="1" b="2"><p:e xmlns:p="urn:x" p:z="9"><in></in></p:e>txcd<?pi d?></r>'
["u"] '<r xmlns:u="urn:unused" a="1" b="2"><p:e xmlns:p="urn:x" p:z="9"><in></in></p:e>txcd<?pi d?></r>'
["p"] '<r xmlns:p="urn:x" a="1" b="2"><p:e p:z="9"><in></in></p:e>txcd<?pi d?></r>'
["p","u"] '<r xmlns:p="urn:x" xmlns:u="urn:unused" a="1" b="2"><p:e p:z="9"><in></in></p:e>txcd<?pi d?></r>'
["u","p"] '<r xmlns:p="urn:x" xmlns:u="urn:unused" a="1" b="2"><p:e p:z="9"><in></in></p:e>txcd<?pi d?></r>'
["zz"] '<r a="1" b="2"><p:e xmlns:p="urn:x" p:z="9"><in></in></p:e>txcd<?pi d?></r>'
["p",5] '<r xmlns:p="urn:x" a="1" b="2"><p:e p:z="9"><in></in></p:e>txcd<?pi d?></r>'
E8: DOMNode::C14N(): Inclusive namespace prefixes only allowed in exclusive mode.
string(112) "<r xmlns:p="urn:x" xmlns:u="urn:unused" a="1" b="2"><p:e p:z="9"><in xmlns:w="urn:w"></in></p:e>txcd<?pi d?></r>"
int(112)
string(112) "<r xmlns:p="urn:x" xmlns:u="urn:unused" a="1" b="2"><p:e p:z="9"><in xmlns:w="urn:w"></in></p:e>txcd<?pi d?></r>"
int(83)
string(83) "<r a="1" b="2"><!--c--><p:e xmlns:p="urn:x" p:z="9"><in></in></p:e>txcd<?pi d?></r>"
int(0)
string(0) ""
bool(false)
E2: DOMNode::C14NFile(/nonexistent-dir-xyz/f.xml): Failed to open stream: No such file or directory
bool(false)
ValueError: DOMNode::C14NFile(): Argument #1 ($uri) must not contain any null bytes
ValueError: Path must not be empty
ValueError: DOMNode::C14NFile(): Argument #4 ($xpath) must have a "query" key
doc Exception: Serialization of 'DOMDocument' is not allowed, unless serialization methods are implemented in a subclass
doc Exception: Serialization of 'DOMDocument' is not allowed, unless serialization methods are implemented in a subclass
doc Exception: Unserialization of 'DOMDocument' is not allowed, unless unserialization methods are implemented in a subclass
el Exception: Serialization of 'DOMElement' is not allowed, unless serialization methods are implemented in a subclass
el Exception: Serialization of 'DOMElement' is not allowed, unless serialization methods are implemented in a subclass
el Exception: Unserialization of 'DOMElement' is not allowed, unless unserialization methods are implemented in a subclass
text Exception: Serialization of 'DOMText' is not allowed, unless serialization methods are implemented in a subclass
text Exception: Serialization of 'DOMText' is not allowed, unless serialization methods are implemented in a subclass
text Exception: Unserialization of 'DOMText' is not allowed, unless unserialization methods are implemented in a subclass
O:11:"DOMDocument":0:{} => Exception: Unserialization of 'DOMDocument' is not allowed, unless unserialization methods are implemented in a subclass
O:10:"DOMElement":0:{} => Exception: Unserialization of 'DOMElement' is not allowed, unless unserialization methods are implemented in a subclass
O:11:"DOMNodeList":0:{} => DOMNodeList
O:15:"DomC14nSubSleep":0:{}
woke
DomC14nSubWake
