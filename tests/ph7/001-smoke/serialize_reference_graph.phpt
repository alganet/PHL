--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
serialize() writes a value's identity once and back-references it after that
--DESCRIPTION--
php's serializer counts the VALUES it writes -- keys take no number, and neither does a
repeated reference -- and a second sighting of the same object writes `r:<n>;` while a
second sighting of the same reference writes `R:<n>;`. That count is the whole contract:
it is what makes two variables holding one object still hold one object after a round
trip, what lets a cyclic value be written at all, and what the reader has to reproduce
exactly, because a number that drifts by one resolves to the wrong value rather than
failing.
PHL wrote neither tag: a shared object came back as two objects, a shared reference came
back as two independent copies, and a cyclic array hit the depth guard and answered false.
An object's property values are counted too, which is where an arithmetic that looks right
goes wrong -- `['d' => $o, 'e' => $o, 'a' => &$i, 'b' => &$i]` numbers `$i` at 5, not 4,
because $o's own property took 3.
The reader's own rules are here too: a well-formed token naming a number it cannot use is
blamed at the byte AFTER the token (php advances its cursor before it looks at the number),
a token whose SHAPE did not match keeps its own start, and a bind naming the very slot it
is landing in -- what a duplicate key asks for -- is refused rather than quietly accepted.
--FILE--
<?php
class SerGraphNode { public $v = 1; public $next; }
class SerGraphPair { public $a; public $b; }

function serGraphShow($label, $value) {
    echo str_pad($label, 26), ' ', serialize($value), "\n";
}

echo "-- an object is written once, and named after that\n";
$serGraphObj = new SerGraphNode;
serGraphShow('two elements', [$serGraphObj, $serGraphObj]);
serGraphShow('a value between', [$serGraphObj, 5, $serGraphObj]);
serGraphShow('one nested deeper', ['k' => ['j' => $serGraphObj], 'm' => $serGraphObj]);
$serGraphPair = new SerGraphPair;
$serGraphPair->a = $serGraphObj;
$serGraphPair->b = $serGraphObj;
serGraphShow('two properties', $serGraphPair);

echo "\n-- so a cycle is a back-reference, not a refusal\n";
$serGraphSelf = new SerGraphPair;
$serGraphSelf->a = $serGraphSelf;
$serGraphSelf->b = 3;
serGraphShow('object holding itself', $serGraphSelf);
$serGraphLeft = new SerGraphPair;
$serGraphRight = new SerGraphPair;
$serGraphLeft->a = $serGraphRight;
$serGraphLeft->b = $serGraphRight;
$serGraphRight->a = $serGraphLeft;
serGraphShow('two objects, both ways', $serGraphLeft);
$serGraphCyc = [1, 2];
$serGraphCyc[] = &$serGraphCyc;
serGraphShow('array holding itself', $serGraphCyc);

echo "\n-- a shared REFERENCE is R:, and it takes no number of its own\n";
$serGraphInt = 1;
serGraphShow('two names, one int', ['u' => &$serGraphInt, 'v' => &$serGraphInt]);
serGraphShow('a value between', ['u' => &$serGraphInt, 'w' => 7, 'v' => &$serGraphInt]);
serGraphShow('a copy is not shared', ['a' => &$serGraphInt, 'b' => &$serGraphInt, 'c' => $serGraphInt]);
$serGraphArr = [1, 2];
serGraphShow('two names, one array', ['x' => &$serGraphArr, 'y' => &$serGraphArr]);
$serGraphProps = new SerGraphPair;
$serGraphProps->a = &$serGraphInt;
$serGraphProps->b = &$serGraphInt;
serGraphShow('two properties', $serGraphProps);
$serGraphAlias = &$serGraphObj;
serGraphShow('a reference to an object', ['p' => $serGraphObj, 'q' => &$serGraphAlias]);
$serGraphMixed = new SerGraphNode;
serGraphShow('the counter stays in step', [
    'd' => $serGraphMixed, 'e' => $serGraphMixed,
    'a' => &$serGraphInt, 'b' => &$serGraphInt,
    'z' => new SerGraphNode, 'y' => null,
]);

echo "\n-- and the graph survives the round trip\n";
$serGraphBack = unserialize(serialize([$serGraphObj, $serGraphObj]));
var_dump($serGraphBack[0] === $serGraphBack[1]);
$serGraphBack = unserialize(serialize($serGraphSelf));
var_dump($serGraphBack === $serGraphBack->a);
$serGraphBack = unserialize(serialize(['u' => &$serGraphInt, 'v' => &$serGraphInt]));
$serGraphBack['u'] = 99;
var_dump($serGraphBack['v']);
$serGraphBack = unserialize(serialize($serGraphProps));
$serGraphBack->a = 42;
var_dump($serGraphBack->b);
$serGraphBack = unserialize(serialize($serGraphCyc));
var_dump($serGraphBack[2][2][2][0], count($serGraphBack));
echo serialize(unserialize(serialize($serGraphLeft))), "\n";

