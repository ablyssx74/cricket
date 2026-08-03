name			$(NAME)
version			$(VERSION)-$(REVISION)
architecture	$(ARCH)
summary 		"cricket"
description 	"cricket - A lightweight, multi-server IRC client."
packager		"ablyss <cricket@epluribusunix.net>"
vendor			"epluribusunix.net Project"
licenses {
	"MIT"
}
copyrights {
	"$(YEAR) ablyss"
}
provides {
	$(NAME) = $(VERSION)-$(REVISION)
}
requires {
	haiku
	nlohmann_json
	lib:libssl$(is32bit)
	lib:libcrypto$(is32bit)
	curl$(is32bit)
}
conflicts {
	hirc	
}
	
urls {
	"https://github.com/ablyssx74/cricket"
}
