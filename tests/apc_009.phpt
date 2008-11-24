--TEST--
APC: apc_delete_file test
--SKIPIF--
<?php require_once(dirname(__FILE__) . '/skipif.inc'); ?>
--INI--
apc.enabled=1
apc.enable_cli=1
apc.file_update_protection=0
report_memleaks=0
--FILE--
<?php

apc_compile_file('apc_009.php');
check_file();
apc_delete_file('apc_009.php');
check_file();

apc_compile_file('apc_009.php');
apc_delete_file(array('apc_009.php'));
check_file();

apc_compile_file('apc_009.php');
$it = new APCIterator('file', '/apc_009.php/');
apc_delete_file($it);
check_file();

function check_file() {
  $info = apc_cache_info('file');
  if (isset($info['cache_list'][0])) {
    echo "Found File\n";
  } else {
    echo "File Not Found\n";
  }
}

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
Found File
File Not Found
File Not Found
File Not Found
===DONE===
