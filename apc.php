<?php
function redir($arg="") {
	$port = getenv('SERVER_PORT');
	$host = getenv('HTTP_HOST');
	$self = $_SERVER['PHP_SELF'];
	if($port==443) $url = "https://";
	else $url = "http://";
	$url .= $host;
	if($port!=80) $url .= ":$port";
	$url .= $self;
	if($arg) $url.="?$arg";
	header("Location: $url");
	exit;
}
if(isset($_GET['action'])) {
    switch($_GET['action']) {
      case 'Clear Opcode Cache': 
        apc_clear_cache();
		redir();
          break;
      case 'Clear User Cache': 
        apc_clear_cache('user');
		redir("action=Switch+to+User+Cache");
          break;
      case 'Show Top 25': $limit=25; break;
      case 'Show Top 100': $limit=100; break;
      case 'Show All Files': $limit=0; break;
	  case 'Switch to User Cache': $limit=0; $mode='user'; $field = 'info'; break;
	  case 'Switch to Opcode Cache': $limit=0; $mode='opcode'; $field = 'filename'; break;
    }
} else {
    $limit = 25; 
	$mode = 'opcode';
	$field = 'filename';
}

if(!$cache_info = @apc_cache_info($mode)) {
	echo "No cache info available.  Did you disable it in your apc.ini file?";
	exit;
}
$ttl = $cache_info['ttl'];
$sma_info = apc_sma_info();
?>
<html>
  <head>
    <title>APC Info</title>
    <meta http-equiv="Pragma" content="no-cache">
  </head>
  <body>
<?php
function menu() {
global $mode;?>
<div align="center">
<form action="<?php echo getenv('SCRIPT_NAME')?>" method="GET">
<input type="submit" name="action" value="Show Top 25" /> 
<input type="submit" name="action" value="Show Top 100" /> 
<?php if($mode=='user'):?>
<input type="submit" name="action" value="Show All User Entries" /> 
<input type="submit" name="action" value="Switch to Opcode Cache" /> 
<input type="submit" name="action" value="Clear User Cache" /> 
<?php else:?>
<input type="submit" name="action" value="Show All Files" /> 
<input type="submit" name="action" value="Switch to User Cache" /> 
<input type="submit" name="action" value="Clear Opcode Cache" /> 
<?php endif;?>
<?php if(isset($_GET['sort'])) { ?>
<input type="hidden" name="sort" value="<?php echo $_GET['sort']?>" />
<?php } ?>
</form>
</div>
<?php }

function hit_sort( $a, $b ) {
	global $field;

    if ( $a['num_hits'] == $b['num_hits'] ) {
		return strcmp( strtolower( $a[$field] ), strtolower( $b[$field] ) );
	}

    return ($a['num_hits'] > $b['num_hits']) ? -1 : 1;
}

function field_sort( $a, $b ) {
	global $field;

	if( $a[$field] == $b[$field]) {
    	return ($a['num_hits'] > $b['num_hits']) ? -1 : 1;
	}
	return strcmp( strtolower( $a[$field] ), strtolower( $b[$field] ) );
}

function mtime_sort( $a, $b ) {
	global $field;

    if ( $a['mtime'] == $b['mtime'] ) {
		return strcmp( strtolower( $a[$field] ), strtolower( $b[$field] ) );
	}

    return ($a['mtime'] > $b['mtime']) ? -1 : 1;
}

function atime_sort( $a, $b ) {
	global $field;

    if ( $a['access_time'] == $b['access_time'] ) {
		return strcmp( strtolower( $a[$field] ), strtolower( $b[$field] ) );
	}

    return ($a['access_time'] > $b['access_time']) ? -1 : 1;
}

if(isset($_GET['sort'])) {
  switch($_GET['sort']) {
    case 'field':
	usort( $cache_info['cache_list'], 'field_sort' );
	break;
    case 'mtime':
	usort( $cache_info['cache_list'], 'mtime_sort' );
	break;
    case 'atime':
	usort( $cache_info['cache_list'], 'atime_sort' );
	break;
    default:
	usort( $cache_info['cache_list'], 'hit_sort' );
	break;
  }
} else usort( $cache_info['cache_list'], 'hit_sort' );
?>

<table border="0" cellpadding="3" cellspacing="1" width="600" bgcolor="#000000" align="center">
<tr valign="middle" bgcolor="#9999cc"><td align="left"><h1>APC Info</h1></td></tr>
</table><br />

