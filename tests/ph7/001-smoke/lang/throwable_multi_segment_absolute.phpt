--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Throwable: multi-segment absolute qualifier in extends and catch
--FILE--
<?php
namespace Msg\Deep;
class Err extends \Exception {}
class Inner extends \Msg\Deep\Err {}
try {
    throw new \Msg\Deep\Inner("deep");
} catch (\Msg\Deep\Err $e) {
    echo get_class($e), ":", $e->getMessage(), "\n";
}
echo (new \Msg\Deep\Inner() instanceof \Throwable) ? "yes\n" : "no\n";
?>
--EXPECT--
Msg\Deep\Inner:deep
yes
--CLEAN--
<?php
