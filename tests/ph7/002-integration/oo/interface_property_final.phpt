--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A property in an interface cannot be final
--DESCRIPTION--
An interface property is a hooked REQUIREMENT, and a requirement no implementor
may restate cannot be one. php words it as the property's problem rather than the
modifier's.
--FILE--
<?php
interface IpfI { final public int $p { get; } }
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Property in interface cannot be final %s
--CLEAN--
<?php
