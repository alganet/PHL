--TEST--
RecursiveFilterIterator: flat accept, RecursiveIterator interface, and recursive descent via getChildren
--FILE--
<?php
class RfiEven extends RecursiveFilterIterator {
    public function accept(): bool {
        $c = $this->getInnerIterator()->current();
        if (is_array($c)) { return true; } // keep subtrees so the walk can descend
        return ($c % 2) === 0;
    }
}
// Flat filtering keeps only the even leaves
$rfiTree = new RecursiveArrayIterator([1, 2, 3, 4]);
$rfiIt = new RfiEven($rfiTree);
$rfiOut = [];
foreach ($rfiIt as $k => $v) { $rfiOut[] = "$k=$v"; }
echo implode(',', $rfiOut), "\n";
// It is a RecursiveIterator
var_dump($rfiIt instanceof RecursiveIterator);
// Recursive descent (RecursiveIteratorIterator drives getChildren()/hasChildren())
$rfiT2 = new RecursiveArrayIterator([1, 2, [3, 4, [6, 7]], 8]);
$rfiRii = new RecursiveIteratorIterator(new RfiEven($rfiT2));
$rfiFlat = [];
foreach ($rfiRii as $v) { $rfiFlat[] = $v; }
echo implode(',', $rfiFlat), "\n";
--EXPECT--
1=2,3=4
bool(true)
2,4,6,8
