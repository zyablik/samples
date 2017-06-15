#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/flags.h"

static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 1) return;
  v8::HandleScope scope(args.GetIsolate());
  v8::Local<v8::Value> arg = args[0];
  v8::String::Utf8Value value(arg);
  printf("log: %s\n", *value);
}

int main(int argc, char* argv[]) {
  printf("%s: main()\n", argv[0]);
//  const char str[] = "--help";
//  const char str[] = "--log --log_all";
//  const char str[] = "--log";
  const char str[] = "";
  v8::V8::SetFlagsFromString(str, sizeof(str));
//  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);

  // Initialize V8.
  v8::V8::InitializeICUDefaultLocation(argv[0]);
  v8::V8::InitializeExternalStartupData(argv[0]);
  v8::Platform * platform = v8::platform::CreateDefaultPlatform();
  v8::V8::InitializePlatform(platform);
  v8::V8::Initialize();

  // Create a new Isolate and make it the current one.
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate * isolate = v8::Isolate::New(create_params);

  {
    v8::Isolate::Scope isolate_scope(isolate);

    // Create a stack-allocated handle scope.
    v8::HandleScope handle_scope(isolate);

    // Create a template for the global object where we set the
    // built-in global functions.
    v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
    global->Set(v8::String::NewFromUtf8(isolate, "log", v8::NewStringType::kNormal).ToLocalChecked(),
                v8::FunctionTemplate::New(isolate, LogCallback));

    // Create a new context.
    v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);

    // Enter the context for compiling and running the hello world script.
    v8::Context::Scope context_scope(context);

    // Create a string containing the JavaScript source code.
     const char * script_str = R"S(
       log("Hello wooorld!");
       0;
     )S";

//      Local<String> source = String::NewFromUtf8(isolate, "'Hello' + ', World!'", NewStringType::kNormal).ToLocalChecked();
    v8::Local<v8::String> source = v8::String::NewFromUtf8(isolate, script_str, v8::NewStringType::kNormal).ToLocalChecked();

    // Compile the source code.
    v8::Local<v8::Script> script = v8::Script::Compile(context, source).ToLocalChecked();

    // Run the script to get the result.
    v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();

    // Convert the result to an UTF8 string and print it.
    v8::String::Utf8Value utf8(result);
    printf("return value = %s\n", *utf8);
  }

  // Dispose the isolate and tear down V8.
  isolate->Dispose();
  v8::V8::Dispose();
  v8::V8::ShutdownPlatform();
  delete platform;
  delete create_params.array_buffer_allocator;
  return 0;
}
