--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_subclass_of checks inheritance
--SKIPIF--
<?php
if (!function_exists('is_subclass_of')) { echo 'skip: is_subclass_of not available'; }
?>
--FILE--
<?php
class ParentClass {}
class ChildClass extends ParentClass {}
class UnrelatedClass {}
$child = new ChildClass();
$result = is_subclass_of($child, 'ParentClass');
if ($result) { echo "subclass_ok\n"; } else { echo "subclass_failed\n"; }
$result = is_subclass_of($child, 'DefinitelyNonExistentClass12345');
if ($result) { echo "not_subclass_failed\n"; } else { echo "not_subclass_ok\n"; }
?>
--EXPECT--
subclass_ok
not_subclass_ok
