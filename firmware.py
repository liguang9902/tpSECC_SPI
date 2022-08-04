from ast import Import
from os import environ


Import("env")
# my_flags = env.ParseFlags(env['BUILD_FLAGS'])
# defines = {k: v for (k, v) in my_flags.get("CPPDEFINES")}

# print(defines)
# env.Replace(PROGNAME="ESP_firmware_%s" % defines.get("VERSION"))

environ.Replace(PROGNAME="ESP_firmware")
