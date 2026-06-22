--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A typed class constant cannot use a forbidden pseudo-type (callable)
--FILE--
<?php
class TypedConstDisallowed {
    const callable X = "strlen";
}
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  Class constant TypedConstDisallowed::X cannot have type callable %s
--CLEAN--
<?php
