--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
try without catch or finally
--FILE--
<?php
try {
echo "hello";
}
--EXPECTF--
%s %s %s  Cannot use try without catch or finally %s
--CLEAN--
<?php

