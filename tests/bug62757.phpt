--TEST--
Bug #62757 (php-fpm carshed when used apc_bin_dumpfile with apc.serializer)
--INI--
apc.enabled=1
apc.enable_cli=1
apc.stat=0
apc.cache_by_default=1
apc.filters=
apc.serializer=php
--FILE--
<?php
$filename = dirname(__FILE__) . '/bug62757_file.php';
$bin_filename = dirname(__FILE__)  . "/bug62757.bin";
$file_contents = '<?php
function test($arr=array()) {
    return true;
}

class ApiLib{
    private $arr = array();
    protected $str = "constant string";
}
';
file_put_contents($filename, $file_contents);
apc_compile_file($filename);
apc_bin_dumpfile(array($filename), null, $bin_filename);
apc_bin_loadfile($bin_filename);
include $filename;

var_dump(test());
echo "okey\n";
?>
--CLEAN--
<?php
unlink(dirname(__FILE__) . '/bug62757_file.php');
unlink(dirname(__FILE__)  . "/bug62757.bin");
?>
--EXPECT--
bool(true)
okey
