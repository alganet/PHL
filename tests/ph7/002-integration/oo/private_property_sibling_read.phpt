--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reading an inaccessible property: a subclass reading a parent-private yields null (undefined property); every other denied read is a catchable "Cannot access" Error (subprocess: uses @ suppression)
--FILE--
<?php
class PvsA { private $priv = 1; protected $prot = 2; }
class PvsSub extends PvsA {
    public function readPriv() { return @$this->priv; }  // parent-private is invisible here -> null
    public function readProt() { return $this->prot; }   // parent-protected is accessible
}
$pvsS = new PvsSub();
echo "sub-priv="; var_export($pvsS->readPriv()); echo "\n";
echo "sub-prot=", $pvsS->readProt(), "\n";
// A genuinely inaccessible read from outside the class is a CATCHABLE Error (not a fatal)
$pvsA = new PvsA();
try { $x = $pvsA->priv; echo "no-throw\n"; }
catch (\Error $e) { echo "caught: ", $e->getMessage(), "\n"; }
try { $y = $pvsA->prot; echo "no-throw\n"; }
catch (\Error $e) { echo "caught: ", $e->getMessage(), "\n"; }
echo "done\n";
?>
--EXPECT--
sub-priv=NULL
sub-prot=2
caught: Cannot access private property PvsA::$priv
caught: Cannot access protected property PvsA::$prot
done
