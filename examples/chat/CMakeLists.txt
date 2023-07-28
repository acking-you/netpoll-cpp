add_executable(chat_server_example chat_server.cc context.cc)

target_link_libraries(chat_server_example netpoll)

add_executable(chat_client_example chat_client.cc context.cc)

target_link_libraries(chat_client_example netpoll)
