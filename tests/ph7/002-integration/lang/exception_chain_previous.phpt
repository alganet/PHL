--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Uncaught report walks the $previous chain: deepest is "Uncaught", outers are "Next"
--FILE--
<?php
try {
  try { throw new RuntimeException("deepest"); }
  catch (Exception $e) { throw new LogicException("middle", 0, $e); }
} catch (Exception $e2) {
  throw new Exception("top", 0, $e2);
}
?>
--EXPECTF--
%s Fatal error:  Uncaught RuntimeException: deepest in %s
Stack trace:
%A
Next LogicException: middle in %s
Stack trace:
%A
Next Exception: top in %s
Stack trace:
%A
  thrown in %s on line %d
--CLEAN--
<?php
