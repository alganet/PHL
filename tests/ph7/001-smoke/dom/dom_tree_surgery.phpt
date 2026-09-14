--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
DOM tree surgery: appendChild re-parenting, insertBefore, replaceChild, removeChild, creators
--FILE--
<?php
$d = new DOMDocument; $d->preserveWhiteSpace = false;
$d->loadXML('<r><logging><log type="junit"/><log type="tap"/></logging><coverage/></r>');
$cov = $d->getElementsByTagName('coverage')->item(0);
$log = $d->getElementsByTagName('log')->item(0);
$moved = $cov->appendChild($log);
var_dump($moved === $log);
$rep = $d->createElement('report');
$rep->setAttribute('outputFile', 'junit.xml');
$cov->insertBefore($rep, $log);
$tap = $d->getElementsByTagName('log')->item(1);
$repl = $d->createElement('replacement');
$tap->parentNode->replaceChild($repl, $tap);
$logging = $d->getElementsByTagName('logging')->item(0);
$removed = $logging->parentNode->removeChild($logging);
var_dump($removed === $logging);
echo $d->saveXML();
$t = $d->createTextNode('tail');
$cov->appendChild($t);
$c = $d->createComment('note');
$cov->appendChild($c);
$cd = $d->createCDATASection('cd&ta');
$cov->appendChild($cd);
echo $d->saveXML($cov), "\n";
try { $d->removeChild($rep); } catch (DOMException $e) { echo get_class($e), "\n"; }
--EXPECT--
bool(true)
bool(true)
<?xml version="1.0"?>
<r><coverage><report outputFile="junit.xml"/><replacement/></coverage></r>
<coverage><report outputFile="junit.xml"/><replacement/>tail<!--note--><![CDATA[cd&ta]]></coverage>
DOMException
