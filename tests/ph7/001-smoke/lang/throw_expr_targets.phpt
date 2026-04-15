--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
throw expression: accepts property, subscript, static, call, and nullsafe targets
--FILE--
<?php
class ThrowExprBox { public Exception $err; }
class ThrowExprErrs { public static Exception $shared; }
class ThrowExprFactory { public function make(): Exception { return new Exception('from method'); } }

$b = new ThrowExprBox();
$b->err = new Exception('from property');
try { null ?? throw $b->err; } catch (Exception $e) { echo $e->getMessage(), "\n"; }

$list = [new Exception('from subscript')];
try { null ?? throw $list[0]; } catch (Exception $e) { echo $e->getMessage(), "\n"; }

ThrowExprErrs::$shared = new Exception('from static');
try { null ?? throw ThrowExprErrs::$shared; } catch (Exception $e) { echo $e->getMessage(), "\n"; }

$f = new ThrowExprFactory();
try { null ?? throw $f->make(); } catch (Exception $e) { echo $e->getMessage(), "\n"; }

function throwExprMakeErr() { return new Exception('from function'); }
try { null ?? throw throwExprMakeErr(); } catch (Exception $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
from property
from subscript
from static
from method
from function
--CLEAN--
<?php
