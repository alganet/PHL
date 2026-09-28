--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An explicit import alias may not be a reserved class name either
--FILE--
<?php
namespace UrcneaZ;
use UrcneaA\Q as iterable;
?>
--EXPECTF--
%s Fatal error:  Cannot use UrcneaA\Q as iterable because 'iterable' is a special class name%A
--CLEAN--
<?php
