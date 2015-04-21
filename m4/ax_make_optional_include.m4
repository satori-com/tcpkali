# Check to see how 'make' supports optional includes.

# serial 1

# AX_CHECK_GNU_STYLE_OPTIONAL_INCLUDE()
# -------------------------------------
# Check to see whether make supports optional includes.
AC_DEFUN([AX_CHECK_GNU_STYLE_OPTIONAL_INCLUDE],
[am_make=${MAKE-make}
AC_MSG_CHECKING([for style of optional includes used by $am_make])
# Try -include style used by GNU make.
cat > confmf << 'END'
-include confinc.nonexistent
all:
	@-:
END
_am_result=none
if $am_make -s -f confmf 2>/dev/null; then _am_result=GNU; fi
AM_CONDITIONAL([GNU_STYLE_OPTIONAL_INCLUDE], [test "x$_am_result" = "xGNU"])
AC_MSG_RESULT([$_am_result])
rm -f confmf
])