<table border="0" cellpadding="3" cellspacing="1" width="600" bgcolor="#000000" align="center">
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>num_seg</b></td><td align="left"><?php echo $sma_info['num_seg']; ?></td></tr>
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>seg_size</b></td><td align="left"><?php echo $sma_info['seg_size']; ?></td></tr>
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>avail_mem</b></td><td align="left"><?php echo $sma_info['avail_mem']; ?></td></tr>
</table><br />

<table border="0" cellpadding="3" cellspacing="1" width="600" bgcolor="#000000" align="center">
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>num_slots</b></td><td align="left"><?php echo $cache_info['num_slots']; ?></td></tr>
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>TTL</b></td><td align="left"><?php echo $ttl; ?></td></tr>
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>num_hits</b></td><td align="left"><?php echo $cache_info['num_hits']; ?></td></tr>
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>num_misses</b></td><td align="left"><?php echo $cache_info['num_misses']; ?></td></tr>
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>cached_files</b></td><td align="left"><?php echo count( $cache_info['cache_list'] ); ?></td></tr>
<tr valign="baseline" bgcolor="#cccccc"><td bgcolor="#ccccff" ><b>deleted_list</b></td><td align="left"><?php echo count( $cache_info['deleted_list'] ); ?></td></tr>
</table><br />
<?php menu();

	if(isset($_GET['action']) && $_GET['action']!='Clear Cache') $action = '&action='.$_GET['action'];
	else $action='';
	if($mode=='user')
	     $name_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=field'.$action.'">User Entry</a>';
	else $name_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=field'.$action.'">File Name (inode)</a>';
	$mtime_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=mtime'.$action.'">Last Modified</a>';
	$atime_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=atime'.$action.'">Last Accessed</a>';
	$hit_sort = '<a href="'.getenv('SCRIPT_NAME') . '?sort=hit'.$action.'">Hits (ref count)</a>';
?>
<table border="0" cellpadding="3" cellspacing="1" width="100%" bgcolor="#000000" align="center">
<tr valign="middle" bgcolor="#9999cc">
<th><?php echo $name_sort?></th>
<th><?php echo $mtime_sort?></th>
<th><?php echo $atime_sort?></th>
<th><?php echo $hit_sort?></th>
</tr>

<?php $cnt=0; foreach ( $cache_info['cache_list'] as $file ) { 
  $now = time();
  $atime = $file['access_time'];
  if($ttl && ($atime < ($now-$ttl))) $col = "#ffffcc";
  else $col = "#ccccff";
?>
<tr valign="baseline" bgcolor="#cccccc">
  <?php if($mode=='user'):?>
  <td bgcolor="<?php echo $col;?>"><?php echo $file[$field]; ?></td>
  <td align="right"><?php echo strftime("%x %X",$file['mtime'])?></td>
  <td align="right"><?php echo strftime("%x %X",$atime)?></td>
  <td align="right"><?php echo $file['num_hits'].' ('.$file['ref_count'].')'; ?></td>
  <?php else:?>
  <td bgcolor="<?php echo $col?>"><?php echo $file[$field].' ('.$file['inode'].')'; ?></td>
  <td align="right"><?php echo strftime("%x %X",$file['mtime'])?></td>
  <td align="right"><?php echo strftime("%x %X",$atime)?></td>
  <td align="right"><?php echo $file['num_hits'].' ('.$file['ref_count'].')'; ?></td>
  <?php endif;?>
</tr>
<?php $cnt++; if($limit && $cnt>$limit) break; } ?>
</table>
<br />
</table>
<?php menu()?>
<br />
<?php if(isset($_GET['sma_debug'])):?>
<table border="0" cellpadding="3" cellspacing="1" width="100%" bgcolor="#000000" align="center">
<tr valign="middle" bgcolor="#9999cc"><th>Block</th><th>offset</th><th>size</th></tr>
<?php
$i=0;
foreach($sma_info['block_lists'][0] as $block) {
	$i++;?>
<tr bgcolor="#cccccc">
<td align="right"><?php echo $i?></td>
<td align="right"><?php echo $block['offset']?></td>
<td align="right"><?php echo $block['size']?></td>
</tr>
<?php } 
endif;?>
</table>
  </body>
</html>
