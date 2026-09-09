--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A #! shebang on line 1 is skipped (not echoed) and line numbering continues at 2
--FILE--
#!/usr/bin/env php
<?php
echo "no shebang echoed\n";
echo __LINE__, "\n";
--EXPECT--
no shebang echoed
4
--CLEAN--
<?php
