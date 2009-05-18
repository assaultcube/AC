function new-patch
{
    param($from, $to)
    # get diff summary as XML
    [xml] $summary = (svn diff $to $from -x --ignore-eol-style --xml --summarize)
    # loop through all diff/paths/path nodes
    # each node represents a modified/new file
    foreach($item in $summary.diff.paths.path)
    {
        # the SVN url
        $url=$item."#text"
        # the relative filename
        $file=($item."#text".Substring($to.Length))
        # the parent directory
        $dir=($file | split-path -parent)
        # create parent directory if it doesn't exist already
        if((test-path $dir) -eq $false) { mkdir $dir -force}
        # export current files from the SVN repository
        svn export $url $file
    }
    # package the current dir (.) into patch.zip
    sevenzip.exe a patch.zip .
}