echo "\n-- what the reader refuses, and where it says the payload went wrong\n";
set_error_handler(function ($no, $msg) {
    if (error_reporting() & $no) { echo '   ', $msg, "\n"; }
    return true;
});
foreach ([
    'r: naming no object'   => 'a:2:{i:0;i:5;i:1;r:2;}',
    'r: naming itself'      => 'r:1;',
    'R: naming itself'      => 'R:1;',
    'R: past the end'       => 'a:1:{i:0;R:5;}',
    'R: at zero'            => 'a:1:{i:0;R:0;}',
    'R: not yet written'    => 'a:1:{i:0;R:2;}',
    'R: out of range'       => 'a:1:{i:0;R:99999999999999999999;}',
    'r: with no digits'     => 'a:1:{i:0;r:x;}',
    'R: with no digits'     => 'a:1:{i:0;R:x;}',
    'R: signed'             => 'a:1:{i:0;R:-1;}',
    'R: unterminated'       => 'a:1:{i:0;R:2',
    'R: an element to itself' => 'a:2:{s:1:"k";i:1;s:1:"k";R:2;}',
    'R: a property to itself' => 'O:8:"stdClass":2:{s:1:"p";i:1;s:1:"p";R:2;}',
] as $serGraphLabel => $serGraphPayload) {
    echo str_pad($serGraphLabel, 24), ' ', var_export(unserialize($serGraphPayload), true), "\n";
}
restore_error_handler();

echo "\n-- a duplicate key naming SOMEBODY ELSE's slot is fine\n";
echo serialize(unserialize('a:3:{i:0;i:1;i:1;i:2;i:1;R:2;}')), "\n";
?>
--EXPECT--
-- an object is written once, and named after that
two elements               a:2:{i:0;O:12:"SerGraphNode":2:{s:1:"v";i:1;s:4:"next";N;}i:1;r:2;}
a value between            a:3:{i:0;O:12:"SerGraphNode":2:{s:1:"v";i:1;s:4:"next";N;}i:1;i:5;i:2;r:2;}
one nested deeper          a:2:{s:1:"k";a:1:{s:1:"j";O:12:"SerGraphNode":2:{s:1:"v";i:1;s:4:"next";N;}}s:1:"m";r:3;}
two properties             O:12:"SerGraphPair":2:{s:1:"a";O:12:"SerGraphNode":2:{s:1:"v";i:1;s:4:"next";N;}s:1:"b";r:2;}

-- so a cycle is a back-reference, not a refusal
object holding itself      O:12:"SerGraphPair":2:{s:1:"a";r:1;s:1:"b";i:3;}
two objects, both ways     O:12:"SerGraphPair":2:{s:1:"a";O:12:"SerGraphPair":2:{s:1:"a";r:1;s:1:"b";N;}s:1:"b";r:2;}
array holding itself       a:3:{i:0;i:1;i:1;i:2;i:2;a:3:{i:0;i:1;i:1;i:2;i:2;R:4;}}

-- a shared REFERENCE is R:, and it takes no number of its own
two names, one int         a:2:{s:1:"u";i:1;s:1:"v";R:2;}
a value between            a:3:{s:1:"u";i:1;s:1:"w";i:7;s:1:"v";R:2;}
a copy is not shared       a:3:{s:1:"a";i:1;s:1:"b";R:2;s:1:"c";i:1;}
two names, one array       a:2:{s:1:"x";a:2:{i:0;i:1;i:1;i:2;}s:1:"y";R:2;}
two properties             O:12:"SerGraphPair":2:{s:1:"a";i:1;s:1:"b";R:2;}
a reference to an object   a:2:{s:1:"p";O:12:"SerGraphNode":2:{s:1:"v";i:1;s:4:"next";N;}s:1:"q";R:2;}
the counter stays in step  a:6:{s:1:"d";O:12:"SerGraphNode":2:{s:1:"v";i:1;s:4:"next";N;}s:1:"e";r:2;s:1:"a";i:1;s:1:"b";R:6;s:1:"z";O:12:"SerGraphNode":2:{s:1:"v";i:1;s:4:"next";N;}s:1:"y";N;}

-- and the graph survives the round trip
bool(true)
bool(true)
int(99)
int(42)
int(1)
int(3)
O:12:"SerGraphPair":2:{s:1:"a";O:12:"SerGraphPair":2:{s:1:"a";r:1;s:1:"b";N;}s:1:"b";r:2;}

-- what the reader refuses, and where it says the payload went wrong
r: naming no object         unserialize(): Error at offset 21 of 22 bytes
false
r: naming itself            unserialize(): Error at offset 4 of 4 bytes
false
R: naming itself            unserialize(): Error at offset 4 of 4 bytes
false
R: past the end             unserialize(): Error at offset 13 of 14 bytes
false
R: at zero                  unserialize(): Error at offset 13 of 14 bytes
false
R: not yet written          unserialize(): Error at offset 13 of 14 bytes
false
R: out of range             unserialize(): Error at offset 32 of 33 bytes
false
r: with no digits           unserialize(): Error at offset 9 of 14 bytes
false
R: with no digits           unserialize(): Error at offset 9 of 14 bytes
false
R: signed                   unserialize(): Error at offset 9 of 15 bytes
false
R: unterminated             unserialize(): Error at offset 9 of 12 bytes
false
R: an element to itself     unserialize(): Error at offset 29 of 30 bytes
false
R: a property to itself     unserialize(): Error at offset 42 of 43 bytes
false

-- a duplicate key naming SOMEBODY ELSE's slot is fine
a:2:{i:0;i:1;i:1;R:2;}
