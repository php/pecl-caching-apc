--TEST--
APC fails to propagate $GLOBALS when auto_globals_jit=Off
--SKIPIF--
<?php
    require_once(dirname(__FILE__) . '/skipif.inc');
    if (PHP_MAJOR_VERSION < 5 || (PHP_MAJOR_VERSION == 5 && PHP_MINOR_VERSION < 4)) {
		die('skip PHP 5.4+ only');
	}
--FILE--
<?php
include "server_test.inc";

$tmp = 'bug_auto_globals_jit_off.tmp.php';
$tmp_file = dirname(__FILE__) . DIRECTORY_SEPARATOR . $tmp;

$file =
<<<FL
<?php
	\$foo = 'bar';
	function foo () {
		global \$foo;
		print_r("\$foo\\n");
		print_r("\${GLOBALS['foo']}\\n");
		\$foo = 'baz';
		print_r("\$foo\\n");
		print_r("\${GLOBALS['foo']}\\n");
	}
	print_r("\${GLOBALS['foo']}\\n");
	foo();
	print_r("\${GLOBALS['foo']}\\n");
FL;

file_put_contents($tmp_file, $file);

$args = array(
	'apc.enabled=1',
	'apc.cache_by_default=1',
	'apc.enable_cli=1',
	'auto_globals_jit=Off',
	'display_errors=On',
	'error_reporting=-1',
	'html_errors=Off',
);

$num_servers = 1;

server_start('', $args, true);

sleep(5);

print_r("first request\n");
run_test_simple('/'.$tmp);
print_r("second request\n");
run_test_simple('/'.$tmp);

print_r("done");
?>
--CLEAN--
<?php
unlink(dirname(__FILE__) . DIRECTORY_SEPARATOR . "bug_auto_globals_jit_off.tmp.php");
?>
--EXPECT--
first request
bar
bar
bar
baz
baz
baz
second request
bar
bar
bar
baz
baz
baz
done
