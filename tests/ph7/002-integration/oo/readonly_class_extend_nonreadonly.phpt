--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class inheritance: a non-readonly class cannot extend a readonly class
--FILE--
<?php
readonly class RoBase {}
class PlainChild extends RoBase {}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Non-readonly class PlainChild cannot extend readonly class RoBase%A
--CLEAN--
<?php
