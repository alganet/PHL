--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
The namespaced DOM document could not be written back out
--FILE--
<?php
/* php 8.4's `Dom\XMLDocument` declares its own writers rather than inheriting
 * DOMDocument's, and until they existed a document that could be parsed could
 * never be serialized again. The bytes are NOT the 2004 saver's, and three
 * measured rules say how they differ:
 *
 *   * php's serializer writes the XML declaration and then each top-level
 *     child back to back. libxml's document saver separates them with a
 *     newline and ends the document with one, so a prolog PI, a trailing
 *     comment and a bare root all disagree. A DOCTYPE is the one child php
 *     does write a newline after.
 *   * A parsed document that declared no encoding is given one -- the override
 *     if the producer was handed one, else UTF-8 -- where the 2004 loaders
 *     leave it unset. So the declaration carries `encoding="UTF-8"` here.
 *   * `LIBXML_NOXMLDECL` reaches the string writer only, exactly as it reaches
 *     `saveXML()` and not `save()`: a written FILE always carries its
 *     declaration.
 */
$d = Dom\XMLDocument::createFromString('<?pi go?><r a="1"><c/></r><!--tail-->');
var_dump($d->saveXml());
var_dump($d->saveXml(null, LIBXML_NOXMLDECL));
var_dump($d->saveXml($d->documentElement->firstChild));
$d->formatOutput = true;
var_dump($d->saveXml());
$d->formatOutput = false;

/* A DOCTYPE, the one join that keeps its newline. */
$e = Dom\XMLDocument::createFromString('<!DOCTYPE r><r/>');
var_dump($e->saveXml());

/* The file writer answers the COUNT of the bytes it wrote, and writes the same
 * ones -- declaration included, whatever the option word says. */
$p = sys_get_temp_dir() . '/phl_dom_ns_writers_' . getmypid() . '.xml';
var_dump($d->saveXmlFile($p, LIBXML_NOXMLDECL));
var_dump(file_get_contents($p));
@unlink($p);

/* The two argument refusals, and the wrong document's node. */
try { $d->saveXmlFile(''); } catch (ValueError $x) { echo $x->getMessage(), "\n"; }
try { $d->saveXmlFile("a\0b"); } catch (ValueError $x) { echo $x->getMessage(), "\n"; }
$o = Dom\XMLDocument::createFromString('<z/>');
try { $d->saveXml($o->documentElement); } catch (DOMException $x) {
    echo get_class($x), ': ', $x->getMessage(), "\n";
}

/* The 2004 document keeps libxml's joins and its unset encoding. */
$l = new DOMDocument();
$l->loadXML('<?pi go?><r/><!--tail-->');
var_dump($l->saveXML());
--EXPECT--
string(76) "<?xml version="1.0" encoding="UTF-8"?>
<?pi go?><r a="1"><c/></r><!--tail-->"
string(37) "<?pi go?><r a="1"><c/></r><!--tail-->"
string(4) "<c/>"
string(80) "<?xml version="1.0" encoding="UTF-8"?>
<?pi go?><r a="1">
  <c/>
</r><!--tail-->"
string(56) "<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE r>
<r/>"
int(76)
string(76) "<?xml version="1.0" encoding="UTF-8"?>
<?pi go?><r a="1"><c/></r><!--tail-->"
Dom\XMLDocument::saveXmlFile(): Argument #1 ($filename) must not be empty
Dom\XMLDocument::saveXmlFile(): Argument #1 ($filename) must not contain any null bytes
DOMException: Wrong Document Error
string(49) "<?xml version="1.0"?>
<?pi go?>
<r/>
<!--tail-->
"
