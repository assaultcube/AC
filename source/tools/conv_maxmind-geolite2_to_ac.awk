# convert Geolite2 lists for Assaultcube servers
# 
# expects to process GeoLite2-Country-Locations-en.csv and GeoLite2-Country-Blocks-IPv4.csv (in that order)
#
# gawk -f conv_maxmind-geolite2_to_ac.awk GeoLite2-Country-Locations-en.csv GeoLite2-Country-Blocks-IPv4.csv > geoip.cfg
#
# note: the ISO 3166-1 alpha-2 list reserves XA..XZ for user-assigned code elements - we use xs, xp and xz here, see below
#
# download lists from http://dev.maxmind.com/geoip/geoip2/geolite2/ (please don't download too often: once a month should be enough)
#
# thanks to MaxMind:
#   This product includes GeoLite2 data created by MaxMind, available from http://www.maxmind.com
#

BEGIN {
FS=","   # expect csv
getcc=0
}

# geoname_id,locale_code,continent_code,continent_name,country_iso_code,country_name
$1=="geoname_id" && $5=="country_iso_code" {
	getcc=1		# use first line to switch mode
	next
}

# network,geoname_id,registered_country_geoname_id,represented_country_geoname_id,is_anonymous_proxy,is_satellite_provider
$1=="network" && $2=="geoname_id" {
	getcc=0		# use first line to switch mode
	next
}

getcc==1 {
	if ( length($5)==2 )
		cctab[$1]=$5		# has country_iso_code
	else if ( length($3)==2 )
		cctab[$1]=tolower($3)	# has continent_code (convert to lowercase to distinguish from country codes)
	else
		print "ERROR: no usable country code in line " $0	# should not occur
	next
}

getcc==0 {
	if ( $2=="" ) {			# pick geoname_id over represented_country_geoname_id over registered_country_geoname_id
		if ( $4 == "" )
			cc=$3
		else
			cc=$4
	} else {
		cc=$2
	}
	if ( cc=="" && $6!=0 )
		print $1 " xs"			# use XS to mark satellite providers with no other country affiliation
	else if ( cc=="" && $5!=0 )
		print $1 " xp"			# use XP to mark anonymous proxies with no other country affiliation
	else if ( cc in cctab )
		print $1 " " cctab[cc]
	else
		print $1 " xz // " $0 " not found"	# use XZ to mark table conversion errors
	next
}

