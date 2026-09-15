--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
declare(ticks=...) is accepted silently
--DESCRIPTION--
php prints nothing for declare(ticks=1). PHL used to emit a NOTICE that also leaked the
upstream engine's name and version ("... the PH7(2.1.4) engine") into user-visible output.
--FILE--
<?php
declare(ticks=1);
echo "done\n";
?>
--EXPECT--
done
