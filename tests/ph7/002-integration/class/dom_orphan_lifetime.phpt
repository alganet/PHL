--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a DROPPED orphan subtree stays parked until the document goes, so the held descendant keeps its parent (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* every unlinked node is parked on its document's orphan set and freed with
 * the document -- php frees a dropped orphan at its last wrapper's death and
 * unlinks the held child. The child is alive and correct either way; only its
 * parentNode differs, and only after its parent's wrapper was thrown away. */
$d = new DOMDocument;
$d->loadXML('<r/>');
$t = $d->createTextNode('T');
$d->createElement('gone')->appendChild($t);
var_dump($t->parentNode ? $t->parentNode->nodeName : null, $t->data, $t->ownerDocument === $d);
--EXPECT--
string(4) "gone"
string(1) "T"
bool(true)
--CLEAN--
<?php
