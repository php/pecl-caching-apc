<?
	// Note: this file must be in your document root to function correctly.
	// If it is NOT in your document root, you must change the path below
	// to point to the location of this page (relative or absolute URL).

	$MY_PATH = "apcinfo.php";

	if ($apc_info != "")
	{
		// $apc_info is set, meaning the user clicked on a
		// filename. Dump that cache object's information.
		
		apc_dump_cache_object($apc_info);
	}
	else
	{
		// Display global cache information.

		if (is_array($apc_rm))
		{
			// $apc_rm is set and is an array, meaning the user
			// selected one or more files for deletion. Remove
			// them all now.
			
			print "Removed the following files:<br>\n";
			print "<ul>\n";
			foreach ($apc_rm as $filename)
			{
				print "<li>$filename\n";
				apc_rm($filename);
			}
			print "</ul>\n";
		}

		apcinfo($MY_PATH);
	}
?>
