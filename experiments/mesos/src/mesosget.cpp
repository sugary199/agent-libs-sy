//
// mesosget.cpp
//
// extracts needed data from the mesos REST API interface,
// translates it to protobuf and prints the result in human readable format
//
// usage: mesosget [http://localhost:80] [v1]
//

#include "sinsp.h"
#include "mesos_proto.h"
#include "mesos_common.h"
#include "mesos_http.h"
#include "mesos.h"
#include "Poco/FileStream.h"
#include "Poco/StreamCopier.h"
#include <unistd.h>

using namespace Poco;

sinsp_logger g_logger;

void print_groups(const ::google::protobuf::RepeatedPtrField< ::draiosproto::marathon_group >& groups)
{
	for(auto group : groups)
	{
		std::cout << group.id() << std::endl;
		for(const auto& app : group.apps())
		{
			std::cout << '\t' << app.id() << std::endl;
			for(const auto& task : app.task_ids())
			{
				std::cout << "\t\t" <<  task << std::endl;
			}
		}
		for(auto subgroup : group.groups())
		{
			print_groups(groups);
		}
	}
}

void print_proto(mesos& m, const std::string& fname = "")
{
	draiosproto::metrics met;
	mesos_proto(met).get_proto(m.get_state());
	//FileOutputStream fos("/home/alex/sysdig/agent/experiments/mesos/" + fname + ".protodump");
	//fos << met.DebugString();
	std::cout << met.DebugString() << std::endl;
	//std::cout << "++++" << std::endl;
	//print_groups(met.mesos().groups());
	//std::cout << "----" << std::endl;
}
/*
mesos* init_mesos_client(string* api_server, bool verbose)
{
	m_verbose_json = verbose;
	if(m_mesos_client == NULL)
	{
		if(api_server)
		{
			// -m <url[,marathon_url]>
			std::string::size_type pos = api_server->find(',');
			if(pos != std::string::npos)
			{
				m_marathon_api_server.clear();
				m_marathon_api_server.push_back(api_server->substr(pos + 1));
			}
			m_mesos_api_server = api_server->substr(0, pos);
		}

		bool is_live = !m_mesos_api_server.empty();
		return new mesos(m_mesos_api_server, mesos::default_state_api,
									m_marathon_api_server,
									mesos::default_groups_api,
									mesos::default_apps_api,
									false, // no leader follow
									mesos::default_timeout_ms,
									is_live,
									m_verbose_json);
	}
}
*/

std::string get_json(const std::string& component)
{
	std::ostringstream json;
	std::string fname = "./test/";
	try
	{
		FileInputStream fis(fname.append(component).append((".json")));
		StreamCopier::copyStream(fis, json);
	}
	catch(FileNotFoundException& ex)
	{
		std::cout << "File not found: " << fname << std::endl;
		return "";
	}
	return json.str();
}

int main(int argc, char** argv)
{
	try
	{
		mesos m(get_json("state"), get_json("groups"), get_json("apps"));
		print_proto(m);
	}
	catch(std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
	}
#if 0
	std::string ip_addr = "52.90.231.127";
	std::vector<std::string> marathon_uris;
	marathon_uris.push_back("http://" + ip_addr + ":8080");
	mesos* m = new mesos("http://" + ip_addr + ":5050", "/master/state",
		marathon_uris,
		mesos::default_groups_api,
		mesos::default_apps_api,
		false, // no leader follow
		mesos::default_timeout_ms,
		true, // is live
		true/*verbose json*/);
	
	time_t last_mesos_refresh = 0;

	while(true)
	{
		std::cout << "<<<<<< receiving ..." <<  std::endl;
		m->collect_data();
		time_t now;
		time(&now);
		if(difftime(now, last_mesos_refresh) > 10)
		{
			m->send_data_request();
			last_mesos_refresh = now;
		}
		print_proto(*m, ip_addr);
		sleep(1);
	}
/*
	//print_proto(m, ip_addr);

	//m.refresh(true);
	//print_proto(*m, ip_addr);
	while(true)
	{
		if(!m->is_alive())
		{
			delete m;
			m = 0;
			m = new mesos("http://" + ip_addr + ":5050", "/master/state", 
				marathon_uris,
				mesos::default_groups_api,
				mesos::default_apps_api,
				mesos::default_watch_api);
		}
		m->refresh();
		print_proto(*m, ip_addr);
		
		//m->watch_marathon();
		std::cout << "----------------------" << std::endl;
		sleep(3);
	}
*/
#endif
	return 0;
}
