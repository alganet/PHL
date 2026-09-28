--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A class import may not occupy a reserved class name
--DESCRIPTION--
Letting a reserved word be a name SEGMENT means the trailing segment can now land on one of
php's reserved class names, which it refuses -- and it refused the ID-spelled ones
(`use A\null;`) before that too, where PHL silently registered the import. Only CLASS imports
are screened: `use function A\self;` and `use const A\self;` are both accepted by php.
--FILE--
<?php
namespace UrcnaZ;
use UrcnaA\self;
?>
--EXPECTF--
%s Fatal error:  Cannot use UrcnaA\self as self because 'self' is a special class name%A
--CLEAN--
<?php
