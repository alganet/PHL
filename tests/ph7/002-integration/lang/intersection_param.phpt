--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Intersection type A&B on a parameter accepts an object implementing both, rejects otherwise
--FILE--
<?php
interface IxCountable {}
interface IxStringy {}
class IxBoth implements IxCountable, IxStringy {}
class IxOnlyOne implements IxCountable {}
function ix_need(IxCountable&IxStringy $x): string { return get_class($x); }
echo ix_need(new IxBoth), "\n";
try {
    ix_need(new IxOnlyOne);
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECTF--
IxBoth
ix_need(): Argument #1 ($x) must be of type IxCountable&IxStringy, IxOnlyOne given%A
--CLEAN--
<?php
