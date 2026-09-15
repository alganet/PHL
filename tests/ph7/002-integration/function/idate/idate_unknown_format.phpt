--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
idate: Unknown format token produces warning

--FILE--
<?php
$result = idate('q');
echo "Result: $result\n";
?>
--EXPECTF--
%Aidate(): Unrecognized date format token in %s on line 2%AResult:%A

--CLEAN--
<?php
unset($result);
