#pragma once

#include <string>
#include <binder/Parcel.h>

std::string native_window_operation(int what);
std::string native_query_operation(int what);
std::string android_status(android::status_t status);

class NativeWindowBuffer;
NativeWindowBuffer * read_native_window_buffer_from_parcel(const android::Parcel& parcel);
void write_native_window_buffer_to_parcel(const NativeWindowBuffer& buffer, android::Parcel& parcel);
