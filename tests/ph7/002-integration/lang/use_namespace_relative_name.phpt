--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A namespace-relative name is not a use path
--DESCRIPTION--
`namespace\X` is php's own name operator and lexes as its own token kind, which the `use`
grammar does not take -- and php names that kind in the message, a wording no token-type word
can spell.
--FILE--
<?php
namespace UnrnZ;
use namespace\Q;
?>
--EXPECTF--
%s Parse error:  syntax error, unexpected namespace-relative name "namespace\Q"%A
--CLEAN--
<?php
