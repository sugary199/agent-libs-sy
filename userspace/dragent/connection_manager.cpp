#include "connection_manager.h"

#include "logger.h"
#include "protocol.h"
#include "draios.pb.h"
#include "utils.h"

#ifndef TCP_USER_TIMEOUT
// Define it here because old glibc versions do not have this flag (eg, Centos6)
#define TCP_USER_TIMEOUT	 18 /* How long for loss retry before timeout */
#endif

const string connection_manager::m_name = "connection_manager";
const chrono::seconds connection_manager::WORKING_INTERVAL_S(10);
const uint32_t connection_manager::RECONNECT_MIN_INTERVAL_S = 1;
const uint32_t connection_manager::RECONNECT_MAX_INTERVAL_S = 60;

connection_manager::connection_manager(dragent_configuration* configuration,
				       protocol_queue* queue,
				       sinsp_worker* sinsp_worker,
				       capture_job_handler *capture_job_handler) :
	m_socket(NULL),
	m_connected(false),
	m_buffer(RECEIVER_BUFSIZE),
	m_buffer_used(0),
	m_configuration(configuration),
	m_queue(queue),
	m_sinsp_worker(sinsp_worker),
	m_capture_job_handler(capture_job_handler),
	m_last_loop_ns(0),
	m_reconnect_interval(0)
{
	Poco::Net::initializeSSL();
}

connection_manager::~connection_manager()
{
	Poco::Net::uninitializeSSL();
}

