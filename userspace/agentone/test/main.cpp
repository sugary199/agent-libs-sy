#include <gtest.h>
#include <string>
#include "common_logger.h"
#include "avoid_block_channel.h"
#include "globally_readable_file_channel.h"
#include <Poco/AutoPtr.h>
#include <Poco/Channel.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/File.h>
#include <Poco/Formatter.h>
#include <Poco/FormattingChannel.h>
#include <Poco/Logger.h>
#include <Poco/Message.h>
#include <Poco/Path.h>
#include <Poco/PatternFormatter.h>

using namespace Poco;
using namespace dragent;

namespace
{
class agentone_environment : public ::testing::Environment {

public:

private:

	void setup_common_logger()
	{
		std::string logDir  = "/tmp";
		File d(logDir);
		d.createDirectories();
		Path p;
		p.parseDirectory(logDir);
		p.setFileName("draios_test.log");
		std::string logsdir = p.toString();

		AutoPtr<globally_readable_file_channel> file_channel(new globally_readable_file_channel(logsdir, false));

		file_channel->setProperty("purgeCount", std::to_string(10));
		file_channel->setProperty("rotation", "10M");
		file_channel->setProperty("archive", "timestamp");

		AutoPtr<Formatter> formatter(new PatternFormatter("%Y-%m-%d %H:%M:%S.%i, %P, %p, %t"));
		AutoPtr<Channel> avoid_block(new avoid_block_channel(file_channel, "machine_test"));
		AutoPtr<Channel> formatting_channel_file(new FormattingChannel(formatter, avoid_block));

		AutoPtr<Channel> console_channel(new ConsoleChannel());
		AutoPtr<Channel> formatting_channel_console(new FormattingChannel(formatter, console_channel));
		Logger& loggerc = Logger::create("DraiosLogC", formatting_channel_console, Message::PRIO_DEBUG);

		Logger& loggerf = Logger::create("DraiosLogF", formatting_channel_file, Message::Priority::PRIO_DEBUG);
		std::vector<std::string> dummy_file_config;
		std::vector<std::string> dummy_console_config;

		g_log = std::unique_ptr<common_logger>(new common_logger(&loggerf,
									 &loggerc,
									 Message::Priority::PRIO_DEBUG,
									 Message::Priority::PRIO_DEBUG,
									 dummy_file_config,
									 dummy_console_config));
	}

	void SetUp() override
	{
		setup_common_logger();
	}

};

class EventListener : public ::testing::EmptyTestEventListener
{
public:
	EventListener(bool keep_capture_files)
	{
		m_keep_capture_files = keep_capture_files;
	}

	// Called before a test starts.
	virtual void OnTestStart(const ::testing::TestInfo &test_info)
	{
	}

	// Called after a failed assertion or a SUCCEED() invocation.
	virtual void OnTestPartResult(
	    const ::testing::TestPartResult &test_part_result)
	{
	}

	// Called after a test ends.
	virtual void OnTestEnd(const ::testing::TestInfo &test_info)
	{
		if(!m_keep_capture_files && !test_info.result()->Failed())
		{
			std::string dump_filename = std::string("./captures/") + test_info.test_case_name() + "_	" + test_info.name() + ".scap";
			remove(dump_filename.c_str());
		}
	}
private:
	bool m_keep_capture_files;
};
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

	::testing::AddGlobalTestEnvironment(new agentone_environment());
    return RUN_ALL_TESTS();
}
