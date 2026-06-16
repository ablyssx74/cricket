name			$(NAME)
version			$(VERSION)-1
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
	$(NAME) = $(VERSION)-1
}
requires {
	haiku
	nlohmann_json	
}
conflicts {
	hirc	
}
	
urls {
	"https://github.com/ablyssx74/cricket"
}
