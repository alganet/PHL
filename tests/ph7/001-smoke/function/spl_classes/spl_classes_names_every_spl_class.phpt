--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_classes names each SPL class as both key and value
--FILE--
<?php
$splAll = spl_classes();
var_dump(count($splAll));
var_dump($splAll['ArrayObject'], $splAll['SplSubject'], $splAll['GlobIterator']);
// Core's own names are not SPL's, and every name answered is declared here.
var_dump(isset($splAll['Traversable']), isset($splAll['Countable']), isset($splAll['Exception']));
$splMissing = [];
foreach ($splAll as $splKey => $splName) {
    if ($splKey !== $splName) { $splMissing[] = "key:$splKey"; }
    if (!class_exists($splName, false) && !interface_exists($splName, false)) { $splMissing[] = $splName; }
}
print_r($splMissing);
$splSorted = array_keys($splAll);
$splCopy = $splSorted;
sort($splCopy, SORT_STRING);
var_dump($splSorted === $splCopy);
?>
--EXPECT--
int(55)
string(11) "ArrayObject"
string(10) "SplSubject"
string(12) "GlobIterator"
bool(false)
bool(false)
bool(false)
Array
(
)
bool(false)
--CLEAN--
<?php
unset($splAll, $splMissing, $splKey, $splName, $splSorted, $splCopy);
