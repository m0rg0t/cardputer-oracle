Import("env")
# The UI formats integers only. Nano newlib avoids the full floating-point
# printf/scanf implementation while retaining normal C/C++ arithmetic.
env.Append(LINKFLAGS=["--specs=nano.specs"])
