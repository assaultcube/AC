// well, this readme requires javascript to fix a dumb IE bug, sorry

function correctPNG() // correctly handle PNG transparency in Win IE 5.5 or higher.
{
    for(var i=0; i<document.images.length; i++)
    {
        var img = document.images[i]
        var imgName = img.src.toUpperCase()
        if (imgName.substring(imgName.length-3, imgName.length) == "PNG")
        {
            var imgID = (img.id) ? "id='" + img.id + "' " : ""
            var imgClass = (img.className) ? "class='" + img.className + "' " : ""
            var imgTitle = (img.title) ? "title='" + img.title + "' " : "title='" + img.alt + "' "
            var imgStyle = "display:inline-block;" + img.style.cssText 
            if (img.align == "left") imgStyle = "float:left;" + imgStyle
            if (img.align == "right") imgStyle = "float:right;" + imgStyle
            if (img.parentElement.href) imgStyle = "cursor:hand;" + imgStyle		
            var strNewHTML = "<span " + imgID + imgClass + imgTitle
            + " style=\"" + "width:" + img.width + "px; height:" + img.height + "px;" + imgStyle + ";"
            + "filter:progid:DXImageTransform.Microsoft.AlphaImageLoader"
            + "(src=\'" + img.src + "\', sizingMethod='scale');\"></span>" 
            img.outerHTML = strNewHTML
            i = i-1
        }
    }
};

if(navigator.appName == "Microsoft Internet Explorer")
{
    var ID = "MSIE";
    var versionIndex = navigator.appVersion.indexOf(ID);
    if(versionIndex >= 0)
    {
        versionIndex += ID.length + 1;
        var version = navigator.appVersion.substr(versionIndex, 3);
        
        if(version == "5.5" || version.substr(0,1) == "6")
        {
           window.attachEvent("onload", correctPNG); 
        }
    }
};


