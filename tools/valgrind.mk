VALGRIND = valgrind --tool=memcheck \
    --leak-check=full \
    --leak-resolution=high \
    --show-reachable=yes \
    --suppressions=$(top_srcdir)/tools/telepathy-glib.supp \
    --child-silent-after-fork=yes \
    --num-callers=20 \
    --error-exitcode=42 \
    --gen-suppressions=all

# other potentially interesting options:
# --read-var-info=yes    better diagnostics from DWARF3 info
# --track-origins=yes    better diagnostics for uninit values (slow)
