name			cricket_qt
version			@VERSION@-@REVISION@
architecture	x86_64
summary 		"Cricket IRC client (Qt 6 version)"
description 	"Cricket - a lightweight, multi-server IRC client, built with Qt 6. It reads the same settings file as the native Haiku version of Cricket and can be installed beside it."
packager		"ablyss <cricket@epluribusunix.net>"
vendor			"epluribusunix.net Project"
licenses {
	"MIT"
}
copyrights {
	"@YEAR@ ablyss"
}
provides {
	cricket_qt = @VERSION@-@REVISION@
	app:cricket_qt = @VERSION@-@REVISION@
}
requires {
	haiku
	qt6_base
	qt6_svg
	aspell
	openssl3
	lib:libaspell
}
urls {
	"https://github.com/ablyssx74/cricket"
}
