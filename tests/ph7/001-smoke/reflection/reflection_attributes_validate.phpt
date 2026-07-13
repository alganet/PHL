--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Attribute newInstance validation: targets, repetition, missing class
--FILE--
<?php
#[Attribute(Attribute::TARGET_CLASS)]
class ReflAtvClsOnly {}
#[Attribute(Attribute::TARGET_ALL)]
class ReflAtvOnce {}
#[ReflAtvClsOnly]
function reflAtvWrong() {}
#[ReflAtvOnce]
#[ReflAtvOnce]
class ReflAtvRep {}
try { (new ReflectionFunction("reflAtvWrong"))->getAttributes()[0]->newInstance(); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { (new ReflectionClass("ReflAtvRep"))->getAttributes()[0]->newInstance(); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
#[ReflAtvNoSuch]
class ReflAtvMissing {}
try { (new ReflectionClass("ReflAtvMissing"))->getAttributes()[0]->newInstance(); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
// use-import resolution + namespaces skipped (PHL namespaced attr in tests later)
--EXPECT--
Error: Attribute "ReflAtvClsOnly" cannot target function (allowed targets: class)
Error: Attribute "ReflAtvOnce" must not be repeated
Error: Attribute class "ReflAtvNoSuch" not found
