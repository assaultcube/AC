# convert geo-ip lists from software77 for Assaultcube servers
# 
# expects to process IPV4-CSV lists from Webnet77 (please consider donations, if you use their lists)
#
# gawk -f conv_software77-geoip_to_ac.awk IpToCountry.csv > geoip.cfg
#
# download lists from http://software77.net/geo-ip/ (please don't download too often: once a month should be enough)
#
# thanks to Webnet77:  http://webnet77.com
#

BEGIN {
FS=","   # expect csv
getcc=0
}

function unquote(q) {
 gsub("\"", "", q)
 return q
}

substr($1,1,1)=="#" {
	next		# comment
}

# expect lines like "38273024","38797311","ripencc","1275523200","KZ","KAZ","Kazakhstan"
{
	print unquote($1) "-" unquote($2) " " unquote($5) " // " unquote($7)
	#print unquote($1) "-" unquote($2) " " unquote($5) 		# use this to drop the comments
}
