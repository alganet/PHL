--TEST--
An autoloader that throws or exits ends the autoload chain
--FILE--
<?php
/* An autoloader that throws ends the chain: php asks no later loader, whichever
 * door asked for the class. A loader that catches its own throw does not. */
$first = function ($c) { echo "first $c\n"; if ($c !== 'Quiet') throw new RuntimeException("no $c"); };
$second = function ($c) { echo "second $c\n"; if ($c === 'Quiet' || $c === 'Later') eval("class $c {}"); };
spl_autoload_register($first);
spl_autoload_register($second);

function probe($label, $fn) {
    try { $r = $fn(); echo "$label: "; var_dump($r); }
    catch (Throwable $e) { echo "$label: ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
probe('new', fn() => new Missing);
probe('static call', fn() => Missing::m());
probe('class_exists', fn() => class_exists('Missing'));
probe('is_a', fn() => is_a('x', 'Missing', true));
probe('spl_autoload_call', fn() => spl_autoload_call('Missing'));
probe('second defines it', fn() => class_exists('Later'));
probe('caught inside', function () {
    spl_autoload_unregister($GLOBALS['first']);
    spl_autoload_register(function ($c) {
        try { throw new LogicException('inner'); } catch (LogicException $e) { echo "caught $c\n"; }
    }, true, true);
    return class_exists('Quiet');
});
probe('after', fn() => class_exists('Later', false));

/* exit() inside a loader ends the chain the same way. */
spl_autoload_register(function ($c) { echo "exiting $c\n"; exit("bye\n"); }, true, true);
register_shutdown_function(function () { echo "shutdown\n"; });
new Gone;
echo "unreachable\n";
--EXPECT--
first Missing
new: RuntimeException: no Missing
first Missing
static call: RuntimeException: no Missing
first Missing
class_exists: RuntimeException: no Missing
first x
is_a: RuntimeException: no x
first Missing
spl_autoload_call: RuntimeException: no Missing
first Later
second defines it: RuntimeException: no Later
caught Quiet
second Quiet
caught inside: bool(true)
after: bool(false)
exiting Gone
bye
shutdown
