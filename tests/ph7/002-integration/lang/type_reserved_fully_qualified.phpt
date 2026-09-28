--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A built-in type word may not be spelled with a qualifier
--DESCRIPTION--
php looks the word up in its built-in type table before anything else and asks for it
UNQUALIFIED, printing it lower-cased whatever the source spelled. `array` and `callable` are
absent from that table -- php's parser gives them their own tokens -- so those two take the
reserved-class-name wording instead.
--FILE--
<?php
namespace TrfqZ;
function trfq(): \INT { }
?>
--EXPECTF--
%s Fatal error:  Type declaration 'int' must be unqualified%A
--CLEAN--
<?php
