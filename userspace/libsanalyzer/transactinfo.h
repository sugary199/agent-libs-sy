#ifdef HAS_ANALYZER

#pragma once

#include <stack>

template<class T> class sinsp_fdinfo;
class sinsp_transaction_manager;
class sinsp_connection;
class sinsp_analyzer;
class sinsp_threadinfo;
class sinsp;
class sinsp_evt;
class sinsp_protocol_parser;

//
// Transaction information class
//
class SINSP_PUBLIC sinsp_partial_transaction
{
public:
	enum type
	{
	    TYPE_UNKNOWN,
	    TYPE_IP,
	    TYPE_HTTP,
	};

	enum family
	{
	    IP,
	    UNIX
	};

	enum direction
	{
	    DIR_UNKNOWN,
	    DIR_IN,
	    DIR_OUT,
	    DIR_CLOSE,  // Not technically a direction, indicates that the connection is being closed
	};

	enum updatestate
	{
	    STATE_ONGOING = (1 << 0),
	    STATE_SWITCHED = (1 << 1),
	    STATE_NO_TRANSACTION = (1 << 2),	// Set when, based on timing observation, this is detected as 
											// not being a client/server transaction.
	};

	sinsp_partial_transaction();
	~sinsp_partial_transaction();
	void reset();
	void update(sinsp_analyzer* analyzer, 
		sinsp_threadinfo* ptinfo,
		void* fdinfo,
		sinsp_connection* pconn,
		uint64_t enter_ts, 
		uint64_t exit_ts, 
		int32_t cpuid,
		direction dir,
#if _DEBUG
		sinsp_evt *evt,
		uint64_t fd,
#endif
		char *data,
		uint32_t original_len, 
		uint32_t len);
	void mark_active_and_reset(sinsp_partial_transaction::type newtype);
	void mark_inactive();
	bool is_active()
	{
		return m_type != type::TYPE_UNKNOWN;
	}

	bool is_ipv4_flow()
	{
		return m_family == family::IP;
	}

	bool is_unix_flow()
	{
		return m_family == family::UNIX;
	}

	sinsp_partial_transaction::type m_type;
	direction m_direction;
	int64_t m_tid;
	int64_t m_fd;
	vector<string> m_protoinfo;

	uint64_t m_start_time;
	uint64_t m_end_time;
	uint64_t m_start_of_transaction_time;

	direction m_prev_direction;
	uint64_t m_prev_start_time;
	uint64_t m_prev_end_time;
	uint64_t m_prev_start_of_transaction_time;
	uint64_t m_prev_prev_start_time;
	uint64_t m_prev_prev_end_time;
	uint64_t m_prev_prev_start_of_transaction_time;
	family m_family;
	uint32_t m_incoming_bytes;
	uint32_t m_outgoing_bytes;
	int32_t m_cpuid;
	uint32_t m_flags;
	sinsp_protocol_parser* m_protoparser;

private:
	inline sinsp_partial_transaction::updatestate update_int(uint64_t enter_ts, 
		uint64_t exit_ts, direction dir, 
		char* data, uint32_t original_len, uint32_t len, 
		bool is_server);
};

//
// Resolved process information for a transaction
//
class SINSP_PUBLIC sinsp_transaction
{
public:
	sinsp_partial_transaction m_trinfo;

	uint64_t m_pid;
	string m_comm;
	uint64_t m_peer_tid;
	uint64_t m_peer_fd;
	uint64_t m_peer_pid;
	string m_peer_comm;

	string m_fd_desc;
};

//
// This little class describes an entry in the per-cpu transaction list that
// is consumed when a sample is created
//
class SINSP_PUBLIC sinsp_trlist_entry
{
public:
	enum flags
	{
	    FL_NONE = 0,
	    FL_FILTERED_OUT = (1 << 0),
	    FL_EXTERNAL = (1 << 1),
	};

	sinsp_trlist_entry(uint64_t stime, uint64_t etime, flags f)
	{
		m_stime = stime;
		m_etime = etime;
		m_flags = f;
	}

	uint64_t m_stime;	// start time
	uint64_t m_etime;	// end time
	int32_t m_flags;	// pid of the program main process
};

struct sinsp_trlist_entry_comparer
{
    bool operator() (const sinsp_trlist_entry& first, const sinsp_trlist_entry& second) const 
	{
		return first.m_stime < second.m_stime;
	}
};

//
// Transaction table class
// This stores the transactions that have been completed and can be accessed by the user.
//
class SINSP_PUBLIC sinsp_transaction_table
{
public:
	sinsp_transaction_table(sinsp* inspector);
	~sinsp_transaction_table();
	uint32_t get_size();
	void clear();

	void emit(sinsp_threadinfo* ptinfo, 
		void* fdinfo,
		sinsp_connection* pconn,
		sinsp_partial_transaction* tr,
#if _DEBUG
		sinsp_evt *evt,
		uint64_t fd,
		uint64_t ts
#endif
	);

	//
	// Stores the global list of transactions.
	// Key is the tid
	//
	unordered_map<int64_t, vector<sinsp_transaction > > m_table;
	uint32_t m_n_client_transactions;
	uint32_t m_n_server_transactions;

private:
	bool is_transaction_server(sinsp_threadinfo *ptinfo);

	sinsp* m_inspector;

	friend class sinsp_partial_transaction;
};

#endif
