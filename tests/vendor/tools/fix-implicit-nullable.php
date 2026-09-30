<?php
/*
 * SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Rewrites php 8.4's DEPRECATED implicitly-nullable parameter -- `T $x = null` -- into
 * the explicit `?T $x = null` that PLAN.md §10 requires, in place, over a directory.
 *
 * This is the `implicit-nullable-param` patch generator for tests/vendor: it is the one
 * §10 surface a project hits by the dozen, and hand-editing it is how a "deprecation
 * patch" quietly turns into an edit nobody reviewed. The rewrite is php's own reading of
 * the same declaration, so the patch it produces changes NO behaviour under php.
 *
 * It splices bytes at the type's own file offsets rather than pretty-printing, so the
 * diff is one character per parameter and everything else in the file is untouched.
 *
 * usage: php tests/vendor/tools/fix-implicit-nullable.php [--dry-run] <autoload.php> <dir> [dir...]
 *        (the autoloader is any tree that has nikic/php-parser 5 installed)
 *        --dry-run reports what it WOULD rewrite and touches nothing, which is how you
 *        size the patch a project needs before generating it.
 */
$args = array_slice($argv, 1);
$dry = false;
if (($args[0] ?? '') === '--dry-run') {
    $dry = true;
    array_shift($args);
}
$autoload = $args[0] ?? null;
if ($autoload === null || !is_file($autoload)) {
    fwrite(STDERR, "usage: fix-implicit-nullable.php <autoload.php> <dir> [dir...]\n");
    exit(2);
}
require $autoload;

use PhpParser\Node;
use PhpParser\NodeTraverser;
use PhpParser\NodeVisitorAbstract;
use PhpParser\ParserFactory;

$parser = (new ParserFactory())->createForNewestSupportedVersion();

/** Collects the byte offset at which each implicitly-nullable parameter needs its `?`. */
final class Collector extends NodeVisitorAbstract
{
    /** @var array<int, array{int, string}> offset => [offset, insertion] */
    public array $edits = [];
    public array $skipped = [];

    public function enterNode(Node $node): void
    {
        if (!$node instanceof Node\Param || $node->type === null) {
            return;
        }
        $default = $node->default;
        if (!$default instanceof Node\Expr\ConstFetch || strtolower($default->name->toString()) !== 'null') {
            return;
        }
        $type = $node->type;
        if ($type instanceof Node\NullableType) {
            return;                                   // already `?T`
        }
        if ($type instanceof Node\Identifier && in_array(strtolower($type->name), ['mixed', 'null'], true)) {
            return;                                   // both already admit null
        }
        if ($type instanceof Node\UnionType) {
            foreach ($type->types as $t) {
                if ($t instanceof Node\Identifier && in_array(strtolower($t->name), ['mixed', 'null'], true)) {
                    return;
                }
            }
            $this->edits[$type->getEndFilePos() + 1] = [$type->getEndFilePos() + 1, '|null'];
            return;
        }
        if ($type instanceof Node\IntersectionType) {
            // `(A&B)|null` needs parentheses the source does not have; leave it and say so.
            $this->skipped[] = $node->var->name ?? '?';
            return;
        }
        $this->edits[$type->getStartFilePos()] = [$type->getStartFilePos(), '?'];
    }
}

$files = [];
foreach (array_slice($args, 1) as $dir) {
    $it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS));
    foreach ($it as $f) {
        if ($f->isFile() && strtolower($f->getExtension()) === 'php') {
            $files[] = $f->getPathname();
        }
    }
}
sort($files);

$changedFiles = 0;
$changedParams = 0;
foreach ($files as $file) {
    $code = file_get_contents($file);
    try {
        $ast = $parser->parse($code);
    } catch (Throwable $e) {
        fwrite(STDERR, "skip (unparseable): $file: {$e->getMessage()}\n");
        continue;
    }
    $collector = new Collector();
    $traverser = new NodeTraverser($collector);
    $traverser->traverse($ast);
    if ($collector->skipped !== []) {
        fwrite(STDERR, "MANUAL: $file has an intersection-typed parameter: " . implode(', ', $collector->skipped) . "\n");
    }
    if ($collector->edits === []) {
        continue;
    }
    krsort($collector->edits);                        // splice from the end, offsets stay valid
    foreach ($collector->edits as [$offset, $insert]) {
        $code = substr($code, 0, $offset) . $insert . substr($code, $offset);
        $changedParams++;
    }
    $changedFiles++;
    if ($dry) {
        echo "would rewrite $file (" . count($collector->edits) . ")\n";
        continue;
    }
    file_put_contents($file, $code);
    echo "rewrote $file\n";
}
echo "$changedParams implicitly-nullable parameters in $changedFiles files\n";
