// Automatically allows *nix based platforms to transform ../reference.xml into ../../config/docs.cfg
// This requires "xsltproc" - which may already be installed on most Linux based operating systems.

xsltproc -o ../../config/docs.cfg cuberef2cubescript.xslt ../reference.xml

echo ""
echo ""
echo "This shell script has finished!"
sleep 10