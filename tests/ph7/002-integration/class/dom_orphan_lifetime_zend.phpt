--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a DROPPED orphan subtree is freed at once, detaching the held descendant (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* the orphan parent's last wrapper dies at the statement's end; php frees the
 * subtree immediately and UNLINKS the wrapped child to keep it alive */
$d = new DOMDocument;
$d->loadXML('<r/>');
$t = $d->createTextNode('T');
$d->createElement('gone')->appendChild($t);
var_dump($t->parentNode, $t->data, $t->ownerDocument === $d);
--EXPECT--
NULL
string(1) "T"
bool(true)
--CLEAN--
<?php
