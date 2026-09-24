--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php: a hook throwing inside a NESTED object leaves the partial export printed (zend half of the twin pair)
--SKIPIF--
<?php
if (!function_exists('zend_version')) {
    echo "skip zend-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* php's var_export prints nothing when the exported object is the one whose hook
 * throws, and nothing when that object is nested in an ARRAY -- but when it is
 * nested inside another OBJECT the buffer built so far reaches the output, ending
 * mid-entry and syntactically invalid. PHL prints nothing in all three; see the
 * PHL half of the pair. */
class VehnInner { public int $v { get => throw new LogicException('deep'); } }
class VehnOuter {
    public $a = 1;
    public VehnInner $i;
    public function __construct() { $this->i = new VehnInner; }
}
try { var_export(new VehnOuter); } catch (Throwable $e) { echo '|caught ', $e->getMessage(), "\n"; }
echo "AFTER\n";
?>
--EXPECT--
\VehnOuter::__set_state(array(
   'a' => 1,
   'i' => 
  \VehnInner::__set_state(array(
,
))|caught deep
AFTER
--CLEAN--
<?php
