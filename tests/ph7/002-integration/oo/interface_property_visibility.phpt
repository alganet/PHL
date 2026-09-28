--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property in an interface may be neither protected nor private
--DESCRIPTION--
php words this one as the property's problem and names neither of the two
visibilities it refuses -- unlike an interface CONSTANT or METHOD, which get
`Access type for interface ... must be public` with the member named.
--FILE--
<?php
interface IpvI { protected int $p { get; } }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Property in interface cannot be protected or private %s
--CLEAN--
<?php
