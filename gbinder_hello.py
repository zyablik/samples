#!/usr/bin/python3

import gbinder

serviceManager = gbinder.ServiceManager("/dev/binder", "aidl3", "aidl3") # aidl3 = android 9+

print(f"serviceManager = {serviceManager}")
print(f"serviceManager.is_present() = {serviceManager.is_present()}")

print(f"serviceManager.list_sync() = {serviceManager.list_sync()}")
