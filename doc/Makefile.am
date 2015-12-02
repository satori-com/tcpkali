
EXTRA_DIST = images/

if HAVE_PANDOC

man_MANS = tcpkali.1

tcpkali.1: tcpkali.man.md
	${PANDOC} --from markdown --to man -o $@    \
	    --standalone --smart                    \
	    --template templates/default.man        \
	    --variable header="Version ${VERSION}"  \
	    --variable adjusting:l                  \
	    $<


endif
