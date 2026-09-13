--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Compile lexer: a line comment ('//' or '#') ends at '?>', which closes the PHP tag
--FILE--
<?php echo "A"; // trailing note it's here ?>
INLINE<?php echo "B\n";
--EXPECT--
AINLINEB
