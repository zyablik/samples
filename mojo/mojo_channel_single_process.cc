#include "mojo/edk/embedder/embedder.h"

// inspired by mojo/edk/embedder/embedder_unittest.cc

int main(int argc, const char ** argv) {
  mojo::edk::Init();

  uint32_t result;
  MojoHandle server_mp, client_mp;
  result = MojoCreateMessagePipe(nullptr, &server_mp, &client_mp);
  printf("MojoCreateMessagePipe resutl = %d server_mp = %d client_mp = %d\n", result, server_mp, client_mp);

  // basic channel read/write

  const std::string kHello = "hello";
  result = MojoWriteMessage(server_mp, kHello.data(), static_cast<uint32_t>(kHello.size()), nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  printf("MojoWriteMessage result = %d\n", result);
  
  printf("before MojoWait\n");
  result = MojoWait(client_mp, MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE, nullptr);
  printf("after MojoWait result = %d\n", result);

  uint32_t message_size = 0;
  result = MojoReadMessage(client_mp, nullptr, &message_size, nullptr, nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("MojoReadMessage: get msg size: result = %d MOJO_RESULT_RESOURCE_EXHAUSTED = %d message_size = %u\n", result, MOJO_RESULT_RESOURCE_EXHAUSTED, message_size);

  std::string message(message_size, 'x');
  MojoReadMessage(client_mp, &message[0], &message_size, nullptr, nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("MojoReadMessage get msg: result = %d message = %s message_size = %d message.size() = %lu\n", result, message.c_str(), message_size, message.size());
  
  // passing mojo handle
  MojoHandle server_mp2, client_mp2;
  MojoCreateMessagePipe(nullptr, &server_mp2, &client_mp2);
  printf("MojoCreateMessagePipe resutl = %d server_mp2 = %d client_mp2 = %d\n", result, server_mp2, client_mp2);

  result = MojoWriteMessage(server_mp2, nullptr, 0, &client_mp, 1, MOJO_WRITE_MESSAGE_FLAG_NONE);
  printf("MojoWriteMessage(server_mp2, client_mp) result = %d\n", result);

  const std::string kHello2 = "hello2";
  result = MojoWriteMessage(server_mp, kHello2.data(), static_cast<uint32_t>(kHello2.size()), nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  printf("MojoWriteMessage(server_mp, %s) result = %d\n", kHello2.c_str(), result);

  printf("before MojoWait(client_mp2)\n");
  MojoHandleSignalsState state;
  MojoWait(client_mp2, MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE, &state);
  printf("after MojoWait state.satisfied_signals = 0x%x\n", state.satisfied_signals);

  uint32_t num_handles = 0;
  message_size = 0;
  result = MojoReadMessage(client_mp2, nullptr, &message_size, nullptr, &num_handles, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("MojoReadMessage client_mp2 result = %d message_size = %d num_handles = %d\n", result, message_size, num_handles);

  MojoHandle port;
  result = MojoReadMessage(client_mp2, nullptr, nullptr, &port, &num_handles, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("MojoReadMessage client_mp2 result = %d  port = %d\n", result, port);

  printf("before MojoWait(port)\n");
  result = MojoWait(port, MOJO_HANDLE_SIGNAL_READABLE, MOJO_DEADLINE_INDEFINITE, nullptr);
  printf("after MojoWait(port) result = %d\n", result);
  
  result = MojoReadMessage(port, nullptr, &message_size, nullptr, nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("MojoReadMessage(port) result = %d, message_size = %u\n", result, message_size);
  
  std::string message2(message_size, 'x');
  result = MojoReadMessage(port, &message2[0], &message_size, nullptr, nullptr, MOJO_READ_MESSAGE_FLAG_NONE);
  printf("MojoReadMessage(port) result = %d, message2 = %s\n", result, message2.c_str());
  
  result = MojoClose(server_mp2);
  printf("close server_mp2 resutl = %d MOJO_RESULT_OK = %d\n", result, MOJO_RESULT_OK);
  result = MojoClose(client_mp2);
  printf("close client_mp2 resutl = %d MOJO_RESULT_OK = %d\n", result, MOJO_RESULT_OK);
  result = MojoClose(server_mp);
  printf("close server_mp resutl = %d MOJO_RESULT_OK = %d\n", result, MOJO_RESULT_OK);
  result = MojoClose(client_mp);
  printf("close client_mp resutl = %d MOJO_RESULT_OK = %d\n", result, MOJO_RESULT_OK);

  return 0;
}
