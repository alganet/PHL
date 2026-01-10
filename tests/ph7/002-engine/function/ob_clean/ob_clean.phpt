--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ob_clean clears output buffer without destroying it
--FILE--
<?php
ob_start();
echo "Hello";
// ob_clean should remove buffer contents without ending buffering
ob_clean();
echo "World";
ob_end_flush();
?>
--EXPECT--
World