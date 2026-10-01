--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: writing a base's private from below creates a dynamic property (zend half of the twin pair)
--DESCRIPTION--
php's answer to the same two writes: the base's private slot is untouched and a
PUBLIC dynamic property of the same plain name is created beside it, behind an
E_DEPRECATED masked here. The scope policy turns that surface into the Error the PHL half pins.
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--INI--
error_reporting=E_ALL & ~E_DEPRECATED
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
O:9:"PidwChild":2:{s:11:"@PidwBase@q";i:1;s:1:"q";i:6;}
