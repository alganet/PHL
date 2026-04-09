--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Interface constants must be public (private/protected is fatal error)
--FILE--
<?php
interface Iface {
    protected const PROT = 3;
}
echo "should not reach here\n";
?>
--EXPECTF--
%s Fatal error:  Access type for interface constant %s
--CLEAN--
<?php

