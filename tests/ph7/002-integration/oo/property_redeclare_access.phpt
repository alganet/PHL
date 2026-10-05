--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A redeclared property may not narrow its parent's visibility
--DESCRIPTION--
Exception's $message is protected, so a subclass may keep it protected or
widen it to public, never make it private.
--FILE--
<?php
class PrdAcError extends Exception {
    private $message = "x";
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Access level to PrdAcError::$message must be protected (as in class Exception) or weaker %s
