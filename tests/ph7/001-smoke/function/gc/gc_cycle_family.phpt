--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
gc_enabled/gc_disable/gc_collect_cycles/gc_status over a real cycle
--FILE--
<?php
/* The gc_* family had NO coverage at all until the 143rd session, which is how
 * gc_status()['running'] answered the wrong question for as long as it existed.
 * Nothing here pins a count the two engines are free to disagree on -- how many
 * roots are buffered, or what the threshold has reached -- only the answers php
 * gives the same way every run. */
class GcFamNode {
    public $peer;
    public static $gone = 0;
    public function __destruct() { GcFamNode::$gone++; }
}
function gcfam_cycle() {
    $a = new GcFamNode();
    $b = new GcFamNode();
    $a->peer = $b;
    $b->peer = $a;
}

echo "enabled: "; var_dump(gc_enabled());

/* A cycle refcounting alone cannot break: the two nodes leave scope holding
 * each other, so neither destructor runs until the collector proves them dead. */
gcfam_cycle();
echo "before: "; var_dump(GcFamNode::$gone);
$n = gc_collect_cycles();
echo "found: "; var_dump($n > 0);
echo "after: "; var_dump(GcFamNode::$gone);

echo "off: "; gc_disable(); var_dump(gc_enabled());
echo "on: ";  gc_enable();  var_dump(gc_enabled());

$s = gc_status();
echo "keys: "; var_dump(array_keys($s));
/* php's "running" is whether a collection is IN PROGRESS -- outside one it is
 * always false. It is not the enabled flag; that is gc_enabled(). */
echo "running: ";   var_dump($s['running']);
echo "protected: "; var_dump($s['protected']);
echo "full: ";      var_dump($s['full']);
echo "counters: ";
var_dump(is_int($s['runs']), is_int($s['collected']), is_int($s['threshold']),
         is_int($s['buffer_size']), is_int($s['roots']));
/* buffer_size is the root buffer's CAPACITY and roots is how much of it is
 * used, so the first can never be under the second. */
echo "capacity: ";  var_dump($s['buffer_size'] > 0, $s['buffer_size'] >= $s['roots']);
echo "ran: ";       var_dump($s['runs'] > 0, $s['collected'] > 0);
?>
--EXPECT--
enabled: bool(true)
before: int(0)
found: bool(true)
after: int(2)
off: bool(false)
on: bool(true)
keys: array(12) {
  [0]=>
  string(7) "running"
  [1]=>
  string(9) "protected"
  [2]=>
  string(4) "full"
  [3]=>
  string(4) "runs"
  [4]=>
  string(9) "collected"
  [5]=>
  string(9) "threshold"
  [6]=>
  string(11) "buffer_size"
  [7]=>
  string(5) "roots"
  [8]=>
  string(16) "application_time"
  [9]=>
  string(14) "collector_time"
  [10]=>
  string(15) "destructor_time"
  [11]=>
  string(9) "free_time"
}
running: bool(false)
protected: bool(false)
full: bool(false)
counters: bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
capacity: bool(true)
bool(true)
ran: bool(true)
bool(true)
--CLEAN--
<?php
gc_enable();
