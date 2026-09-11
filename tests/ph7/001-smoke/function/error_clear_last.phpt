--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
error_clear_last() exists and makes error_get_last() return null
--FILE--
<?php
error_clear_last();
echo (error_get_last() === null) ? "null\n" : "notnull\n";
// Idempotent / callable repeatedly.
error_clear_last();
error_clear_last();
echo (error_get_last() === null) ? "still-null\n" : "notnull\n";
echo function_exists('error_clear_last') ? "exists\n" : "missing\n";
echo "done\n";
?>
--EXPECT--
null
still-null
exists
done
--CLEAN--
<?php
