#pragma once

#include "main.h"
#include "configuration.h"
#include "blocking_queue.h"
#include "sinsp_worker.h"

class connection_manager : public Runnable
{
public:
	connection_manager(dragent_configuration* configuration, 
		protocol_queue* queue, sinsp_worker* sinsp_worker);
	~connection_manager();

	bool init();
	void run();

private:
	bool connect();
	void disconnect();
	bool transmit_buffer(const char* buffer, uint32_t buflen);
	void receive_message();
	void handle_dump_request(uint8_t* buf, uint32_t size);
	void handle_ssh_open_channel(uint8_t* buf, uint32_t size);
	void handle_ssh_data(uint8_t* buf, uint32_t size);
	void handle_ssh_close_channel(uint8_t* buf, uint32_t size);
	void handle_auto_update();

	static const uint32_t RECEIVER_BUFSIZE = 32 * 1024;
	static const uint32_t SOCKET_TIMEOUT_DURING_CONNECT_US = 60 * 1000 * 1000;
	static const uint32_t SOCKET_TIMEOUT_AFTER_CONNECT_US = 100 * 1000;
	static const string m_name;

	SharedPtr<SocketAddress> m_sa;
	SharedPtr<StreamSocket> m_socket;
	Buffer<uint8_t> m_buffer;
	dragent_configuration* m_configuration;
	protocol_queue* m_queue;
	sinsp_worker* m_sinsp_worker;
};
