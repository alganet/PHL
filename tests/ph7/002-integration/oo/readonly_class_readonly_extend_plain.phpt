--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class inheritance: a readonly class cannot extend a non-readonly class
--FILE--
<?php
class PlainBase {}
readonly class RoChild extends PlainBase {}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Readonly class RoChild cannot extend non-readonly class PlainBase%A
--CLEAN--
<?php
