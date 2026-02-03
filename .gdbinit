# GDB init file for my_raylib_games

# Pretty printing
set print pretty on
set print array on
set print array-indexes on

# History
set history save on
set history filename .gdb_history
set history size 10000

# Disable pagination for smoother debugging
set pagination off

# Confirm off for quit
set confirm off

# Catch common signals but don't stop on SIGPIPE
handle SIGPIPE nostop noprint pass

# Source local .gdbinit if it exists in the build directory
# (useful for game-specific settings)
python
import os
if os.path.exists('build/.gdbinit'):
    gdb.execute('source build/.gdbinit')
end