bool connection_manager::init()
{
	if(m_configuration->m_server_addr != "" && m_configuration->m_server_port != 0)
	{
		if(m_configuration->m_ssl_enabled)
		{
			g_log->information("SSL enabled, initializing context");

			Poco::Net::Context::VerificationMode verification_mode;

			if(m_configuration->m_ssl_verify_certificate)
			{
				verification_mode = Poco::Net::Context::VERIFY_STRICT;
			}
			else
			{
				verification_mode = Poco::Net::Context::VERIFY_NONE;
			}

			Poco::Net::Context::Ptr ptrContext = new Poco::Net::Context(
				Poco::Net::Context::CLIENT_USE,
				"",
				"",
				m_configuration->m_ssl_ca_certificate,
				verification_mode,
				9,
				false,
				"ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

			Poco::Net::SSLManager::instance().initializeClient(0, 0, ptrContext);
		}

		return true;
	}
	else
	{
		g_log->warning("Server address has not been specified");
		return false;
	}
}

bool connection_manager::connect()
{
	try
	{
		ASSERT(m_socket.isNull());

		SocketAddress sa(m_configuration->m_server_addr, m_configuration->m_server_port);

		g_log->information("Connecting to collector " + sa.toString());

		if(m_configuration->m_ssl_enabled)
		{
			m_socket = new Poco::Net::SecureStreamSocket();

			((Poco::Net::SecureStreamSocket*) m_socket.get())->setLazyHandshake(true);
			((Poco::Net::SecureStreamSocket*) m_socket.get())->setPeerHostName(m_configuration->m_server_addr);
			((Poco::Net::SecureStreamSocket*) m_socket.get())->connect(sa, SOCKET_TIMEOUT_DURING_CONNECT_US);
		}
		else
		{
			m_socket = new Poco::Net::StreamSocket();
			m_socket->connect(sa, SOCKET_TIMEOUT_DURING_CONNECT_US);
		}

		if(m_configuration->m_ssl_enabled)
		{
			//
			// This is done to prevent getting stuck forever waiting during the handshake
			// if the server doesn't speak to us
			//
			m_socket->setSendTimeout(SOCKET_TIMEOUT_DURING_CONNECT_US);
			m_socket->setReceiveTimeout(SOCKET_TIMEOUT_DURING_CONNECT_US);

			int32_t ret = ((Poco::Net::SecureStreamSocket*) m_socket.get())->completeHandshake();
			if(ret != 1)
			{
				g_log->error(m_name + ": SSL Handshake didn't complete");
				disconnect();
				return false;
			}

			((Poco::Net::SecureStreamSocket*) m_socket.get())->verifyPeerCertificate();

			g_log->information("SSL identity verified");
		}

		//
		// Set the send buffer size for the socket
		//
		m_socket->setSendBufferSize(m_configuration->m_transmitbuffer_size);
		m_socket->setSendTimeout(SOCKET_TIMEOUT_AFTER_CONNECT_US);
		m_socket->setReceiveTimeout(SOCKET_TIMEOUT_AFTER_CONNECT_US);
		try
		{
			// This option makes the connection fail earlier in case of unplugged cable
			m_socket->setOption(IPPROTO_TCP, TCP_USER_TIMEOUT, SOCKET_TCP_TIMEOUT_MS);
		}
		catch(std::exception&)
		{
			// ignore if kernel does not support this
			// alternatively, could be a setsockopt() call to avoid exception
		}

		g_log->information("Connected to collector");
		m_connected = true;
		return true;
	}
	catch(Poco::IOException& e)
	{
		g_log->error(m_name + ":connect():IOException: " + e.displayText());
		disconnect();
		return false;
	}
	catch(Poco::TimeoutException& e)
	{
		g_log->error(m_name + ":connect():Timeout: " + e.displayText());
		disconnect();
		return false;
	}
	return false;
}

void connection_manager::disconnect()
{
	if(chrono::system_clock::now() - m_last_connection_failure >= WORKING_INTERVAL_S)
	{
		m_reconnect_interval = RECONNECT_MIN_INTERVAL_S;
	}
	else
	{
		m_reconnect_interval = std::min(std::max(connection_manager::RECONNECT_MIN_INTERVAL_S, m_reconnect_interval * 2), RECONNECT_MAX_INTERVAL_S);
	}

	if(!m_socket.isNull())
	{
		m_socket->close();
		m_socket = NULL;
		m_connected = false;
		m_buffer_used = 0;
	}
}

void connection_manager::run()
{
	m_pthread_id = pthread_self();

	g_log->information(m_name + ": Starting");

	if(init())
	{
		std::shared_ptr<protocol_queue_item> item;

		while(!dragent_configuration::m_terminate)
		{
			m_last_loop_ns = sinsp_utils::get_current_time_ns();

			//
			// Make sure we have a valid connection
			//
			if(m_socket.isNull())
			{
				g_log->information(string("Waiting to connect ") + std::to_string(m_reconnect_interval) + " s");
				for(uint32_t waited_time = 0; waited_time < m_reconnect_interval && !dragent_configuration::m_terminate; ++waited_time)
				{
					m_last_loop_ns = sinsp_utils::get_current_time_ns();
					Thread::sleep(1000);
				}

				if(dragent_configuration::m_terminate)
				{
					break;
				}

				m_last_connection_failure = chrono::system_clock::now();

				if(!connect())
				{
					continue;
				}
			}

			//
			// Check if we received a message. We do it before so nothing gets lost if ELBs
			// still negotiates a connection and then sends us out at the first read/write
			//
			receive_message();

			if(!item)
			{
				//
				// Wait 300ms to get a message from the queue
				//
				m_queue->get(&item, 300);
			}

			if(item)
			{
				//
				// Got a message, transmit it
				//
				if(transmit_buffer(m_last_loop_ns, item))
				{
					item = NULL;
				}
			}
		}
	}

	g_log->information(m_name + ": Terminating");
}

bool connection_manager::transmit_buffer(uint64_t now, std::shared_ptr<protocol_queue_item> &item)
{
	// Sometimes now can be less than ts_ns. The timestamp in
	// metrics messages is rounded up to the following metrics
	// interval.

	if (now > item->ts_ns &&
	    (now - item->ts_ns) > 5000000000UL)
	{
		g_log->warning("Transmitting delayed message. type=" + to_string(item->message_type)
			       + ", now=" + to_string(now)
			       + ", ts=" + to_string(item->ts_ns)
			       + ", delay_ms=" + to_string((now - item->ts_ns)/ 1000000.0));
	}

	try
	{
		if(m_socket.isNull())
		{
			return false;
		}

		int32_t res = m_socket->sendBytes(item->buffer.data(), item->buffer.size());
		if(res == Poco::Net::SecureStreamSocket::ERR_SSL_WANT_READ ||
			res == Poco::Net::SecureStreamSocket::ERR_SSL_WANT_WRITE)
		{
			return false;
		}

		if(res != (int32_t) item->buffer.size())
		{
			g_log->error(m_name + ": sendBytes sent just "
				+ NumberFormatter::format(res)
				+ ", expected " + NumberFormatter::format(item->buffer.size()));

			disconnect();

			ASSERT(false);
			return false;
		}

		g_log->information(m_name + ": Sent msgtype="
				   + to_string((int) item->message_type)
				   + " len="
				   + Poco::NumberFormatter::format(item->buffer.size()) + " to collector");

		return true;
	}
	catch(Poco::IOException& e)
	{
		// When the output buffer gets full sendBytes() results in
		// a TimeoutException for SSL connections and EWOULDBLOCK for non-SSL
		// connections, so we'll treat them the same.
		if ((e.code() == POCO_EWOULDBLOCK) || (e.code() == POCO_EAGAIN))
		{
			// We shouldn't risk hanging indefinitely if the EWOULDBLOCK is
			// caused by an attempted send larger than the buffer size
			if (item->buffer.size() > m_configuration->m_transmitbuffer_size)
			{
				g_log->error(m_name + ":transmit larger than bufsize failed ("
					+ NumberFormatter::format(item->buffer.size()) + ">" +
					NumberFormatter::format(m_configuration->m_transmitbuffer_size)
					 + "): " + e.displayText());
				disconnect();
			}
			else
			{
				g_log->debug(m_name + ":transmit: Ignoring: " + e.displayText());
			}
		}
		else
		{
			g_log->error(m_name + ":transmit:IOException: " + e.displayText());
			disconnect();
		}
	}
	catch(Poco::TimeoutException& e)
	{
		g_log->debug(m_name + ":transmit:Timeout: " + e.displayText());
	}

	return false;
}

void connection_manager::receive_message()
{
	try
	{
		if(m_socket.isNull())
		{
			return;
		}

		// If the socket has nothing readable, return
		// immediately. This ensures that when the queue has
		// multiple items queued we don't limit the rate at
		// which we dequeue and send messages.
		if (!m_socket->poll(Poco::Timespan(), Poco::Net::Socket::SELECT_READ))
		{
			return;
		}

		if(m_buffer_used == 0)
		{
			// We begin by reading and processing the protocol header
			int32_t res = m_socket->receiveBytes(m_buffer.begin(), sizeof(dragent_protocol_header), MSG_WAITALL);
			if(res == Poco::Net::SecureStreamSocket::ERR_SSL_WANT_READ ||
			   res == Poco::Net::SecureStreamSocket::ERR_SSL_WANT_WRITE)
			{
				return;
			}

			if(res == 0)
			{
				g_log->error(m_name + ": Lost connection (reading header)");
				disconnect();
				return;
			}

			// TODO: clean up buffering of received data. Remains of protocol
			// header might come in the next recv().
			if(res != sizeof(dragent_protocol_header))
			{
				g_log->error(m_name + ": Protocol error: couldn't read full header: " + NumberFormatter::format(res));
				ASSERT(false);
				disconnect();
				return;
			}

			dragent_protocol_header* header = (dragent_protocol_header*) m_buffer.begin();
			header->len = ntohl(header->len);

			if((header->len < sizeof(dragent_protocol_header)) ||
				(header->len > MAX_RECEIVER_BUFSIZE))
			{
				g_log->error(m_name + ": Protocol error: invalid header length " + NumberFormatter::format(header->len));
				ASSERT(false);
				disconnect();
				return;
			}

			if(header->len > m_buffer.size())
			{
				m_buffer.resize(header->len);
			}

			m_buffer_used = sizeof(dragent_protocol_header);
		}

		// Then we read the actual message, it may arrive in
		// several chunks, in this case the function will be called
		// at the next loop cycle and will continue reading
		auto header = (dragent_protocol_header*) m_buffer.begin();
		auto res = m_socket->receiveBytes(
				m_buffer.begin() + m_buffer_used,
				header->len - m_buffer_used,
				MSG_WAITALL);

		if(res == Poco::Net::SecureStreamSocket::ERR_SSL_WANT_READ ||
			res == Poco::Net::SecureStreamSocket::ERR_SSL_WANT_WRITE)
		{
			return;
		}

		if(res == 0)
		{
			g_log->error(m_name + ": Lost connection (reading message)");
			disconnect();
			ASSERT(false);
			return;
		}

		if(res < 0)
		{
			g_log->error(m_name + ": Connection error while reading: " +
				NumberFormatter::format(res));
			disconnect();
			ASSERT(false);
			return;
		}

		m_buffer_used += res;
		g_log->debug(m_name + ": Receiving message version=" + NumberFormatter::format(header->version) +
					 " len=" + NumberFormatter::format(header->len) + " messagetype=" + NumberFormatter::format(header->messagetype) +
					" received=" + NumberFormatter::format(m_buffer_used));

		if(m_buffer_used == header->len)
		{
			m_buffer_used = 0;

			if(header->version != dragent_protocol::PROTOCOL_VERSION_NUMBER)
			{
				g_log->error(m_name + ": Received command for incompatible version protocol "
							 + NumberFormatter::format(header->version));
				ASSERT(false);
				return;
			}

			// When the message is complete, process it
			// and reset the buffer
			g_log->information(m_name + ": Received command "
							   + NumberFormatter::format(header->messagetype)
							   + " (" + draiosproto::message_type_Name((draiosproto::message_type) header->messagetype) + ")");

			switch(header->messagetype)
			{
			case draiosproto::message_type::DUMP_REQUEST_START:
				handle_dump_request_start(
						m_buffer.begin() + sizeof(dragent_protocol_header),
						header->len - sizeof(dragent_protocol_header));
				break;
			case draiosproto::message_type::DUMP_REQUEST_STOP:
				handle_dump_request_stop(
						m_buffer.begin() + sizeof(dragent_protocol_header),
						header->len - sizeof(dragent_protocol_header));
				break;
			case draiosproto::message_type::CONFIG_DATA:
				handle_config_data(
						m_buffer.begin() + sizeof(dragent_protocol_header),
						header->len - sizeof(dragent_protocol_header));
				break;
			case draiosproto::message_type::ERROR_MESSAGE:
				handle_error_message(
					m_buffer.begin() + sizeof(dragent_protocol_header),
					header->len - sizeof(dragent_protocol_header));
				break;
#ifndef CYGWING_AGENT
			case draiosproto::message_type::POLICIES:
				handle_policies_message(
					m_buffer.begin() + sizeof(dragent_protocol_header),
					header->len - sizeof(dragent_protocol_header));
				break;
			case draiosproto::message_type::COMP_CALENDAR:
				handle_compliance_calendar_message(
					m_buffer.begin() + sizeof(dragent_protocol_header),
					header->len - sizeof(dragent_protocol_header));
				break;
			case draiosproto::message_type::ORCHESTRATOR_EVENTS:
				handle_orchestrator_events(
					m_buffer.begin() + sizeof(dragent_protocol_header),
					header->len - sizeof(dragent_protocol_header));
				break;
			case draiosproto::message_type::BASELINES:
				handle_baselines_message(
					m_buffer.begin() + sizeof(dragent_protocol_header),
					header->len - sizeof(dragent_protocol_header));
				break;
#endif
			default:
				g_log->error(m_name + ": Unknown message type: "
							 + NumberFormatter::format(header->messagetype));
				ASSERT(false);
			}
		}

		if(m_buffer_used > header->len)
		{
			g_log->error(m_name + ": Protocol out of sync, disconnecting");
			disconnect();
			ASSERT(false);
			return;
		}
	}
	catch(Poco::IOException& e)
	{
		g_log->error(m_name + ":receive:IOException: " + e.displayText());
		disconnect();
	}
	catch(Poco::TimeoutException& e)
	{
		g_log->debug(m_name + ":receive:Timeout: " + e.displayText());
	}
}

void connection_manager::handle_dump_request_start(uint8_t* buf, uint32_t size)
{
	draiosproto::dump_request_start request;
	if(!dragent_protocol::buffer_to_protobuf(buf, size, &request))
	{
		return;
	}

	std::shared_ptr<capture_job_handler::dump_job_request> job_request =
		make_shared<capture_job_handler::dump_job_request>();

	job_request->m_start_details = make_unique<capture_job_handler::start_job_details>();

	job_request->m_request_type = capture_job_handler::dump_job_request::JOB_START;
	job_request->m_token = request.token();

	if(request.has_filters())
	{
		job_request->m_start_details->m_filter = request.filters();
	}

	if(request.has_duration_ns())
	{
		job_request->m_start_details->m_duration_ns = request.duration_ns();
	}

	if(request.has_max_size())
	{
		job_request->m_start_details->m_max_size = request.max_size();
	}

	if(request.has_past_duration_ns())
	{
		job_request->m_start_details->m_past_duration_ns = request.past_duration_ns();
	}

	if(request.has_past_size())
	{
		job_request->m_start_details->m_past_size = request.past_size();
	}

	// Note: sending request via sinsp_worker so it can add on
	// needed state (e.g. sinsp_dumper)
	m_sinsp_worker->queue_job_request(job_request);
}

void connection_manager::handle_dump_request_stop(uint8_t* buf, uint32_t size)
{
	draiosproto::dump_request_stop request;
	if(!dragent_protocol::buffer_to_protobuf(buf, size, &request))
	{
		return;
	}

	std::shared_ptr<capture_job_handler::dump_job_request> job_request =
		make_shared<capture_job_handler::dump_job_request>();

	job_request->m_stop_details = make_unique<capture_job_handler::stop_job_details>();

	job_request->m_request_type = capture_job_handler::dump_job_request::JOB_STOP;
	job_request->m_token = request.token();

	// For captures created by the connection manager,
	// m_defer_send is never true, so there isn't any need to
	// worry about stopping a deferred capture. But set this for
	// completeness.
	job_request->m_stop_details->m_remove_unsent_job = false;

	// This could go directly to the capture handler as there's no
	// need to add any state when stopping a job. However, still
	// sending it via the sinsp_worker so there's no chance of the
	// stop message arriving at the capture handler before the
	// start. (Unlikely, but just being safe).
	m_sinsp_worker->queue_job_request(job_request);
}

void connection_manager::handle_config_data(uint8_t* buf, uint32_t size)
{
	if(m_configuration->m_auto_config)
	{
		draiosproto::config_data request;
		if(!dragent_protocol::buffer_to_protobuf(buf, size, &request))
		{
			return;
		}
		for(const auto& config_file_proto : request.config_files())
		{
			std::string errstr;

			if(m_configuration->save_auto_config(config_file_proto.name(),
							     config_file_proto.content(),
							     errstr) < 0)
			{
				g_log->error(errstr);
			}
		}
	}
	else
	{
		g_log->debug("Auto config disabled, ignoring CONFIG_DATA message");
	}
}

void connection_manager::handle_error_message(uint8_t* buf, uint32_t size) const
{
	draiosproto::error_message err_msg;
	if(!dragent_protocol::buffer_to_protobuf(buf, size, &err_msg))
	{
		g_log->error(m_name + ": received unparsable error message");
		return;
	}

	string err_str = "unknown error";
	bool term = false;

	// Log as much useful info as possible from the error_message
	if(err_msg.has_type())
	{
		const draiosproto::error_type err_type = err_msg.type();
		if (draiosproto::error_type_IsValid(err_type)) {
			err_str = draiosproto::error_type_Name(err_type);

			if(err_msg.has_description() && !err_msg.description().empty())
			{
				err_str += " (" + err_msg.description() + ")";
			}

			if(err_type == draiosproto::error_type::ERR_INVALID_CUSTOMER_KEY ||
			   err_type == draiosproto::error_type::ERR_PROTO_MISMATCH)
			{
				term = true;
				err_str += ", terminating the agent";
			}
		}
		else
		{
			err_str = ": received invalid error type: " + std::to_string(err_type);
		}
	}

	g_log->error(m_name + ": received " + err_str);

	if(term)
	{
		dragent_configuration::m_terminate = true;
	}
}

#ifndef CYGWING_AGENT
void connection_manager::handle_policies_message(uint8_t* buf, uint32_t size)
{
	draiosproto::policies policies;
	string errstr;

	if(!m_configuration->m_security_enabled)
	{
		g_log->debug("Security disabled, ignoring POLICIES message");
		return;
	}

	if(m_configuration->m_security_policies_file != "")
	{
		g_log->information("Security policies file configured in dragent.yaml, ignoring POLICIES message");
		return;
	}

	if(!dragent_protocol::buffer_to_protobuf(buf, size, &policies))
	{
		g_log->error("Could not parse policies message");
		return;
	}

	if (!m_sinsp_worker->load_policies(policies, errstr))
	{
		g_log->error("Could not load policies message: " + errstr);
		return;
	}
}

void connection_manager::handle_compliance_calendar_message(uint8_t* buf, uint32_t size)
{
	draiosproto::comp_calendar calendar;
	string errstr;

	if(!m_configuration->m_security_enabled)
	{
		g_log->debug("Security disabled, ignoring COMP_CALENDAR message");
		return;
	}

	if(m_configuration->m_security_compliance_schedule != "")
	{
		g_log->information("Security compliance schedule configured in dragent.yaml, ignoring COMP_CALENDAR message");
		return;
	}

	if(!dragent_protocol::buffer_to_protobuf(buf, size, &calendar))
	{
		g_log->error("Could not parse comp_calendar message");
		return;
	}

	if (!m_sinsp_worker->set_compliance_calendar(calendar, errstr))
	{
		g_log->error("Could not set compliance calendar: " + errstr);
		return;
	}
}
#endif

#ifndef CYGWING_AGENT
void connection_manager::handle_orchestrator_events(uint8_t* buf, uint32_t size)
{
	draiosproto::orchestrator_events evts;

	if(!m_configuration->m_security_enabled)
	{
		g_log->debug("Security disabled, ignoring ORCHESTRATOR_EVENTS message");
		return;
	}

	if(!dragent_protocol::buffer_to_protobuf(buf, size, &evts))
	{
		g_log->error("Could not parse orchestrator_events message");
		return;
	}

	m_sinsp_worker->receive_hosts_metadata(evts);
}
#endif

#ifndef CYGWING_AGENT
void connection_manager::handle_baselines_message(uint8_t* buf, uint32_t size)
{
	draiosproto::baselines baselines;
	string errstr;

	if(!m_configuration->m_security_enabled)
	{
		g_log->debug("Security disabled, ignoring BASELINES message");
		return;
	}

	if(m_configuration->m_security_baselines_file != "")
	{
		g_log->information("Security baselines file configured in dragent.yaml, ignoring BASELINES message");
		return;
	}

	if(!dragent_protocol::buffer_to_protobuf(buf, size, &baselines))
	{
		g_log->error("Could not parse baselines message");
		return;
	}

	if (!m_sinsp_worker->load_baselines(baselines, errstr))
	{
		g_log->error("Could not load baselines message: " + errstr);
		return;
	}
}
#endif
