--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strnatcmp() is case-SENSITIVE (and natsort() with it)
--DESCRIPTION--
strnatcmp() and strnatcasecmp() share one implementation picked by the called name,
and the test for the fold flag looked at the character both names carry ('strnat|c|mp'
as much as 'strnat|c|asecmp'), so the case-sensitive function folded too: 'Hello' and
'hello' compared EQUAL. natsort(), natcasesort() and ArrayObject::natsort() are prelude
wrappers over these two, so natsort() ordered case-insensitively as well.
--FILE--
<?php
$pairs = [['Hello', 'hello'], ['hello', 'Hello'], ['Hello', 'Hello'],
          ['IMG10.png', 'img2.png'], ['img10.png', 'IMG2.png'],
          ['a10', 'A9'], ['A10', 'a9'], ['Z', 'a'], ['img12', 'img12']];
foreach ($pairs as [$a, $b]) {
    printf("%-10s %-10s cmp=%2d casecmp=%2d\n", $a, $b, strnatcmp($a, $b), strnatcasecmp($a, $b));
}

$files = ['IMG10.png', 'img2.png', 'IMG1.png', 'img12.png', 'IMG2.png'];
$s = $files; natsort($s);     echo 'natsort=', json_encode(array_values($s)), "\n";
$s = $files; natcasesort($s); echo 'natcasesort=', json_encode(array_values($s)), "\n";
$s = $files; usort($s, 'strnatcmp'); echo 'usort=', json_encode($s), "\n";

// SORT_NATURAL and its FLAG_CASE variant ride the same comparison core.
$s = $files; sort($s, SORT_NATURAL);
echo 'sort_natural=', json_encode($s), "\n";
$s = $files; sort($s, SORT_NATURAL | SORT_FLAG_CASE);
echo 'sort_natural_case=', json_encode($s), "\n";

$ao = new ArrayObject($files);
$ao->natsort();
echo 'ArrayObject=', json_encode(array_values($ao->getArrayCopy())), "\n";
?>
--EXPECT--
Hello      hello      cmp=-1 casecmp= 0
hello      Hello      cmp= 1 casecmp= 0
Hello      Hello      cmp= 0 casecmp= 0
IMG10.png  img2.png   cmp=-1 casecmp= 1
img10.png  IMG2.png   cmp= 1 casecmp= 1
a10        A9         cmp= 1 casecmp= 1
A10        a9         cmp=-1 casecmp= 1
Z          a          cmp=-1 casecmp= 1
img12      img12      cmp= 0 casecmp= 0
natsort=["IMG1.png","IMG2.png","IMG10.png","img2.png","img12.png"]
natcasesort=["IMG1.png","img2.png","IMG2.png","IMG10.png","img12.png"]
usort=["IMG1.png","IMG2.png","IMG10.png","img2.png","img12.png"]
sort_natural=["IMG1.png","IMG2.png","IMG10.png","img2.png","img12.png"]
sort_natural_case=["IMG1.png","img2.png","IMG2.png","IMG10.png","img12.png"]
ArrayObject=["IMG1.png","IMG2.png","IMG10.png","img2.png","img12.png"]
--CLEAN--
<?php
