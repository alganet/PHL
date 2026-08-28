--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: SPL decorator iterators (band D SPL slice 2)
--FILE--
<?php
// LimitIterator
$li = new LimitIterator(new ArrayIterator(["a","b","c","d","e"]), 1, 2);
foreach ($li as $k => $v) echo "$k=$v "; echo "| pos=", $li->getPosition(), "\n";
try { new LimitIterator(new ArrayIterator([1]), -1); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
try { new LimitIterator(new ArrayIterator([1]), 0, -2); } catch (ValueError $e) { echo $e->getMessage(), "\n"; }
$l2 = new LimitIterator(new ArrayIterator(["a","b","c","d"]), 1, 3);
$l2->seek(2); echo $l2->current(), $l2->getPosition(), "\n";
try { $l2->seek(0); } catch (OutOfBoundsException $e) { echo $e->getMessage(), "\n"; }
echo get_class($l2->getInnerIterator()), "\n";
// unlimited limit
$l3 = new LimitIterator(new ArrayIterator([1,2,3]), 1);
foreach ($l3 as $v) echo $v; echo "\n";
// CallbackFilterIterator
$cf = new CallbackFilterIterator(new ArrayIterator([1,2,3,4]), fn($v, $k, $it) => $v % 2 == 0);
foreach ($cf as $k => $v) echo "$k=$v "; echo "\n";
// AppendIterator
$ap = new AppendIterator();
$ap->append(new ArrayIterator(["x" => 1]));
$ap->append(new ArrayIterator(["y" => 2]));
$ap->append(new ArrayIterator([]));
$ap->append(new ArrayIterator(["z" => 3]));
foreach ($ap as $k => $v) echo "$k=$v(", $ap->getIteratorIndex(), ") "; echo "\n";
echo count($ap->getArrayIterator()), "\n";
$empty = new AppendIterator();
echo $empty->valid() ? "T" : "F", "\n";
// RegexIterator
$src = ["apple 1", "banana 22", "no"];
$ri = new RegexIterator(new ArrayIterator(["apple", "banana", "cherry"]), "/an/");
foreach ($ri as $v) echo $v, " "; echo "\n";
$r1 = new RegexIterator(new ArrayIterator($src), "/(\w+) (\d+)/", RegexIterator::GET_MATCH);
foreach ($r1 as $m) echo implode(",", $m), "|"; echo "\n";
$r2 = new RegexIterator(new ArrayIterator($src), "/(\d+)/", RegexIterator::REPLACE);
$r2->replacement = "N";
foreach ($r2 as $v) echo $v, "|"; echo "\n";
$r3 = new RegexIterator(new ArrayIterator(["a1" => "x", "bb" => "y"]), "/\d/", RegexIterator::MATCH, RegexIterator::USE_KEY);
foreach ($r3 as $k => $v) echo "$k=$v"; echo "\n";
$r4 = new RegexIterator(new ArrayIterator(["apple", "banana"]), "/an/", RegexIterator::MATCH, RegexIterator::INVERT_MATCH);
foreach ($r4 as $v) echo $v; echo "\n";
$r5 = new RegexIterator(new ArrayIterator(["a,b;c"]), "/[,;]/", RegexIterator::SPLIT);
foreach ($r5 as $v) print_r($v); echo "\n";
echo RegexIterator::MATCH, RegexIterator::GET_MATCH, RegexIterator::ALL_MATCHES, RegexIterator::SPLIT, RegexIterator::REPLACE, RegexIterator::USE_KEY, RegexIterator::INVERT_MATCH, "\n";
$r1->setMode(RegexIterator::SPLIT); echo $r1->getMode(), $r1->getRegex(), $r1->getFlags(), $r1->getPregFlags(), "\n";
// IteratorIterator / Infinite / NoRewind / Empty
foreach (new IteratorIterator(new ArrayIterator([5 => "q"])) as $k => $v) echo "$k=$v", "\n";
foreach (new IteratorIterator(new ArrayObject([7])) as $v) echo $v; echo "\n";
echo (new IteratorIterator(new ArrayIterator([]))) instanceof OuterIterator ? "T" : "F", "\n";
try { new IteratorIterator("x"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
$inf = new InfiniteIterator(new ArrayIterator([1,2]));
$n = 0; foreach ($inf as $v) { echo $v; if (++$n >= 5) break; } echo "\n";
$nr = new NoRewindIterator(new ArrayIterator([1,2,3]));
$nr->next(); foreach ($nr as $v) echo $v; echo "\n";
try { (new EmptyIterator)->current(); } catch (BadMethodCallException $e) { echo $e->getMessage(), "\n"; }
try { (new EmptyIterator)->key(); } catch (BadMethodCallException $e) { echo $e->getMessage(), "\n"; }
echo (new EmptyIterator)->valid() ? "T" : "F", "\n";
foreach (new EmptyIterator as $v) echo "never"; echo "ok\n";
// iterator_* over the decorators
echo iterator_count(new LimitIterator(new ArrayIterator([1,2,3,4]), 1)), "\n";
print_r(iterator_to_array(new CallbackFilterIterator(new ArrayIterator(["a"=>1,"b"=>2]), fn($v) => $v > 1)));
$cnt = 0;
echo iterator_apply(new ArrayIterator([1,2,3]), function() use (&$cnt) { $cnt++; return true; }), $cnt, "\n";
echo iterator_apply(new ArrayIterator([1,2,3]), fn() => false), "\n";
?>
--EXPECT--
1=b 2=c | pos=3
LimitIterator::__construct(): Argument #2 ($offset) must be greater than or equal to 0
LimitIterator::__construct(): Argument #3 ($limit) must be greater than or equal to -1
c2
Cannot seek to 0 which is below the offset 1
ArrayIterator
23
1=2 3=4 
x=1(0) y=2(1) z=3(3) 
4
F
banana 
apple 1,apple,1|banana 22,banana,22|
apple N|banana N|
a1=x
apple
Array
(
    [0] => a
    [1] => b
    [2] => c
)

0123412
3/(\w+) (\d+)/00
5=q
7
T
IteratorIterator::__construct(): Argument #1 ($iterator) must be of type Traversable, string given
12121
23
Accessing the value of an EmptyIterator
Accessing the key of an EmptyIterator
F
ok
3
Array
(
    [b] => 2
)
33
1
--CLEAN--
<?php
unset($li, $l2, $l3, $cf, $ap, $empty, $ri, $r1, $r2, $r3, $r4, $r5, $inf, $nr, $src, $cnt, $k, $v, $m, $n);
