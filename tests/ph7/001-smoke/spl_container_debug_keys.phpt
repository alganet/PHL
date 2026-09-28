--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An SPL container's debug keys are php's MANGLED private names
--DESCRIPTION--
php keeps SplDoublyLinkedList's and SplHeap's state in a C struct and shows it through
get_debug_info alone: the (array) cast and get_mangled_object_vars() are empty, and the
two or three keys print/dump does show are php's mangled private names — the class that
DECLARED the slot, which for SplStack, SplQueue and any userland subclass is still the
base's. So php prints `[flags:SplDoublyLinkedList:private]` and
`[flags:SplHeap:private]` where PHL printed a plain `[flags]`, and `__debugInfo()` —
which php declares, so a program can read the same array by name — carried the plain
names with it. SplPriorityQueue extends nothing, so its own name is the one it shows.
--FILE--
<?php
// php's SPL containers keep their state in a C struct and SHOW it through
// get_debug_info alone: the (array) cast and the raw table are empty, and the two
// or three keys the dump prints are php's MANGLED private names -- the class that
// DECLARED the slot, which for a subclass is still the base's.
$dll = new SplDoublyLinkedList(); $dll->push('a');
$stack = new SplStack(); $stack->push('a');
$queue = new SplQueue(); $queue->enqueue('a');
$min = new SplMinHeap(); $min->insert(3); $min->insert(1);
$pq = new SplPriorityQueue(); $pq->insert('x', 1);
class VkStack extends SplStack {}
$sub = new VkStack(); $sub->push('a');
class VkHeap extends SplMinHeap {}
$subh = new VkHeap(); $subh->insert(2);
foreach (['SplDoublyLinkedList' => $dll, 'SplStack' => $stack, 'SplQueue' => $queue,
          'SplMinHeap' => $min, 'SplPriorityQueue' => $pq,
          'VkStack' => $sub, 'VkHeap' => $subh] as $label => $o) {
    echo $label, "\n";
    echo '  cast=', json_encode(array_keys((array) $o)),
         ' raw=', json_encode(array_keys(get_mangled_object_vars($o))),
         ' debugInfo=', json_encode(array_keys($o->__debugInfo())), "\n";
    print_r($o);
    ob_start(); var_dump($o); echo preg_replace('/#\d+ \(/', '#N (', ob_get_clean());
}
--EXPECT--
SplDoublyLinkedList
  cast=[] raw=[] debugInfo=["\u0000SplDoublyLinkedList\u0000flags","\u0000SplDoublyLinkedList\u0000dllist"]
SplDoublyLinkedList Object
(
    [flags:SplDoublyLinkedList:private] => 0
    [dllist:SplDoublyLinkedList:private] => Array
        (
            [0] => a
        )

)
object(SplDoublyLinkedList)#N (2) {
  ["flags":"SplDoublyLinkedList":private]=>
  int(0)
  ["dllist":"SplDoublyLinkedList":private]=>
  array(1) {
    [0]=>
    string(1) "a"
  }
}
SplStack
  cast=[] raw=[] debugInfo=["\u0000SplDoublyLinkedList\u0000flags","\u0000SplDoublyLinkedList\u0000dllist"]
SplStack Object
(
    [flags:SplDoublyLinkedList:private] => 6
    [dllist:SplDoublyLinkedList:private] => Array
        (
            [0] => a
        )

)
object(SplStack)#N (2) {
  ["flags":"SplDoublyLinkedList":private]=>
  int(6)
  ["dllist":"SplDoublyLinkedList":private]=>
  array(1) {
    [0]=>
    string(1) "a"
  }
}
SplQueue
  cast=[] raw=[] debugInfo=["\u0000SplDoublyLinkedList\u0000flags","\u0000SplDoublyLinkedList\u0000dllist"]
SplQueue Object
(
    [flags:SplDoublyLinkedList:private] => 4
    [dllist:SplDoublyLinkedList:private] => Array
        (
            [0] => a
        )

)
object(SplQueue)#N (2) {
  ["flags":"SplDoublyLinkedList":private]=>
  int(4)
  ["dllist":"SplDoublyLinkedList":private]=>
  array(1) {
    [0]=>
    string(1) "a"
  }
}
SplMinHeap
  cast=[] raw=[] debugInfo=["\u0000SplHeap\u0000flags","\u0000SplHeap\u0000isCorrupted","\u0000SplHeap\u0000heap"]
SplMinHeap Object
(
    [flags:SplHeap:private] => 0
    [isCorrupted:SplHeap:private] => 
    [heap:SplHeap:private] => Array
        (
            [0] => 1
            [1] => 3
        )

)
object(SplMinHeap)#N (3) {
  ["flags":"SplHeap":private]=>
  int(0)
  ["isCorrupted":"SplHeap":private]=>
  bool(false)
  ["heap":"SplHeap":private]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(3)
  }
}
SplPriorityQueue
  cast=[] raw=[] debugInfo=["\u0000SplPriorityQueue\u0000flags","\u0000SplPriorityQueue\u0000isCorrupted","\u0000SplPriorityQueue\u0000heap"]
SplPriorityQueue Object
(
    [flags:SplPriorityQueue:private] => 1
    [isCorrupted:SplPriorityQueue:private] => 
    [heap:SplPriorityQueue:private] => Array
        (
            [0] => Array
                (
                    [data] => x
                    [priority] => 1
                )

        )

)
object(SplPriorityQueue)#N (3) {
  ["flags":"SplPriorityQueue":private]=>
  int(1)
  ["isCorrupted":"SplPriorityQueue":private]=>
  bool(false)
  ["heap":"SplPriorityQueue":private]=>
  array(1) {
    [0]=>
    array(2) {
      ["data"]=>
      string(1) "x"
      ["priority"]=>
      int(1)
    }
  }
}
VkStack
  cast=[] raw=[] debugInfo=["\u0000SplDoublyLinkedList\u0000flags","\u0000SplDoublyLinkedList\u0000dllist"]
VkStack Object
(
    [flags:SplDoublyLinkedList:private] => 6
    [dllist:SplDoublyLinkedList:private] => Array
        (
            [0] => a
        )

)
object(VkStack)#N (2) {
  ["flags":"SplDoublyLinkedList":private]=>
  int(6)
  ["dllist":"SplDoublyLinkedList":private]=>
  array(1) {
    [0]=>
    string(1) "a"
  }
}
VkHeap
  cast=[] raw=[] debugInfo=["\u0000SplHeap\u0000flags","\u0000SplHeap\u0000isCorrupted","\u0000SplHeap\u0000heap"]
VkHeap Object
(
    [flags:SplHeap:private] => 0
    [isCorrupted:SplHeap:private] => 
    [heap:SplHeap:private] => Array
        (
            [0] => 2
        )

)
object(VkHeap)#N (3) {
  ["flags":"SplHeap":private]=>
  int(0)
  ["isCorrupted":"SplHeap":private]=>
  bool(false)
  ["heap":"SplHeap":private]=>
  array(1) {
    [0]=>
    int(2)
  }
}
