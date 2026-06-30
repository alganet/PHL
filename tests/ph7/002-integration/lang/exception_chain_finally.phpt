--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A throw from finally supersedes the in-flight exception and chains it as $previous
--FILE--
<?php
// finally-supersede: B replaces A, and B->getPrevious() is the real A (PHP data model)
try {
  try { throw new Exception("A"); }
  finally { throw new Exception("B"); }
} catch (Exception $e) {
  echo "caught: ".$e->getMessage()."\n";
  echo "previous: ".$e->getPrevious()->getMessage()."\n";
}

// An explicitly-constructed previous on the finally exception is NOT overridden.
$other = new Exception("OTHER");
try {
  try { throw new Exception("A2"); }
  finally { throw new Exception("B2", 0, $other); }
} catch (Exception $e) {
  echo "caught: ".$e->getMessage()."\n";
  echo "previous: ".$e->getPrevious()->getMessage()."\n";
}

// An exception thrown AND caught inside the finally is not chained (its previous stays null).
try {
  try { throw new Exception("A3"); }
  finally {
    try { throw new RuntimeException("inner"); }
    catch (RuntimeException $r) { echo "inner previous: ".var_export($r->getPrevious(), true)."\n"; }
    throw new Exception("B3");
  }
} catch (Exception $e) {
  echo "caught: ".$e->getMessage()."\n";
  echo "previous: ".$e->getPrevious()->getMessage()."\n";
}
?>
--EXPECT--
caught: B
previous: A
caught: B2
previous: OTHER
inner previous: NULL
caught: B3
previous: A3
--CLEAN--
<?php
