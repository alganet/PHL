--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: SPL data structures (band D)
--FILE--
<?php
$d = new SplDoublyLinkedList();
$d->push(1); $d->push(2); $d->unshift(0);
foreach ($d as $k => $v) echo "$k=$v "; echo "| ", $d->top(), $d->bottom(), count($d), "\n";
echo $d->pop(), $d->shift(), count($d), $d->isEmpty() ? "E" : "N", "\n";
$e = new SplDoublyLinkedList();
foreach (["pop", "shift", "top", "bottom"] as $m) {
  try { $e->$m(); } catch (RuntimeException $x) { echo $x->getMessage(), "\n"; }
}
try { $e->offsetGet(5); } catch (OutOfRangeException $x) { echo $x->getMessage(), "\n"; }
try { $e->offsetSet(3, "x"); } catch (OutOfRangeException $x) { echo $x->getMessage(), "\n"; }
try { $e->add(5, "x"); } catch (OutOfRangeException $x) { echo $x->getMessage(), "\n"; }
$s = new SplStack();
$s->push("a"); $s->push("b");
foreach ($s as $k => $v) echo "$k=$v "; echo "\n";
try { $s->setIteratorMode(SplDoublyLinkedList::IT_MODE_FIFO); } catch (RuntimeException $x) { echo $x->getMessage(), "\n"; }
$q = new SplQueue();
$q->enqueue("x"); $q->enqueue("y");
echo $q->dequeue(); foreach ($q as $k => $v) echo " $k=$v"; echo "\n";
try { (new SplQueue)->dequeue(); } catch (RuntimeException $x) { echo $x->getMessage(), "\n"; }
echo SplDoublyLinkedList::IT_MODE_LIFO, SplDoublyLinkedList::IT_MODE_FIFO, SplDoublyLinkedList::IT_MODE_DELETE, SplDoublyLinkedList::IT_MODE_KEEP, "\n";
$d2 = new SplDoublyLinkedList(); $d2->push(1); $d2->push(2);
$d2->setIteratorMode(SplDoublyLinkedList::IT_MODE_LIFO);
foreach ($d2 as $k => $v) echo "$k=$v "; echo "\n";
$d2->setIteratorMode(SplDoublyLinkedList::IT_MODE_FIFO | SplDoublyLinkedList::IT_MODE_DELETE);
foreach ($d2 as $k => $v) echo "$k=$v "; echo count($d2), "\n";
$d3 = new SplDoublyLinkedList(); $d3->push("a"); $d3->push("c"); $d3->add(1, "b");
foreach ($d3 as $v) echo $v; echo " ", $d3[1], isset($d3[2]) ? "y" : "n", isset($d3[9]) ? "y" : "n", "\n";
$d3[1] = "B"; $d3->offsetUnset(0); foreach ($d3 as $v) echo $v; echo "\n";
// heaps
$h = new SplMinHeap();
foreach ([5, 1, 3] as $x) $h->insert($x);
echo $h->top(), $h->extract(), $h->extract(), count($h), "\n";
foreach ($h as $k => $v) echo "$k=$v "; echo "|", count($h), "\n";
try { (new SplMinHeap)->extract(); } catch (RuntimeException $x) { echo $x->getMessage(), "\n"; }
try { (new SplMinHeap)->top(); } catch (RuntimeException $x) { echo $x->getMessage(), "\n"; }
$mx = new SplMaxHeap();
foreach ([5, 1, 3, 9, 7] as $x) $mx->insert($x);
foreach ($mx as $v) echo $v; echo "\n";
class SplTNegH extends SplHeap { protected function compare($a, $b): int { return $a <=> $b; } }
$nh = new SplTNegH(); foreach ([2, 9, 4] as $x) $nh->insert($x);
foreach ($nh as $v) echo $v, " "; echo "\n";
// priority queue
$pq = new SplPriorityQueue();
$pq->insert("low", 1); $pq->insert("high", 9); $pq->insert("mid", 5);
echo $pq->top(), " ", count($pq), "\n";
foreach ($pq as $k => $v) echo "$k=$v "; echo "|", count($pq), "\n";
$pq2 = new SplPriorityQueue();
$pq2->setExtractFlags(SplPriorityQueue::EXTR_BOTH);
$pq2->insert("a", 3);
print_r($pq2->top()); print_r($pq2->extract());
$pq3 = new SplPriorityQueue();
$pq3->insert("first", 5); $pq3->insert("second", 5); $pq3->insert("third", 5);
echo $pq3->extract(), " ", $pq3->extract(), " ", $pq3->extract(), "\n";
try { (new SplPriorityQueue)->extract(); } catch (RuntimeException $x) { echo $x->getMessage(), "\n"; }
echo SplPriorityQueue::EXTR_DATA, SplPriorityQueue::EXTR_PRIORITY, SplPriorityQueue::EXTR_BOTH, "\n";
// fixed array
$f = new SplFixedArray(3);
$f[0] = "a"; $f[2] = "c";
echo count($f), $f->getSize(), var_export($f[1], true), "\n";
foreach ($f as $k => $v) echo $k, "=", var_export($v, true), " "; echo "\n";
try { $f[5] = 1; } catch (OutOfBoundsException $x) { echo $x->getMessage(), "\n"; }
try { echo $f[-1]; } catch (OutOfBoundsException $x) { echo $x->getMessage(), "\n"; }
try { $f["x"] = 1; } catch (TypeError $x) { echo $x->getMessage(), "\n"; }
$f2 = new SplFixedArray(2); $f2[0] = null; $f2[1] = 5;
echo isset($f2[0]) ? "y" : "n", isset($f2[1]) ? "y" : "n", isset($f2[5]) ? "y" : "n", "\n";
$f->setSize(2); print_r($f->toArray());
echo json_encode(SplFixedArray::fromArray([1, null, "x"])), "\n";
print_r(SplFixedArray::fromArray([3 => "c", 1 => "a"])->toArray());
print_r(SplFixedArray::fromArray([3 => "c", 1 => "a"], false)->toArray());
try { SplFixedArray::fromArray(["k" => 1]); } catch (InvalidArgumentException $x) { echo $x->getMessage(), "\n"; }
// object storage
$os = new SplObjectStorage();
$a = new stdClass; $b = new stdClass;
$os[$a] = "infoA"; $os[$b] = null;
echo count($os), isset($os[$a]) ? "y" : "n", "\n";
echo $os[$a], "\n";
$os[$b] = "infoB"; echo $os->offsetGet($b), "\n";
foreach ($os as $i => $obj) echo $i, ":", $os->getInfo() ?? "-", " "; echo "\n";
unset($os[$a]); echo count($os), "\n";
try { $os[$a]; } catch (UnexpectedValueException $x) { echo $x->getMessage(), "\n"; }
echo $os->getHash($b) === spl_object_hash($b) ? "H" : "-", "\n";
$os2 = new SplObjectStorage(); $os2[$a] = "back";
$os->addAll($os2); echo count($os), $os[$a], "\n";
echo interface_exists("SplObserver"), interface_exists("SplSubject"), "\n";
?>
--EXPECT--
0=0 1=1 2=2 | 203
201N
Can't pop from an empty datastructure
Can't shift from an empty datastructure
Can't peek at an empty datastructure
Can't peek at an empty datastructure
SplDoublyLinkedList::offsetGet(): Argument #1 ($index) is out of range
SplDoublyLinkedList::offsetSet(): Argument #1 ($index) is out of range
SplDoublyLinkedList::add(): Argument #1 ($index) is out of range
1=b 0=a 
Iterators' LIFO/FIFO modes for SplStack/SplQueue objects are frozen
x 0=y
Can't shift from an empty datastructure
2010
1=2 0=1 
0=1 0=2 0
abc byn
Bc
1131
0=5 |0
Can't extract from an empty heap
Can't peek at an empty heap
97531
9 4 2 
high 3
2=high 1=mid 0=low |0
Array
(
    [data] => a
    [priority] => 3
)
Array
(
    [data] => a
    [priority] => 3
)
first third second
Can't extract from an empty heap
123
33NULL
0='a' 1=NULL 2='c' 
Index invalid or out of range
Index invalid or out of range
Cannot access offset of type string on SplFixedArray
nyn
Array
(
    [0] => a
    [1] => 
)
[1,null,"x"]
Array
(
    [0] => 
    [1] => a
    [2] => 
    [3] => c
)
Array
(
    [0] => c
    [1] => a
)
array must contain only positive integer keys
2y
infoA
infoB
0:infoA 1:infoB 
1
Object not found
H
2back
11
--CLEAN--
<?php
unset($d, $e, $s, $q, $d2, $d3, $h, $mx, $nh, $pq, $pq2, $pq3, $f, $f2, $os, $os2, $a, $b, $k, $v, $m, $x, $i, $obj);
