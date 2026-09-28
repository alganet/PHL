--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A declared default that names a constant is one, and answers all three questions
--DESCRIPTION--
php keeps the SOURCE of an internal parameter's default, so `int $type =
PDO::PARAM_STR` answers three separate questions: the VALUE it evaluates to,
that it IS a constant reference, and WHICH constant. This engine answered the
first only -- every internal parameter reported isDefaultValueConstant() false
and getDefaultValueConstantName() null, 72 of them where php names a constant.

Two shapes are deliberately not constant references. A `|` fold of them
evaluates to a number no constant carries, so php answers false and null while
still printing the fold in the export; and `C::class` reads a class NAME rather
than a constant.

Under all of it, a FLOAT default read back as 0.0: the reducer took
SyStrToReal's STATUS for its answer and never looked at the value it writes,
which is why `log()`'s M_E was 0.0 and curl_multi_select's 1.0 was too.
--FILE--
<?php
$sq9show = function ($label, $fn) {
    try { $out = json_encode($fn()); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo str_pad($label, 42), ' => ', $out, "\n";
};
$sq9desc = function ($what, $pos) {
    $r = is_array($what) ? new ReflectionMethod($what[0], $what[1]) : new ReflectionFunction($what);
    $p = $r->getParameters()[$pos];
    return [$p->getDefaultValue(), $p->isDefaultValueConstant(), $p->getDefaultValueConstantName()];
};

/* A declared default that NAMES a constant is one, and php answers all three
   questions about it: the value, that it is a constant, and which. */
$sq9show('a global constant', fn () => $sq9desc('trigger_error', 1));
$sq9show('a class constant', fn () => $sq9desc(['PDOStatement', 'bindValue'], 2));
$sq9show('a namespaced class constant', fn () => $sq9desc(['Pdo\Sqlite', 'openBlob'], 4));
$sq9show('one this engine spells itself', fn () => $sq9desc('str_pad', 3));

/* A `|` fold of them evaluates, and names nothing */
$sq9show('a fold of global constants', fn () => $sq9desc(['SQLite3', '__construct'], 1));
$sq9show('a fold of class constants', fn () => $sq9desc(['FilesystemIterator', '__construct'], 1));

/* `C::class` is a class NAME expression rather than a constant reference */
$sq9show('the ::class form', fn () => $sq9desc(['SplFileInfo', 'setFileClass'], 0));

/* A FLOAT default is the number it spells */
$sq9show('a float', fn () => $sq9desc('log', 1));
$sq9show('another float', fn () => $sq9desc('curl_multi_select', 1));

/* and the export prints the SOURCE for all of them */
foreach ([['trigger_error', 1], ['log', 1], ['str_pad', 3]] as $c) {
    $p = (new ReflectionFunction($c[0]))->getParameters()[$c[1]];
    echo str_pad('export ' . $c[0], 42), ' => ', (string)$p, "\n";
}
foreach ([[['SQLite3', '__construct'], 1], [['PDOStatement', 'bindValue'], 2],
          [['SplFileInfo', 'setFileClass'], 0], [['XMLWriter', 'startDocument'], 0]] as $c) {
    $p = (new ReflectionMethod($c[0][0], $c[0][1]))->getParameters()[$c[1]];
    echo str_pad('export ' . $c[0][0] . '::' . $c[0][1], 42), ' => ', (string)$p, "\n";
}
--EXPECT--
a global constant                          => [1024,true,"E_USER_NOTICE"]
a class constant                           => [2,true,"PDO::PARAM_STR"]
a namespaced class constant                => [1,true,"Pdo\\Sqlite::OPEN_READONLY"]
one this engine spells itself              => [1,true,"STR_PAD_RIGHT"]
a fold of global constants                 => [6,false,null]
a fold of class constants                  => [4096,false,null]
the ::class form                           => ["SplFileObject",false,null]
a float                                    => [2.718281828459045,true,"M_E"]
another float                              => [1,false,null]
export trigger_error                       => Parameter #1 [ <optional> int $error_level = E_USER_NOTICE ]
export log                                 => Parameter #1 [ <optional> float $base = M_E ]
export str_pad                             => Parameter #3 [ <optional> int $pad_type = STR_PAD_RIGHT ]
export SQLite3::__construct                => Parameter #1 [ <optional> int $flags = SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE ]
export PDOStatement::bindValue             => Parameter #2 [ <optional> int $type = PDO::PARAM_STR ]
export SplFileInfo::setFileClass           => Parameter #0 [ <optional> string $class = SplFileObject::class ]
export XMLWriter::startDocument            => Parameter #0 [ <optional> ?string $version = "1.0" ]
