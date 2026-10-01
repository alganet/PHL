--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: writing a base's private from below is a dynamic property (PHL half of the twin pair)
--DESCRIPTION--
A base's private is invisible below it, so `$b->q = 5` names nothing the class
declares and php CREATES a dynamic property beside the hidden slot, behind
`Creation of dynamic property B::$q`. The non-deprecated policy rejects it
loudly, so the write is the Error every other undeclared write takes here. The
read, the isset and the unset all match php and are pinned in
001-smoke/oo/private_inherited_visibility.phpt.
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
class PidwBase { private $q = 1; }
class PidwChild extends PidwBase {
    public function writeFromChild($v) { $this->q = $v; }
}
$pidw = new PidwChild;
try { $pidw->q = 5; } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
try { $pidw->writeFromChild(6); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
echo str_replace("\0", '@', serialize($pidw)), "\n";
?>
--EXPECT--
Error: Cannot create dynamic property PidwChild::$q
Error: Cannot create dynamic property PidwChild::$q
O:9:"PidwChild":1:{s:11:"@PidwBase@q";i:1;}
