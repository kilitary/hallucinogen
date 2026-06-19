savedcmd_dmesg_injector.mod := printf '%s\n'   dmesg_injector.o | awk '!x[$$0]++ { print("./"$$0) }' > dmesg_injector.mod
