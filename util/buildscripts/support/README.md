This folder contains various files not included with the build scripts, but which can be used.

* dbghelp.dll, pdbstr.exe, symstore.exe, symsrv.dll, symsrv.yes - used for setting up source server and symbol store with built symbols. Taken from the Debuggers/x64 folder in the Windows 10 SDK
* key.pfx, key.pass - used for signing resulting binaries
* llvm_arm32, llvm_arm64 - used for building android with interceptor-lib.
* emailhost - an optional file containing an ssh-compatible user@host string, which is used to send error emails (the email is sent via running 'mail' on that remote host)
