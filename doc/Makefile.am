
EXTRA_DIST = images/ tcpkali.man.md

dist_man_MANS = tcpkali.1

if HAVE_PANDOC

tcpkali.1: tcpkali.man.md
	${PANDOC} --from markdown --to man -o $@    \
	    --variable header="Version ${VERSION}"  \
	    --variable adjusting:l                  \
	    --variable hyphenate:""                 \
	    --standalone                            \
	    $<


endif
