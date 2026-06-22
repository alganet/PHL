--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
readonly class: an untyped promoted constructor property is a fatal (must have type)
--FILE--
<?php
readonly class RoClassPromotedUntyped {
    public function __construct(public $x) {}
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Readonly property RoClassPromotedUntyped::$x must have type%A
--CLEAN--
<?php
