savedcmd_dmesg_inject.mod := printf '%s\n'   dmesg_inject.o | awk '!x[$$0]++ { print("./"$$0) }' > dmesg_inject.mod
