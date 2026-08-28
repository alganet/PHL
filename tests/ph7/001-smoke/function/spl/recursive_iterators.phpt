--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: RecursiveArrayIterator + RecursiveIteratorIterator (band D SPL slice 3)
--FILE--
<?php
$a = ["x" => 1, "sub" => ["y" => 2, "deep" => ["z" => 3]], "w" => 4];
foreach (new RecursiveIteratorIterator(new RecursiveArrayIterator($a)) as $k => $v) echo "$k=$v "; echo "\n";
$rii = new RecursiveIteratorIterator(new RecursiveArrayIterator($a), RecursiveIteratorIterator::SELF_FIRST);
foreach ($rii as $k => $v) { echo "$k@", $rii->getDepth(), is_array($v) ? "[A]" : "=$v", " "; } echo "\n";
$rii2 = new RecursiveIteratorIterator(new RecursiveArrayIterator($a), RecursiveIteratorIterator::CHILD_FIRST);
foreach ($rii2 as $k => $v) { echo "$k@", $rii2->getDepth(), is_array($v) ? "[A]" : "=$v", " "; } echo "\n";
echo RecursiveIteratorIterator::LEAVES_ONLY, RecursiveIteratorIterator::SELF_FIRST, RecursiveIteratorIterator::CHILD_FIRST, RecursiveIteratorIterator::CATCH_GET_CHILD, "\n";
$rai2 = new RecursiveArrayIterator(["a" => [1], "b" => 2]);
$rai2->rewind();
var_export($rai2->hasChildren()); echo get_class($rai2->getChildren()), "\n";
$rai2->next();
var_export($rai2->hasChildren()); echo "\n";
try { $rai2->getChildren(); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
$r3 = new RecursiveIteratorIterator(new RecursiveArrayIterator($a), RecursiveIteratorIterator::SELF_FIRST);
$r3->setMaxDepth(0);
foreach ($r3 as $k => $v) echo "$k "; echo "| ", var_export($r3->getMaxDepth(), true), "\n";
echo get_class($r3->getSubIterator()), var_export($r3->getSubIterator(5), true), "\n";
$o = new stdClass; $o->p = 1;
$rai3 = new RecursiveArrayIterator(["obj" => $o]);
$rai3->rewind(); var_export($rai3->hasChildren()); echo "\n";
var_export($rai3 instanceof RecursiveIterator && $rai3 instanceof ArrayIterator); echo "\n";
var_export($rii instanceof OuterIterator); echo "\n";
// re-iteration + empty arrays + nested empties
$b = ["e" => [], "f" => ["g" => []], "h" => 5];
foreach (new RecursiveIteratorIterator(new RecursiveArrayIterator($b)) as $k => $v) echo "$k=$v "; echo "\n";
$rb = new RecursiveIteratorIterator(new RecursiveArrayIterator($b), RecursiveIteratorIterator::SELF_FIRST);
foreach ($rb as $k => $v) echo "$k "; echo "\n";
$rc = new RecursiveIteratorIterator(new RecursiveArrayIterator($b), RecursiveIteratorIterator::CHILD_FIRST);
foreach ($rc as $k => $v) echo "$k "; echo "\n";
// iterator_to_array over RII
print_r(iterator_to_array(new RecursiveIteratorIterator(new RecursiveArrayIterator(["p" => ["q" => 7]])), false));
?>
--EXPECT--
x=1 y=2 z=3 w=4 
x@0=1 sub@0[A] y@1=2 deep@1[A] z@2=3 w@0=4 
x@0=1 y@1=2 z@2=3 deep@1[A] sub@0[A] w@0=4 
01216
trueRecursiveArrayIterator
false
ArrayIterator::__construct(): Argument #1 ($array) must be of type array, int given
x sub w | 0
RecursiveArrayIteratorNULL
true
true
true
h=5 
e f g h 
e g f h 
Array
(
    [0] => 7
)
--CLEAN--
<?php
unset($a, $rii, $rii2, $rai2, $r3, $o, $rai3, $b, $rb, $rc, $k, $v);
