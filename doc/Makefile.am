
EXTRA_DIST = images/

man_MANS = tcpkali.1

if HAVE_PANDOC

tcpkali.1: tcpkali.man.md
	${PANDOC} --from markdown --to man -o $@    \
	    --template templates/default.man        \
	    --variable header="Version ${VERSION}"  \
	    --variable adjusting:l                  \
	    --variable hyphenate:""                 \
	    $<


endif
