--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge_recursive() stops on a cycle, reads an object as its properties, and carries a reference
--FILE--
<?php
// A container that is its own descendant used to recurse until the allocator
// gave out -- the embedded-PHP implementation had no recursion guard at all.
// php marks the destination table it descends into and refuses.
$amrA = ['k' => [1]];
$amrA['k']['self'] = &$amrA;
try {
    array_merge_recursive($amrA, $amrA);
    echo "NO THROW\n";
} catch (Error $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}

// A SHARED reference that is not a cycle still merges normally.
$amrV = ['x' => 1];
$amrB = ['k' => &$amrV];
var_dump(array_merge_recursive($amrB, ['k' => ['x' => 2]]));

// An OBJECT is read as its property array on the source side; the object
// itself is not modified.
class AmrHolder { public $p = 7; public $q = 8; }
$amrO = new AmrHolder;
var_dump(array_merge_recursive(['k' => [1]], ['k' => $amrO]));
var_dump($amrO->p);
// ... and on the destination side, where it becomes the array it merges into.
var_dump(array_merge_recursive(['k' => $amrO], ['k' => 2]));

// A referenced element crosses into the result as a reference.
$amrR = 5;
$amrC = ['k' => &$amrR];
$amrOut = array_merge_recursive($amrC, ['j' => 1]);
$amrR = 6;
var_dump($amrOut);

// php's null rule: the destination becomes [null] before the merge, so the
// null is KEPT rather than replaced.
var_dump(array_merge_recursive(['k' => null], ['k' => 2]));
var_dump(array_merge_recursive(['k' => false], ['k' => 2]));

// Every argument is screened before anything is merged.
foreach ([[1, []], [[], "s"], [[], [], null], [[], true]] as $amrBad) {
    try {
        array_merge_recursive(...$amrBad);
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
var_dump(array_merge_recursive());
?>
--EXPECT--
Error: Recursion detected
array(1) {
  ["k"]=>
  array(1) {
    ["x"]=>
    array(2) {
      [0]=>
      int(1)
      [1]=>
      int(2)
    }
  }
}
array(1) {
  ["k"]=>
  array(3) {
    [0]=>
    int(1)
    ["p"]=>
    int(7)
    ["q"]=>
    int(8)
  }
}
int(7)
array(1) {
  ["k"]=>
  array(3) {
    ["p"]=>
    int(7)
    ["q"]=>
    int(8)
    [0]=>
    int(2)
  }
}
array(2) {
  ["k"]=>
  &int(6)
  ["j"]=>
  int(1)
}
array(1) {
  ["k"]=>
  array(2) {
    [0]=>
    NULL
    [1]=>
    int(2)
  }
}
array(1) {
  ["k"]=>
  array(2) {
    [0]=>
    bool(false)
    [1]=>
    int(2)
  }
}
array_merge_recursive(): Argument #1 must be of type array, int given
array_merge_recursive(): Argument #2 must be of type array, string given
array_merge_recursive(): Argument #3 must be of type array, null given
array_merge_recursive(): Argument #2 must be of type array, true given
array(0) {
}
--CLEAN--
<?php
