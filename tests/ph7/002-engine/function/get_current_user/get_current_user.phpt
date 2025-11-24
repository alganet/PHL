--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: get_current_user returns a string
--FILE--
<?php
echo "get_current_user_type=" . gettype(get_current_user()) . "\n";
?>
--EXPECT--
get_current_user_type=string
